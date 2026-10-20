# Experimental Kokoro full-precision British male Daniel worker, CPU-only.
# Model and Python environment reside in the PARENT research directory.
import base64, io, json, os, pathlib, sys, time, wave
os.environ['ONNX_PROVIDER']='CPUExecutionProvider'
import numpy as np
import onnxruntime as ort
import soundfile as sf
from kokoro_onnx import Kokoro

def send(payload):
    sys.stdout.write(json.dumps(payload,separators=(",",":"),ensure_ascii=True)+"\n")
    sys.stdout.flush()

try:
    root=pathlib.Path(os.environ.get('RENEGADE_DANIEL_MODEL_DIR') or pathlib.Path(__file__).resolve().parents[2])
    model=root/'kokoro-v1.0.onnx'
    voices=root/'voices-v1.0.bin'
    opt=ort.SessionOptions()
    opt.intra_op_num_threads=8
    opt.inter_op_num_threads=1
    opt.execution_mode=ort.ExecutionMode.ORT_SEQUENTIAL
    opt.graph_optimization_level=ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    started=time.perf_counter()
    session=ort.InferenceSession(str(model),sess_options=opt,providers=['CPUExecutionProvider'])
    voice=Kokoro.from_session(session,str(voices))
    send({'event':'ready','loadMs':round((time.perf_counter()-started)*1000),'voice':'Daniel FP32'})
except Exception as e:
    send({'event':'fatal','error':str(e)})
    sys.exit(1)

for line in sys.stdin:
    msg={}
    try:
        msg=json.loads(line)
        if msg.get('type')=='quit':break
        ident=msg.get('id')
        text=msg.get('text')
        if not isinstance(ident,int) or not isinstance(text,str) or not 0<len(text.strip())<=900:
            send({'id':ident,'error':'Invalid request'});continue
        start=time.perf_counter()
        # A modest cadence increase over Kokoro's stock speed=1.0.
        # This is speaking rate, independent of how quickly Qwen replies.
        speed=float(os.environ.get('RENEGADE_DANIEL_SPEED','1.10'))
        speed=min(1.25,max(.90,speed))
        samples,sample_rate=voice.create(text.strip(),voice='bm_daniel',lang='en-gb',speed=speed)
        wav=io.BytesIO()
        sf.write(wav,samples,sample_rate,format='WAV',subtype='PCM_16')
        data=wav.getvalue()
        if data[:4]!=b'RIFF' or data[8:12]!=b'WAVE' or len(data)>22*1024*1024:raise ValueError('Invalid generated WAV')
        send({'id':ident,'audio':base64.b64encode(data).decode('ascii'),'bytes':len(data),'synthMs':round((time.perf_counter()-start)*1000)})
    except Exception as e:
        send({'id':msg.get('id') if isinstance(msg,dict) else None,'error':str(e)})
