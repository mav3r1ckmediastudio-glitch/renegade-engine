'use strict';
// Live read-only guidance. Does not manipulate Studio, chat or model processes.
const assert=require('node:assert/strict');
const {answerVerified}=require('../chat/verified-knowledge');
(async()=>{
 for(const [question,expected] of [
  ['How do I add Player Start?','**Content/Player**'],
  ['How do I save a player prefab?','**SAVE AS PLAYER PREFAB**'],
  ['How do I reset the player prefab?','**RESET TO PREFAB**'],
  ['How do I create a new projectile?','6. Click **SAVE AS NEW**'],
  ['How do I assign a projectile?','6. Click **APPLY TO THIS PLAYER**']
 ]){
  const result=await answerVerified([{role:'user',content:question}]);
  assert.equal(result.policy,'source-verified');
  assert(result.answer.includes(expected));
  assert(result.answer.indexOf('**Before you start:**')<result.answer.indexOf('\n1. '));
  assert(result.sources.length>0);
  assert(!result.answer.includes('[Source:'));
  console.log('PASS',question,result.elapsedMs+' ms');
 }
})().catch(error=>{console.error(error);process.exitCode=1});
