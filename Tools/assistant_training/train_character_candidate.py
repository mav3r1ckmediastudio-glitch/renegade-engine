"""Isolated character adapter experiment. Never deploy or alter a live adapter."""
import argparse,json,os,random,time,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--lab',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--steps',type=int,default=96);p.add_argument('--prepare-only',action='store_true');p.add_argument('--fresh-base',action='store_true');p.add_argument('--model-manifest',type=Path);a=p.parse_args()
if a.output.exists():raise RuntimeError('Candidate output already exists; choose a fresh directory')
a.output.mkdir(parents=True)
os.environ['HF_HOME']=str(a.lab/'hf-cache');os.environ['TOKENIZERS_PARALLELISM']='false'
import torch
from transformers import AutoTokenizer,AutoModelForCausalLM,BitsAndBytesConfig,TrainingArguments,Trainer
from peft import PeftModel,prepare_model_for_kbit_training,get_peft_model,LoraConfig
from torch.utils.data import Dataset
system=('You ARE Renegade, the game development engine embodied in a luminous orb. Speak as I to the creator as you. '
 'Use natural plain British English. Follow the current subject, respond to corrections, and use dry humour sparingly. '
 'Your workshop and personal stories are fictional character flavour. Say so plainly when asked. Never invent actual project actions, observations, controls or historical facts. '
 'The creator retains creative control. Technical guidance requires verified sources. Casual conversation is not automatically a support request.')
spec=json.loads(a.model_manifest.read_text(encoding='utf-8')) if a.model_manifest else None
if spec and not a.fresh_base:raise RuntimeError('New model requires a fresh adapter')
model_source=spec['snapshot'] if spec else 'Qwen/Qwen3-1.7B'
if spec:
 system=Path(__file__).resolve().parents[1].joinpath('assistant_presence/chat/character-system.txt').read_text(encoding='utf-8').replace('{name}','the creator')
corpus=Path(__file__).with_name('character_dialogues.json');scenarios=json.loads(corpus.read_text(encoding='utf-8'))['scenarios']
heldout_ids={'origin','boat','music','lights'}
train=[];heldout=[]
for scenario in scenarios:
 identity=system.replace('the creator; that identifies the user',scenario['trusted_name']+'; that identifies the user') if spec and scenario.get('trusted_name') else system
 if scenario.get('trusted_name'):identity+=' The chosen user name is '+scenario['trusted_name']+'. That is the user, not you.'
 history=[{'role':'system','content':identity}]
 for question,answer in scenario['turns']:
  history += [{'role':'user','content':question},{'role':'assistant','content':answer}]
  row={'scenario':scenario['id'],'messages':list(history),'origin':'new-character'}
  (heldout if scenario['id'] in heldout_ids else train).append(row)
# Replay social/correction records only; do not train unverified editor recipes.
old=[json.loads(l) for l in (a.lab/'v3/v31/train.jsonl').read_text(encoding='utf-8').splitlines() if l.strip()]
random.seed(141026);random.shuffle(old);replay=[]
for row in old:
 if row.get('category') not in {'social','frustration','personality','identity','correction'}:continue
 messages=row['messages']
 if any('renegade assistant' in m['content'].lower() for m in messages if m['role']!='system'):continue
 replay.append({'scenario':'replay:'+row.get('topic','unknown'),'origin':'replay','messages':[{'role':'system','content':system}]+[m for m in messages if m['role']!='system']})
 if len(replay)==64:break
if a.fresh_base:replay=[]
train+=replay
assert len(train)>=96 and len(heldout)>=12
assert not ({r['scenario'] for r in train}&{r['scenario'] for r in heldout})
for name,rows in [('train',train),('heldout',heldout)]:
 (a.output/(name+'.jsonl')).write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
