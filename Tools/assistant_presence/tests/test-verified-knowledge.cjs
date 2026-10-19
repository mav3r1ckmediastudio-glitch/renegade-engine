'use strict';
const assert=require('node:assert/strict');
const http=require('node:http');
const {answerVerified,isEngineQuestion}=require('../chat/verified-knowledge');
const {askQwenStream}=require('../chat/stream-client');
async function run(){
 assert(isEngineQuestion('How do I apply my arrow projectile?'));
 assert(isEngineQuestion('Can Renegade create a whole world automatically?'));
 assert(!isEngineQuestion('Alright?'));
 assert(!isEngineQuestion('What did you have for breakfast?'));
 const server=http.createServer((req,res)=>{
  if(req.method!=='POST'||req.url!=='/chat'){res.writeHead(404);res.end();return;}
  let raw='';
  req.on('data',c=>raw+=c.toString());
  req.on('end',()=>{
   const body=JSON.parse(raw);
   const q=body.messages.at(-1).content;
   res.writeHead(200,{'Content-Type':'application/json'});
   res.end(JSON.stringify({route:'local_verified_knowledge',
    answer:'Choose SAVE AS NEW, then APPLY TO THIS PLAYER. [Source: Studio/src/PlayerProjectileEditor.cpp]'}));
  });
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 try{
  const port=server.address().port;
  const answer=await answerVerified([{role:'user',content:'How do I apply my arrow projectile?'}],{port});
  assert.match(answer.answer,/SAVE AS NEW/);
  assert.equal(answer.policy,'source-verified');
  const neutral=await answerVerified([{role:'user',content:'Alright?'}],{port});
  assert.equal(neutral,null);
  const unavailable=await answerVerified([{role:'user',content:'How does my bow work?'}],{port:65525,timeout:200});
  assert.match(unavailable.answer,/won't guess/);
  assert.equal(unavailable.policy,'verified-service-unavailable');
 }finally{
  await new Promise(resolve=>server.close(resolve));
 }
 // End-to-end prefix routing uses the live local knowledge bridge on DESIGN.
 // Test source fallback semantics with the real installed bridge if healthy.
 const live=await answerVerified([{role:'user',content:'How do I save a projectile and assign it to the player?'}],{timeout:3000});
 assert(live && live.answer && (live.answer.includes('SAVE AS NEW')||live.answer.includes("won't guess")));
 console.log('PASS: V1 companion verified-knowledge priority, ordinary chat separation, no hallucinating when index unavailable');
}
run().catch(err=>{console.error(err);process.exitCode=1;});
