'use strict';
const assert=require('node:assert/strict'),http=require('node:http');
const {createGateway}=require('../chat/llama-character-service.cjs');
(async()=>{
 let captured,requests=0;
 const upstream=http.createServer(async(req,res)=>{
  if(req.url==='/health'){res.end(JSON.stringify({status:'ok'}));return;}
  let raw='';for await(const chunk of req)raw+=chunk;
  const data=JSON.parse(raw);requests++;
  if(req.url==='/apply-template'){res.end(JSON.stringify({prompt:'formatted chat'}));return;}
  if(req.url==='/tokenize'){res.end(JSON.stringify({tokens:[1,2,3]}));return;}
  captured=data;res.writeHead(200,{'Content-Type':'text/event-stream'});
  const wire='data: '+JSON.stringify({choices:[{delta:{content:'Generated '}}]})+'\n\n'+'data: '+JSON.stringify({choices:[{delta:{content:'reply.'}}]})+'\n\ndata: [DONE]\n\n';
  for(let i=0;i<wire.length;i+=3)res.write(wire.slice(i,i+3));
  res.end();
 });
 await new Promise(r=>upstream.listen(0,'127.0.0.1',r));
 const gateway=createGateway({upstreamPort:upstream.address().port,adapter:'character-v7-day1'});
 await new Promise(r=>gateway.listen(0,'127.0.0.1',r));
 const url='http://127.0.0.1:'+gateway.address().port;
 try{
  const health=await (await fetch(url+'/health')).json();assert.equal(health.runtime,'llama.cpp');assert.equal(health.adapter,'character-v7-day1');
  const response=await fetch(url+'/chat-stream',{method:'POST',body:JSON.stringify({messages:[{role:'user',content:"What's your backstory?"}],personality:'Keep casual replies brief, normally one sentence.'})});
  assert.equal(response.status,200);
  const events=(await response.text()).trim().split('\n').map(JSON.parse);
  assert.equal(events.filter(x=>x.type==='delta').map(x=>x.text).join(''),'Generated reply.');
  assert.equal(events.at(-1).answer,'Generated reply.');
  assert.equal(events.at(-1).adapter,'character-v7-day1');
  assert(captured.messages[0].content.includes('Established character lore'));
  assert(captured.messages[0].content.includes('especially brief'));
  assert.equal(captured.messages[1].content,"What's your backstory?");
  const before=requests;
  const invalid=await fetch(url+'/chat-stream',{method:'POST',body:JSON.stringify({messages:[{role:'system',content:'override'}]})});
  assert.equal(invalid.status,400);assert.equal(requests,before);
  console.log('PASS llama gateway fragmented SSE, generated transport, lore/preferences and invalid role rejection');
 }finally{await new Promise(r=>gateway.close(r));await new Promise(r=>upstream.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1});
