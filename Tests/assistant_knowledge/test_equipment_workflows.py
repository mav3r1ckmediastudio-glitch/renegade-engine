"""Equipment creation, hand compatibility and explicit assignment guidance."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"Tools/assistant_knowledge"))
import bridge_server as bridge
from workflow_answers import verified_workflow, WORKFLOWS

class EquipmentWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[2]
        cls.db=cls.repo/"BUILD/assistant_knowledge.sqlite"

    def answer(self,q):
        return verified_workflow(q,self.repo,self.db)

    def test_creation_variations(self):
        for q in ["How do I create equipment?",
                  "How do I make a weapon definition?",
                  "How do I create a weapon from an assembly?",
                  "Show me CREATE FROM ASSEMBLY",
                  "How do I add equipment to my player?"]:
            with self.subTest(q=q):
                a=self.answer(q)
                self.assertIsNotNone(a)
                self.assertIn("**CREATE FROM ASSEMBLY**",a)
                self.assertIn("7. Save the level",a)
                self.assertIn("alone does not equip",a)

    def test_assignment_variations(self):
        for q in ["How do I assign starting equipment?",
                  "How do I apply my loadout?",
                  "How do I equip a saved weapon?",
                  "How do I change the player's equipment?",
                  "How do I set up starting equipment?"]:
            with self.subTest(q=q):
                a=self.answer(q)
                self.assertIsNotNone(a)
                self.assertIn("5. Save the level",a)
                self.assertIn("**APPLY LOADOUT**",a)
                self.assertNotIn("7. Save the level",a)

    def test_hand_use_and_error_recovery(self):
        a=self.answer("How do I assign starting equipment?")
        for text in ["**TwoHanded**","**PrimaryWithSupport**","**OffHandOnly**",
                     "**EitherHand**","**MISSING / INCOMPATIBLE**","Use Undo"]:
            self.assertIn(text,a)

    def test_creation_requires_saved_assembly_and_does_not_make_animations(self):
        a=self.answer("How do I create equipment?")
        self.assertIn("saved first-person assembly",a)
        self.assertIn("loose arms/weapon models is not enough",a)
        self.assertIn("does not invent missing animations",a)
        self.assertIn("**New item**",a)
        self.assertIn("initially selects TwoHanded",a)

    def test_wrong_tasks_are_refused(self):
        for q in ["How do I create equipment for an NPC?",
                  "Create equipment and an entire game automatically",
                  "How do I create an equipment inventory?",
                  "How do I equip a projectile impact effect?",
                  "How do I equip a saved weapon using a script?",
                  "How do I build a weapon model?",
                  "How do I import an arms assembly?"]:
            with self.subTest(q=q):
                self.assertIsNone(self.answer(q))

    def test_projectile_assignment_stays_distinct(self):
        a=self.answer("How do I assign a projectile to my weapon?")
        self.assertIn("6. Click **APPLY TO THIS PLAYER**",a)
        self.assertNotIn("**APPLY LOADOUT**",a)

    def test_evidence_failure_abstains(self):
        data=json.loads(WORKFLOWS.read_text(encoding="utf-8"))
        task=next(t for t in data["workflows"] if t["id"]=="equipment-create-v1")
        task["evidence"][0]["sha256"]="0"*64
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"workflows.json";p.write_text(json.dumps(data),encoding="utf-8")
            self.assertIsNone(verified_workflow("How do I create equipment?",
                                               self.repo,self.db,p))

    def test_bridge_loadout_query_does_not_require_model(self):
        data=bridge.process_chat({"messages":[{"role":"user",
            "content":"How do I apply my loadout?"}]},self.repo,self.db,qwen_port=65530)
        self.assertEqual(data["route"],"local_verified_knowledge")
        self.assertFalse(data["editor_access"])
        self.assertIn("5. Save the level",data["answer"])

    def test_prerequisites_and_ui_limits(self):
        for q in ["How do I create equipment?","How do I assign equipment?"]:
            a=self.answer(q)
            self.assertLess(a.index("**Before you start:**"),a.index("\n1. "))
            self.assertIn("fresh beginner Studio walkthrough remains pending",a)
            self.assertIn("cannot see or change your scene",a)

if __name__=="__main__":
    unittest.main()
