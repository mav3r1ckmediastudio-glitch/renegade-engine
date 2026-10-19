"""Saved projectile assignment: routing, evidence, prerequisites and bridge."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"Tools/assistant_knowledge"))
import bridge_server as bridge
import knowledge
from workflow_answers import verified_workflow, WORKFLOWS

class AssignmentWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[2]
        cls.db=cls.repo/"BUILD/assistant_knowledge.sqlite"

    def answer(self,q):
        return verified_workflow(q,self.repo,self.db)

    def test_assignment_phrasings(self):
        for q in [
            "How do I assign a saved projectile to my weapon?",
            "How do I apply my arrow projectile?",
            "How do I connect a projectile to the player's bow?",
            "How do I use an existing bullet in my weapon?",
            "How do I make my bow shoot?",
            "How do I get my weapon to fire?",
            "How do I add an arrow to my bow?",
            "I saved an arrow, how do I equip it?",
            "How do I fire an existing projectile?",
            "How do I assign a bullet?",
            "How do I equip a bolt?",
        ]:
            with self.subTest(q=q):
                a=self.answer(q)
                self.assertIsNotNone(a)
                self.assertIn("Assign a saved projectile".lower(),a)
                self.assertIn("6. Click **APPLY TO THIS PLAYER**",a)
                self.assertIn("**Ctrl+S**",a)

    def test_beginner_prerequisites_and_troubleshooting(self):
        a=self.answer("How do I assign a projectile?")
        for text in ["saved weapon","launch-capable action","**SAVE AS NEW**",
                     "**Expected result:**","**Troubleshooting:**","Weapon changed",
                     "blank-project","cannot inspect or change"]:
            self.assertIn(text,a)
        self.assertIn("do not assume every bow",a)
        self.assertNotIn("0.133333",a)

    def test_creation_route_preserved(self):
        a=self.answer("How do I create a new projectile?")
        self.assertIn("6. Click **SAVE AS NEW**",a)
        self.assertNotIn("**Troubleshooting:**",a)

    def test_neighbour_tasks_are_not_assignment(self):
        for q in [
            "How do I assign a projectile to an NPC?",
            "How do I apply an arrow impact effect?",
            "How do I attach a particle to my arrow?",
            "How do I equip a weapon model?",
            "How do I retarget the bow animation?",
            "How do I use a script to launch a projectile?",
            "Make my weapon fire and create a whole game automatically",

        ]:
            with self.subTest(q=q):
                self.assertIsNone(self.answer(q))

    def mutated_registry(self,mutate):
        data=json.loads(WORKFLOWS.read_text(encoding="utf-8"))
        task=next(t for t in data["workflows"] if t["id"]=="projectile-assign-v1")
        mutate(task)
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"workflows.json"
            p.write_text(json.dumps(data),encoding="utf-8")
            return verified_workflow("How do I assign a projectile?",self.repo,self.db,p)

    def test_stale_new_workflow_source_is_refused(self):
        self.assertIsNone(self.mutated_registry(
            lambda t:t["evidence"][0].update(sha256="0"*64)))

    def test_unperformed_native_step_is_refused(self):
        self.assertIsNone(self.mutated_registry(
            lambda t:t["native_checks"].append("Created weapon from blank project")))

    def test_malformed_troubleshooting_is_refused(self):
        self.assertIsNone(self.mutated_registry(
            lambda t:t.update(troubleshooting=[""])))

    def test_changed_checkout_is_refused(self):
        with patch.object(knowledge,"git_value",return_value="different-head"):
            self.assertIsNone(self.answer("How do I assign a projectile?"))

    def test_bridge_works_without_qwen(self):
        data=bridge.process_chat({"messages":[{"role":"user",
            "content":"How do I assign a bullet?"}]},
            self.repo,self.db,qwen_port=65530)
        self.assertEqual(data["route"],"local_verified_knowledge")
        self.assertFalse(data["editor_access"])
        self.assertIn("6. Click **APPLY TO THIS PLAYER**",data["answer"])

if __name__=="__main__":
    unittest.main()
