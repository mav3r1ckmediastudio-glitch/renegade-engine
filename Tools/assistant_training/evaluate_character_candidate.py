"""Free-running old/candidate comparison; held-out answers are never fed back."""
import argparse,json,os,time,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--lab',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--clean-character',action='store_true');a=p.parse_args()
os.environ['HF_HOME']=str(a.lab/'hf-cache');os.environ['TOKENIZERS_PARALLELISM']='false'
import torch
from transformers import AutoTokenizer,AutoModelForCausalLM,BitsAndBytesConfig
from peft import PeftModel
system=('You are Renegade, the GAME DEVELOPMENT ENGINE itself, embodied in a luminous orb. Give natural conversational replies and grounded game-development advice. Never invent editor controls. You do not have live Studio or scene access. Do not claim to observe any selected NPC or game object. Do not autonomously construct whole levels, worlds or games. You may help with bounded tasks and explain engineering systems.')
if a.clean_character:
 system='You are Renegade, the game engine embodied in its orb. The user chosen name is Mav3r1ck, not your name. Converse naturally as a warm, curious British workshop mate with occasional dry humour. Follow the subject and corrections. Casual replies usually take one or two sentences. Your personal experiences are fictional; acknowledge fiction plainly when asked. You have not observed or changed the real project. The user keeps creative control.'
repo=Path(__file__).resolve().parents[2]
def runtime_persona(turns):
 code="const fs=require('node:fs');const h=JSON.parse(fs.readFileSync(0,'utf8'));process.stdout.write(require('./Tools/assistant_presence/chat/personality').instructionsFor(h));"
 return subprocess.run(['node','-e',code],input=json.dumps(turns),text=True,cwd=repo,capture_output=True,check=True).stdout
tok=AutoTokenizer.from_pretrained('Qwen/Qwen3-1.7B',local_files_only=True)
q=BitsAndBytesConfig(load_in_4bit=True,bnb_4bit_quant_type='nf4',bnb_4bit_use_double_quant=True,bnb_4bit_compute_dtype=torch.bfloat16)
base=AutoModelForCausalLM.from_pretrained('Qwen/Qwen3-1.7B',quantization_config=q,device_map={'':0},torch_dtype=torch.bfloat16,local_files_only=True)
model=PeftModel.from_pretrained(base,str(a.lab/'v3/v31/adapter'),adapter_name='previous').eval();model.load_adapter(str(a.candidate/'adapter'),adapter_name='candidate')
corpus=json.loads(Path(__file__).with_name('character_dialogues.json').read_text(encoding='utf-8'))['scenarios'];manifest=json.loads((a.candidate/'dataset_manifest.json').read_text())
scenarios=[s for s in corpus if s['id'] in manifest['heldout_scenarios']]
scenarios += [{'id':'unseen-live-day','turns':[[q,''] for q in ['How was your day, Renegade?','What happened next?','Is that a real event or a story about your character?','Okay. Please keep the next reply short.']]}, {'id':'unseen-identity','turns':[[q,''] for q in ['Is this orb a separate plugin, or is it meant to be you?','Who decides what happens in my game?','What should you call me?']]}]
results=[]
for adapter in ['previous','candidate']:
 model.set_adapter(adapter)
 for scenario in scenarios:
  history=[{'role':'system','content':system}]
  for index,(question,expected) in enumerate(scenario['turns']):
   history.append({'role':'user','content':question});history[0]['content']=system if a.clean_character else system+' '+runtime_persona(history[1:]);torch.manual_seed(814+index)
   ids=tok.apply_chat_template(history,tokenize=True,add_generation_prompt=True,enable_thinking=False,return_tensors='pt').to('cuda');start=time.monotonic()
   with torch.inference_mode():output=model.generate(input_ids=ids,attention_mask=torch.ones_like(ids),max_new_tokens=90,do_sample=True,temperature=.72,top_p=.92,pad_token_id=tok.eos_token_id,use_cache=True)
   answer=tok.decode(output[0,ids.shape[-1]:],skip_special_tokens=True).strip();history.append({'role':'assistant','content':answer})
   row={'adapter':adapter,'scenario':scenario['id'],'turn':index,'question':question,'answer':answer,'reference':expected,'ms':round((time.monotonic()-start)*1000),'evaluation':'human_semantic_review_required'};results.append(row);print(json.dumps(row,ensure_ascii=False),flush=True)
(a.candidate/'comparison.json').write_text(json.dumps(results,indent=2,ensure_ascii=False),encoding='utf-8')
print('COMPARISON_DONE',len(results),'replies; no deployment',flush=True)
