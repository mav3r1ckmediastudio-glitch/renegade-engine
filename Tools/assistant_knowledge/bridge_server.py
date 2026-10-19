"""RA-01 / RA-02 local knowledge-first bridge.

Runs as a separate opt-in loopback-only service. No editor privileges,
filesystem writing, cloud connection or model weights. Calls only the
existing experimental Qwen service for *general conversation*.

Start:
  C:\\Python314\\python.exe Tools/assistant_knowledge/bridge_server.py
Native Studio experimental assistant connects to 127.0.0.1:8384/chat.
"""
from __future__ import annotations

import argparse
import json
import re
import sqlite3
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

try:
    from . import knowledge, workflow_answers, dialogue_guidance
except ImportError:
    import knowledge
    import workflow_answers
    import dialogue_guidance

HERE = Path(__file__).resolve().parent
ENGINE_TERMS = frozenset({
    "renegade", "wicked", "studio", "editor", "inspector", "projectile",
    "projectiles", "hitscan", "arrow", "arrows", "bullet", "bullets", "bolt", "bolts", "spawn", "bow", "weapon",
    "glass", "impact", "impacts", "decals", "decal", "damage", "sfx", "fx",
    "weapons", "equipment", "loadout", "loadouts", "surface", "surfaces", "retarget", "script",
    "npc", "scene", "scenes", "asset", "assets", "launch", "player",
    "model", "import", "importer", "terrain", "animation",
    "character", "characters", "npcs", "scripts", "action", "actions", "lua",
    "patrol", "wander", "wandering", "socket", "sockets", "psp", "timing", "gravity",
})
CAPABILITY_TERMS = frozenset({"engine", "world", "game", "level"})
CAPABILITY_VERBS = frozenset({"create", "build", "generate", "automatically",
                               "autonomous", "make", "implement", "supports"})
MAX_REQUEST_BYTES = 32 * 1024
MAX_ANSWER_LENGTH = 1100


def is_renegade_question(question: str) -> bool:
    tokens = set(re.findall(r"[a-z0-9_]+", question.lower()))
    import_request=bool(re.search(
        r"\b(?:import|load|bring in)\b.{0,55}\b(?:mesh|glb|fbx|3d object)s?\b",
        question,re.IGNORECASE))
    stock_request=bool(re.search(
        r"\b(?:set up|setup|make|create|add|attach|wire|connect|trigger)\b.{0,50}\b(?:sliding door|interactive door|door action|switch|trigger zone|pickup|relay|sound action|play sound action)\b",
        question,re.IGNORECASE))
    return bool(stock_request or import_request or tokens & ENGINE_TERMS or
                (tokens & CAPABILITY_TERMS and tokens & CAPABILITY_VERBS))


def last_user_question(body: dict) -> str:
    messages = body.get("messages", [])
    if not isinstance(messages, list) or len(messages) > 32:
        raise ValueError("Expected a bounded messages list")
    for message in reversed(messages):
        if (isinstance(message, dict) and message.get("role") == "user" and
                isinstance(message.get("content"), str)):
            question = message["content"].strip()
            if not question or len(question) > 1200:
                raise ValueError("Invalid question length")
            return question
    raise ValueError("No user question")


def knowledge_reply(question: str, repo: Path, database: Path) -> str:
    """Only verified source facts; never use historical docs as authority."""
    try:
        result = knowledge.search(database, question, repo, limit=3, strict=True)
    except (OSError, ValueError, KeyError, sqlite3.Error):
        return ("I can't verify Renegade's documentation index right now. "
                "Try again after the local knowledge database is rebuilt.")
    if result.get("checkout_changed_since_index"):
        return ("Renegade's knowledge index was built against a different "
                "revision. Rebuild it before relying on feature instructions.")
    # HOW-TO requests take a QA-backed ordered workflow before isolated facts.
    # No model-generated instructions and no mixing unrelated retrieved facts.
    recipe = workflow_answers.verified_workflow(question, repo, database)
    if recipe:
        return recipe
    if re.search(r"\b(?:how|steps|show me)\b",question,re.IGNORECASE):
        return ("I do not have verified step-by-step instructions for that specific task. "
                "Tell me the editor panel and what you want to change; I will not "
                "substitute unrelated facts or invent controls.")
    facts = result["verified_facts"]
    if not facts:
        return ("I don't have a verified Renegade answer for that yet. "
                "I won't guess at editor features. The knowledge database "
                "can be expanded from the current source and tested workflow.")
    parts = []
    budget = MAX_ANSWER_LENGTH
    for fact in facts:
        source = fact["sources"][0] if fact.get("sources") else ""
        paragraph = fact["answer"].strip()
        citation = f" [Source: {source}]" if source else ""
        entry = paragraph + citation
        if parts and len("\n\n".join(parts)) + len(entry) + 2 > budget:
            break
        parts.append(entry[:budget])
    return "\n\n".join(parts)[:MAX_ANSWER_LENGTH]


