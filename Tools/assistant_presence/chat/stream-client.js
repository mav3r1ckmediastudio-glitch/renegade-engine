'use strict';
// Progressive Qwen output: isolated 8384 service, with unchanged 8383 as
// non-streaming fallback. Only local model output, no editor tools.
const http=require('node:http');
const {validateHistory,cannotObserveEditor,isUnsupportedClaim,askQwen}=require('./model-client');
const {tidyText}=require('./format-text');
const {answerVerified}=require('./verified-knowledge');
const {conversationCheck}=require('./conversation-check');
const {socialReply}=require('./social-flow');
const {instructionsFor}=require('./personality');
const NO_EDITOR="I can't inspect or control your open Renegade Studio project from this standalone companion yet. The Editor Bridge isn't connected. Describe the object or problem, and I'll help you work through it without claiming to see the scene.";
async function askQwenStream(history,{signal,onDelta=()=>{},onMeta=()=>{},maxTokens=240,port=8384,naturalConversation=process.env.RENEGADE_CHARACTER_MODE==='learned'}={}){
 const turns=validateHistory(history);
 const social=naturalConversation?null:socialReply(history.filter(m=>m&&typeof m.content==='string'&&['user','assistant'].includes(m.role)));
 if(social){onDelta(social);return {answer:social,adapter:'social-flow',policy:'social-conversation',firstTokenMs:0,elapsedMs:0,streaming:false};}
 const check=conversationCheck(turns.at(-1).content);
 if(check){onDelta(check);return {answer:check,adapter:'conversation-check',firstTokenMs:0,elapsedMs:0,streaming:false};}
 if(cannotObserveEditor(turns.at(-1).content)){
  onDelta(NO_EDITOR);
  return {answer:NO_EDITOR,adapter:'local-boundary',policy:'no-editor-bridge',firstTokenMs:0,elapsedMs:0,streaming:false};
 }
 // Check the verified knowledge base before any generative model call.
 // Neither Qwen nor the renderer is allowed to invent engine capabilities.
 const knowledge=await answerVerified(turns,{signal});
 if(knowledge){
  onDelta(knowledge.answer);
  return {...knowledge,adapter:'renegade-verified',verified:true,streaming:false};
 }
 const started=performance.now();
 const request=Buffer.from(JSON.stringify({messages:turns,personality:instructionsFor(history),max_tokens:Math.max(64,Math.min(320,maxTokens))}),'utf8');
 if(request.length>48000)throw Error('Request exceeds local model input limit.');
 try{
  return await new Promise((resolve,reject)=>{
   let settled=false,buffer='',output='',firstTokenMs=null,done=null,bytes=0;
   const finish=(error,result)=>{
    if(settled)return;settled=true;
    if(signal)signal.removeEventListener('abort',abort);
    if(error)reject(error);else resolve(result);
   };
   const req=http.request({hostname:'127.0.0.1',port,path:'/chat-stream',method:'POST',agent:false,
    headers:{'Content-Type':'application/json','Content-Length':request.length,'Accept':'application/x-ndjson'}},
    res=>{
     if(res.statusCode!==200){finish(Error('Streaming server HTTP '+res.statusCode));res.resume();return;}
     res.setEncoding('utf8');
     res.on('data',raw=>{
      if(settled)return;
      bytes+=Buffer.byteLength(raw,'utf8');
      if(bytes>160000){req.destroy(Error('Streaming answer exceeded size limit'));return;}
      buffer+=raw;
      let cut;
      while((cut=buffer.indexOf('\n'))>=0){
       const line=buffer.slice(0,cut);buffer=buffer.slice(cut+1);
       if(!line.trim())continue;
       try{
        const event=JSON.parse(line);
        if(event.type==='meta'){onMeta(event)}
        else if(event.type==='delta'){
         if(typeof event.text!=='string')throw Error('Bad token event');
         if(firstTokenMs===null&&event.text)firstTokenMs=Math.round(performance.now()-started);
         if(output.length+event.text.length>9000)throw Error('Streaming text exceeded limit');
         const piece=tidyText(event.text);
         output+=piece;onDelta(piece);
        }else if(event.type==='error'){throw Error(String(event.error||'Model generation failed'))}
        else if(event.type==='done'){done=event;}
       }catch(e){req.destroy(e);return;}
      }
     });
     res.on('end',()=>{
      if(settled)return;
      if(!done){finish(Error('Streaming server closed before completing the answer.'));return;}
      const candidate=tidyText(String(done.answer||output)).trim();
      if(!candidate){finish(Error('Qwen returned no answer'));return;}
      const bad=isUnsupportedClaim(candidate);
      finish(null,{
       answer:bad?NO_EDITOR:candidate,adapter:done.adapter||'v31',policy:bad?'unverified-editor-claim':done.policy||null,
       firstTokenMs,elapsedMs:Math.round(performance.now()-started),streaming:true,
       correction:bad
      });
     });
     res.on('error',e=>finish(e));
    });
   const abort=()=>req.destroy(Error('Request cancelled'));
   req.on('error',e=>finish(e));
   req.setTimeout(65000,()=>req.destroy(Error('Qwen stream timed out after 65 seconds')));
   if(signal){if(signal.aborted){req.destroy(Error('Request cancelled'));return}
    signal.addEventListener('abort',abort,{once:true});}
   req.end(request);
  });
 }catch(e){
  if(signal?.aborted)throw Error('Request cancelled');
  // Treat 8384 as optional. HTTP 404 means another service owns this port.
  // Always preserve the healthy 8383 /chat path on endpoint mismatch.
  // If streaming isn't available, retain basic local Qwen conversation.
  // Never claim a fallback reply was streamed.
  onMeta({fallback:true});
  const result=await askQwen(history,{signal,maxTokens});
  onDelta(tidyText(result.answer));
  return {...result,firstTokenMs:result.elapsedMs,streaming:false,fallback:true};
 }
}
module.exports={askQwenStream};
