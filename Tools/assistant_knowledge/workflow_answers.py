"""Strict, source-checked step-by-step workflow answers for Renegade.

Reference-only Markdown is never promoted by search ranking. Every workflow
must be explicitly curated, have fresh source evidence, and reference a
source-matched manual with native QA receipt. Read-only. No LLM interpolation.
"""
from __future__ import annotations
import hashlib
import json
import re
import sqlite3
from contextlib import closing
from pathlib import Path

try:
    from . import knowledge
except ImportError:
    import knowledge

HERE=Path(__file__).resolve().parent
WORKFLOWS=HERE/"workflows.json"

def verified_workflow(question: str, repo: Path, database: Path,
                      registry: Path = WORKFLOWS) -> str | None:
    repo=repo.resolve()
    try:
        data=json.loads(registry.read_text(encoding="utf-8"))
        if data.get("schema")!=1 or not isinstance(data.get("workflows"),list):
            return None
        for task in data["workflows"]:
            patterns=task["match"]
            if not any(re.search(pattern,question,re.IGNORECASE) for pattern in patterns):
                continue
            if any(re.search(p,question,re.IGNORECASE) for p in task.get("exclude",[])):
                continue
            if not all(isinstance(s,str) and s.strip() for s in
                       [task.get("prerequisites"),task.get("follow_up"),
                        task.get("limit"),*task["steps"]]):
                return None
            with closing(sqlite3.connect(
              f"file:{database.resolve().as_posix()}?mode=ro",uri=True)) as db:
                indexed=db.execute(
                    "SELECT value FROM metadata WHERE key='commit'").fetchone()
                row=db.execute(
                    "SELECT status, manual_path, manual_hash, evidence, issues "
                    "FROM manual_guides WHERE id=?",(task["guide_id"],)).fetchone()
            if not indexed or indexed[0]!=knowledge.git_value(repo,"rev-parse","HEAD"):
                return None
            if not row or row[0]!="draft_source_matched" or json.loads(row[4]):
                return None
            manual=knowledge.relative_file(repo,row[1])
            if knowledge.sha(manual)!=row[2]:
                return None
            # Enforce the already indexed evidence for the native-tested manual,
            # not just a match on a coincidental workflow keyword.
            for proof in json.loads(row[3]):
                p=knowledge.relative_file(repo,proof["path"])
                if knowledge.sha(p)!=proof["sha256"]:
                    return None
            receipt_path=task.get("receipt_path","Tools/assistant_knowledge/projectile_guide.json")
            receipt=json.loads(knowledge.relative_file(repo,receipt_path).read_text(encoding="utf-8"))
            accepted_status=task.get("acceptance_status","native_workflow_passed_existing_bow_fixture")
            if accepted_status not in {
                "native_workflow_passed_existing_bow_fixture",
                "source_checked_prior_owner_checkpoint_native_service_pass",
                "source_checked_native_service_tests_passed_ui_pending",
            }:
                return None
            if receipt.get("id")!=task["guide_id"] or (
                receipt.get("end_to_end_acceptance",{}).get("status")!=accepted_status
            ):
                return None
            checks=task.get("native_checks",[])
            if checks and not all(check in receipt["end_to_end_acceptance"].get(
                    "steps_confirmed",[]) for check in checks):
                return None
            for proof in task["evidence"]:
                p=knowledge.relative_file(repo,proof["path"])
                if proof["needle"] not in p.read_text(encoding="utf-8"):
                    return None
                # New task evidence is pinned to whole files, not only labels.
                if proof.get("sha256") and knowledge.sha(p)!=proof["sha256"]:
                    return None
            extra=""
            if "expected_result" in task:
                if not isinstance(task["expected_result"],str) or not task["expected_result"].strip():
                    return None
                extra+=f"**Expected result:** {task['expected_result']}\n\n"
            if "troubleshooting" in task:
                tips=task["troubleshooting"]
                if not isinstance(tips,list) or not tips or not all(
                        isinstance(t,str) and t.strip() for t in tips):
                    return None
                extra+="**Troubleshooting:**\n"+"\n".join(f"- {t}" for t in tips)+"\n\n"
            steps="\n".join(f"{i}. {value}" for i,value in enumerate(task["steps"],1))
            sources=" ".join(f"[Source: {path}]" for path in task["provenance"])
            return (
                f"Here's how to {task['label'].lower()} in Renegade Studio:\n\n"
                f"**Before you start:** {task['prerequisites']}\n\n"
                f"{steps}\n\n"
                f"**Next:** {task['follow_up']}\n\n"
                f"{extra}{task['limit']}\n\n{sources}"
            )
    except (OSError, ValueError, KeyError, TypeError, sqlite3.Error, re.error):
        # A stale or malformed recipe is not grounds to invent steps.
        return None
    return None
