'use strict';
const fs=require('node:fs'),path=require('node:path');
const profile=require('./studio-profile');
function requestPath(){return process.env.LOCALAPPDATA?path.join(process.env.LOCALAPPDATA,'RenegadeStudio','living-core-startup.cfg'):null}
function readRequest(file=requestPath()){
 try{
  if(!file||Date.now()-fs.statSync(file).mtimeMs>15*60*1000)return null;
  const text=fs.readFileSync(file,'utf8');
  const stage=text.match(/^stage=(name|handshake|hub)$/m)?.[1];
  const pid=Number(text.match(/^pid=(\d+)$/m)?.[1]);if(!stage||!pid)return null;
  try{process.kill(pid,0)}catch{return null}
  return {stage,pid,key:pid+':'+stage};
 }catch{return null}
}
class StartupGuide{
 constructor({read=readRequest,name=profile.readName,save=profile.saveName}={}){
  this.read=read;this.name=name;this.save=save;this.key=null;this.stage=null;this.pending=null;this.firstMeeting=false;
 }
 poll(){
  const request=this.read();
  if(!request){this.stage=null;this.pending=null;return null}
  if(request.key===this.key)return null;
  this.key=request.key;this.stage=request.stage;this.pending=null;
  if(this.stage==='name'){this.firstMeeting=true;return "I'm Renegade. This orb is me, and this is where we'll build things together. Before we meet the Hub, what would you like me to call you? Type your name, or enable voice conversation if you'd rather tell me.";}
  if(this.stage==='handshake')return (this.firstMeeting?"Good to meet you, ":"Welcome back, ")+ (this.name()||'creator')+". Your name is saved. The handshake is ready; click ENTER HUB when you're ready to begin.";
  return null;
 }
 answer(text){
  if(this.stage!=='name'||this.name())return null;
  const trimmed=String(text).trim(),lower=trimmed.toLowerCase().replace(/[.!]/g,'');
  if(this.pending){
   if(/^(yes|yeah|yep|correct|that's right|that is right)$/.test(lower)){
    const name=this.save(this.pending);this.pending=null;
    return "Good to meet you, "+name+". I've saved your chosen name. Next comes the handshake, then ENTER HUB.";
   }
   if(/^(no|nope|cancel)$/.test(lower)){this.pending=null;return "No problem. What name would you prefer?"}
  }
  const explicit=trimmed.match(/^(?:my name is|call me)\s+(.+)$/i);
  const candidate=profile.normaliseName(explicit?explicit[1]:trimmed);
  if(!candidate||(!explicit&&/[?]|\b(?:how|why|what|import|create|hello|hi)\b/i.test(candidate)))
   return "What would you like me to call you? You can type just your name.";
  this.pending=candidate;
  return "I'll call you "+candidate+". Is that right? Say yes to save it, or give me a different name.";
 }
}
module.exports={StartupGuide,readRequest,requestPath};
