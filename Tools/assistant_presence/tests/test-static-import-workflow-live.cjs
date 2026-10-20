'use strict';
const assert=require('node:assert/strict');
const {answerVerified,isEngineQuestion}=require('../chat/verified-knowledge');
const {cleanForVoice}=require('../chat/live-speech');
(async()=>{
 assert(!isEngineQuestion('Did you enjoy that movie in 3D?'));
 for(const question of [
  'How do I import a static model?',
  'How do I import an FBX model?',
  'How do I load a GLB?',
  'How do I bring in a mesh?',
  'How do I load a 3D object?'
 ]){
  assert(isEngineQuestion(question));
  const result=await answerVerified([{role:'user',content:question}]);
  assert.equal(result.policy,'source-verified');
  assert(result.answer.includes('7. Drag that imported card'));
  assert(result.answer.includes('COMMITTED; REVEAL FAILED'));
  assert(result.answer.indexOf('**Before you start:**')<result.answer.indexOf('\n1. '));
  assert(result.sources.includes('Studio/src/StudioApplication.cpp'));
  assert(!result.answer.includes('[Source:'));
  assert(!cleanForVoice(result.answer).includes('Studio/src/'));
  console.log('PASS',question,result.elapsedMs+' ms');
 }
})().catch(error=>{console.error(error);process.exitCode=1});
