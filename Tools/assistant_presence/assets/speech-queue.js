'use strict';
// Audio-only transport for owner-directed Renegade speech. Does not alter V1
// painting, window sizes, or visual design. Schedules individual generated
// WAV chunks on one AudioContext clock to avoid load/play gaps.
(()=>{
 let audioCtx=null, analyser=null, samples=null, active=null;
 let queue=Promise.resolve();
 const diagnostics={starts:0,chunks:0,gapsMs:[],decodeMs:[],lastError:''};
 async function ensureAudio(){
  if(!audioCtx){
   audioCtx=new (window.AudioContext||window.webkitAudioContext)();
   analyser=audioCtx.createAnalyser();analyser.fftSize=2048;
   samples=new Uint8Array(analyser.frequencyBinCount);
   analyser.connect(audioCtx.destination);
  }
  if(audioCtx.state==='suspended')await audioCtx.resume();
  return audioCtx;
 }
 function begin(requestId,total){
  stop();
  active={id:requestId,total:Math.max(1,Number(total)||1),sources:new Set(),pending:0,ended:false,
          finished:false,firstPlayed:false,lastEnd:0,scheduled:0,playing:0,parts:0};
  queue=Promise.resolve();
  diagnostics.starts++;diagnostics.chunks=0;diagnostics.gapsMs=[];diagnostics.decodeMs=[];diagnostics.lastError='';
 }
 function getStream(){return active}
 function maybeFinish(stream){
  if(active!==stream||!stream.finished||stream.pending!==0||stream.sources.size!==0)return;
  active=null;
  window.orbHost?.audioEnded(stream.id);
 }
 function readWav(dataUrl){
  if(typeof dataUrl!=='string'||!dataUrl.startsWith('data:audio/wav;base64,'))throw new Error('Invalid WAV data');
  const binary=atob(dataUrl.slice('data:audio/wav;base64,'.length));
  if(binary.length>24*1024*1024)throw new Error('Audio chunk too large');
  const result=new ArrayBuffer(binary.length),bytes=new Uint8Array(result);
  for(let i=0;i<binary.length;i++)bytes[i]=binary.charCodeAt(i);
  return result;
 }
 function chunk(requestId,index,dataUrl){
  const stream=active;
  if(!stream||stream.id!==requestId)return;
  stream.pending++;
  queue=queue.then(async()=>{
    if(active!==stream)return;
    const ctx=await ensureAudio();
    if(active!==stream)return;
    const start=performance.now();
    const pcm=await ctx.decodeAudioData(readWav(dataUrl));
    diagnostics.decodeMs.push(Math.round(performance.now()-start));
    if(active!==stream)return;
    const node=ctx.createBufferSource();node.buffer=pcm;node.connect(analyser);
    const now=ctx.currentTime;
    const at=Math.max(now+.02,stream.scheduled||0);
    if(stream.firstPlayed){
      const gap=Math.max(0,at-stream.lastEnd);
      if(gap>.02)diagnostics.gapsMs.push(Math.round(gap*1000));
    }
    stream.scheduled=at+pcm.duration;
    stream.lastEnd=stream.scheduled;
    stream.sources.add(node);
    stream.playing++;
    stream.parts++;
    diagnostics.chunks++;
    node.onended=()=>{
      stream.sources.delete(node);stream.playing--;
      if(active!==stream)return;
      maybeFinish(stream);
    };
    if(!stream.firstPlayed){
      stream.firstPlayed=true;
      window.orbHost?.audioStarted(stream.id);
    }
    node.start(at);
  }).catch(err=>{
    diagnostics.lastError=String(err?.message||err);
    if(active===stream){
      stop();
      window.orbHost?.audioFailed(diagnostics.lastError,stream.id);
    }
  }).finally(()=>{stream.pending--;maybeFinish(stream)});
 }
 function finish(requestId){
  if(!active||active.id!==requestId)return;
  active.finished=true;maybeFinish(active);
 }
 function stop(){
  const previous=active;active=null;
  if(!previous)return;
  for(const node of previous.sources){node.onended=null;try{node.stop()}catch{};try{node.disconnect()}catch{}}
  previous.sources.clear();
 }
 function getSamples(){
  if(!active||!active.firstPlayed||!analyser||!samples)return null;
  return {analyser,samples};
 }
 function getDiagnostics(){
  return {...diagnostics,active:active?{id:active.id,finished:active.finished,parts:active.parts,pending:active.pending,queued:active.sources.size}:null};
 }
 window.renegadeSpeechQueue=Object.freeze({begin,chunk,finish,stop,getSamples,getDiagnostics});
})();
