import sys
from pathlib import Path
import unittest
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"voice"))
from turn_detector import TurnDetector

class TurnTests(unittest.TestCase):
    def setUp(self):
        self.silence=np.zeros(1600,dtype=np.float32)
        self.speech=np.sin(np.arange(1600)*.1).astype(np.float32)*.05
    def test_silence_never_submits(self):
        detector=TurnDetector()
        for _ in range(400):
            self.assertIsNone(detector.feed(self.silence))
        self.assertLessEqual(len(detector.prefix),3)
    def test_brief_noise_is_not_a_turn(self):
        detector=TurnDetector()
        detector.feed(self.speech)
        for _ in range(12):
            self.assertIsNone(detector.feed(self.silence))
    def test_pause_then_completed_utterance(self):
        detector=TurnDetector()
        for _ in range(5): detector.feed(self.speech)
        for _ in range(3): self.assertIsNone(detector.feed(self.silence))
        for _ in range(3): self.assertIsNone(detector.feed(self.speech))
        result=None
        for _ in range(12):
            current=detector.feed(self.silence)
            if current is not None: result=current
        self.assertIsNotNone(result)
        self.assertGreater(len(result),len(self.speech)*8)
        self.assertEqual(detector.chunks,[])
    def test_quiet_headset_speech_completes(self):
        detector=TurnDetector()
        background=np.ones(1600,dtype=np.float32)*.0006
        quiet=self.speech*(.0022/float(np.sqrt(np.mean(self.speech**2))))
        for _ in range(20): self.assertIsNone(detector.feed(background))
        for _ in range(8): detector.feed(quiet)
        results=[detector.feed(background) for _ in range(12)]
        self.assertTrue(any(result is not None for result in results))
    def test_background_below_threshold_never_submits(self):
        detector=TurnDetector()
        for _ in range(400):
            self.assertIsNone(detector.feed(np.ones(1600,dtype=np.float32)*.0009))
    def test_maximum_turn_and_cancel_clear_audio(self):
        detector=TurnDetector(max_seconds=1)
        result=None
        for _ in range(12):
            current=detector.feed(self.speech)
            if current is not None: result=current
        self.assertIsNotNone(result)
        detector.reset()
        self.assertEqual(detector.chunks,[])
        self.assertEqual(len(detector.prefix),0)
if __name__=="__main__":
    unittest.main()
