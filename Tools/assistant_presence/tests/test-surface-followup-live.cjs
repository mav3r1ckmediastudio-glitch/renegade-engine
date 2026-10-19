'use strict';
const assert=require('node:assert/strict');
const {answerVerified}=require('../chat/verified-knowledge');
(async()=>{
 const history=[];
 for(const q of ["A glass impact classification means my window will shatter, doesn't it?",
                  'Then what does the classification actually change?',
                  'What does that mean?']){
  history.push({role:'user',content:q});
  const r=await answerVerified(history);
  assert.equal(r?.policy,'source-verified',q);
  assert.match(r.answer,/Glass classification does not make a pane destructible/i);
  assert.match(r.answer,/without changing every object sharing its material/i);
  assert(!/stick into|energy absorbed|jagged split/i.test(r.answer));
  assert(r.sources.length>0);
  history.push({role:'assistant',content:r.answer});
 }
 for(const q of ['What are you?','Tell me your backstory.','I like reading novels.']){
  assert.equal(await answerVerified([...history,{role:'user',content:q}]),null);
  assert.equal(await answerVerified([...history,{role:'user',content:q},
    {role:'assistant',content:'Okay.'},
    {role:'user',content:'Then what does the classification actually change?'}]),null);
 }
 console.log('PASS verified glass classification, chained explanations, casual and identity reset');
})().catch(e=>{console.error(e);process.exitCode=1});
