'use strict';
const assert=require('node:assert/strict');
const {WarmDaniel}=require('../voice/warm-daniel');
const {cleanForVoice}=require('../chat/live-speech');
(async()=>{
 const broken='Renegade can offer details. '+String.fromCharCode(0xDC9D)+' The editor remains yours.';
 const safe=cleanForVoice(broken);
 assert(!/[\uD800-\uDFFF]/.test(safe),'no malformed surrogate in speech');
 assert(safe.includes('editor'));
 const mojibake='Now \u00e2\u20ac\u201d review it.';
 assert(cleanForVoice(mojibake).includes('—'),'repair common em dash mojibake');
 const worker=new WarmDaniel();
 try{
  const start=performance.now();const result=await worker.generate(broken);
  console.log('SANITIZED_REAL_DANIEL_WAV',result.data.length,'ROUNDTRIP_MS',Math.round(performance.now()-start));
  assert.equal(result.data.toString('ascii',0,4),'RIFF');
  assert(result.data.length>5000);
  console.log('PASS: malformed streamed Unicode sanitized before the Python neural synthesizer');
 }finally{worker.stop()}
})().catch(e=>{console.error(e);process.exit(1)});
