'use strict';
// Read-only support answer routing. The Python service owns source verification;
// this client only classifies explicit engine questions and forwards them.
// If offline, *never* treat Qwen as an authoritative source about editor features.
const http=require('node:http');
const KEYWORDS=new Set([
 'renegade','wicked','studio','editor','inspector','projectile','projectiles',
 'hitscan','arrow','arrows','bullet','bullets','bolt','bolts','spawn','bow','weapon','weapons','equipment','loadout','loadouts',
 'surface','surfaces','retarget','script','npc','scene','scenes',
 'asset','assets','launch','player','model','import','importer','terrain',
 'animation','character','characters','npcs','scripts','action','actions','lua',
 'patrol','wander','wandering','socket','sockets','psp','timing','gravity','glass','impact','impacts','decals','decal','damage','sfx','fx'
]);
const CAPABILITY=new Set(['engine','world','game','level']);
const VERBS=new Set(['create','build','generate','automatically','autonomous','make','implement','supports']);
const SAFE_UNAVAILABLE="Renegade's verified knowledge service is unavailable. I won't guess about engine controls. Start the local knowledge bridge, then ask again.";
function isEngineQuestion(question){
 if(/^(?:hello|hi|hey|hiya|alright|good morning|good afternoon|good evening)(?: there)?(?:[, ]+renegade)?[.!? ]*$/i.test(String(question).trim()))return false;
 question=String(question).replace(/^\s*(?:(?:hello|hi|hey)\s+)?renegade\s*[,!:]\s*/i,'')
  .replace(/,\s*renegade\s*[.!?]*\s*$/i,'');
 // Questions about the orb's fictional identity are conversational; editor
 // Character import/configuration still goes through verified guidance.
 if(/^(?:is that something you actually did(?:,? or part of your character)?|tell me about your (?:fictional )?(?:day|backstory)|who are you|what are you)[?.! ]*$/i.test(question) &&
    !/\b(?:import|editor|scene|project|npc|script|animation)\b/i.test(question))return false;
 const words=new Set(String(question).toLowerCase().match(/[a-z0-9_]+/g)||[]);
 const importRequest=/\b(?:import|load|bring in)\b.{0,55}\b(?:mesh|glb|fbx|3d object)s?\b/i.test(String(question));
 const stockRequest=/\b(?:set up|setup|make|create|add|attach|wire|connect|trigger)\b.{0,50}\b(?:sliding door|interactive door|door action|switch|trigger zone|pickup|relay|sound action|play sound action)\b/i.test(String(question));
 return stockRequest || importRequest || [...words].some(w=>KEYWORDS.has(w)) ||
  ([...words].some(w=>CAPABILITY.has(w)) && [...words].some(w=>VERBS.has(w)));
}
// Source references remain available on demand, but must not pollute the
// conversational answer or be read aloud by Daniel.
function separateSources(raw){
 const sources=[];
 const clean=String(raw??'').replace(/\s*\[Source:\s*([^\]\r\n]+)\]/g,(_match,path)=>{
  const source=path.trim();
  if(source && !sources.includes(source))sources.push(source);
  return '';
 }).replace(/[ \t]+\n/g,'\n').trim();
 return {answer:clean,sources};
}
async function answerVerified(history,{signal,timeout=4500,port=18384}={}){
 const question=history?.at(-1)?.content;
 const followup=/^(?:what(?:'s| is)? next|where (?:do i|should i) click next|what (?:do i need|are the prerequisites)|before (?:i|we) start|it (?:doesn't|does not) work|why (?:doesn't|does not) it work|what about (?:one with )?gravity off|i (?:don't|do not) have (?:that|the prerequisites))[?.! ]*$/i;
 const prior=history?.slice(0,-1).filter(m=>m.role==='user').slice(-8);
 const previousTask=prior?.filter(m=>!followup.test(String(m.content).trim())&&!/^(?:then\s+)?what does (?:the classification|that|it) (?:actually )?(?:change|do|mean)[?.! ]*$/i.test(String(m.content).trim())).at(-1);
 const explanation=/^(?:then\s+)?what does (?:the classification|that|it) (?:actually )?(?:change|do|mean)[?.! ]*$/i;
 const contextual=typeof question==='string'&&(followup.test(question.trim())||explanation.test(question.trim()))&&
   previousTask && isEngineQuestion(previousTask.content);
 const stockTask=typeof question==='string'&&/\b(?:set up|setup|make|create|add|attach|wire|connect|trigger)\b.{0,50}\b(?:sliding door|interactive door|door action|switch|trigger zone|pickup|relay|sound action|play sound action)\b/i.test(question);
 if(typeof question!=='string'||(!isEngineQuestion(question)&&!contextual&&!stockTask))return null;
 const start=performance.now();
 const body=Buffer.from(JSON.stringify({adapter:'v31',messages:history.slice(-12),max_tokens:480}), 'utf8');
 if(body.length>32000)return {answer:SAFE_UNAVAILABLE,policy:'knowledge-size-limit',firstTokenMs:0,elapsedMs:0};
 try {
  return await new Promise(resolve=>{
   let settled=false;
   const finish=obj=>{
    if(settled)return;settled=true;
    signal?.removeEventListener('abort',abort);
    resolve(obj);
   };
   const fail=()=>finish({answer:SAFE_UNAVAILABLE,policy:'verified-service-unavailable',
    firstTokenMs:0,elapsedMs:Math.round(performance.now()-start)});
   const req=http.request({
    hostname:'127.0.0.1',port,path:'/chat',method:'POST',agent:false,
    headers:{'Content-Type':'application/json','Content-Length':body.length}
   },res=>{
    const chunks=[];let bytes=0;
    res.on('data',buf=>{
     bytes+=buf.length;
     if(bytes>20000){req.destroy();return;}
     chunks.push(buf);
    });
    res.on('end',()=>{
     try{
      if(res.statusCode!==200)throw Error('Knowledge HTTP '+res.statusCode);
      const data=JSON.parse(Buffer.concat(chunks).toString('utf8'));
      if(data.route!=='local_verified_knowledge'||typeof data.answer!=='string'||!data.answer.trim())
       throw Error('Invalid knowledge service result');
      const elapsedMs=Math.round(performance.now()-start);
      const parsed=separateSources(data.answer);
      if(!parsed.answer)throw Error('Empty verified knowledge answer');
      finish({...parsed,policy:'source-verified',
              firstTokenMs:elapsedMs,elapsedMs});
     }catch(_){fail();}
    });
    res.on('error',fail);
   });
   const abort=()=>req.destroy(new Error('Request cancelled'));
   req.on('error',err=>{if(signal?.aborted)finish({answer:'Request cancelled.',policy:'cancelled',firstTokenMs:0,elapsedMs:0});else fail();});
   req.setTimeout(timeout,()=>req.destroy(new Error('Knowledge request timed out')));
   if(signal){if(signal.aborted){abort();return;} signal.addEventListener('abort',abort,{once:true});}
   req.end(body);
  });
 }catch(_){
  return {answer:SAFE_UNAVAILABLE,policy:'verified-service-unavailable',firstTokenMs:0,elapsedMs:0};
 }
}
module.exports={isEngineQuestion,answerVerified,separateSources,SAFE_UNAVAILABLE};