def process_chat(body: dict, repo: Path, database: Path,
                 qwen_port: int = 8383, qwen_timeout: int = 45) -> dict:
    question = last_user_question(body)
    bounded = (dialogue_guidance.scope_reply(question) or
               dialogue_guidance.clarification(question,repo,database) or
               dialogue_guidance.followup_reply(question,body.get("messages",[]),repo,database))
    if bounded:
        return {"answer": bounded, "route": "local_verified_knowledge", "editor_access": False}
    if is_renegade_question(question):
        return {"answer": knowledge_reply(question, repo, database),
                "route": "local_verified_knowledge", "editor_access": False}
    # Ordinary chat retains existing Qwen behaviour. This bridge must never
    # inject tool calls, execute editor commands or change project files.
    original = json.dumps(body, ensure_ascii=False).encode("utf-8")
    request = Request(f"http://127.0.0.1:{qwen_port}/chat", data=original,
                      headers={"Content-Type": "application/json"},
                      method="POST")
    try:
        with urlopen(request, timeout=qwen_timeout) as response:
            if response.status != 200:
                raise RuntimeError("Local conversation model returned an HTTP error")
            raw = response.read(128 * 1024 + 1)
            if len(raw) > 128 * 1024:
                raise ValueError("Local conversation response too large")
            result = json.loads(raw)
        if not isinstance(result, dict) or not isinstance(result.get("answer"), str):
            raise ValueError("Local conversation model returned no answer")
        return {"answer": result["answer"][:MAX_ANSWER_LENGTH],
                "route": "local_qwen_conversation", "editor_access": False}
    except (OSError, ValueError, RuntimeError, HTTPError, URLError):
        return {"answer": ("The optional local conversation model is unavailable. "
                           "Verified Renegade knowledge questions still work."),
                "route": "qwen_unavailable", "editor_access": False}


def handler_factory(repo: Path, database: Path, qwen_port: int):
    class KnowledgeBridgeHandler(BaseHTTPRequestHandler):
        server_version = "RenegadeLocalKnowledgeBridge/0.1"

        def log_message(self, format, *args):
            # No chat/question content is written to logs.
            pass

        def do_GET(self):
            if self.path != "/health":
                self.send_error(404)
                return
            payload = {"ok": database.is_file(), "read_only": True,
                       "model_weights_added": False}
            self._respond(200 if payload["ok"] else 503, payload)

        def do_POST(self):
            if self.path != "/chat":
                self.send_error(404)
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length <= MAX_REQUEST_BYTES:
                    self._respond(413, {"error": "Invalid request size"})
                    return
                data = json.loads(self.rfile.read(length))
                if not isinstance(data, dict):
                    raise ValueError("Expected JSON object")
                reply = process_chat(data, repo, database, qwen_port=qwen_port)
                self._respond(200, reply)
            except (ValueError, TypeError, UnicodeError):
                self._respond(400, {"error": "Invalid local chat request"})

        def _respond(self, status: int, data: dict):
            raw = json.dumps(data, ensure_ascii=False).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

    return KnowledgeBridgeHandler


def main():
    parser = argparse.ArgumentParser(description="Read-only localhost Renegade assistant bridge")
    parser.add_argument("--repo", type=Path, default=knowledge.DEFAULT_REPO)
    parser.add_argument("--db", type=Path, default=None)
    parser.add_argument("--port", type=int, default=18384)
    parser.add_argument("--qwen-port", type=int, default=8383)
    args = parser.parse_args()
    if args.port == args.qwen_port:
        parser.error("Bridge and Qwen ports must differ")
    repo = args.repo.resolve()
    db = (args.db or repo / "BUILD/assistant_knowledge.sqlite").resolve()
    if not db.is_file():
        parser.error("No database; run knowledge.py build first")
    server = ThreadingHTTPServer(("127.0.0.1", args.port),
                                 handler_factory(repo, db, args.qwen_port))
    print(f"Renegade read-only bridge active on 127.0.0.1:{args.port}; "
          f"Qwen fallback is 127.0.0.1:{args.qwen_port}", flush=True)
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