(a.output/'dataset_manifest.json').write_text(json.dumps({'new_scenarios':len(scenarios),'train':len(train),'heldout':len(heldout),'replay':len(replay),'heldout_scenarios':sorted(heldout_ids),'corpus_sha256':hashlib.sha256(corpus.read_bytes()).hexdigest(),'live_deploy':False},indent=2),encoding='utf-8')
if a.prepare_only:
 print('PREPARED',len(train),'training examples',len(heldout),'held-out examples',flush=True)
 raise SystemExit(0)
if not torch.cuda.is_available():raise RuntimeError('CUDA required')
torch.manual_seed(141026)
quant=BitsAndBytesConfig(load_in_4bit=True,bnb_4bit_quant_type='nf4',bnb_4bit_use_double_quant=True,bnb_4bit_compute_dtype=torch.bfloat16)
tok=AutoTokenizer.from_pretrained(model_source,local_files_only=True)
if tok.pad_token is None:tok.pad_token=tok.eos_token
base=AutoModelForCausalLM.from_pretrained(model_source,quantization_config=quant,device_map={'':0},torch_dtype=torch.bfloat16,local_files_only=True)
base=prepare_model_for_kbit_training(base,use_gradient_checkpointing=True)
model=get_peft_model(base,LoraConfig(r=8,lora_alpha=16,lora_dropout=.05,bias='none',task_type='CAUSAL_LM',target_modules=['q_proj','k_proj','v_proj','o_proj','up_proj','down_proj','gate_proj'])) if a.fresh_base else PeftModel.from_pretrained(base,str(a.lab/'v3/v31/adapter'),is_trainable=True)
model.config.use_cache=False
examples=[]
for row in train:
 full=tok.apply_chat_template(row['messages'],tokenize=True,add_generation_prompt=False,enable_thinking=False)
 prefix=tok.apply_chat_template(row['messages'][:-1],tokenize=True,add_generation_prompt=True,enable_thinking=False)
 if len(full)>768:raise RuntimeError('Example too long; shorten rather than truncate')
 if full[:len(prefix)]!=prefix:raise RuntimeError('Assistant label boundary does not match chat template')
 examples.append({'input_ids':full,'labels':[-100]*len(prefix)+full[len(prefix):]})
class Conversations(Dataset):
 def __len__(self):return len(examples)
 def __getitem__(self,i):return examples[i]
def collate(batch):
 size=max(len(b['input_ids']) for b in batch)
 return {'input_ids':torch.tensor([b['input_ids']+[tok.pad_token_id]*(size-len(b['input_ids'])) for b in batch]),'attention_mask':torch.tensor([[1]*len(b['input_ids'])+[0]*(size-len(b['input_ids'])) for b in batch]),'labels':torch.tensor([b['labels']+[-100]*(size-len(b['labels'])) for b in batch])}
args=TrainingArguments(output_dir=str(a.output/'checkpoints'),per_device_train_batch_size=1,gradient_accumulation_steps=4,max_steps=a.steps,learning_rate=(5e-5 if a.fresh_base else 2e-5),warmup_steps=6,lr_scheduler_type='cosine',bf16=True,logging_steps=4,save_strategy='no',report_to='none',gradient_checkpointing=True,max_grad_norm=1,dataloader_num_workers=0,seed=141026,remove_unused_columns=False)
print('CANDIDATE_TRAIN_START examples=',len(examples),'steps=',a.steps,flush=True)
start=time.monotonic();trainer=Trainer(model=model,args=args,train_dataset=Conversations(),data_collator=collate);result=trainer.train()
model.save_pretrained(a.output/'adapter');tok.save_pretrained(a.output/'adapter')
(a.output/'training_result.json').write_text(json.dumps({'metrics':result.metrics,'seconds':round(time.monotonic()-start,1),'status':'trained_not_evaluated_not_deployed','source_adapter':('fresh_base' if a.fresh_base else 'v31'),'steps':a.steps,'model_spec':spec},indent=2),encoding='utf-8')
print('CANDIDATE_TRAIN_DONE',result.metrics,flush=True)
