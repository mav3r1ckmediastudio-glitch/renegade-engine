'use strict';
const assert=require('node:assert/strict');
const {LiveSpeech}=require('../chat/live-speech');
(async()=>{
 const events=[];
 let enabled=true;
 const s=new LiveSpeech({requestId:22,signal:new AbortController().signal,
  valid:()=>enabled,
  send:ev=>events.push(ev),
  generate:async text=>({data:Buffer.from('RIFF-real-wav-'+text)})});
 s.deliver('One short sentence. Next, we check the selected object; ');
 s.deliver('it should respond at runtime. A final sentence follows.');
 await s.finish();
 assert.equal(events[0].type,'speech-begin');
 assert(events.filter(e=>e.type==='speech-chunk').length>=2,'multiple streaming parts');
 assert.equal(events.at(-1).type,'speech-finish');
 assert(events.every(e=>e.requestId===22));
 assert.equal(events.filter(e=>e.type==='speech-begin').length,1);
 const stopped=[];
 const slow=new LiveSpeech({requestId:33,signal:new AbortController().signal,
  valid:()=>enabled,send:e=>stopped.push(e),generate:()=>new Promise(r=>setTimeout(()=>r({data:Buffer.from('RIFF')}),110))});
 slow.deliver('Some first sentence. ');
 slow.cancel();
 await slow.finish();
 assert(!stopped.some(e=>e.type==='speech-chunk'||e.type==='speech-finish'));
 console.log('PASS: incremental clauses, first WAV, continuous stream lifecycle, cancelled audio never restarts',events.map(e=>e.type));
})().catch(e=>{console.error('FAILED',e);process.exit(1)});
