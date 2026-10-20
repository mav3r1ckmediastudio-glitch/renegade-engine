'use strict';
// Actual companion/stream routes against live knowledge; never mutates Studio.
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path');
const {answerVerified}=require('../chat/verified-knowledge');
const {askQwenStream}=require('../chat/stream-client');
const {cleanForVoice}=require('../chat/live-speech');
(async()=>{
 const root=path.resolve(__dirname,'../../..');
 const cases=JSON.parse(fs.readFileSync(path.join(root,'Tests/assistant_knowledge/remaining_workflow_evaluation.json'),'utf8')).cases;
 const tasks=new Map(JSON.parse(fs.readFileSync(path.join(root,'Tools/assistant_knowledge/workflows.json'),'utf8')).workflows.map(t=>[t.id,t]));
 const times=[];
 for(const c of cases){
  const r=await answerVerified([{role:'user',content:c.question}]);
  assert(r,'client must route '+c.question);
  assert.equal(r.policy,'source-verified',c.question);
  assert(r.answer.split('\n',1)[0].includes(tasks.get(c.expected_workflow).label.toLowerCase()),c.question);
  assert(r.answer.includes(c.required),c.question);
  assert(r.sources.length>0,c.question);
  assert(!r.answer.includes('[Source:'));
  assert(!cleanForVoice(r.answer).includes('Studio/src/'));
  times.push(r.elapsedMs);
 }
 for(const [q,expected] of [
  ['How do I add animation?','Do you mean'],
  ['How do I import an asset?','Are you importing'],
  ['How do I add a script?','entity SCRIPT'],
  ['Create an entire world with NPCs automatically','cannot autonomously']
 ]){
  const r=await askQwenStream([{role:'user',content:q}]);
  assert.equal(r.adapter,'renegade-verified',q);
  assert(r.answer.includes(expected),q);
  assert(!r.answer.includes('\n1. '),q);
 }
 for(const [q,expected] of [
  ["What's next?",'Next stage'],
  ['Where do I click next?','cannot see which step'],
  ['What do I need?','Before you start'],
  ["It doesn't work",'Direct Interaction']
 ]){
  const r=await askQwenStream([
   {role:'user',content:'How do I set up a sliding door?'},
   {role:'assistant',content:'Earlier guide.'},{role:'user',content:q}
  ]);
  assert.equal(r.adapter,'renegade-verified',q);
  assert(r.answer.includes(expected),q);
 }
 const general=await answerVerified([{role:'user',content:'How do I set up a sliding door?'},
  {role:'assistant',content:'Earlier guide.'},{role:'user',content:'Breakfast?'},
  {role:'assistant',content:'Breakfast conversation.'},{role:'user',content:"What's next?"}]);
 assert.equal(general,null,'ordinary conversation must clear context');
 times.sort((a,b)=>a-b);
 console.log('PASS: full live workflows, clarification, follow-up and context boundary',
  {cases:cases.length,minMs:times[0],medianMs:times[Math.floor(times.length/2)],maxMs:times.at(-1)});
})().catch(e=>{console.error(e);process.exitCode=1});
