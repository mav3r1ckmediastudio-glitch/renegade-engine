"""Task-driven knowledge regression: practical how-tos versus isolated facts."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"Tools"/"assistant_knowledge"))
import bridge_server as bridge
from workflow_answers import verified_workflow, WORKFLOWS

class GuidedWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[2]
        cls.db=cls.repo/"BUILD/assistant_knowledge.sqlite"

    def test_create_new_projectile_is_a_real_walkthrough(self):
        answer=bridge.knowledge_reply("how do i create a new projectile?",self.repo,self.db)
        self.assertIn("1. Select **Player Start**",answer)
        self.assertIn("2. Open **STARTING EQUIPMENT**",answer)
        self.assertIn("3. Set **Fire mode**",answer)
        self.assertIn("4. **NEW** starts",answer)
        self.assertIn("5. Configure speed",answer)
        self.assertIn("6. Click **SAVE AS NEW**",answer)
        self.assertIn("**Next:**",answer)
        self.assertIn("**Before you start:**",answer)
        self.assertNotIn("EDIT COPY loads",answer)
        self.assertIn("[Source: docs/manual/",answer)
        for label in ["source-coded","command service","definition to update"]:
            self.assertNotIn(label,answer.lower())

    def test_variations_trigger_same_native_workflow(self):
        questions=[
            "How do I create a new projectile?",
            "How do I make an arrow in Renegade?",
            "Can you show me how to add a projectile?",
            "I want to set up a new arrow projectile",
            "how do i start a new projectile",
            "What steps to create a bullet?",
        ]
        for question in questions:
            with self.subTest(question=question):
                answer=verified_workflow(question,self.repo,self.db)
                self.assertIsNotNone(answer)
                self.assertIn("**SAVE AS NEW**",answer)

    def test_nonworkflows_never_select_steps(self):
        questions=[
            "Can Renegade create an entire world automatically?",
            "What is hitscan?",
            "Why are my impact decals rotated?",
            "Where is my selected NPC?",
            "How can I build a whole game with a projectile system?",
            "What's the difference between NEW and EDIT COPY?",
            "How do I create a projectile impact effect?",
            "How do I make an arrow trail?",
            "How do I create a particle effect for an arrow?",
        ]
        for q in questions:
            with self.subTest(question=q):
                self.assertIsNone(verified_workflow(q,self.repo,self.db))

    def test_unverified_recipe_is_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/"workflows.json"
            registry=json.loads(WORKFLOWS.read_text(encoding="utf-8"))
            next(t for t in registry["workflows"] if t["id"]=="projectile-create-v1")["evidence"][0]["needle"]="AN_IMAGINARY_BUTTON_NOBODY_BUILT"
            path.write_text(json.dumps(registry),encoding="utf-8")
            self.assertIsNone(verified_workflow(
                "How do I create a new projectile?",self.repo,self.db,path))

    def test_live_request_shape_uses_guidance_not_freeform_model(self):
        data=bridge.process_chat({"adapter":"v31","messages":[{"role":"user","content":"How do I create a new projectile?"}]},
                                 self.repo,self.db,qwen_port=65530)
        self.assertEqual(data["route"],"local_verified_knowledge")
        self.assertFalse(data["editor_access"])
        self.assertIn("1. Select",data["answer"])
        self.assertIn("6. Click",data["answer"])

if __name__=="__main__":
    unittest.main()
