'use strict';
// Explicit live integration check: requires the read-only knowledge bridge.
// Never starts/restarts Electron, model or voice services; never edits scenes.
const assert=require('node:assert/strict');
const {answerVerified,isEngineQuestion}=require('../chat/verified-knowledge');
const {cleanForVoice}=require('../chat/live-speech');
(async()=>{
 const times=[];
 for(const q of [
  'How do I assign a saved projectile to my weapon?',
  'How do I add an arrow to my bow?',
  'How do I assign a bullet?',
  'How do I equip a bolt?'
 ]){
  assert(isEngineQuestion(q));
  const result=await answerVerified([{role:'user',content:q}]);
  assert.equal(result.policy,'source-verified');
  assert.match(result.answer,/6\. Click \*\*APPLY TO THIS PLAYER\*\*/);
  assert.match(result.answer,/\*\*Troubleshooting:\*\*/);
  assert.match(result.answer,/Ctrl\+S/);
  assert(result.sources.includes('Studio/src/PlayerProjectileEditor.cpp'));
  assert(!result.answer.includes('[Source:'));
  const spoken=cleanForVoice(result.answer);
  assert(!spoken.includes('Studio/src/'));
  assert(!spoken.includes('MANUAL_E2E_STATUS'));
  times.push({q,ms:result.elapsedMs});
 }
 const original=await answerVerified([{role:'user',content:'How do I create a new projectile?'}]);
 assert.equal(original.policy,'source-verified');
 assert.match(original.answer,/6\. Click \*\*SAVE AS NEW\*\*/);
 console.log('PASS: live ordered assignment, creation preserved, View source separation and speech sanitation',times);
})().catch(error=>{console.error(error);process.exitCode=1;});
