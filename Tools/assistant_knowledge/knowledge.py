"""Renegade RA-01: local, provenance-aware, read-only knowledge index.

Standard library only. This is not an LLM prompt executor or editor bridge.
Uncurated repository documents are reference material, never verified instructions.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import subprocess

HERE = Path(__file__).resolve().parent
DEFAULT_REPO = HERE.parent.parent
STOP = set("a an and are as at be by can could did do does for from how i in is it me my of on or please should that the this to use was what when where who why will with you your".split())


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative_file(root: Path, relative: str) -> Path:
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError("Missing/unsafe source path: " + relative)
    return path


def git_value(root: Path, *args: str) -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", str(root), *args], text=True,
            stderr=subprocess.DEVNULL, timeout=8
        ).strip()
    except (OSError, subprocess.SubprocessError):
        return "unknown"


def passages(markdown: str, max_length: int = 1100):
    section = "Document"
    buffer = ""
    for line in markdown.splitlines():
        if line.lstrip().startswith("#"):
            section = line.lstrip("# ").strip()[:150] or section
        if not line.strip():
            if buffer:
                while buffer:
                    yield section, buffer[:max_length]
                    buffer = buffer[max_length:]
            continue
        if buffer:
            buffer += "\n"
        buffer += line
        while len(buffer) >= max_length:
            yield section, buffer[:max_length]
            buffer = buffer[max_length:]
    while buffer:
        yield section, buffer[:max_length]
        buffer = buffer[max_length:]


def build(repo: Path, database: Path, facts_file: Path, legacy_file: Path | None):
    repo = repo.resolve()
    if not (repo / "README.md").exists():
        raise ValueError("Not a Renegade repo: README.md missing")
    definition = json.loads(facts_file.read_text(encoding="utf-8"))
    if definition.get("schema") != 1:
        raise ValueError("Unsupported facts schema")
    database.parent.mkdir(parents=True, exist_ok=True)
    temporary = database.with_name(database.name + ".building")
    if temporary.exists():
        temporary.unlink()
    db = sqlite3.connect(temporary)
    db.executescript("""
        CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE TABLE facts (
            id TEXT PRIMARY KEY, title TEXT NOT NULL, answer TEXT NOT NULL,
            tags TEXT NOT NULL, authority TEXT NOT NULL,
            evidence TEXT NOT NULL, failure TEXT NOT NULL
        );
        CREATE VIRTUAL TABLE facts_fts USING fts5(id, title, tags, answer);
        CREATE TABLE legacy_guides (
            id TEXT PRIMARY KEY, title TEXT NOT NULL, tags TEXT NOT NULL,
            pinned_revision TEXT NOT NULL, evidence TEXT NOT NULL
        );
        CREATE VIRTUAL TABLE legacy_fts USING fts5(id, title, tags);
        CREATE TABLE manual_guides (
            id TEXT PRIMARY KEY, title TEXT NOT NULL, manual_path TEXT NOT NULL,
            status TEXT NOT NULL, evidence TEXT NOT NULL, issues TEXT NOT NULL,
            manual_hash TEXT NOT NULL
        );
        CREATE VIRTUAL TABLE manual_guides_fts USING fts5(id, title);
        CREATE TABLE passages (
            id INTEGER PRIMARY KEY, path TEXT NOT NULL, title TEXT NOT NULL,
            body TEXT NOT NULL, authority TEXT NOT NULL,
            file_hash TEXT NOT NULL
        );
        CREATE VIRTUAL TABLE passages_fts USING fts5(title, body, path);
    """)
    revision = git_value(repo, "rev-parse", "HEAD")
    branch = git_value(repo, "branch", "--show-current")
    dirty = git_value(repo, "status", "--porcelain")
    metadata = {
        "schema": "1", "commit": revision, "branch": branch,
        "dirty_checkout_at_build": str(bool(dirty)).lower(),
        "built_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "sources": "local checkout, no internet; Markdown is reference-only",
    }
    db.executemany("INSERT INTO metadata VALUES (?, ?)", metadata.items())
    verified = 0
    for fact in definition["facts"]:
        evidence = []
        errors = []
        for source in fact["sources"]:
            try:
                path = relative_file(repo, source["path"])
                content = path.read_text(encoding="utf-8")
                if source["needle"] not in content:
                    errors.append("Missing evidence in " + source["path"])
                evidence.append({"path": source["path"], "sha256": sha(path),
                                 "needle": source["needle"]})
            except (OSError, ValueError, UnicodeError) as error:
                errors.append(str(error))
        status = "source_verified" if not errors else "requires_review"
        verified += int(status == "source_verified")
        tags = " ".join(fact["tags"])
        db.execute("INSERT INTO facts VALUES (?,?,?,?,?,?,?)",
                   (fact["id"], fact["title"], fact["answer"], tags,
                    status, json.dumps(evidence), "; ".join(errors)))
        db.execute("INSERT INTO facts_fts(id,title,tags,answer) VALUES (?,?,?,?)",
                   (fact["id"], fact["title"], tags, fact["answer"]))
    manual_count = 0
    for guide_path in sorted(facts_file.parent.glob("*_guide.json")):
        guide = json.loads(guide_path.read_text(encoding="utf-8"))
        if not (repo / guide["manual_path"]).is_file():
            continue
        if guide.get("schema") != 1:
            raise ValueError("Unsupported manual-guide schema")
        manual = relative_file(repo, guide["manual_path"])
        issues = []
        evidence = []
        for fact in guide["facts"]:
            for source in fact["sources"]:
                try:
                    source_file = relative_file(repo, source["path"])
                    if source["needle"] not in source_file.read_text(encoding="utf-8"):
                        issues.append(fact["id"] + ": missing source marker: " + source["path"])
                    evidence.append({"path": source["path"], "sha256": sha(source_file),
                                     "needle": source["needle"], "fact": fact["id"]})
                except (OSError, ValueError, UnicodeError) as error:
                    issues.append(fact["id"] + ": " + str(error))
        # Source verification is not the same as creator acceptance in Studio.
        status = "draft_source_matched" if not issues else "draft_evidence_failed"
        db.execute("INSERT INTO manual_guides VALUES (?,?,?,?,?,?,?)",
                   (guide["id"], guide.get("title", "Create your first projectile weapon"),
                    guide["manual_path"], status, json.dumps(evidence),
                    json.dumps(issues), sha(manual)))
        db.execute("INSERT INTO manual_guides_fts VALUES (?,?)",
                   (guide["id"], guide.get("title", "Create your first projectile weapon")))
        manual_count += 1
    legacy_count = 0
    if legacy_file and legacy_file.is_file():
        legacy = json.loads(legacy_file.read_text(encoding="utf-8"))
        for guide in legacy.get("guides", []):
            tags = " ".join(guide.get("tags", []))
            db.execute("INSERT INTO legacy_guides VALUES (?,?,?,?,?)",
                       (guide["id"], guide["title"], tags,
                        legacy.get("revision", "unknown"),
                        json.dumps(guide.get("sources", []))))
            db.execute("INSERT INTO legacy_fts VALUES (?,?,?)",
                       (guide["id"], guide["title"], tags))
            legacy_count += 1
    documents = [repo / "README.md", *(repo / "docs").rglob("*.md")]
    document_count = 0
    chunk_count = 0
    for doc in sorted(documents):
        if not doc.is_file():
            continue
        path = doc.relative_to(repo).as_posix()
        digest = sha(doc)
        try:
            data = doc.read_text(encoding="utf-8")
        except UnicodeError:
            continue
        document_count += 1
        for title, body in passages(data):
            db.execute("INSERT INTO passages(path,title,body,authority,file_hash) VALUES (?,?,?,?,?)",
                       (path, title, body, "reference_only", digest))
            rid = db.execute("SELECT last_insert_rowid()").fetchone()[0]
            db.execute("INSERT INTO passages_fts(rowid,title,body,path) VALUES (?,?,?,?)",
                       (rid, title, body, path))
            chunk_count += 1
    db.commit()
    db.close()
    os.replace(temporary, database)
    return {"database": str(database), "commit": revision, "branch": branch,
            "dirty_checkout": bool(dirty), "verified_facts": verified,
            "all_facts": len(definition["facts"]), "manual_guides": manual_count,
            "legacy_guides": legacy_count,
            "reference_documents": document_count, "reference_passages": chunk_count}


def match_expression(question: str) -> str:
    tokens = [word for word in re.findall(r"[a-z0-9_]+", question.lower())
              if len(word) > 1 and word not in STOP]
    return " OR ".join('"' + token + '"' for token in dict.fromkeys(tokens[:12]))


def search(database: Path, question: str, repo: Path, limit: int = 4, strict: bool = False):
    expression = match_expression(question)
    if not expression:
        return {"verified_facts": [], "historical_guides": [],
                "references": [], "answer_policy": "No grounded answer; ask for a specific topic."}
    db = sqlite3.connect(f"file:{database.resolve().as_posix()}?mode=ro", uri=True)
    db.row_factory = sqlite3.Row
    facts = []
    query_terms = set(re.findall(r"[a-z0-9_]+", question.lower())) - STOP
    # A generic product name is not evidence that an unrelated capability exists.
    meaningful_terms = query_terms - {"renegade", "studio", "engine", "game", "assistant", "create", "make", "build", "help"}
    for row in db.execute("""
        SELECT f.* FROM facts_fts
        JOIN facts f ON facts_fts.id=f.id
        WHERE facts_fts MATCH ? ORDER BY bm25(facts_fts) LIMIT ?
    """, (expression, max(limit * 3, 12))):
        tags_and_title = set(re.findall(
            r"[a-z0-9_]+", (row["tags"] + " " + row["title"]).lower()))
        if not (query_terms & tags_and_title):
            continue
        if meaningful_terms and not (meaningful_terms & tags_and_title):
            continue
        evidence = json.loads(row["evidence"])
        current = row["authority"] == "source_verified"
        for source in evidence:
            try:
                current = current and (sha(relative_file(repo, source["path"])) == source["sha256"])
            except (OSError, ValueError):
                current = False
        if current:
            facts.append({"id": row["id"], "title": row["title"],
                          "answer": row["answer"], "authority": "source_verified",
                          "sources": [s["path"] for s in evidence]})
            if len(facts) == limit:
                break
    guides = []
    manual_guides = []
    references = []
    if not strict:
        for row in db.execute("""
            SELECT m.* FROM manual_guides_fts
            JOIN manual_guides m ON manual_guides_fts.id=m.id
            WHERE manual_guides_fts MATCH ? LIMIT ?
        """, (expression, limit)):
            evidence = json.loads(row["evidence"])
            valid = True
            try:
                valid = sha(relative_file(repo, row["manual_path"])) == row["manual_hash"]
                for source in evidence:
                    valid = valid and sha(relative_file(repo, source["path"])) == source["sha256"]
            except (OSError, ValueError):
                valid = False
            manual_guides.append({
                "id": row["id"], "path": row["manual_path"],
                "authority": row["status"] if valid else "draft_stale_rebuild_required",
                "issues": json.loads(row["issues"]),
                "publication_approved": False
            })
        for row in db.execute("""
            SELECT g.* FROM legacy_fts
            JOIN legacy_guides g ON legacy_fts.id=g.id
            WHERE legacy_fts MATCH ? ORDER BY bm25(legacy_fts) LIMIT ?
        """, (expression, limit)):
            guides.append({"id": row["id"], "title": row["title"],
                           "authority": "historical_needs_revalidation",
                           "source_revision": row["pinned_revision"]})
        for row in db.execute("""
            SELECT p.path,p.title,p.body,p.file_hash FROM passages_fts
            JOIN passages p ON passages_fts.rowid=p.id
            WHERE passages_fts MATCH ? ORDER BY bm25(passages_fts) LIMIT ?
        """, (expression, limit)):
            try:
                if sha(relative_file(repo, row["path"])) != row["file_hash"]:
                    continue
            except (OSError, ValueError):
                continue
            references.append({"path": row["path"], "title": row["title"],
                               "excerpt": row["body"][:600],
                               "authority": "reference_only_not_feature_proof"})
    metadata = dict(db.execute("SELECT key,value FROM metadata").fetchall())
    db.close()
    changed_checkout = git_value(repo, "rev-parse", "HEAD") != metadata["commit"]
    return {
        "verified_facts": facts,
        "historical_guides": guides, "manual_guides": manual_guides,
        "references": references,
        "indexed_commit": metadata["commit"],
        "checkout_changed_since_index": changed_checkout,
        "answer_policy": ("Use verified_facts only for claims about implemented features. "
                          "References may mix plans, handoffs and old versions. "
                          "Never claim current selection/project access or action execution "
                          "without an editor receipt." if facts else
                          "No verified answer for this topic; do not infer implementation "
                          "from reference passages or historical guides.")
    }


def main():
    parser = argparse.ArgumentParser(description="Read-only Renegade knowledge database")
    sub = parser.add_subparsers(dest="action", required=True)
    for action in ("build", "ask"):
        p = sub.add_parser(action)
        p.add_argument("--repo", type=Path, default=DEFAULT_REPO)
        p.add_argument("--db", type=Path, default=None)
        if action == "build":
            p.add_argument("--facts", type=Path, default=HERE / "facts.json")
            p.add_argument("--legacy", type=Path, default=HERE / "legacy_guides.json")
        else:
            p.add_argument("question")
            p.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    database = args.db or args.repo / "BUILD" / "assistant_knowledge.sqlite"
    if args.action == "build":
        result = build(args.repo, database, args.facts, args.legacy)
    else:
        result = search(database, args.question, args.repo, strict=args.strict)
    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
