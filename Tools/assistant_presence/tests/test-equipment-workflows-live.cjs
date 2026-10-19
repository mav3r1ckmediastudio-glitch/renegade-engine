'use strict';
const assert=require('node:assert/strict');
const {answerVerified}=require('../chat/verified-knowledge');
const {cleanForVoice}=require('../chat/live-speech');
(async()=>{
 for(const [question,expected] of [
  ['How do I create equipment?','7. Save the level'],
  ['How do I create a weapon from an assembly?','7. Save the level'],
  ['How do I assign starting equipment?','5. Save the level'],
  ['How do I apply my loadout?','5. Save the level'],
  ['How do I assign a projectile to my weapon?','6. Click **APPLY TO THIS PLAYER**']
 ]){
  const result=await answerVerified([{role:'user',content:question}]);
  assert.equal(result.policy,'source-verified');
  assert(result.answer.includes(expected));
  assert(result.answer.indexOf('**Before you start:**')<result.answer.indexOf('\n1. '));
  assert(result.sources.length>0);
  assert(!result.answer.includes('[Source:'));
  assert(!cleanForVoice(result.answer).includes('Studio/src/'));
  console.log('PASS',question,result.elapsedMs+' ms');
 }
})().catch(error=>{console.error(error);process.exitCode=1});
