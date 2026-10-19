'use strict';
// Standalone experimental client for EXISTING Qwen3 V3.1 lab. No editor tools.
const http=require('node:http');
const {instructionsFor}=require('./personality');
function validateHistory(history){
 if(!Array.isArray(history)||!history.length)throw Error('Conversation is empty');
 const turns=history.slice(-12).map(m=>{
  if(!m||!['user','assistant'].includes(m.role)||typeof m.content!=='string')throw Error('Invalid conversation entry');
  const content=m.content.trim();if(!content||content.length>3000)throw Error('Invalid message length');
  return {role:m.role,content};
 });
 if(turns.at(-1).role!=='user')throw Error('A question is required');
 if(turns.at(-1).content.length>900)throw Error('Question must be under 900 characters');
 return turns;
}
function cannotObserveEditor(message){
 return /\b(?:look at|inspect|check|see|show|tell me about|what is|what's)\b.{0,100}\b(?:my|the|currently)\s+(?:selected|open|current|active)\s+(?:npc|character|entity|level|scene|object|project|terrain|editor|hierarchy)\b/i.test(message)
 || /\b(?:can you see|can you access|do you see|can you inspect|are you looking at)\b.{0,110}\b(?:editor|scene|selection|npc|viewport|level|project|hierarchy)\b/i.test(message)
 || /\b(?:select|move|delete|place|spawn|open|load|save|change)\b.{0,90}\b(?:in|inside|within|on)\s+(?:the|my)\s+(?:editor|scene|project|level|viewport)\b/i.test(message);
}
const noEditor="I can't inspect or control your open Renegade Studio project from this standalone companion yet. The Editor Bridge isn't connected. Describe the object or problem, and I'll help you work through it without claiming to see the scene.";
function isUnsupportedClaim(text){
 return /\b(?:I can see|I see|I'm looking at|I have inspected|I checked your|I've selected|I've opened|I opened your|I've changed|I have changed|I placed|I created your|I have generated your)\b.{0,130}\b(?:editor|viewport|scene|level|selected|selection|object|npc|project|mesh|hierarchy|terrain|game)\b/i.test(text);
}
async function askQwen(history,{signal,maxTokens=300}={}){
 const turns=validateHistory(history),latest=turns.at(-1).content;
 if(cannotObserveEditor(latest))return {answer:noEditor,adapter:'local-boundary',policy:'no-editor-bridge',elapsedMs:0};
 const body=Buffer.from(JSON.stringify({adapter:'v31',messages:turns,personality:instructionsFor(history),max_tokens:Math.max(64,Math.min(360,Math.round(maxTokens)))}),'utf8');
 if(body.length>48*1024)throw Error('Conversation request is too large');
 const start=performance.now();
 return new Promise((resolve,reject)=>{
  const req=http.request({hostname:'127.0.0.1',port:8383,path:'/chat',method:'POST',agent:false,headers:{'Content-Type':'application/json','Content-Length':body.length}},res=>{
   const chunks=[];let n=0;res.on('data',b=>{n+=b.length;if(n>90*1024){req.destroy(Error('Local model response exceeded size limit'));return}chunks.push(b)});
   res.on('end',()=>{try{
    if(res.statusCode!==200)throw Error('Qwen returned HTTP '+res.statusCode);
    const data=JSON.parse(Buffer.concat(chunks).toString('utf8'));
    if(data.error)throw Error('Local Qwen: '+String(data.error).slice(0,300));
    if(typeof data.answer!=='string'||!data.answer.trim())throw Error('No answer from local model');
    const cleaned=data.answer.replace(/<think>[\s\S]*?<\/think>/g,'').trim().slice(0,6000);
    if(!cleaned)throw Error('Local Qwen returned empty text');
    const disallowed=isUnsupportedClaim(cleaned);
    resolve({answer:disallowed?noEditor:cleaned,adapter:data.adapter||'v31',policy:disallowed?'unverified-editor-claim':data.policy||null,elapsedMs:Math.round(performance.now()-start)});
   }catch(e){reject(e)}})
  });
  req.on('error',e=>reject(signal?.aborted?Error('Request cancelled'):e));
  req.setTimeout(65000,()=>req.destroy(Error('Local Qwen timed out after 65 seconds')));
  if(signal){if(signal.aborted){req.destroy(Error('Request cancelled'));return}signal.addEventListener('abort',()=>req.destroy(Error('Request cancelled')),{once:true})}
  req.end(body);
 });
}
function speakableExcerpt(answer){
 if(typeof answer!=='string'||!answer.trim())throw Error('Nothing to speak');
 const plain=answer.replace(/\*\*/g,'').replace(/https?:\/\/\S+/g,'a reference link').replace(/\s+/g,' ').trim();
 if(plain.length<=900)return {text:plain,wasTruncated:false};
 const head=plain.slice(0,895);
 const stop=Math.max(head.lastIndexOf('. '),head.lastIndexOf('? '),head.lastIndexOf('! '));
 return {text:(stop>250?head.slice(0,stop+1):head).trim(),wasTruncated:true};
}
module.exports={askQwen,validateHistory,cannotObserveEditor,isUnsupportedClaim,speakableExcerpt};
