'use strict';
const assert=require('node:assert/strict');
const {normaliseForDaniel,WarmDaniel}=require('../voice/warm-daniel');
const {cleanForVoice}=require('../chat/live-speech');
assert.equal(
 normaliseForDaniel('Choose NEW, then SAVE AS NEW. EDIT COPY and APPLY TO THIS PLAYER are separate controls.'),
 'Choose new, then SAVE AS new. EDIT COPY and APPLY TO THIS PLAYER are separate controls.'
);
assert.equal(normaliseForDaniel('FPS, UI, NPC and PSP'), 'FPS, UI, NPC and PSP');
assert.equal(normaliseForDaniel('RENEW, NEWEST, NEWS and a new arrow'), 'RENEW, NEWEST, NEWS and a new arrow');
assert.equal(normaliseForDaniel('Click NEW to create an arrow.'), 'Click new to create an arrow.');
// Both voice routes converge on WarmDaniel.generate. Ensure any prior formatting
// cannot reintroduce uppercase UI labels before synthesis.
assert.equal(normaliseForDaniel(cleanForVoice('Click **NEW** then SAVE AS NEW.')),
             'Click new then SAVE AS new.');
(async()=>{
 const worker=new WarmDaniel();
 try {
  const result=await worker.generate('Click NEW to create an arrow. Then choose SAVE AS NEW.');
  assert.equal(result.data.toString('ascii',0,4),'RIFF');
  assert(result.data.length>5000);
  console.log('PASS: only NEW is normalised for Daniel, all other labels preserved, WAV generated',
    {bytes:result.data.length,synthMs:result.elapsedMs});
 }finally{worker.stop()}
})().catch(err=>{console.error(err);process.exitCode=1});
