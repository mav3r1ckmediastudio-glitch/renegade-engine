"""Versioned multi-workflow evaluation plus stale evidence and dialogue boundaries."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"Tools/assistant_knowledge"))
import bridge_server as bridge
import dialogue_guidance
from workflow_answers import verified_workflow,WORKFLOWS
HERE=Path(__file__).resolve().parent

class RemainingWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=HERE.parents[1];cls.db=cls.repo/"BUILD/assistant_knowledge.sqlite"
        cls.registry=json.loads(WORKFLOWS.read_text(encoding="utf-8"))
        cls.tasks={t["id"]:t for t in cls.registry["workflows"]}
        cls.cases=json.loads((HERE/"remaining_workflow_evaluation.json").read_text(encoding="utf-8"))["cases"]

    def test_versioned_correct_and_alternative_phrasings(self):
        for case in self.cases:
            with self.subTest(q=case["question"]):
                a=verified_workflow(case["question"],self.repo,self.db)
                self.assertIsNotNone(a)
                self.assertIn(self.tasks[case["expected_workflow"]]["label"].lower(),a)
                self.assertIn(case["required"],a)
                self.assertLess(a.index("**Before you start:**"),a.index("\n1. "))
                self.assertIn("**Troubleshooting:**",a)
                self.assertIn("[Source:",a)

    def test_each_new_workflow_refuses_corrupt_source(self):
        for case in {c["expected_workflow"]:c for c in self.cases}.values():
            with self.subTest(id=case["expected_workflow"]),tempfile.TemporaryDirectory() as tmp:
                d=json.loads(WORKFLOWS.read_text(encoding="utf-8"))
                t=next(t for t in d["workflows"] if t["id"]==case["expected_workflow"])
                t["evidence"][0]["sha256"]="0"*64
                p=Path(tmp)/"workflows.json";p.write_text(json.dumps(d),encoding="utf-8")
                self.assertIsNone(verified_workflow(case["question"],self.repo,self.db,p))

    def test_read_only_bridge_routes_every_task_without_qwen(self):
        for case in self.cases:
            with self.subTest(q=case["question"]):
                d=bridge.process_chat({"messages":[{"role":"user","content":case["question"]}]},
                                     self.repo,self.db,qwen_port=65530)
                self.assertEqual(d["route"],"local_verified_knowledge")
                self.assertFalse(d["editor_access"])
                self.assertIn(self.tasks[case["expected_workflow"]]["label"].lower(),d["answer"])

    def test_wrong_workflow_neighbours(self):
        for q,forbidden in [
          ("How do I add a Player Start?","spawn-point-v1"),
          ("How do I edit a projectile spawn point?","player-start-place-v1"),
          ("How do I import an animation FBX?","static-model-import-v1"),
          ("How do I add a global script?","script-attach-v1"),
          ("How do I attach an entity script?","global-script-v1"),
          ("How do I set up a sliding door?","interaction-switch-v1"),
          ("How do I create a character prefab?","player-prefab-save-v1"),
          ("How do I make an NPC patrol?","npc-wander-v1"),
          ("How do I configure hitscan?","projectile-create-v1")]:
            with self.subTest(q=q):
                a=verified_workflow(q,self.repo,self.db)
                self.assertIsNotNone(a)
                self.assertNotIn(self.tasks[forbidden]["label"].lower(),a.split("\n",1)[0])

    def chat(self,previous,current):
        return bridge.process_chat({"messages":[{"role":"user","content":previous},
             {"role":"assistant","content":"An earlier response."},
             {"role":"user","content":current}]},self.repo,self.db,qwen_port=65530)

    def test_followup_uses_verified_workflow_not_editor_state(self):
        a=self.chat("How do I set fire time in animation?","What's next?")["answer"]
        self.assertIn("**Next stage:**",a)
        self.assertIn("not confirmation",a)
        a=self.chat("How do I create equipment?","What do I need?")["answer"]
        self.assertIn("saved first-person assembly",a)
        a=self.chat("How do I create equipment?","Where do I click next?")["answer"]
        self.assertIn("cannot see which step",a)

    def test_followup_troubleshooting_and_gravity_clarification(self):
        a=self.chat("How do I set up a sliding door?","It doesn't work")["answer"]
        self.assertIn("Direct Interaction",a)
        a=self.chat("How do I create a projectile?","What about one with gravity off?")["answer"]
        self.assertIn("different settings",a)
        self.assertIn("Hitscan",a)

    def test_followup_does_not_guess_without_valid_context(self):
        self.assertIsNone(dialogue_guidance.followup_reply("What's next?",
            [{"role":"user","content":"Breakfast?"},{"role":"user","content":"What's next?"}],
            self.repo,self.db))
        with patch.object(dialogue_guidance,"verified_workflow",return_value=None):
            a=dialogue_guidance.followup_reply("What's next?",
                [{"role":"user","content":"How do I create a projectile?"},
                 {"role":"user","content":"What's next?"}],self.repo,self.db)
            self.assertIn("cannot verify",a)

    def test_clarification_alternatives_require_fresh_evidence(self):
        with patch.object(dialogue_guidance,"verified_workflow",return_value=None):
            a=dialogue_guidance.clarification("How do I import an asset?",self.repo,self.db)
            self.assertIn("cannot verify",a)
            self.assertNotIn("rigged Character",a)

    def test_intervening_ordinary_conversation_clears_followup_context(self):
        a=dialogue_guidance.followup_reply("What's next?",[
            {"role":"user","content":"How do I create a projectile?"},
            {"role":"user","content":"Breakfast?"},
            {"role":"user","content":"What's next?"}],self.repo,self.db)
        self.assertIsNone(a)

    def test_ambiguous_requests_ask_a_question(self):
        for q in ["How do I import an asset?","How do I add animation?","How do I add a script?"]:
            with self.subTest(q=q):
                d=bridge.process_chat({"messages":[{"role":"user","content":q}]},
                                     self.repo,self.db,qwen_port=65530)
                self.assertEqual(d["route"],"local_verified_knowledge")
                self.assertIn("?",d["answer"])
                self.assertNotIn("\n1. ",d["answer"])

    def test_autonomous_game_world_requests_do_not_become_instructions(self):
        for q in ["Build an entire game with NPCs and scripts",
                  "Create a whole world with projectiles automatically"]:
            d=bridge.process_chat({"messages":[{"role":"user","content":q}]},
                                 self.repo,self.db,qwen_port=65530)
            self.assertIn("cannot autonomously",d["answer"])
            self.assertNotIn("\n1. ",d["answer"])
            self.assertFalse(d["editor_access"])

    def test_unsupported_tasks_abstain_instead_of_dumping_facts(self):
        for q in ["How do I author a beam laser projectile?",
                  "How do I use a global game-scope script?",
                  "How do I retarget arbitrary FPS arms to different rigs?"]:
            d=bridge.process_chat({"messages":[{"role":"user","content":q}]},
                                 self.repo,self.db,qwen_port=65530)
            self.assertNotIn("\n1. ",d["answer"])
            self.assertEqual(d["route"],"local_verified_knowledge")

if __name__=="__main__":
    unittest.main()
