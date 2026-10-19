'use strict';
// Renderer-only deterministic test: no GPU, microphone, TTS model, audio playback,
// or desktop focus required. Measures the actual central V1 waveform bar geometry.
const vm=require('node:vm');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
let now=0, next=null, amp=.12, state='idle';
const bars=[];
const context={
 strokeStyle:'#fff',lineWidth:1,
 setTransform(){},clearRect(){},beginPath(){},arc(){},fill(){},
 stroke(){if(['#50dcff','#8dbaff','#e9a0ff'].includes(this.strokeStyle))
  bars.push(this.lastBar||0);},
 ellipse(){},clip(){},moveTo(x,y){this.startY=y;this.lastBar=0;},
 lineTo(x,y){this.lastBar=Math.abs(y-this.startY);},quadraticCurveTo(){},
 fillRect(){},save(){},restore(){},translate(){},
 createRadialGradient(){return{addColorStop(){}}},
 createLinearGradient(){return{addColorStop(){}}},
};
const ids=new Map();
const element=id=>{
 if(!ids.has(id))ids.set(id,{textContent:'',value:220,
  classList:{toggle(){}},getAttribute(){return 'true'},
  setAttribute(){},addEventListener(){},click(){},disabled:false});
 return ids.get(id);
};
const root={getBoundingClientRect(){return{width:370,height:370}}};
const canvas={width:370,height:370,
 getContext(){return context},addEventListener(){}};
const samples=new Uint8Array(1024);
const fake={
 document:{
  getElementById(id){return id==='orbCanvas'?canvas:id==='scene'?root:element(id)},
  querySelectorAll(){return []}
 },
 window:{
  addEventListener(){},
  devicePixelRatio:1,
  renegadeSpeechQueue:{getSamples(){
    if(state!=='speaking')return null;
    return{analyser:{getByteTimeDomainData(arr){
      for(let i=0;i<arr.length;i++){
        const harmonic=Math.sin(i*.19+now*.007)+Math.sin(i*.061+now*.009)*.28;
        arr[i]=Math.max(0,Math.min(255,Math.round(128+harmonic*amp*128)));
      }
    }},samples};
  }},
  orbHost:{onCommand(){},rendererReady(){},reportPerformance(){}}
 },
 ResizeObserver:class{observe(){}},
 requestAnimationFrame(cb){next=cb;},
 performance:{now(){return now}},
 Audio:class{},
};
vm.createContext(fake);
vm.runInContext(fs.readFileSync(path.join(__dirname,'../assets/v1-renderer.js'),'utf8'),fake,{timeout:3000});
assert.equal(typeof fake.window.renegadeCore.setState,'function');
function measure(newState,newAmp,frames){
 state=newState;amp=newAmp;fake.window.renegadeCore.setState(newState);
 let values=[];
 for(let i=0;i<frames;i++){
  bars.length=0;now+=16.66;
  const draw=next;next=null;assert(draw);
  draw(now);
  if(bars.length)values.push(bars.reduce((sum,n)=>sum+n,0)/bars.length);
 }
 return {frames:values.length,avg:values.reduce((x,y)=>x+y,0)/Math.max(1,values.length),
  last:values.at(-1)||0};
}
const idle=measure('idle',0,30);
const shortQuiet=measure('speaking',.065,18);
const louder=measure('speaking',.22,38);
const returnIdle=measure('idle',0,45);
console.log({idle,shortQuiet,louder,returnIdle});
assert(idle.frames>0,'idle core should breathe subtly');
assert(shortQuiet.frames>0,'waveform should be visible immediately when Daniel starts');
assert(shortQuiet.avg>idle.avg*1.8,
  'even quiet Daniel speech should animate significantly more than idle');
assert(louder.last>shortQuiet.last*1.15,
  'waveform must visibly respond to louder PCM');
assert(returnIdle.frames>0,'quiet idle waveform should return after speech');
console.log('PASS: approved V1 orb waveform visibly reacts to quiet and louder speech and breathes subtly in idle');
