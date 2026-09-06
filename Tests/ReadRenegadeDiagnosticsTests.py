import contextlib
import importlib.util
import json
from pathlib import Path
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

path = Path(__file__).resolve().parents[1] / "Tools" / "Read-RenegadeDiagnostics.py"
spec = importlib.util.spec_from_file_location("reader", path)
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)


def snapshot(kind="studio", commit="a" * 40, pid=1):
    return {"schema": "renegade.diagnostics.v2", "heartbeat_age_ms": 0,
            "application_heartbeat_age_ms": 0, "transport_alive": True,
            "process_foreground": True,
            "state": {"process": {"type": kind, "build_commit": commit, "pid": pid}}, "events": []}


@contextlib.contextmanager
def serve(value):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            data = json.dumps(value).encode()
            self.send_response(200)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        def log_message(self, *_):
            pass
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server.server_port
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


class ReaderTests(unittest.TestCase):
    def test_identity_staleness_and_state(self):
        value = snapshot()
        value["heartbeat_age_ms"] = 10000
        value["application_heartbeat_age_ms"] = 10000
        value["state"]["last_action"] = {"stage": "blocked", "reason": "test consumer unavailable"}
        with serve(value) as port:
            result = reader.collect("studio", port)
        self.assertTrue(result["studio"]["available"])
        self.assertFalse(result["studio"]["responsive"])
        self.assertTrue(result["studio"]["transport_responsive"])
        self.assertFalse(result["studio"]["application_responsive"])
        self.assertEqual(result["studio"]["snapshot"]["state"]["last_action"]["stage"], "blocked")
        self.assertEqual(len(result["findings"]), 1)

    def test_unfocused_runtime_suspension_is_not_a_false_hang(self):
        value = snapshot("runtime")
        value["heartbeat_age_ms"] = 10000
        value["application_heartbeat_age_ms"] = 10000
        value["process_foreground"] = False
        with serve(value) as port:
            result = reader.collect("runtime", runtime_port=port)
        self.assertTrue(result["runtime"]["available"])
        self.assertTrue(result["runtime"]["responsive"])
        self.assertFalse(result["runtime"]["application_responsive"])
        self.assertTrue(result["runtime"]["suspended_unfocused"])
        self.assertFalse(result["findings"])

    def test_wrong_process_and_schema_rejected(self):
        with serve(snapshot("runtime")) as port:
            self.assertFalse(reader.read_snapshot("studio", port)["available"])
        value = snapshot()
        value["schema"] = "renegade.diagnostics.v1"
        with serve(value) as port:
            self.assertFalse(reader.read_snapshot("studio", port)["available"])

    def test_build_and_child_mismatch(self):
        studio = snapshot()
        studio["state"]["test_level"] = {"active": True, "child_pid": 2}
        with serve(studio) as sp, serve(snapshot("runtime", "b" * 40, 3)) as rp:
            result = reader.collect("both", sp, rp)
        self.assertFalse(result["build_match"])
        self.assertFalse(result["runtime_is_studio_child"])
        self.assertEqual(len(result["findings"]), 2)

    def test_expected_commit_and_matching_child(self):
        studio = snapshot()
        studio["state"]["test_level"] = {"active": True, "child_pid": 2}
        with serve(studio) as sp, serve(snapshot("runtime", pid=2)) as rp:
            result = reader.collect("both", sp, rp, "a" * 40)
        self.assertTrue(result["build_match"])
        self.assertTrue(result["runtime_is_studio_child"])
        self.assertFalse(result["findings"])

    def test_unavailable_is_not_success(self):
        with serve(snapshot()) as closed_port:
            pass
        self.assertFalse(reader.read_snapshot("studio", closed_port)["available"])


if __name__ == "__main__":
    unittest.main()
