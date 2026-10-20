'use strict';
process.env.RENEGADE_PROFILE_PATH=require('node:path').join(require('node:os').tmpdir(),'renegade-test-no-profile-'+process.pid+'.cfg');
const assert=require('node:assert/strict');
const {askQwenStream}=require('../chat/stream-client');
const {instructions}=require('../chat/personality');
assert(instructions.length<3000);
const history=[];
(async()=>{
 for(const text of ['Hello Renegade','Call me Maverick','How are you doing?', "I'm good thanks","What's my name?", "What did I ask you to call me?",'That is why I said hello Renegade, I was talking to you.','Thanks Renegade']){
  history.push({role:'user',content:text});
  const response=await askQwenStream(history);
  assert.equal(response.adapter,'social-flow',text);
  if(text.includes('my name')||text.includes('call me?'))assert.match(response.answer,/Maverick/);
  assert(response.answer.length<220,text);
  assert(!/knowledge database|verified answer|editor features|how can i help/i.test(response.answer),text);
  history.push({role:'assistant',content:response.answer});
 }
 history.push({role:'user',content:'Hello Renegade, how do I import a character?'});
 const workflow=await askQwenStream(history);
 assert(workflow.verified);assert(workflow.sources.length);assert.match(workflow.answer,/Before you start/i);
 console.log('PASS 7-turn persona dialogue, name recall, correction, brief acknowledgements and verified Character handoff');
})().catch(e=>{console.error(e);process.exitCode=1});
