"""Simulates the exact Git-free KnowledgeSource + SQLite layout in a ZIP."""
from __future__ import annotations

import argparse
import json
import shutil
import sqlite3
import sys
import tempfile
from pathlib import Path
from contextlib import closing


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--repo",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    repo=args.repo.resolve()
    args.output.mkdir(parents=True,exist_ok=True)
    sys.path.insert(0,str(repo/"Tools"/"assistant_knowledge"))
    import knowledge
    import bridge_server
    import package_source_identity

    with tempfile.TemporaryDirectory(dir=args.output,prefix="packaged-kb-") as temp_name:
        temp=Path(temp_name)
        index=temp/"qa-knowledge.sqlite"
        knowledge.build(repo,index,repo/"Tools/assistant_knowledge/facts.json",
                        repo/"Tools/assistant_knowledge/legacy_guides.json")
        source=temp/"KnowledgeSource"
        source.mkdir(parents=True)
        database=temp/"BUILD"/"assistant_knowledge.sqlite"
        database.parent.mkdir(parents=True)
        shutil.copy2(index,database)
        with closing(sqlite3.connect(index)) as db:
            evidence=db.execute("SELECT evidence FROM facts WHERE authority='source_verified'").fetchall()
        source_files=set()
        for (raw,) in evidence:
            for proof in json.loads(raw):
                source_files.add(proof["path"])
        # Existing guides must survive packaging too, not only the new river cards.
        def collect(value):
            if isinstance(value, dict):
                for key in ("path", "manual_path", "receipt_path"):
                    relative = value.get(key)
                    if isinstance(relative, str) and (repo / relative).is_file():
                        source_files.add(relative)
                for item in value.values(): collect(item)
            elif isinstance(value, list):
                for item in value: collect(item)
        for definition in (repo / "Tools/assistant_knowledge").glob("*.json"):
            source_files.add(definition.relative_to(repo).as_posix())
            collect(json.loads(definition.read_text(encoding="utf-8")))
        for relative in source_files:
            dst=source/relative
            dst.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(knowledge.relative_file(repo,relative),dst)
        assert not (source/".git").exists()
        assert knowledge.source_revision(source,database)=="unknown"
        identity=package_source_identity.write_identity(repo,source,database)
        revision=knowledge.git_value(repo,"rev-parse","HEAD")
        assert knowledge.source_revision(source,database)==revision
        qa={
          "What is Renegade?":"Windows-first game engine",
          "How do I make a river?":"LINK / UPDATE BED",
          "Can I add a waterfall?":"ADD WATERFALL"
        }
        for question,needle in qa.items():
            answer=bridge_server.knowledge_reply(question,source,database)
            assert needle.casefold() in answer.casefold(),(question,answer)
        import workflow_answers
        registry=source/"Tools/assistant_knowledge/workflows.json"
        for question,needle in {
            "How do I import an OBJ model?":"CHOOSE / REPLACE MAP",
            "How do I save a projectile and assign it to my player?":"APPLY TO THIS PLAYER",
        }.items():
            answer=workflow_answers.verified_workflow(question,source,database,registry)
            assert answer and needle in answer,(question,answer)
        changed=source/"Studio/src/PlayerProjectileEditor.cpp"
        with changed.open("a",encoding="utf-8") as f: f.write("\n// source changed after packaging\n")
        assert workflow_answers.verified_workflow(
            "How do I save a projectile and assign it to my player?",source,database,registry) is None
        # Fail closed if the package database no longer matches its identity.
        manifest=json.loads(identity.read_text(encoding="utf-8"))
        manifest["database_sha256"]="0"*64
        identity.write_text(json.dumps(manifest),encoding="utf-8")
        assert knowledge.source_revision(source,database)=="unknown"
        rejected=bridge_server.knowledge_reply("What is Renegade?",source,database)
        assert "different revision" in rejected,(rejected)
        # Restoring the manifest does not bypass per-fact source file hashes.
        manifest["database_sha256"]=knowledge.sha(database)
        identity.write_text(json.dumps(manifest),encoding="utf-8")
        target=source/"README.md"
        with target.open("a",encoding="utf-8") as f:
            f.write("\nQA source-hash mismatch\n")
        untrusted=knowledge.search(database,"What is Renegade?",source,strict=True)
        assert not any(x["id"]=="renegade-identity" for x in untrusted["verified_facts"])
        print("PASS: Gitless package identity, three routed answers, existing Gitless workflows, stale index and changed source rejection")


if __name__=="__main__":
    main()
