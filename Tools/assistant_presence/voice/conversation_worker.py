"""Local Whisper worker. Microphone opens only after an explicit listen command."""
import argparse
import io
import json
import math
import os
from pathlib import Path
import queue
import sys
import threading
import time
import wave
import numpy as np
from faster_whisper import WhisperModel
from faster_whisper.audio import decode_audio
from turn_detector import TurnDetector
from speech_gate import SpeechGate, normalise_audio

OUTPUT_LOCK=threading.Lock()
def emit(kind, **values):
    with OUTPUT_LOCK:
        print(json.dumps(dict(type=kind,**values)),flush=True)

def transcribe(model, audio, epoch):
    start=time.perf_counter()
    if not isinstance(audio,(str,Path)):
        audio=normalise_audio(audio)
    emit("transcribing",epoch=epoch)
    segments,_=model.transcribe(audio,language="en",beam_size=1,vad_filter=True,
        condition_on_previous_text=False,initial_prompt="Renegade Studio. Player Start. Character. Projectile. Equipment. NPC.")
    segments=[s for s in segments if s.no_speech_prob<.6]
    text=" ".join(s.text.strip() for s in segments).strip()
    if not text:
        emit("uncertain",epoch=epoch)
        return
    confidence=min(math.exp(s.avg_logprob) for s in segments)
    emit("result",epoch=epoch,text=text,confidence=confidence,
        elapsedMs=round((time.perf_counter()-start)*1000))

def main():
    args=argparse.ArgumentParser()
    args.add_argument("--wave")
    args.add_argument("--probe",action="store_true")
    options=args.parse_args()
    default=Path(__file__).resolve().parents[3]/"BUILD"/"assistant-stt-model"
    folder=Path(os.environ.get("RENEGADE_STT_MODEL_DIR",str(default)))
    if not (folder/"model.bin").is_file():
        raise RuntimeError("Local speech model is missing. Complete the voice setup before starting voice conversation.")
    model=WhisperModel(str(folder),device="cpu",compute_type="int8",cpu_threads=4,local_files_only=True)
    emit("ready",language="en",engine="Whisper base.en / CPU int8")
    if options.probe:
        return
    if options.wave:
        transcribe(model,options.wave,1)
        return
    import sounddevice as sd
    commands=queue.Queue()
    audio=queue.Queue(maxsize=400)
    def read_commands():
        for line in sys.stdin:
            commands.put(line.strip())
        commands.put("quit")
    threading.Thread(target=read_commands,daemon=True).start()
    stream=None
    epoch=0
    detector=None
    gate=None
    listening=False
    last_meter=0.
    def close_input():
        nonlocal stream,detector
        if stream is not None:
            stream.close()
            stream=None
        detector=None
        while True:
            try: audio.get_nowait()
            except queue.Empty: break
    try:
        while True:
            try:
                command=commands.get(timeout=.025 if stream is None else .001)
            except queue.Empty:
                command=None
            if command is not None:
                listening=False
                while True:
                    try: audio.get_nowait()
                    except queue.Empty: break
                parts=command.split()
                if not parts or parts[0]=="quit":
                    close_input()
                    break
                if parts[0]=="listen":
                    epoch=int(parts[1])
                    settings=Path(__file__).resolve().parents[3]/"BUILD"/"assistant-stt-input.json"
                    preferred=os.environ.get("RENEGADE_STT_INPUT_DEVICE","")
                    if not preferred and settings.is_file():
                        preferred=json.loads(settings.read_text(encoding="utf-8-sig")).get("name","")
                    device_index=None
                    if preferred:
                        matches=[(i,d) for i,d in enumerate(sd.query_devices())
                            if d["max_input_channels"]>0 and d["name"]==preferred]
                        if not matches:
                            raise RuntimeError("Selected microphone is unavailable: "+preferred)
                        matches.sort(key=lambda item: sd.query_hostapis(item[1]["hostapi"])["name"]!="Windows WASAPI")
                        device_index=matches[0][0]
                    device=sd.query_devices(device_index,kind="input")
                    rate=int(device["default_samplerate"])
                    detector=TurnDetector(rate,silence_seconds=.55)
                    gate=SpeechGate(rate)
                    def callback(data,frames,timing,status):
                        if not listening: return
                        try: audio.put_nowait((epoch,data[:,0].copy(),bool(status)))
                        except queue.Full: pass
                    reused=stream is not None
                    if stream is None:
                        stream=sd.InputStream(device=device_index,samplerate=rate,channels=1,dtype="float32",
                            blocksize=rate//10,callback=callback)
                        stream.start()
                    listening=True
                    emit("listening",epoch=epoch,device=device["name"],streamReused=reused)
                else:
                    emit("paused")
            if stream is None or not listening:
                continue
            try: captured,samples,overflow=audio.get_nowait()
            except queue.Empty: continue
            if captured!=epoch:
                continue
            if overflow:
                raise RuntimeError("Microphone audio was interrupted. Check the selected Windows input device.")
            voiced=gate.feed(samples)
            if time.perf_counter()-last_meter>=.4:
                last_meter=time.perf_counter()
                emit("capture",epoch=epoch,voiced=voiced,probability=round(gate.probability,2))
            utterance=detector.feed(samples,voiced=voiced)
            if utterance is None:
                continue
            rate=detector.rate
            saved_epoch=epoch
            listening=False  # Keep Bluetooth connected, discard input during replies.
            output=io.BytesIO()
            with wave.open(output,"wb") as wav:
                wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(rate)
                wav.writeframes((np.clip(utterance,-1,1)*32767).astype("<i2").tobytes())
            output.seek(0)
            transcribe(model,decode_audio(output,sampling_rate=16000),saved_epoch)
    finally:
        close_input()

if __name__=="__main__":
    try:
        main()
    except Exception as error:
        emit("error",message=str(error)[:220])
