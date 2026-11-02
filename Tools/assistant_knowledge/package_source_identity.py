"""Build a verified identity for a Git-free Renegade pre-alpha package.

Usage (after staging files and building the knowledge database):
  python Tools/assistant_knowledge/package_source_identity.py \
    --checkout <exact-git-worktree> --source <staged-KnowledgeSource> \
    --db <staged-BUILD/assistant_knowledge.sqlite>

This does not modify the release ZIP. Run for a NEW package at the release gate.
"""
from __future__ import annotations

import argparse
import json
import re
import sqlite3
from pathlib import Path
from contextlib import closing

try:
    from . import knowledge
except ImportError:
    import knowledge


def write_identity(checkout: Path, source: Path, database: Path) -> Path:
    checkout=checkout.resolve()
    source=source.resolve()
    database=database.resolve()
    if not source.is_dir() or (source / ".git").exists():
        raise ValueError("Package source must be staged and Git-free.")
    commit=knowledge.git_value(checkout, "rev-parse", "HEAD")
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Cannot verify the Git source revision.")
    with closing(sqlite3.connect(f"file:{database.as_posix()}?mode=ro",uri=True)) as db:
        metadata=dict(db.execute("SELECT key,value FROM metadata").fetchall())
        evidence=list(db.execute("SELECT id,evidence FROM facts WHERE authority='source_verified'"))
    if metadata.get("commit") != commit:
        raise ValueError("Knowledge database was not built from this exact Git commit.")
    if not evidence:
        raise ValueError("No source-verified facts available.")
    file_paths=set()
    for fact_id,source_json in evidence:
        for proof in json.loads(source_json):
            path=knowledge.relative_file(source,proof["path"])
            if knowledge.sha(path)!=proof["sha256"]:
                raise ValueError(f"Packaged source mismatch: {fact_id}: {proof['path']}")
            file_paths.add(proof["path"])
    record={
        "schema":1,
        "kind":"renegade_gitless_package",
        "commit":commit,
        "package_version":metadata.get("package_version",""),
        "database_sha256":knowledge.sha(database),
        "verified_fact_count":len(evidence),
        "verified_file_count":len(file_paths),
    }
    output=source/knowledge.PACKAGED_SOURCE_IDENTITY
    temporary=output.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(record,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    temporary.replace(output)
    if knowledge.source_revision(source,database)!=commit:
        raise ValueError("Packaged identity did not verify after writing.")
    return output


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--checkout",type=Path,required=True)
    parser.add_argument("--source",type=Path,required=True)
    parser.add_argument("--db",type=Path,required=True)
    args=parser.parse_args()
    print(f"VERIFIED_PACKAGED_IDENTITY={write_identity(args.checkout,args.source,args.db)}")


if __name__=="__main__":
    main()
