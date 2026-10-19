'use strict';
// Renegade voice prototype: LOCAL files only, fixed executables/voice IDs, no shell strings.
// Source: installed Piper en_GB-alan-medium voice or Windows SAPI fallback.
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const {spawn} = require('node:child_process');
const {performance} = require('node:perf_hooks');
const VOICES = Object.freeze([
 {id:'daniel', label:'Daniel - British male (Kokoro full-quality neural)'},
 {id:'hazel', label:'Hazel - British female (Windows offline)'},
 {id:'david', label:'David - American male (Windows offline)'}
]);
const PYTHON = path.join(__dirname,'.venv','Scripts','python.exe');
const MODEL = path.join(__dirname,'en_GB-alan-medium.onnx');
const WINDOWS_SCRIPT = path.join(__dirname,'windows-tts.ps1');
const psExe = path.join(process.env.SystemRoot || 'C:\\Windows','System32','WindowsPowerShell','v1.0','powershell.exe');

function validateSpeech(text,voice){
 if(typeof text !== 'string') throw new Error('Text must be a string.');
 const value = text.replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F]/g,' ').trim();
 if(!value || value.length > 900) throw new Error('Please enter between 1 and 900 characters.');
 if(!VOICES.some(v=>v.id===voice)) throw new Error('Unsupported voice.');
 return value;
}
function run(exe,args,text,signal){
 return new Promise((resolve,reject)=>{
  let proc;
  try{proc=spawn(exe,args,{windowsHide:true,stdio:['pipe','ignore','pipe'],shell:false,signal})}catch(err){reject(err);return;}
  let stderr='',settled=false;
  const timeout=setTimeout(()=>{if(!settled)proc.kill()},30000);
  proc.stderr.on('data',v=>{stderr=(stderr+v.toString()).slice(-4096)});
  proc.on('error',err=>{if(!settled){settled=true;clearTimeout(timeout);reject(err)}});
  proc.on('close',(code,sign)=>{
   if(settled)return;
   settled=true;clearTimeout(timeout);
   if(code===0)return resolve();
   reject(new Error(signal?.aborted?'Speech interrupted.':'Voice generator failed ('+code+'): '+(stderr.trim()||sign||'no diagnostic')));
  });
  proc.stdin.on('error',()=>{});
  proc.stdin.end(text,'utf8');
 });
}
async function generate({text,voice='alan',signal}){
 const clean=validateSpeech(text,voice);
 const dir=await fsp.mkdtemp(path.join(os.tmpdir(),'renegade-voice-'));
 const wav=path.join(dir,'spoken.wav');
 const start=performance.now();
 try{
  let engine;
  if(voice==='alan'){
   if(!fs.existsSync(PYTHON)||!fs.existsSync(MODEL))throw new Error('Offline Alan voice is not installed. Use Hazel or install the voice model.');
   await run(PYTHON,['-m','piper','-m',MODEL,'-f',wav],clean,signal);
   engine='Piper / Alan en-GB';
  }else{
   const name=voice==='david'?'Microsoft David Desktop':'Microsoft Hazel Desktop';
   await run(psExe,['-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',WINDOWS_SCRIPT,'-OutputPath',wav,'-VoiceName',name],clean,signal);
   engine='Windows / '+name;
  }
  const data=await fsp.readFile(wav);
  if(data.length<256 || data.toString('ascii',0,4)!=='RIFF' || data.toString('ascii',8,12)!=='WAVE')throw new Error('Speech output is not a valid WAV file.');
  if(data.length>20*1024*1024)throw new Error('Speech output exceeded the maximum allowed WAV size.');
  return {data,engine,elapsedMs:Math.round(performance.now()-start)};
 }finally{
  await fsp.rm(dir,{force:true,recursive:true}).catch(()=>{});
 }
}
module.exports={VOICES,generate,validateSpeech};
