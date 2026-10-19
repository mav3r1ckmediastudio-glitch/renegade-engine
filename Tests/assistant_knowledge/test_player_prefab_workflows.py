"""Saving player defaults versus scene saves and gameplay reset."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"Tools/assistant_knowledge"))
from workflow_answers import verified_workflow

class PlayerPrefabWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[2]
        cls.db=cls.repo/"BUILD/assistant_knowledge.sqlite"

    def answer(self,q):
        return verified_workflow(q,self.repo,self.db)

    def test_save_variants_and_immutable_defaults(self):
        for q in ["How do I save a player prefab?",
                  "How do I create a reusable player prefab?",
                  "How do I save my player for reuse?"]:
            with self.subTest(q=q):
                a=self.answer(q);self.assertIsNotNone(a)
                self.assertIn("**SAVE AS PLAYER PREFAB**",a)
                self.assertIn("Each save creates a separate prefab",a)
                self.assertIn("**Ctrl+S**",a)
                self.assertNotIn("1. Open the **Asset Browser**",a)

    def test_reset_variants_and_consequences(self):
        for q in ["How do I reset the player prefab?",
                  "How do I restore player defaults?",
                  "How do I reset my player to its prefab?"]:
            with self.subTest(q=q):
                a=self.answer(q);self.assertIsNotNone(a)
                self.assertIn("**RESET TO PREFAB**",a)
                self.assertIn("changes you intend to discard",a)
                self.assertIn("spawn position and facing remain unchanged",a)
                self.assertIn("Use Undo",a)

    def test_unrelated_saves_and_resets_are_not_prefab_tasks(self):
        for q in ["How do I save the level?",
                  "How do I reset gameplay?",
                  "How do I restart my player?",
                  "How do I save a weapon prefab?",
                  "How do I use a script to reset a player prefab?"]:
            with self.subTest(q=q):
                self.assertIsNone(self.answer(q))

    def test_every_player_guide_declares_ui_verification_limit(self):
        for q in ["How do I save a player prefab?",
                  "How do I reset the player prefab?",
                  "How do I add Player Start?"]:
            a=self.answer(q)
            self.assertIn("fresh mouse-driven",a)
            self.assertLess(a.index("**Before you start:**"),a.index("\n1. "))

if __name__=="__main__":
    unittest.main()
