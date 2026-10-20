"""Train recovery answers only; preceding refusals are masked context, never targets."""
import argparse,json,os,re,time,hashlib,math
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--manifest',type=Path,required=True)
p.add_argument('--source-adapter',type=Path,required=True)
p.add_argument('--replay',type=Path,required=True)
p.add_argument('--heldout',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--prepare-only',action='store_true')
p.add_argument('--steps',type=int,default=74)
p.add_argument('--learning-rate',type=float,default=1e-5)
a=p.parse_args()
if a.output.exists():raise RuntimeError('Output already exists; use a fresh candidate directory')
if not 1<=a.steps<=1000 or not 0<a.learning_rate<=2e-5:raise ValueError('Invalid continuation schedule')
repo=Path(__file__).resolve().parents[2]
corpus=Path(__file__).with_name('character_recovery.json')
spec=json.loads(a.manifest.read_text(encoding='utf-8-sig'))
adapter_config=json.loads((a.source_adapter/'adapter_config.json').read_text(encoding='utf-8'))
if os.path.normcase(os.path.realpath(adapter_config['base_model_name_or_path']))!=os.path.normcase(os.path.realpath(spec['snapshot'])):
 raise RuntimeError('Adapter does not match pinned base snapshot')
source_hash=hashlib.sha256((a.source_adapter/'adapter_model.safetensors').read_bytes()).hexdigest()
system=repo.joinpath('Tools/assistant_presence/chat/character-system.txt').read_text(encoding='utf-8')
examples=json.loads(corpus.read_text(encoding='utf-8'))['examples']
if len(examples)!=20 or len({row[0] for row in examples})!=20:
 raise ValueError('Expected 20 distinct recovery examples')
train=[]
for row in examples:
 if len(row)!=5 or any(not isinstance(v,str) or not v.strip() for v in row):
  raise ValueError('Invalid recovery example')
 identifier,request,bad_answer,followup,correct_answer=row
 messages=[{'role':'system','content':system.replace('{name}',json.dumps('Maverick'))},
           {'role':'user','content':request},{'role':'assistant','content':bad_answer},
           {'role':'user','content':followup},{'role':'assistant','content':correct_answer}]
 train.append({'scenario':'recovery_'+identifier,'messages':messages,'origin':'recovery-final-answer-only'})

def import_rows(file,origin):
 rows=[]
 for line in file.read_text(encoding='utf-8').splitlines():
  if not line.strip():continue
  row=json.loads(line);messages=row['messages']
  match=re.search(r"The user's chosen name is (.*?); that identifies",messages[0]['content'])
  name=match.group(1).strip('"') if match else 'creator'
  if name=='the creator':name='creator'
  rows.append({'scenario':row['scenario'],'messages':[{'role':'system','content':system.replace('{name}',json.dumps(name,ensure_ascii=False))}]+messages[1:],'origin':origin})
 return rows
replay=import_rows(a.replay,'v6-replay');heldout=import_rows(a.heldout,'heldout-conversation')
if len(replay)!=128 or len(heldout)!=16:raise ValueError('Unexpected preserved V6 split')
train+=replay
if {r['scenario'] for r in train}&{r['scenario'] for r in heldout}:raise ValueError('Whole-conversation split overlaps')
os.environ['TOKENIZERS_PARALLELISM']='false'
from transformers import AutoTokenizer
tok=AutoTokenizer.from_pretrained(spec['snapshot'],local_files_only=True)
if tok.pad_token is None:tok.pad_token=tok.eos_token
encoded=[];lengths=[]
for split,rows in [('train',train),('heldout',heldout)]:
 for row in rows:
  full=tok.apply_chat_template(row['messages'],tokenize=True,add_generation_prompt=False)
  prefix=tok.apply_chat_template(row['messages'][:-1],tokenize=True,add_generation_prompt=True)
  if len(full)>1024:raise RuntimeError('Overlong example '+row['scenario']+': '+str(len(full)))
  if full[:len(prefix)]!=prefix or len(full)<=len(prefix):raise RuntimeError('Invalid assistant label boundary')
  lengths.append(len(full))
  if split=='train':encoded.append({'input_ids':full,'labels':[-100]*len(prefix)+full[len(prefix):]})
a.output.mkdir(parents=True)
for name,rows in [('train',train),('heldout',heldout)]:
 (a.output/(name+'.jsonl')).write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
manifest={'new_conversations':20,'new_answer_examples':20,'replay':len(replay),'train':len(train),'heldout':len(heldout),'max_tokens':max(lengths),'limit':1024,'corpus_sha256':hashlib.sha256(corpus.read_bytes()).hexdigest(),'system_sha256':hashlib.sha256(system.encode()).hexdigest(),'source_adapter_sha256':source_hash,'steps':a.steps,'learning_rate':a.learning_rate,'reference_turns_are_training_only':True,'live_deploy':False}
(a.output/'dataset_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('DAY_DATA_VALIDATED',json.dumps(manifest),flush=True)
if a.prepare_only:raise SystemExit(0)
import torch
from transformers import AutoModelForCausalLM,BitsAndBytesConfig,TrainingArguments,Trainer
from peft import PeftModel,prepare_model_for_kbit_training
from torch.utils.data import Dataset
if not torch.cuda.is_available():raise RuntimeError('CUDA required')
torch.manual_seed(151026)
quant=BitsAndBytesConfig(load_in_4bit=True,bnb_4bit_quant_type='nf4',bnb_4bit_use_double_quant=True,bnb_4bit_compute_dtype=torch.bfloat16)
base=AutoModelForCausalLM.from_pretrained(spec['snapshot'],quantization_config=quant,device_map={'':0},dtype=torch.bfloat16,local_files_only=True)
base=prepare_model_for_kbit_training(base,use_gradient_checkpointing=True)
model=PeftModel.from_pretrained(base,str(a.source_adapter),is_trainable=True)
model.config.use_cache=False
class Conversations(Dataset):
 def __len__(self):return len(encoded)
 def __getitem__(self,index):return encoded[index]
def collate(batch):
 size=max(len(b['input_ids']) for b in batch)
 return {'input_ids':torch.tensor([b['input_ids']+[tok.pad_token_id]*(size-len(b['input_ids'])) for b in batch]),'attention_mask':torch.tensor([[1]*len(b['input_ids'])+[0]*(size-len(b['input_ids'])) for b in batch]),'labels':torch.tensor([b['labels']+[-100]*(size-len(b['labels'])) for b in batch])}
args=TrainingArguments(output_dir=str(a.output/'checkpoints'),per_device_train_batch_size=1,gradient_accumulation_steps=4,max_steps=a.steps,learning_rate=a.learning_rate,warmup_steps=6,lr_scheduler_type='cosine',bf16=True,logging_steps=5,save_strategy='no',report_to='none',gradient_checkpointing=True,max_grad_norm=1,dataloader_num_workers=0,seed=151026,remove_unused_columns=False,disable_tqdm=True)
print('DAY_CANDIDATE_TRAIN_START',len(encoded),a.steps,flush=True)
started=time.monotonic()
trainer=Trainer(model=model,args=args,train_dataset=Conversations(),data_collator=collate)
result=trainer.train()
model.save_pretrained(a.output/'adapter');tok.save_pretrained(a.output/'adapter')
if hashlib.sha256((a.source_adapter/'adapter_model.safetensors').read_bytes()).hexdigest()!=source_hash:raise RuntimeError('Source adapter changed unexpectedly')
(a.output/'training_result.json').write_text(json.dumps({'metrics':result.metrics,'seconds':round(time.monotonic()-started,1),'source_adapter_sha256':source_hash,'model_spec':spec,'status':'trained_not_evaluated_not_deployed'},indent=2),encoding='utf-8')
print('DAY_CANDIDATE_TRAIN_DONE',result.metrics,flush=True)
