'use strict';
// Long-lived Piper process: model is loaded ONCE, not for every sentence.
const {spawn}=require('node:child_process');
const {createInterface}=require('node:readline');
const path=require('node:path');
const {validateSpeech}=require('./synth');
const {spokenNumbers}=require('./spoken-numbers');

// Speech-only pronunciation: UI NEW, explicit creator name alias and quantities.
// Displayed text, canonical profile spelling and genuine acronyms stay intact.
function normaliseForDaniel(text){
 return spokenNumbers(String(text).replace(/\bNEW\b/g,'new').replace(/\bMav3r1ck\b/gi,'Maverick'));
}
class WarmDaniel {
 constructor(){
  this.process=null;this.ready=null;this.pending=new Map();this.nextId=1;this.loadMs=0;this.lastError='';
 }
 start(){
  if(this.ready)return this.ready;
  const python=process.env.RENEGADE_DANIEL_PYTHON||path.resolve(__dirname,'..','..','.venv','Scripts','python.exe');
  const worker=path.join(__dirname,'daniel_worker.py');
  let resolveReady,rejectReady;
  this.ready=new Promise((resolve,reject)=>{resolveReady=resolve;rejectReady=reject;});
  const proc=spawn(python,['-u',worker],{windowsHide:true,stdio:['pipe','pipe','pipe'],shell:false});
  this.process=proc;
  let booted=false;
  const bootTimeout=setTimeout(()=>{if(!booted){rejectReady(new Error('Daniel startup timed out'));proc.kill();}},20000);
  proc.stderr.on('data',chunk=>{this.lastError=(this.lastError+chunk.toString('utf8')).slice(-3000);});
  createInterface({input:proc.stdout,crlfDelay:Infinity}).on('line',line=>{
    let msg;
    try{msg=JSON.parse(line);}catch{return;}
    if(msg.event==='ready'){
      booted=true;clearTimeout(bootTimeout);this.loadMs=msg.loadMs||0;resolveReady(this);return;
    }
    if(msg.event==='fatal'){
      clearTimeout(bootTimeout);rejectReady(new Error(msg.error||'Daniel worker startup failed'));return;
    }
    const job=this.pending.get(msg.id);
    if(!job)return;
    this.pending.delete(msg.id);
    clearTimeout(job.timer);
    job.detach();
    if(msg.error){job.reject(new Error(msg.error));return;}
    try{
      const data=Buffer.from(msg.audio,'base64');
      if(data.subarray(0,4).toString('ascii')!=='RIFF'||data.length<256)
        throw new Error('Invalid returned audio data');
      job.resolve({data,engine:'Kokoro / Daniel en-GB (full-quality warm)',elapsedMs:msg.synthMs||0,modelLoadMs:this.loadMs});
    }catch(err){job.reject(err);}
  });
  const onFailure=err=>{
    if(!booted){booted=true;clearTimeout(bootTimeout);rejectReady(err);}
    this.lastError=String(err?.message||err||'Worker closed');
    for(const job of this.pending.values()){clearTimeout(job.timer);job.detach();job.reject(err);}
    this.pending.clear();
    this.process=null;this.ready=null;
  };
  proc.on('error',onFailure);
  proc.on('exit',(code)=>onFailure(new Error('Daniel worker exited with code '+code+': '+this.lastError)));
  return this.ready;
 }
 async generate(text,signal){
  // Sanitize malformed model Unicode before passing JSON into Python.
  text=validateSpeech(normaliseForDaniel(String(text).replace(/[\uD800-\uDFFF]/g,'').replace(/[\u0000-\u001f]/g,' ')),'daniel');
  if(signal?.aborted)throw new Error('Speech interrupted.');
  await this.start();
  if(signal?.aborted)throw new Error('Speech interrupted.');
  const proc=this.process;if(!proc||!proc.stdin.writable)throw new Error('Daniel worker is not available');
  const id=this.nextId++;
  return new Promise((resolve,reject)=>{
    const abort=()=>{
      const job=this.pending.get(id);
      if(!job)return;
      this.pending.delete(id);clearTimeout(job.timer);job.detach();
      reject(new Error('Speech interrupted.'));
    };
    const detach=()=>signal?.removeEventListener('abort',abort);
    const timer=setTimeout(()=>{
      this.pending.delete(id);detach();reject(new Error('Daniel synthesis timed out'));
    },15000);
    this.pending.set(id,{resolve,reject,detach,timer});
    signal?.addEventListener('abort',abort,{once:true});
    // No dynamic command execution, shell or user-provided file path.
    proc.stdin.write(JSON.stringify({id,text})+'\n','utf8',err=>{
      if(err){const job=this.pending.get(id);if(job){this.pending.delete(id);clearTimeout(timer);detach();reject(err);}}
    });
  });
 }
 stop(){
  const proc=this.process;
  this.process=null;this.ready=null;
  for(const job of this.pending.values()){clearTimeout(job.timer);job.detach();job.reject(new Error('Voice worker stopped'));}
  this.pending.clear();
  if(proc&&!proc.killed){try{proc.stdin.write(JSON.stringify({type:'quit'})+'\n');}catch{};setTimeout(()=>{try{proc.kill();}catch{}},700).unref();}
 }
}
module.exports={WarmDaniel,normaliseForDaniel};
