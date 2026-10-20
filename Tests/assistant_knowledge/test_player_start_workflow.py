"""Independent Player Start receipt, prerequisites-first layout and routing."""
import json
from pathlib import Path
import sys
import tempfile
import shutil
import os
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"Tools/assistant_knowledge"))
import bridge_server as bridge
from workflow_answers import verified_workflow, WORKFLOWS

class PlayerStartWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[2]
        cls.db=cls.repo/"BUILD/assistant_knowledge.sqlite"

    def test_placement_variations(self):
        for q in ["How do I add Player Start?","How do I create a player spawn?",
                  "Can you show me how to place a Player Start?",
                  "I want to set up a player start","Where do I add a player start?",
                  "How do I put a player start in my level?"]:
            with self.subTest(q=q):
                a=verified_workflow(q,self.repo,self.db)
                self.assertIsNotNone(a)
                self.assertIn("1. Open the **Asset Browser**",a)
                self.assertIn("**Content/Player**",a)
                self.assertIn("**Basic Player Start**",a)
                self.assertIn("6. Save the level",a)
                self.assertIn("fresh mouse-driven",a)
                self.assertNotIn("NEW** starts",a)

    def test_prerequisites_precede_steps_for_all_workflows(self):
        for q in ["How do I create a projectile?",
                  "How do I assign a projectile?","How do I place Player Start?"]:
            a=verified_workflow(q,self.repo,self.db)
            self.assertLess(a.index("**Before you start:**"),a.index("\n1. "))

    def test_neighbours_are_not_placement(self):
        for q in ["How do I add a second Player Start?",
                  "How do I add multiple player starts?",
                  "How do I create an NPC spawn?",
                  "How do I create a whole game with a player start?",
                  "Automatically add player start to my world"]:
            with self.subTest(q=q):
                self.assertIsNone(verified_workflow(q,self.repo,self.db))

    def test_limits_and_recovery(self):
        a=verified_workflow("How do I add Player Start?",self.repo,self.db)
        for text in ["floor","collision","PLAYER ALREADY PLACED","Add menu",
                     "does not create a weapon","cannot see or change"]:
            self.assertIn(text,a)

    def test_independent_receipt_required(self):
        data=json.loads(WORKFLOWS.read_text(encoding="utf-8"))
        task=next(t for t in data["workflows"] if t["id"]=="player-start-place-v1")
        task["receipt_path"]="Tools/assistant_knowledge/projectile_guide.json"
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"workflows.json";p.write_text(json.dumps(data),encoding="utf-8")
            self.assertIsNone(verified_workflow("How do I add Player Start?",
                                               self.repo,self.db,p))

    def test_unknown_acceptance_status_refused(self):
        data=json.loads(WORKFLOWS.read_text(encoding="utf-8"))
        task=next(t for t in data["workflows"] if t["id"]=="player-start-place-v1")
        task["acceptance_status"]="invented_full_ui_pass"
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"workflows.json";p.write_text(json.dumps(data),encoding="utf-8")
            self.assertIsNone(verified_workflow("How do I add Player Start?",
                                               self.repo,self.db,p))

    def test_workflow_releases_database_for_windows_replacement(self):
        with tempfile.TemporaryDirectory() as tmp:
            db=Path(tmp)/"knowledge.sqlite"
            replacement=Path(tmp)/"replacement.sqlite"
            shutil.copy2(self.db,db)
            shutil.copy2(self.db,replacement)
            self.assertIsNotNone(verified_workflow("How do I add Player Start?",
                                                  self.repo,db))
            os.replace(replacement,db)
            self.assertIsNotNone(verified_workflow("How do I add Player Start?",
                                                  self.repo,db))

    def test_bridge_without_model(self):
        data=bridge.process_chat({"messages":[{"role":"user",
            "content":"How do I add Player Start?"}]},self.repo,self.db,qwen_port=65530)
        self.assertEqual(data["route"],"local_verified_knowledge")
        self.assertFalse(data["editor_access"])
        self.assertIn("**Content/Player**",data["answer"])

if __name__=="__main__":
    unittest.main()
