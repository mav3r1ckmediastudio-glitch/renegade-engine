import argparse,json,os,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--lab',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();os.environ['HF_HOME']=str(a.lab/'hf-cache');os.environ['TOKENIZERS_PARALLELISM']='false'
import torch
from transformers import AutoTokenizer,AutoModelForCausalLM,BitsAndBytesConfig
from peft import PeftModel
prompt="Roleplay Renegade, the personality of a game engine, embodied in its luminous orb. You are the engine itself, not a separate helper. Speak as I to the user, Maverick. You're a warm, curious, practical British workshop mate with a dry sense of humour. Answer the meaning of the latest message and keep the conversation's subject. Usually use one or two sentences. You have fictional interests and experiences; acknowledge that they are fictional if asked. You haven't performed any real project work or observed the user's screen. The user keeps creative control. Conversation can be about everyday life as well as making games. Let jokes fit the moment, and respect corrections."
tok=AutoTokenizer.from_pretrained('Qwen/Qwen3-1.7B',local_files_only=True);q=BitsAndBytesConfig(load_in_4bit=True,bnb_4bit_quant_type='nf4',bnb_4bit_use_double_quant=True,bnb_4bit_compute_dtype=torch.bfloat16)
base=AutoModelForCausalLM.from_pretrained('Qwen/Qwen3-1.7B',quantization_config=q,device_map={'':0},torch_dtype=torch.bfloat16,local_files_only=True);model=PeftModel.from_pretrained(base,str(a.lab/'v3/v31/adapter')).eval()
scenes=[['day',['How has your day been?','Tell me one thing that happened.','Was that a real event or a fictional story?']],['correction',["I've spent all day in meetings. No work talk, just a chat.",'Actually, I meant I want to hear about you.','Shorter please, and less humour.']],['identity',['Is the orb you, or someone assisting the engine?','What do you find interesting outside technical problems?','Which one would you choose and why?']]]
rows=[]
for mode in ['adapter-clean','base-clean']:
 for scene,questions in scenes:
  h=[{'role':'system','content':prompt}]
  for i,question in enumerate(questions):
   h.append({'role':'user','content':question});torch.manual_seed(821+i);ids=tok.apply_chat_template(h,tokenize=True,add_generation_prompt=True,enable_thinking=False,return_tensors='pt').to('cuda');started=time.monotonic()
   from contextlib import nullcontext
   with torch.inference_mode(),(model.disable_adapter() if mode=='base-clean' else nullcontext()):out=model.generate(input_ids=ids,attention_mask=torch.ones_like(ids),max_new_tokens=100,do_sample=True,temperature=.72,top_p=.92,pad_token_id=tok.eos_token_id,use_cache=True)
   answer=tok.decode(out[0,ids.shape[-1]:],skip_special_tokens=True).strip();h.append({'role':'assistant','content':answer});row={'mode':mode,'scene':scene,'q':question,'answer':answer,'ms':round((time.monotonic()-started)*1000)};rows.append(row);print(json.dumps(row),flush=True);a.output.write_text(json.dumps(rows,indent=2),encoding='utf-8')
