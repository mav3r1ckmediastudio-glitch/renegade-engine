'use strict';
process.env.RENEGADE_PROFILE_PATH=require('node:path').join(require('node:os').tmpdir(),'renegade-test-no-profile-'+process.pid+'.cfg');
const assert=require('node:assert/strict');
const {socialReply}=require('../chat/social-flow');
const {askQwenStream}=require('../chat/stream-client');
const single=t=>[{role:'user',content:t}];
const cases=['Hello Renegade','Hi','Alright?','Hello Renegade, how are you?','How are you doing?',"I'm good thanks",'Thanks Renegade','Cheers mate','That is why I said hello Renegade, I was talking to you.','I was just saying hello','Can we just chat?','Call me Maverick','Bye'];
for(const t of cases)assert(socialReply(single(t)),t);
for(const t of ['Hello Renegade, how do I import a character?','Thanks, but how do I save a player prefab?','I said hello and I was talking to you about importing a character','How do I make an NPC patrol?','What is Renegade?','yes','what next','it does not work'])assert.equal(socialReply(single(t)),null,t);
assert.match(socialReply([...single('Hello Renegade'),{role:'assistant',content:'Hello!'},{role:'user',content:'Hello Renegade'}]),/here/);
assert.match(socialReply([...single('Call me Maverick'),{role:'assistant',content:'Hi Maverick'},{role:'user',content:"What's my name?"}]),/Maverick/);
assert.match(socialReply(single("What's my name?")),/haven't told/);
(async()=>{
 for(const text of cases){
  const result=await askQwenStream(single(text));
  assert.equal(result.adapter,'social-flow',text);assert(!result.verified);
  assert(!/verified|knowledge database|source|editor features/i.test(result.answer),text);
 }
 const result=await askQwenStream(single('Hello Renegade, how do I import a character?'));
 assert(result.verified);assert.match(result.answer,/Character/i);
 console.log('PASS 13 live social cases, 8 mixed/workflow exclusions, greeting continuity and session name recall');
})().catch(e=>{console.error(e);process.exitCode=1});
