"""Bounded in-memory utterance capture. No recording files or background activation."""
from collections import deque
import numpy as np

class TurnDetector:
    def __init__(self, sample_rate=16000, silence_seconds=.9, max_seconds=30, min_rms=.0015):
        self.min_rms=min_rms
        self.rate=sample_rate
        self.silence_seconds=silence_seconds
        self.max_seconds=max_seconds
        self.reset()

    def reset(self):
        self.prefix=deque(maxlen=3)
        self.chunks=[]
        self.voice_seconds=0
        self.silent_seconds=0
        self.total_seconds=0
        self.noise=self.min_rms/6

    def feed(self, samples, voiced=None):
        samples=np.asarray(samples,dtype=np.float32).reshape(-1)
        if not len(samples):
            return None
        seconds=len(samples)/self.rate
        rms=float(np.sqrt(np.mean(samples*samples)))
        if voiced is None:
            voiced=rms>max(self.min_rms,self.noise*3)
        if not self.chunks:
            if not voiced:
                self.noise=.9*self.noise+.1*min(rms,self.min_rms/3)
                self.prefix.append(samples.copy())
                return None
            self.chunks=list(self.prefix)
            self.prefix.clear()
        self.chunks.append(samples.copy())
        self.total_seconds+=seconds
        if voiced:
            self.voice_seconds+=seconds
            self.silent_seconds=0
        else:
            self.silent_seconds+=seconds
        if self.silent_seconds>=self.silence_seconds or self.total_seconds>=self.max_seconds:
            result=np.concatenate(self.chunks) if self.voice_seconds>=.2 else None
            self.reset()
            return result
        return None
