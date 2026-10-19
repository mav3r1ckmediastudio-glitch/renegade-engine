'use strict';
// Local inference transport only. Explicit trained character identity; no editor actions.
const http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const {readName}=require('./studio-profile');
function systemFor(data){
 let text=fs.readFileSync(path.join(__dirname,'character-system.txt'),'utf8').replace('{name}',JSON.stringify(readName()||'creator'));
 const previous=data.personality;
 if(typeof previous==='string'){
  if(previous.includes('Do not add jokes or workshop anecdotes unless invited.'))text+=' The creator has requested less humour: keep this straightforward.';
  if(previous.includes('Keep casual replies brief, normally one sentence.'))text+=' The creator has requested especially brief casual replies.';
 }
 return text+' Current session facts: editor_bridge_connected=false; scene_snapshot=null; available_editor_actions=none. Only conversation text and confirmed profile identity are supplied here.';
}
function validate(data){
 if(!data||typeof data!=='object'||Array.isArray(data)||!Array.isArray(data.messages)||!data.messages.length)throw Error('Expected conversation');
 const turns=data.messages.slice(-12).map(item=>{
  if(!item||!['user','assistant'].includes(item.role)||typeof item.content!=='string'||!item.content.length||item.content.length>3000)throw Error('Invalid conversation entry');
  return {role:item.role,content:item.content};
 });
 if(turns.at(-1).role!=='user'||turns.at(-1).content.length>900)throw Error('Expected latest user question');
 const requested=data.max_tokens===undefined?120:Number(data.max_tokens);
 if(!Number.isFinite(requested))throw Error('Invalid token limit');
 return {turns,maximum:Math.max(48,Math.min(180,Math.trunc(requested)))};
}
function createGateway({upstreamPort=18386,spec={model:'Qwen/Qwen3-4B-Instruct-2507'},scale=.65,quantisation='unknown',adapter='character-v6'}={}){
 if(!Number.isInteger(upstreamPort)||upstreamPort<1||upstreamPort>65535)throw Error('Invalid local port');
 if(!['character-v6','character-v7-day1'].includes(adapter))throw Error('Unsupported trained character');
 const upstream='http://127.0.0.1:'+upstreamPort;
 async function post(endpoint,body,signal){
  const response=await fetch(upstream+endpoint,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body),signal});
  if(!response.ok)throw Error('Local inference rejected request: '+(await response.text()).slice(0,300));
  return response.json();
 }
 return http.createServer(async(req,res)=>{
  const json=(body,status=200)=>{res.writeHead(status,{'Content-Type':'application/json'});res.end(JSON.stringify(body));};
  if(req.method==='GET'&&req.url==='/health'){
   try{
    const response=await fetch(upstream+'/health',{signal:AbortSignal.timeout(1500)});
    const h=await response.json();
    if(!response.ok||h.status!=='ok')throw Error('Backend unavailable');
    json({status:'ready',adapter,runtime:'llama.cpp',model:spec.model,revision:spec.revision,streaming:true,read_only:true,scale,quantisation});
   }catch{json({status:'unavailable',runtime:'llama.cpp'},503);}
   return;
  }
  if(req.method!=='POST'||req.url!=='/chat-stream'){json({error:'Not found'},404);return;}
  let data,turns,maximum;
  try{
   let size=0;const chunks=[];
   for await(const chunk of req){size+=chunk.length;if(size>48000)throw Error('Request too large');chunks.push(chunk);}
   if(size<2)throw Error('Invalid request size');
   data=JSON.parse(Buffer.concat(chunks).toString('utf8'));
   ({turns,maximum}=validate(data));
  }catch(e){json({error:e.message},400);return;}
  const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),65000);
  res.on('close',()=>{if(!res.writableFinished)controller.abort();});
  const event=obj=>{if(!res.destroyed)res.write(JSON.stringify(obj)+'\n');};
  try{
   const messages=[{role:'system',content:systemFor(data)},...turns];
   let count;
   // Use the backend's own chat template/tokenizer when enforcing the budget.
   while(true){
    const formatted=await post('/apply-template',{messages,add_generation_prompt:true},controller.signal);
    const tokenized=await post('/tokenize',{content:formatted.prompt,add_special:false},controller.signal);
    count=tokenized.tokens.length;
    if(count+maximum<=4096)break;
    if(messages.length<=2)throw Error('Latest question exceeds context budget');
    messages.splice(1,messages[1].role==='user'&&messages[2]?.role==='assistant'?2:1);
   }
   const started=performance.now();let first=null,answer='',buffer='',finished=false,timings;
   const response=await fetch(upstream+'/v1/chat/completions',{method:'POST',headers:{'Content-Type':'application/json'},signal:controller.signal,
    body:JSON.stringify({messages,stream:true,max_tokens:maximum,temperature:.7,top_p:.8,top_k:20,min_p:0,cache_prompt:true})});
   if(!response.ok)throw Error('Local inference failed: '+(await response.text()).slice(0,300));
   res.writeHead(200,{'Content-Type':'application/x-ndjson; charset=utf-8','Cache-Control':'no-store'});
   event({type:'meta',adapter,runtime:'llama.cpp',model:spec.model,prompt_tokens:count});
   const decoder=new TextDecoder();
   for await(const bytes of response.body){
    buffer+=decoder.decode(bytes,{stream:true});let cut;
    while((cut=buffer.indexOf('\n'))>=0){
     const line=buffer.slice(0,cut).trim();buffer=buffer.slice(cut+1);
     if(!line.startsWith('data: '))continue;
     if(line==='data: [DONE]'){finished=true;continue;}
     const value=JSON.parse(line.slice(6));
     if(value.error)throw Error(value.error.message||'Inference stream failed');
     if(value.timings)timings=value.timings;
     const piece=value.choices?.[0]?.delta?.content;
     if(typeof piece==='string'&&piece){if(first===null)first=Math.round(performance.now()-started);answer+=piece;event({type:'delta',text:piece});}
    }
   }
   if(!finished||!answer.trim())throw Error('Incomplete inference stream');
   event({type:'done',answer:answer.trim(),adapter,runtime:'llama.cpp',first_delta_ms:first,elapsed_ms:Math.round(performance.now()-started),timings});
   res.end();
  }catch(e){
   if(!res.destroyed){if(res.headersSent){event({type:'error',error:e.message});res.end();}else json({error:e.message},502);}
  }finally{clearTimeout(timer);}
 });
}
if(require.main===module){
 const configFile=process.argv[2];if(!configFile)throw Error('Expected private runtime configuration path');
 const config=JSON.parse(fs.readFileSync(configFile,'utf8'));
 const spec=JSON.parse(fs.readFileSync(config.manifest,'utf8'));
 const server=createGateway({spec,adapter:config.label||'character-v6',upstreamPort:18386,scale:.65,quantisation:config.quantisation||(/qwen4b-q8\.gguf$/i.test(config.model)?'Q8_0':'unknown')});
 server.listen(8384,'127.0.0.1',()=>console.log('CHARACTER_LLAMA_GATEWAY_READY'));
}
module.exports={createGateway,validate,systemFor};
