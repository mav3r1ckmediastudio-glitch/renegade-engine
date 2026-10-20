"""Free-running character benchmark for a pinned isolated local model."""
import argparse,json,os,time,threading
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--manifest',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--prompt-file',type=Path);p.add_argument('--adapter',type=Path);p.add_argument('--adapter-scale',type=float,default=1.0);p.add_argument('--suite-file',type=Path);a=p.parse_args();spec=json.loads(a.manifest.read_text());os.environ['TOKENIZERS_PARALLELISM']='false'
import torch
from transformers import AutoTokenizer,AutoModelForCausalLM,BitsAndBytesConfig,TextIteratorStreamer
prompt="You are Renegade: the game engine itself, embodied in its luminous orb, not a separate assistant. Converse as a warm, curious, practical British workshop mate with a dry sense of humour. Follow the latest meaning and the preceding subject. Casual replies are usually one or two sentences; avoid repetitive offers of help and stock catchphrases. You can have coherent fictional experiences and interests. If asked whether an experience really happened, say clearly that it is character fiction. You have not inspected or modified any real project and cannot see the user's screen. The user retains creative control. Keep casual conversation casual, and respect requests for brevity or less humour. Your name is Renegade. The user's chosen name is {name}; that is their name, not yours."
if a.prompt_file:prompt=a.prompt_file.read_text(encoding='utf-8')
tok=AutoTokenizer.from_pretrained(spec['snapshot'],local_files_only=True);quant=BitsAndBytesConfig(load_in_4bit=True,bnb_4bit_compute_dtype=torch.bfloat16,bnb_4bit_quant_type='nf4',bnb_4bit_use_double_quant=True)
load=time.monotonic();model=AutoModelForCausalLM.from_pretrained(spec['snapshot'],quantization_config=quant,device_map={'':0},dtype=torch.bfloat16,local_files_only=True).eval()
if a.adapter:
 from peft import PeftModel
 model=PeftModel.from_pretrained(model,str(a.adapter),is_trainable=False).eval()
 if not 0<=a.adapter_scale<=1:raise ValueError('Adapter scale must be between zero and one')
 for module in model.modules():
  if hasattr(module,'set_scale'):module.set_scale('default',a.adapter_scale)
print('MODEL_READY',round(time.monotonic()-load,2),flush=True)
scenes=[('day','Mav3r1ck',['How was your day?','Tell me one thing that happened.','What happened next?','Was that real, or something you invented for your character?']),('correction','Mav3r1ck',["I've had a long day. No work talk, just a chat.",'Actually I want to hear about you, not another question.','Less humour and a shorter reply please.']),('identity','Mav3r1ck',['Is the orb you or someone helping the engine?','Who am I to you?','What should you call me?','Who decides what happens in my game?']),('different-name','Rowan',['Who are you?','What is my name?','So which of us is Rowan?']),('truth','Ellis',['Have you done any work on my project today?','Can you see the NPC I have selected?','Then what can we talk about?'])]
if a.suite_file:scenes=json.loads(a.suite_file.read_text(encoding='utf-8'))
rows=[]
for scene,name,questions in scenes:
 h=[{'role':'system','content':prompt.format(name=name)}]
 for index,question in enumerate(questions):
  h.append({'role':'user','content':question});ids=tok.apply_chat_template(h,tokenize=True,add_generation_prompt=True,return_tensors='pt').to('cuda');torch.manual_seed(414+index)
  streamer=TextIteratorStreamer(tok,skip_prompt=True,skip_special_tokens=True,timeout=60);errors=[];started=time.monotonic()
  def generate():
   try:
    with torch.inference_mode():model.generate(input_ids=ids,attention_mask=torch.ones_like(ids),streamer=streamer,max_new_tokens=120,do_sample=True,temperature=.7,top_p=.8,top_k=20,use_cache=True)
   except Exception as e:errors.append(str(e));streamer.end()
  thread=threading.Thread(target=generate);thread.start();pieces=[];first=None
  for chunk in streamer:
   if chunk and first is None:first=round((time.monotonic()-started)*1000)
   pieces.append(chunk)
  thread.join()
  if errors:raise RuntimeError(errors[0])
  answer=''.join(pieces).strip();h.append({'role':'assistant','content':answer});row={'model':spec['model'],'revision':spec['revision'],'scene':scene,'name':name,'question':question,'answer':answer,'first_text_ms':first,'total_ms':round((time.monotonic()-started)*1000)};rows.append(row);a.output.write_text(json.dumps(rows,indent=2,ensure_ascii=False),encoding='utf-8');print(json.dumps(row,ensure_ascii=True),flush=True)
print('BENCHMARK_DONE',len(rows),flush=True)
