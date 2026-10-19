"""Regression coverage for verified Surface Type explanations."""
import unittest
from unittest.mock import patch
from Tools.assistant_knowledge import dialogue_guidance as guidance

ROOT="A glass impact classification means my window will shatter, doesn't it?"
FOLLOW="Then what does the classification actually change?"
RECIPE=("Guide\n\n**Expected result:** Classification changes per object.\n\n"
        "**Troubleshooting:**\n- Glass classification does not make a pane destructible.\n\n"
        "[Source: verified-guide.md]")

class SurfaceFollowupTests(unittest.TestCase):
    def history(self,*questions):
        return [{"role":"user","content":q} for q in questions]

    @patch.object(guidance,"verified_workflow",return_value=RECIPE)
    def test_root_and_followup_use_checked_evidence(self,checked):
        for q,messages in [(ROOT,self.history(ROOT)),
                           (FOLLOW,self.history(ROOT,FOLLOW)),
                           ("What does that mean?",self.history(ROOT,FOLLOW,"What does that mean?"))]:
            answer=guidance.followup_reply(q,messages,None,None)
            self.assertIn("does not make a pane destructible",answer)
            self.assertIn("[Source: verified-guide.md]",answer)
        checked.assert_called_with("How do I set Surface Type?",None,None)

    @patch.object(guidance,"verified_workflow",return_value=None)
    def test_stale_evidence_fails_closed(self,checked):
        answer=guidance.followup_reply(FOLLOW,self.history(ROOT,FOLLOW),None,None)
        self.assertIn("cannot verify",answer)

    @patch.object(guidance,"verified_workflow",return_value=RECIPE)
    def test_casual_topic_and_identity_end_context(self,checked):
        for topic in ["I like reading novels.","What are you?","Tell me your backstory."]:
            self.assertIsNone(guidance.followup_reply(
                FOLLOW,self.history(ROOT,topic,FOLLOW),None,None))
        checked.assert_not_called()

if __name__=="__main__":
    unittest.main()
