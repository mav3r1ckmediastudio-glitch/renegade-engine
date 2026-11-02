"""Stage a recovery alpha from a clean commit; archive only after recorded acceptance.

Use --help. This never changes a previous archive, source checkout or user profile.
Inherited private assistant dependencies/media must come from a previous owner-approved ZIP.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import shutil
import sqlite3
import subprocess
import sys
import zipfile
from pathlib import Path, PurePosixPath

REQUIRED_CI = {"Windows x64 Debug", "Windows x64 Release",
               "Renegade Studio Windows x64 Debug", "Renegade Studio Windows x64 Release"}
REQUIRED_SMOKE = {"startup", "onboarding", "importer", "river", "weather",
                  "test_game", "build_game", "assistant"}

def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()

def safe_relative(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "\\" in name or ":" in name:
        raise ValueError(f"Unsafe archive path: {name}")
    return Path(*path.parts)

def copy_tree(source, target):
    if source.is_dir():
        shutil.copytree(source, target, dirs_exist_ok=True)

def tracked_tree(checkout, relative, target):
    paths = git(checkout, "ls-files", "-z", "--", relative).split("\0")
    for rel in filter(None, paths):
        src = checkout / rel
        if src.is_file():
            dst = target / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

def stage(args):
    checkout, build, root = args.checkout.resolve(), args.build.resolve(), args.output.resolve()
    commit = git(checkout, "rev-parse", "HEAD")
    # Dependency pins are verified separately; a dirty unused nested vcpkg example
    # must be preserved, not reset by a release packager.
    if git(checkout, "status", "--porcelain", "--untracked-files=no", "--ignore-submodules=all"):
        raise ValueError("Commit tracked source changes before staging")
    for sub in ("WickedEngine", "ThirdParty/imnodes"):
        pin = git(checkout, "rev-parse", f"HEAD:{sub}")
        if git(checkout / sub, "rev-parse", "HEAD") != pin:
            raise ValueError(f"Dependency pin differs: {sub}")
    if root.exists():
        raise FileExistsError("Refusing to overwrite an existing candidate")
    studio, runtime = build / "Studio/Release", build / "Runtime/Release"
    for binary in (studio / "RenegadeStudio.exe", runtime / "RenegadeRuntime.exe"):
        if not binary.is_file():
            raise FileNotFoundError(binary)
    root.mkdir(parents=True)
    with zipfile.ZipFile(args.base_zip) as previous:
        for entry in previous.infolist():
            rel = safe_relative(entry.filename)
            keep = entry.filename.startswith(("AssistantRuntime/", "LICENSES/", "Content/",
                    "Tools/assistant_presence/node_modules/"))
            keep |= len(rel.parts) == 1 and (rel.suffix.lower() == ".dll" or rel.name == "START_LIVING_CORE.cmd")
            if keep and not entry.is_dir():
                destination = root / rel
                destination.parent.mkdir(parents=True, exist_ok=True)
                # Streaming extraction validates ZIP CRC without a multi-GB allocation.
                with previous.open(entry) as source, destination.open("wb") as target:
                    shutil.copyfileobj(source, target)
    for rel in ("AssistantRuntime", "LICENSES", "Tools/assistant_presence/node_modules"):
        if not (root / rel).is_dir():
            raise ValueError(f"Previous package lacks required private runtime: {rel}")
    for rel in ("Tools/assistant_presence", "Tools/assistant_knowledge"):
        tracked_tree(checkout, rel, root)
    tracked_tree(checkout, "Tools/Read-RenegadeDiagnostics.py", root)
    for folder in ("Content", "BuildInputs", "shaders"):
        copy_tree(studio / folder, root / folder)
    for name in ("RenegadeStudio.exe", "dxcompiler.dll"):
        shutil.copy2(studio / name, root / name)
    (root / "Runtime").mkdir()
    for name in ("RenegadeRuntime.exe", "dxcompiler.dll"):
        shutil.copy2(runtime / name, root / "Runtime" / name)
    copy_tree(runtime / "Content", root / "Runtime/Content")
    source = root / "KnowledgeSource"
    for folder in ("EngineBridge", "Studio", "Runtime", "ScreenRenderer", "docs", "Tools/assistant_knowledge"):
        tracked_tree(checkout, folder, source)
    for name in ("README.md", "HANDOFF.md"):
        shutil.copy2(checkout / name, source / name)
    # Curated facts may cite source outside the usual companion source folders.
    def evidence(value):
        if isinstance(value, dict):
            name = value.get("path")
            if isinstance(name, str):
                rel = safe_relative(name)
                if (checkout / rel).is_file():
                    destination = source / rel
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(checkout / rel, destination)
            for item in value.values(): evidence(item)
        elif isinstance(value, list):
            for item in value: evidence(item)
    # Preserve evidence for existing manual/workflow guides as well as fact cards.
    for definition in (checkout / "Tools/assistant_knowledge").glob("*.json"):
        evidence(json.loads(definition.read_text(encoding="utf-8")))
    database = root / "BUILD/assistant_knowledge.sqlite"
    database.parent.mkdir()
    sys.path.insert(0, str(checkout / "Tools/assistant_knowledge"))
    import knowledge
    import package_source_identity
    knowledge.build(checkout, database, checkout / "Tools/assistant_knowledge/facts.json",
                    checkout / "Tools/assistant_knowledge/legacy_guides.json")
    # Version is attached before identity hashing; source revision is never fabricated.
    with sqlite3.connect(database) as db:
        db.execute("INSERT OR REPLACE INTO metadata(key,value) VALUES('package_version',?)", (args.version,))
    package_source_identity.write_identity(checkout, source, database)
    documentation = root / "Documentation"
    documentation.mkdir()
    shutil.copy2(checkout / "docs/releases/RECOVERY_ALPHA_2026-11-02.md", documentation / "RECOVERY_NOTES.md")
    if args.manual_zip:
        with zipfile.ZipFile(args.manual_zip) as manual:
            files = [name for name in manual.namelist() if name.lower().endswith(".pdf")]
            if len(files) != 1:
                raise ValueError("Expected one tester handbook PDF")
            with manual.open(files[0]) as src, (documentation / "Tester_Handbook_V4.pdf").open("wb") as dst:
                shutil.copyfileobj(src, dst)
    (root / "START_RENEGADE_ALPHA.cmd").write_text('@echo off\ncd /d "%~dp0"\nstart "" "%~dp0RenegadeStudio.exe" dx12\n')
    (root / "START_HERE_TESTERS.txt").write_text(
        f"Renegade recovery pre-alpha {args.version}\nExtract the complete ZIP to a writable folder.\n"
        "Run START_RENEGADE_ALPHA.cmd. First-run Living Core opens automatically for the name/handshake.\n"
        "Use copies of projects. Read Documentation/RECOVERY_NOTES.md before testing.\n"
        "The earlier V4 handbook is supplemented by these version-specific notes.\n"
        "Terrain darkness remains a known intermittent issue; report it rather than assuming it is fixed.\n")
    (root / "VERSION.txt").write_text(args.version + "\n" + commit + "\n")
    manifest = {"schema": 1, "version": args.version, "source_commit": commit,
        "source_tree": git(checkout, "rev-parse", "HEAD^{tree}"),
        "wicked_commit": git(checkout / "WickedEngine", "rev-parse", "HEAD"),
        "base_archive_sha256": sha(args.base_zip), "status": "staged_pending_package_acceptance",
        "provenance": "Private dependencies/media inherited; native binaries and companion source replaced from recovery build",
        "files": {}}
    for path in sorted(root.rglob("*")):
        if path.is_file(): manifest["files"][path.relative_to(root).as_posix()] = sha(path)
    (root / "RELEASE_MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"stage": str(root), "source_commit": commit, "files": len(manifest["files"])}))

def verify(root):
    manifest = json.loads((root / "RELEASE_MANIFEST.json").read_text())
    for name, digest in manifest["files"].items():
        if sha(root / safe_relative(name)) != digest:
            raise ValueError(f"Staged file mismatch: {name}")
    return manifest

def archive(args):
    root = args.stage.resolve()
    if args.output.resolve().is_relative_to(root):
        raise ValueError("Archive output must be outside the stage")
    manifest = verify(root)
    acceptance = json.loads(args.validation.read_text())
    commit = manifest["source_commit"]
    github = acceptance["github"]
    if acceptance["source_commit"] != commit or github["head_sha"] != commit:
        raise ValueError("Acceptance belongs to a different source revision")
    passed = {check["name"] for check in github["checks"] if check["conclusion"].upper() == "SUCCESS"}
    if not REQUIRED_CI <= passed or acceptance["local"]["ctest_exit_code"] != 0:
        raise ValueError("All four exact-commit CI checks and local CTest are required")
    smoke = acceptance["packaged_smoke"]
    if not all(smoke.get(name, {}).get("passed") is True for name in REQUIRED_SMOKE):
        raise ValueError("Actual packaged workflow acceptance is incomplete")
    if args.output.exists():
        raise FileExistsError("Refusing to overwrite an existing alpha archive")
    shutil.copy2(args.validation, root / "VALIDATION.json")
    manifest["files"]["VALIDATION.json"] = sha(root / "VALIDATION.json")
    manifest["status"] = "private_testing_candidate_verified"
    (root / "RELEASE_MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n")
    with zipfile.ZipFile(args.output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=1, allowZip64=True) as output:
        # Runtime smoke logs, profiles and other unlisted test outputs are not shipped.
        for name in sorted(set(manifest["files"]) | {"RELEASE_MANIFEST.json"}):
            output.write(root / safe_relative(name), name)
    # Read every ZIP member to validate CRC, without retaining another full copy.
    with zipfile.ZipFile(args.output) as output:
        failed = output.testzip()
        if failed: raise ValueError(f"Archive CRC failure: {failed}")
    digest = sha(args.output)
    args.output.with_suffix(args.output.suffix + ".sha256").write_text(digest + "  " + args.output.name + "\n")
    print(json.dumps({"archive": str(args.output), "sha256": digest, "bytes": args.output.stat().st_size}))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    prepare = sub.add_parser("stage")
    for option in ("checkout", "build", "base-zip", "output"):
        prepare.add_argument("--" + option, type=Path, required=True)
    prepare.add_argument("--version", required=True)
    prepare.add_argument("--manual-zip", type=Path)
    finish = sub.add_parser("archive")
    for option in ("stage", "validation", "output"):
        finish.add_argument("--" + option, type=Path, required=True)
    inspect = sub.add_parser("verify")
    inspect.add_argument("--stage", type=Path, required=True)
    args = parser.parse_args()
    if args.action == "stage": stage(args)
    elif args.action == "archive": archive(args)
    else: print(json.dumps({"source_commit": verify(args.stage)["source_commit"], "verified": True}))

if __name__ == "__main__":
    main()
