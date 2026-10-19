"""Read-only bridge tests; do not require Qwen or Studio to be installed."""
import json
from pathlib import Path
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.request import Request, urlopen

TOOLS = Path(__file__).resolve().parents[2] / "Tools" / "assistant_knowledge"
sys.path.insert(0, str(TOOLS))
import bridge_server as bridge


class BridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo = Path(__file__).resolve().parents[2]
        cls.db = cls.repo / "BUILD/assistant_knowledge.sqlite"

    def test_projectile_question_is_grounded_and_cited(self):
        response = bridge.process_chat({
            "messages": [{"role": "user",
                          "content": "How do I save a projectile and assign it to my player?"}]
        }, self.repo, self.db, qwen_port=65530)
        self.assertEqual(response["route"], "local_verified_knowledge")
        self.assertIn("SAVE AS NEW", response["answer"])
        self.assertIn("Source:", response["answer"])
        self.assertFalse(response["editor_access"])

    def test_engine_capability_must_abstain(self):
        answer = bridge.process_chat({
            "messages": [{"role": "user",
                          "content": "Can Renegade create an entire world automatically?"}]
        }, self.repo, self.db, qwen_port=65530)
        self.assertEqual(answer["route"], "local_verified_knowledge")
        self.assertIn("won't guess", answer["answer"])
        self.assertNotIn("Source:", answer["answer"])

    def test_stale_fact_refused(self):
        # A missing database must not be interpreted as evidence.
        response = bridge.knowledge_reply(
            "How can I assign my projectile?", self.repo,
            self.db.with_name("missing-never-created-knowledge.sqlite"))
        self.assertIn("can't verify", response)

    def test_general_conversation_falls_back_gracefully(self):
        answer = bridge.process_chat({
            "messages": [{"role": "user", "content": "Alright, how's it going?"}]
        }, self.repo, self.db, qwen_port=65530)
        self.assertEqual(answer["route"], "qwen_unavailable")
        self.assertIn("optional local conversation model", answer["answer"])

    def test_bounded_input_rejects_invalid_payloads(self):
        for message in ({"messages": []}, {"messages": "hello"},
                        {"messages": [{"role": "user", "content": "x" * 1201}]}):
            with self.subTest(message=repr(message)[:40]):
                with self.assertRaises(ValueError):
                    bridge.last_user_question(message)

    def test_loopback_http_compatibility(self):
        server = ThreadingHTTPServer(("127.0.0.1", 0),
                  bridge.handler_factory(self.repo, self.db, 65530))
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/chat"
            payload = {"adapter": "v31", "messages": [
                {"role": "user", "content": "How do I configure an arrow projectile?"}]}
            req = Request(url, data=json.dumps(payload).encode(),
                          headers={"Content-Type": "application/json"}, method="POST")
            with urlopen(req, timeout=5) as resp:
                result = json.load(resp)
            self.assertEqual(result["route"], "local_verified_knowledge")
            self.assertIn("answer", result)
            with urlopen(f"http://127.0.0.1:{server.server_port}/health") as resp:
                health = json.load(resp)
            self.assertTrue(health["read_only"])
            self.assertFalse(health["model_weights_added"])
        finally:
            server.shutdown()
            server.server_close()
            worker.join(timeout=3)


if __name__ == "__main__":
    unittest.main()
