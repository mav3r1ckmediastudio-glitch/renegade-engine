"""Loopback-only experimental generated character backend. No editor tools."""
import argparse,json,os,sys,time,threading,queue
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
p=argparse.ArgumentParser();p.add_argument('--manifest',type=Path,required=True);p.add_argument('--adapter',type=Path,required=True);p.add_argument('--scale',type=float,default=.65);p.add_argument('--port',type=int,default=18385);a=p.parse_args()
if not 0<=a.scale<=1:raise ValueError('Invalid adapter scale')
import torch
from transformers import AutoTokenizer,AutoModelForCausalLM,BitsAndBytesConfig,TextIteratorStreamer,StoppingCriteria,StoppingCriteriaList
from peft import PeftModel
spec=json.loads(a.manifest.read_text(encoding='utf-8'));os.environ['TOKENIZERS_PARALLELISM']='false'
started=time.monotonic();tok=AutoTokenizer.from_pretrained(spec['snapshot'],local_files_only=True)
q=BitsAndBytesConfig(load_in_4bit=True,bnb_4bit_compute_dtype=torch.bfloat16,bnb_4bit_quant_type='nf4',bnb_4bit_use_double_quant=True)
base=AutoModelForCausalLM.from_pretrained(spec['snapshot'],quantization_config=q,device_map={'':0},dtype=torch.bfloat16,local_files_only=True)
model=PeftModel.from_pretrained(base,str(a.adapter),is_trainable=False).eval()
for module in model.modules():
 if hasattr(module,'set_scale'):module.set_scale('default',a.scale)
load_ms=round((time.monotonic()-started)*1000);lock=threading.Lock()
def chosen_name():
 file=Path(os.environ['RENEGADE_PROFILE_PATH']) if os.environ.get('RENEGADE_PROFILE_PATH') else Path(os.environ.get('LOCALAPPDATA',''))/'RenegadeStudio/preferences.cfg'
 try:
  if file.stat().st_size>65536:return 'creator'
  for line in file.read_text(encoding='utf-8').splitlines():
   if line.startswith('developer_identity='):
    name=line[19:].strip()
    if name and len(name.encode('utf-16-le'))<=64 and not any(ord(c)<32 or ord(c)==127 for c in name):return name
 except (OSError,UnicodeError):pass
 return 'creator'
def system_for(data):
 text=Path(__file__).with_name('character-system.txt').read_text(encoding='utf-8').replace('{name}',json.dumps(chosen_name(),ensure_ascii=False))
 previous=data.get('personality','')
 if isinstance(previous,str):
  if 'Do not add jokes or workshop anecdotes unless invited.' in previous:text+=' The creator has requested less humour: keep this straightforward.'
  if 'Keep casual replies brief, normally one sentence.' in previous:text+=' The creator has requested especially brief casual replies.'
 return text+' Current session facts: editor_bridge_connected=false; scene_snapshot=null; available_editor_actions=none. Only conversation text and confirmed profile identity are supplied here.'
class Stop(StoppingCriteria):
 def __init__(self,event):self.event=event
 def __call__(self,input_ids,scores,**kwargs):return self.event.is_set()
class Handler(BaseHTTPRequestHandler):
 protocol_version='HTTP/1.0'
 def log_message(self,*args):pass
 def send_json(self,obj,status=200):
  raw=json.dumps(obj,ensure_ascii=True).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
 def do_GET(self):
  if self.path=='/health':self.send_json({'status':'ready','adapter':'character-v6','model':spec['model'],'revision':spec['revision'],'streaming':True,'read_only':True,'port':a.port,'load_ms':load_ms,'scale':a.scale})
  else:self.send_error(404)
 def event(self,obj):self.wfile.write((json.dumps(obj,ensure_ascii=True)+'\n').encode());self.wfile.flush()
 def do_POST(self):
  if self.path!='/chat-stream':self.send_error(404);return
  try:
   size=int(self.headers.get('Content-Length','0'))
   if not 2<=size<=48000:raise ValueError('Invalid request size')
   data=json.loads(self.rfile.read(size));raw=data.get('messages')
   if not isinstance(raw,list) or not raw:raise ValueError('Expected conversation')
   turns=[]
   for item in raw[-12:]:
    if not isinstance(item,dict) or item.get('role') not in ('user','assistant') or not isinstance(item.get('content'),str) or not 0<len(item['content'])<=3000:raise ValueError('Invalid conversation entry')
    turns.append({'role':item['role'],'content':item['content']})
   if turns[-1]['role']!='user' or len(turns[-1]['content'])>900:raise ValueError('Expected latest user question')
   maximum=max(48,min(180,int(data.get('max_tokens',120))))
   messages=[{'role':'system','content':system_for(data)}]+turns
   ids=tok.apply_chat_template(messages,tokenize=True,add_generation_prompt=True,return_tensors='pt')
   while ids.shape[-1]>4096 and len(messages)>2:
    del messages[1];ids=tok.apply_chat_template(messages,tokenize=True,add_generation_prompt=True,return_tensors='pt')
   if ids.shape[-1]>4096:raise ValueError('Latest question exceeds context budget')
  except (ValueError,TypeError,KeyError,OSError) as e:self.send_json({'error':str(e)},400);return
  with lock:
   stopped=threading.Event();errors=[];worker=None
   try:
    self.send_response(200);self.send_header('Content-Type','application/x-ndjson; charset=utf-8');self.send_header('Cache-Control','no-store');self.end_headers()
    begin=time.monotonic();first=None;parts=[];ids=ids.to('cuda');stream=TextIteratorStreamer(tok,skip_prompt=True,skip_special_tokens=True,timeout=30)
    self.event({'type':'meta','adapter':'character-v6','model':spec['model'],'prompt_tokens':int(ids.shape[-1])})
    def generate():
     try:
      with torch.inference_mode():model.generate(input_ids=ids,attention_mask=torch.ones_like(ids),streamer=stream,max_new_tokens=maximum,do_sample=True,temperature=.7,top_p=.8,top_k=20,use_cache=True,stopping_criteria=StoppingCriteriaList([Stop(stopped)]))
     except Exception as e:errors.append(str(e));stream.end()
    worker=threading.Thread(target=generate,daemon=True);worker.start()
    for piece in stream:
     if not piece:continue
     if first is None:first=round((time.monotonic()-begin)*1000)
     parts.append(piece);self.event({'type':'delta','text':piece})
    worker.join(timeout=10)
    if errors:raise RuntimeError(errors[0])
    self.event({'type':'done','answer':''.join(parts).strip(),'adapter':'character-v6','first_delta_ms':first,'elapsed_ms':round((time.monotonic()-begin)*1000)})
   except (BrokenPipeError,ConnectionResetError):stopped.set()
   except Exception as e:
    stopped.set()
    try:self.event({'type':'error','error':str(e)[:300]})
    except OSError:pass
   finally:
    stopped.set()
    if worker:worker.join(timeout=10)
print('CHARACTER_SERVICE_READY',a.port,load_ms,flush=True)
ThreadingHTTPServer(('127.0.0.1',a.port),Handler).serve_forever()
