#!/usr/bin/env python3
"""Read running Renegade on THIS machine. Python 3.9+, standard library only."""
import argparse
import concurrent.futures
import http.client
import json
import sys

MAX_BYTES = 4 * 1024 * 1024


def read_snapshot(kind, port, timeout=2.0):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        connection.request("GET", "/snapshot", headers={"Connection": "close"})
        response = connection.getresponse()
        if response.status != 200:
            raise ValueError(f"HTTP {response.status}")
        raw = response.read(MAX_BYTES + 1)
        if len(raw) > MAX_BYTES:
            raise ValueError("snapshot exceeds size limit")
        snapshot = json.loads(raw)
        if not isinstance(snapshot, dict):
            raise ValueError("snapshot must be an object")
        if snapshot.get("schema") != "renegade.diagnostics.v2":
            raise ValueError("unsupported diagnostics schema")
        if not isinstance(snapshot.get("state"), dict):
            raise ValueError("state must be an object")
        identity = snapshot["state"]["process"]
        if not isinstance(identity, dict):
            raise ValueError("process identity must be an object")
        if identity.get("type") != kind:
            raise ValueError("endpoint process type does not match requested process")
        age = snapshot["heartbeat_age_ms"]
        if not isinstance(age, int) or age < 0:
            raise ValueError("invalid heartbeat age")
        return {"available": True, "responsive": age <= 2000, "snapshot": snapshot}
    except (OSError, ValueError, KeyError, TypeError, http.client.HTTPException) as error:
        return {"available": False, "error": str(error)}
    finally:
        connection.close()


def collect(process="both", studio_port=38741, runtime_port=38742, expected_commit=None):
    requested = {"studio": studio_port, "runtime": runtime_port}
    if process != "both":
        requested = {process: requested[process]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        tasks = {kind: pool.submit(read_snapshot, kind, port) for kind, port in requested.items()}
        result = {kind: task.result() for kind, task in tasks.items()}
    findings = []
    for kind, item in result.items():
        if not item["available"]:
            continue
        snapshot = item["snapshot"]
        commit = snapshot["state"]["process"].get("build_commit")
        if expected_commit and commit != expected_commit:
            findings.append(f"{kind}: build does not match expected commit {expected_commit}")
        if not item["responsive"]:
            findings.append(f"{kind}: endpoint responds but application heartbeat is stale")
    studio = result.get("studio", {})
    runtime = result.get("runtime", {})
    if studio.get("available") and runtime.get("available"):
        ss, rs = studio["snapshot"]["state"], runtime["snapshot"]["state"]
        a, b = ss["process"].get("build_commit"), rs["process"].get("build_commit")
        result["build_match"] = a == b if a and b and a != "unknown" and b != "unknown" else None
        if result["build_match"] is False:
            findings.append("Studio and Runtime build commits differ")
        child = ss.get("test_level", {}).get("child_pid", 0)
        result["runtime_is_studio_child"] = bool(child and child == rs["process"].get("pid"))
        if child and not result["runtime_is_studio_child"]:
            findings.append("Runtime endpoint belongs to a different process than Studio's Test Level child")
    if studio.get("available") and studio["snapshot"]["state"].get("test_level", {}).get("active") and not runtime.get("available") and process == "both":
        findings.append("Studio reports active Test Level, but Runtime diagnostics are unavailable")
    result["findings"] = findings
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--process", choices=("studio", "runtime", "both"), default="both")
    parser.add_argument("--studio-port", type=int, default=38741)
    parser.add_argument("--runtime-port", type=int, default=38742)
    parser.add_argument("--expected-commit", help="full 40-character artifact commit")
    parser.add_argument("--events", choices=("all", "errors", "warnings"), default="all")
    args = parser.parse_args()
    if any(not 1 <= port <= 65535 for port in (args.studio_port, args.runtime_port)):
        parser.error("ports must be 1..65535")
    result = collect(args.process, args.studio_port, args.runtime_port, args.expected_commit)
    if args.events != "all":
        severity = "error" if args.events == "errors" else "warning"
        for kind in ("studio", "runtime"):
            if result.get(kind, {}).get("available"):
                snapshot = result[kind]["snapshot"]
                snapshot["events"] = [e for e in snapshot.get("events", []) if e.get("severity") == severity]
    print(json.dumps(result, indent=2, ensure_ascii=True))
    available = any(result.get(k, {}).get("available") for k in ("studio", "runtime"))
    return 2 if not available else 1 if result["findings"] else 0


if __name__ == "__main__":
    sys.exit(main())
