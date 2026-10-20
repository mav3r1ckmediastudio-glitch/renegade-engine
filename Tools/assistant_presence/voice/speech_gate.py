"""Speech classification for quiet microphones; audio remains in memory."""
import numpy as np
from faster_whisper.vad import get_vad_model

def normalise_audio(audio):
    audio=np.asarray(audio,dtype=np.float32).reshape(-1)
    if not len(audio): return audio
    peak=float(np.max(np.abs(audio)))
    return audio*min(40.,.65/max(peak,.00001))

class SpeechGate:
    def __init__(self, rate):
        self.rate=rate
        self.model=get_vad_model()
        self.window=np.zeros(0,dtype=np.float32)
        self.probability=0.
    def feed(self, samples):
        self.window=np.concatenate((self.window,samples))[-int(self.rate*.48):]
        count=int(len(self.window)*16000/self.rate)
        if count<512: return False
        audio=np.interp(np.arange(count)*self.rate/16000,
                        np.arange(len(self.window)),self.window).astype(np.float32)
        audio=normalise_audio(audio)
        audio=np.pad(audio,(0,(-len(audio))%512))
        probabilities=self.model(audio).reshape(-1)
        self.probability=float(np.max(probabilities[-4:]))
        return self.probability>=.35
