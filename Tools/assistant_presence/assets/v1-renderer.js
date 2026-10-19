// Derived from approved V1 SizeFix; original archived separately.

(() => {
'use strict';
const canvas=document.getElementById('orbCanvas');
const root=document.getElementById('scene');
const ctx=canvas.getContext('2d',{alpha:true,desynchronized:true});
if(!ctx){document.getElementById('sceneTitle').textContent='Canvas unavailable';return;}
const TAU=Math.PI*2;
const states={
 idle:{label:'Intelligence, contained.',desc:'A quiet presence until Renegade has something to say.',mix:[0.16,0.65,1],expand:.23,activity:.12},
 listening:{label:'I’m listening.',desc:'A focused response to your voice, without interrupting the creative flow.',mix:[0.20,0.85,1],expand:.76,activity:.52},
 thinking:{label:'Connecting the details.',desc:'Subtle violet energy gathers as Renegade works through the problem.',mix:[0.57,0.45,1],expand:.84,activity:.64},
 speaking:{label:'The engine has a voice.',desc:'A luminous core waveform and flowing plasma respond as Renegade speaks.',mix:[0.25,0.69,1],expand:1.0,activity:1},
 success:{label:'Nicely done.',desc:'A brief, composed signal of confirmation. Then back to work.',mix:[0.17,0.9,.8],expand:.85,activity:.58},
 warning:{label:'Something needs attention.',desc:'A restrained amber signal. Visible, but never intrusive.',mix:[1,.50,.27],expand:.83,activity:.70}
};
let state='idle',expand=.23,brightness=.75,expandedDiameter=220,idleDiameter=84,reduced=false,showWave=true,voiceAmplitude=0.4,simAmplitude=0.42;
let w=1,h=1,dpr=1,center={x:.5,y:.50},isDragging=false,hasDragged=true,dragOffset={x:0,y:0},t=0,last=0,frameCount=0,fpsT=0,fpsSmooth=60,drawMs=0;
let audioEl=null,audioCtx=null,analyser=null,audioSrc=null,audioBuf=null,playedObjectUrl=null,dataArray=null;
let isAudioPlaying=false;let loadedAudioName='';
const particles=Array.from({length:64},(_,i)=>({a:(i*2.399963229728653)%TAU,r:.2+.83*fract(Math.sin(i*138.59)*43758.545),z:fract(Math.sin(i*63.127)*13758.545),size:.5+fract(i*0.713)*1.5,spd:.15+fract(i*.7543)*.45}));
const waveArr=new Float32Array(110);
function fract(x){return x-Math.floor(x)}
function clamp(x,a,b){return Math.max(a,Math.min(b,x))}
function lerp(a,b,k){return a+(b-a)*k}
function rgb(r,g,b,a=1){return `rgba(${clamp(r,0,255)|0},${clamp(g,0,255)|0},${clamp(b,0,255)|0},${clamp(a,0,1)})`}
function tint(r,g,b,a=1){
 if(state==='warning')return rgb(Math.max(150,r*.95),g*.40+36,b*.16+18,a);
 if(state==='success')return rgb(r*.48,Math.max(155,g),b*.77,a);
 if(state==='thinking')return rgb(Math.max(112,r*.93),g*.76,Math.max(195,b),a);
 if(state==='listening')return rgb(r*.7,Math.max(166,g),Math.max(215,b),a);
 return rgb(r,g,b,a);
}
function resize(){let rect=root.getBoundingClientRect();w=Math.max(1,rect.width);h=Math.max(1,rect.height);dpr=Math.min(window.devicePixelRatio||1,1.7);canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);ctx.setTransform(dpr,0,0,dpr,0,0)}
new ResizeObserver(resize).observe(root);resize();
function pathSmooth(points){ctx.moveTo(points[0][0],points[0][1]);for(let i=1;i<points.length-1;i++){const mx=(points[i][0]+points[i+1][0])/2,my=(points[i][1]+points[i+1][1])/2;ctx.quadraticCurveTo(points[i][0],points[i][1],mx,my)}ctx.lineTo(points[points.length-1][0],points[points.length-1][1])}
function line(points,col,width,glow=0,blur=0){if(points.length<2)return;ctx.beginPath();pathSmooth(points);ctx.strokeStyle=col;ctx.lineWidth=width;ctx.lineCap='round';ctx.lineJoin='round';ctx.stroke();ctx.shadowBlur=0}
function gradientLine(points,colors,width,glow){if(!points.length)return;const g=ctx.createLinearGradient(points[0][0],points[0][1],points[points.length-1][0],points[points.length-1][1]);colors.forEach((c,i)=>g.addColorStop(i/(colors.length-1),c));line(points,g,width,glow)}
function ellipse(cx,cy,rx,ry,rotation,col,width,blur=0,start=0,end=TAU){ctx.beginPath();ctx.ellipse(cx,cy,Math.abs(rx),Math.abs(ry),rotation,start,end);ctx.strokeStyle=col;ctx.lineWidth=width;ctx.stroke();ctx.shadowBlur=0}
function drawStar(x,y,s,a=1,typ=0){ctx.save();ctx.translate(x,y);ctx.globalAlpha=a;ctx.strokeStyle='#e5fcff';ctx.lineWidth=.7;ctx.shadowBlur=0;ctx.beginPath();ctx.moveTo(-s,0);ctx.lineTo(s,0);ctx.moveTo(0,-s);ctx.lineTo(0,s);ctx.stroke();ctx.fillStyle='#fff';ctx.beginPath();ctx.arc(0,0,typ?1.2:0.65,0,TAU);ctx.fill();ctx.restore()}
function orbRadius(){const eased=clamp((expand-states.idle.expand)/(states.speaking.expand-states.idle.expand),0,1);const maxD=Math.min(w*.75,h*.75,expandedDiameter);const activeRadius=maxD/2;const restRadius=Math.min(idleDiameter,w*.72,h*.72)/2;return lerp(restRadius,activeRadius,eased)}
function draw(t,dt){const t0=performance.now();ctx.setTransform(dpr,0,0,dpr,0,0);ctx.globalCompositeOperation='source-over';ctx.shadowBlur=0;ctx.clearRect(0,0,w,h);
 const x=center.x*w,y=center.y*h;
 const r=orbRadius();
 const perf=reduced?0.12:1,act=states[state].activity,bright=brightness;
 // Quiet at rest; during Daniel speech the glass sphere should visibly
 // breathe and expand to the rhythm of *real playback*, not merely change state.
 // The existing waveform amplitude is already derived from the audio analyser.
 const speechActive=state==='speaking';
 const pulse=1+Math.sin(t*(speechActive?3.35:1.65))*(speechActive?.038:.011)*perf+
   (speechActive?Math.min(.088,Math.max(0,voiceAmplitude)*.14):0)*perf;
 const R=r*pulse;
 // Atmosphere and ambient aura.
 const ambient=ctx.createRadialGradient(x,y,R*.15,x,y,R*1.76);
 ambient.addColorStop(0,rgb(28,82,169,.10*bright));ambient.addColorStop(.44,rgb(20,95,204,.075*bright));ambient.addColorStop(.67,rgb(38,74,154,.08*bright));ambient.addColorStop(1,'rgba(0,0,0,0)');ctx.fillStyle=ambient;ctx.fillRect(x-R*1.9,y-R*1.9,R*3.8,R*3.8);
 ctx.globalCompositeOperation='lighter';
 // Wide translucent primary spherical body.
 const body=ctx.createRadialGradient(x-R*.29,y-R*.31,R*.09,x+R*.13,y+R*.11,R*1.13);
 body.addColorStop(0,rgb(23,75,142,.11));body.addColorStop(.29,rgb(7,26,91,.28));body.addColorStop(.58,rgb(5,26,82,.43));body.addColorStop(.81,rgb(18,68,148,.31));body.addColorStop(.955,rgb(70,163,247,.37));body.addColorStop(1,'rgba(0,0,0,0)');ctx.beginPath();ctx.arc(x,y,R*1.02,0,TAU);ctx.fillStyle=body;ctx.fill();
 // Outer boundary — backlit transparent glass.
 ellipse(x,y,R*.990,R*.990,0,rgb(58,145,238,.30*bright),1.45,8);
 ellipse(x,y,R*.962,R*.962,0,rgb(88,174,255,.13*bright),.7,12);
 // Core shells and holographic rings.
 ellipse(x,y,R*.62,R*.62,0,rgb(82,147,250,.15),1,8);
 ellipse(x,y,R*.79,R*.79,0,rgb(92,173,255,.16),.75,12);
 ellipse(x,y,R*.36,R*.36,0,rgb(90,202,255,.21),.8,7);
 // Spherical lattices: 3D wire geometry generated in original local coordinates.
 ctx.save();ctx.beginPath();ctx.arc(x,y,R*.968,0,TAU);ctx.clip();
 for(let j=0;j<8;j++){
  const q=j/8;const points=[];
  for(let k=0;k<=70;k++){
   const a=k/70*TAU;const phi= Math.acos(2*q-1.02)+(reduced?0:Math.sin(t*.16+j*.7)*.075);
   const psi=a+(reduced?0:t*(j%2?-0.08:.06));
   const xx=R*(Math.sin(phi)*Math.cos(psi)),yy=R*(Math.cos(phi)*.72+Math.sin(phi)*Math.sin(psi)*.18);
   const wobble= 1+ .036*Math.sin(a*5+j*.7+t*.32);
   points.push([x+xx*wobble,y+yy*wobble]);
  }
  line(points,rgb(j%3?72:129,j%4?143:101,255,.075+(j%4)*.022),.55,4);
 }
 for(let j=0;j<14;j++){
   const points=[]; const seed=j*2.399;
   for(let k=0;k<44;k++){
      const a=k/43*TAU;const mod=0.49+0.40*Math.sin(a*1.6+seed*0.8+t*(j%2?-.26:.29));
      const theta=a+seed+t*(j%2?.035:-.043);
      points.push([x+Math.cos(theta)*mod*R, y+Math.sin(theta)*mod*R*(.63+.09*Math.sin(seed))]);
   }
   line(points,rgb(j%4===0?167:45,j%3===0?115:126,255,.05+(j%6)*.016),.6,3);
 }
 ctx.restore();
 // Filament ribbons on spherical paths, clipped rear half by painting inner body later.
 function ribbon(index,front){
  const angle=(index*2.399+t*(index%2?-.055:.07)*perf)%TAU;
  const incl=.25+fract(index*.618)*.85;
  const tilt=-.65+fract(index*.425)*1.45;
  const points=[];
  const color=index%7===0?'warm':(index%3===1?'violet':'cyan');
  for(let i=0;i<=68;i++){
    const a=i/68*TAU+angle;
    const v=a*2.1+index;
    const rad=(.67+.20*Math.sin(a*1.7+index*.2+t*.30*perf))*R;
    let px=Math.cos(a)*rad,py=Math.sin(a)*rad*incl;
    const xx=px*Math.cos(tilt)-py*Math.sin(tilt),yy=px*Math.sin(tilt)+py*Math.cos(tilt);
    const zz=Math.sin(a+index*1.7)*Math.cos(incl*3);
    if((front&&zz>-.18)||(!front&&zz<=-.18))points.push([x+xx,y+yy+Math.sin(v)*R*.018]);
    else if(points.length>=2){drawRibbonPiece(points,color,index,front);points.length=0;}else points.length=0;
  }
  if(points.length>=2)drawRibbonPiece(points,color,index,front);
 }
 function drawRibbonPiece(points,color,index,front){
  const brightScale=bright*(front?1:.48);
  let c0,c1,c2;
  if(color==='warm'){c0=tint(253,122,149,.11*brightScale);c1=tint(255,194,134,.86*brightScale);c2=tint(255,248,224,.61*brightScale)}
  else if(color==='violet'){c0=tint(94,83,255,.12*brightScale);c1=tint(161,100,255,.84*brightScale);c2=tint(225,165,255,.68*brightScale)}
  else {c0=tint(27,75,251,.13*brightScale);c1=tint(25,166,255,.76*brightScale);c2=tint(176,247,255,.67*brightScale)}
  line(points,c0,18,0);line(points,c1,3.0+(index%3)*.5,0);if(index%2===0)line(points,c2,1.0,0);
 }
 // Soft core eye — intentionally no mascot pupils, human features or mouth.
 let central=ctx.createRadialGradient(x,y,0,x,y,R*.45);
 central.addColorStop(0,rgb(61,139,241,.19*bright));central.addColorStop(.22,rgb(19,70,158,.15*bright));central.addColorStop(1,'rgba(0,0,0,0)');ctx.fillStyle=central;ctx.fillRect(x-R*.5,y-R*.5,R,R);
 for(let i=0;i<12;i++)ribbon(i,false);
 // Spherical veil and deep center, preserving fine glass edge.
 const front=ctx.createRadialGradient(x-R*.36,y-R*.32,R*.05,x+R*.17,y+R*.16,R*1.12);
 front.addColorStop(0,rgb(2,29,81,.015));front.addColorStop(.45,rgb(11,22,70,.018));front.addColorStop(.85,rgb(60,105,255,.07));front.addColorStop(1,'rgba(0,0,0,0)');ctx.fillStyle=front;ctx.beginPath();ctx.arc(x,y,R,0,TAU);ctx.fill();
 for(let i=0;i<12;i++)ribbon(i,true);
 // Multi-plane orbit loops cut behind/foreground by change in radiance.
 for(let i=0;i<6;i++){
    const a=t*(.035+(i%3)*.006)*perf+i*.77,rx=R*(1.0+.12*Math.sin(i*2.1)),ry=R*(.27+.08*Math.sin(i*.76));
    const rot=i*.48 +.12*Math.sin(t*.15*perf+i);
    const c=i%5===0?rgb(242,153,208,.37*bright):i%3===0?rgb(145,89,255,.48*bright):rgb(50,189,255,.34*bright);
    ellipse(x,y,rx,ry,rot,c,i%3===0?1.3:.65,i%3===0?11:6,a,a+TAU*0.64);
 }
 // Outside motion shells: precise broken flow arcs, sharper than the soft plasma.
 for(let i=0;i<16;i++){
   const a=i*2.399+t*(i%2?-.12:.085)*perf;
   const rad=R*(.93+.055*Math.sin(i*3.03+t*.38));
   const start=a+(i%4)*.1,len=(.035+.02*(i%3))*TAU;
   ellipse(x,y,rad,rad*.82,.45*Math.sin(i*2.1),rgb(i%6===0?218:65,i%5===0?133:180,255,(.12+.19*fract(i*.45))*bright),i%5===0?1.3:.55,5,start,start+len);
 }
 // Surface-skimming luminous plasma ribbons. Broad translucent under-strokes
 // create a volumetric sense of energy without post-processing bloom.
 for(let j=0;j<6;j++){
   const ribbonPts=[];
   const start=j*.93-t*(j%2?.15:-.11)*perf;
   const span=TAU*(.35+.085*(j%3));
   for(let k=0;k<=50;k++){
     const v=k/50, a=start+span*v;
     const rr=R*(.85+.08*Math.sin(a*2.4+t*.25*perf+j)+.04*Math.sin(v*TAU*2+j));
     const yy=Math.sin(a)*rr*(.72+.22*Math.sin(j*2.8));
     const xx=Math.cos(a)*rr;
     const tilted=j*.38-.41;
     ribbonPts.push([x+xx*Math.cos(tilted)-yy*Math.sin(tilted),y+xx*Math.sin(tilted)+yy*Math.cos(tilted)]);
   }
   const col=j%5===0?tint(246,159,168,.55*bright):j%3===0?tint(174,113,255,.58*bright):tint(38,165,255,.52*bright);
   const white=j%5===0?tint(255,236,214,.74*bright):j%3===0?tint(221,188,255,.74*bright):tint(185,247,255,.85*bright);
   line(ribbonPts,col,16);line(ribbonPts,white,2.6);
 }
 // Micro-particles orbit outside and within the shell.
 particles.forEach((p,i)=>{
   const a=p.a+t*p.spd*(i%2?-1:1)*perf;
   const rr=R*(.15+p.r*1.15);
   const warp=Math.sin(a*3+t*.13+i)*.035*R;
   const px=x+Math.cos(a)*(rr+warp);const py=y+Math.sin(a)*rr*(.72+.19*p.z);
   const alpha=clamp(.20+.35*Math.sin(i+t*.8*perf)+.25*p.z,.06,.70)*bright;
   ctx.fillStyle=i%6===0?rgb(222,145,255,alpha):i%7===0?rgb(255,197,157,alpha):rgb(127,229,255,alpha);
   ctx.beginPath();ctx.arc(px,py,p.size*(.7+.2*act),0,TAU);ctx.fill();
   if(i%17===0)drawStar(px,py,2+(i%4),alpha*.8,1);
 });
 // Thin glass fresnel highlights.
 ellipse(x,y,R*1.009,R*.991,-.25,rgb(113,212,255,.20*bright),.9,11,Math.PI*.87,Math.PI*1.7);
 ellipse(x,y,R*.987,R*.987,0,rgb(202,171,255,.21*bright),.7,8,.04,1.4);
 // Holographic nucleus: voice waveform is embedded in the sphere, not pasted UI outside it.
 if(showWave && (state==='speaking'||state==='listening'||state==='idle')){
   // Keep a *quiet* breathing line when idle. As soon as speech begins, make
   // the waveform visible even while the V1 sphere is still expanding.
   // Previously the expansion opacity gate hid short Daniel sentences.
   const opacity=state==='idle'?.24:
     state==='speaking'?.72+.28*clamp((expand-.23)/.52,0,1):.46;
   const width=R*.75;
   const baseline=R*.014;
   ctx.save();ctx.globalAlpha=opacity;
   ellipse(x,y,R*.64,R*.32,0,rgb(67,183,255,.20),.8,7);
   line([[x-width,y],[x+width,y]],rgb(52,130,246,.35),.72,5);
   // Soft luminous audio envelope, always centered on the spherical core.
   const top=[],bottom=[],linePoints=[];
   // Daniel's normal speech RMS is modest. The old renderer multiplied
   // RMS * instantaneous PCM amplitude, attenuating the visual signal twice.
   // Visual-only compression restores readable motion without boosting sound.
   const amp=state==='idle'?.18:
     state==='speaking'?clamp(voiceAmplitude*1.38+.08,.1,.98):
     clamp(voiceAmplitude*1.25,.15,.7);
   for(let i=0;i<waveArr.length;i++){
     const u=i/(waveArr.length-1),q=u*2-1;
     const fall=Math.pow(Math.max(0,1-Math.abs(q)),.67);
     const fractal=(Math.sin(i*1.61+t*5.7*perf)+Math.sin(i*.76-t*3.1*perf)*.52+Math.sin(i*.23+t*7.8*perf)*.43)/1.95;
     const a=state==='speaking'
       ? Math.min(.93,Math.sqrt(Math.abs(waveArr[i])*2.35))
       : Math.abs(fractal)*(state==='idle'?.35:.19);
     const h=baseline+(R*.60*amp*a*fall);
     top.push([x+q*width,y-h]);bottom.push([x+q*width,y+h]);linePoints.push([x+q*width,y-Math.sin(i*.58+t*1.9*perf)*h*.38]);
   }
   // Voice bars deliberately narrow and organic to evoke audio, not a dashboard equalizer.
   ctx.save();ctx.shadowBlur=0;
   for(let i=2;i<top.length-2;i+=2){
     const u=i/(top.length-1),col=u<.55?'#50dcff':u<.78?'#8dbaff':'#e9a0ff';
     ctx.strokeStyle=col;ctx.globalAlpha=.75+.23*amp;ctx.lineWidth=R>150?1.9:1.3;ctx.beginPath();ctx.moveTo(top[i][0],top[i][1]);ctx.lineTo(bottom[i][0],bottom[i][1]);ctx.stroke();
   }
   ctx.restore();
   gradientLine(linePoints,[rgb(38,144,255,.48),rgb(130,255,255,.89),rgb(196,117,255,.82)],1.4,12);
   ctx.restore();
 }
 // A few bright precise highlights / orbit nodes.
 for(let i=0;i<9;i++){
   const a=(i/9)*TAU+t*.22*perf;const rr=R*(.94+.09*Math.sin(i*1.5));
   drawStar(x+Math.cos(a)*rr,y+Math.sin(a)*rr*.84,2.1+fract(i*.381)*4,(.23+.42*fract(i*.7))*bright,i%2);
 }
 ctx.globalCompositeOperation='source-over';
 // Size hint for prototype: unobtrusive orb when idle, bigger while activated.
 drawMs=lerp(drawMs,performance.now()-t0,.06);
}
function setState(s){if(!states[s])return;state=s;document.querySelectorAll('[data-state]').forEach(b=>b.classList.toggle('selected',b.dataset.state===s));document.getElementById('sceneTitle').textContent=states[s].label;document.getElementById('sceneSub').textContent=states[s].desc;if(s!=='speaking'&&isAudioPlaying)stopAudio()}
function updateSimWave(t,dt){ // Smoothed pseudo-phoneme envelope for DEMO MOTION ONLY, not synthetic audio output.
 const vowels=Math.pow(Math.abs(Math.sin(t*2.65)),1.15),syll=Math.pow(Math.abs(Math.sin(t*6.6+.6)),1.35);
 simAmplitude=.06+vowels*.44+syll*.18+Math.pow(Math.max(0,Math.sin(t*.83)),8)*.13;
 let real=false;
 const speechSamples=window.renegadeSpeechQueue?.getSamples();
 const liveAnalyser=speechSamples?.analyser||(isAudioPlaying?analyser:null),liveArray=speechSamples?.samples||(isAudioPlaying?dataArray:null);
 if(liveAnalyser&&liveArray){liveAnalyser.getByteTimeDomainData(liveArray);let sum=0;for(let i=0;i<liveArray.length;i++){let v=(liveArray[i]-128)/128;sum+=v*v}const rms=Math.sqrt(sum/liveArray.length);voiceAmplitude=lerp(voiceAmplitude,clamp(rms*3.6,0,1),.36);real=true;}
 if(!real){voiceAmplitude=lerp(voiceAmplitude,state==='speaking'?simAmplitude:(state==='listening'?.12:0.03),.14)}
 for(let i=0;i<waveArr.length;i++){
  const phase=i/waveArr.length;
  let target;
  if(real){let ix=Math.floor(phase*(liveArray.length-1));target=(liveArray[ix]-128)/128;}
  else target=clamp(Math.sin(i*.61+t*10)*.30+Math.sin(i*.22-t*15)*.3+Math.sin(i*1.73+t*23)*.17+Math.sin(i*.103+t*6)*.18,-1,1);
  waveArr[i]=lerp(waveArr[i],target,real?.44:.22);
 }
}
function frame(now){if(state==='idle' && last && now-last<31){requestAnimationFrame(frame);return;}if(!last)last=now;const dt=Math.min((now-last)/1000,.09);last=now;t+=reduced?dt*.035:dt;expand=lerp(expand,states[state].expand,.075);if(!hasDragged){const desired=state==='idle'?{x:.79,y:.77}:{x:.50,y:.50};center.x=lerp(center.x,desired.x,.055);center.y=lerp(center.y,desired.y,.055);}updateSimWave(t,dt);draw(t,dt);frameCount++;fpsT+=dt;if(fpsT>=.65){const f=frameCount/fpsT;fpsSmooth=lerp(fpsSmooth,f,.35);document.getElementById('fps').textContent=Math.round(fpsSmooth)+' FPS';document.getElementById('drawTime').textContent=drawMs.toFixed(1)+' MS';window.orbHost?.reportPerformance(Math.round(fpsSmooth),Number(drawMs.toFixed(2)));frameCount=0;fpsT=0}requestAnimationFrame(frame)}
requestAnimationFrame(frame);
document.querySelectorAll('[data-state]').forEach(b=>b.addEventListener('click',()=>setState(b.dataset.state)));
for(const id of ['size','energy']){const el=document.getElementById(id);el.addEventListener('input',()=>{if(id==='size'){expandedDiameter=+el.value;document.getElementById('sizeValue').textContent=expandedDiameter+' px';}else{brightness=+el.value/100;document.getElementById('energyValue').textContent=el.value+'%';}})}
function toggleButton(id,callback){const el=document.getElementById(id);el.addEventListener('click',()=>{let on=el.getAttribute('aria-pressed')!=='true';el.setAttribute('aria-pressed',String(on));el.classList.toggle('on',on);callback(on)})}
toggleButton('motion',val=>{reduced=val});toggleButton('waveSwitch',val=>{showWave=val});
// This is the approved V1 renderer; the desktop host owns native window movement.
canvas.addEventListener('pointerdown',e=>{
 if(e.button!==0)return;
 const rx=e.offsetX-center.x*w, ry=e.offsetY-center.y*h;
 if(Math.hypot(rx,ry)>orbRadius()*1.25)return;
 isDragging=true;
 try{canvas.setPointerCapture(e.pointerId)}catch(_){}
 window.orbHost?.startDrag();
});
canvas.addEventListener('pointerup',e=>{
 if(isDragging)window.orbHost?.endDrag();
 isDragging=false;
 try{if(canvas.hasPointerCapture(e.pointerId))canvas.releasePointerCapture(e.pointerId)}catch(_){}
});
canvas.addEventListener('pointercancel',()=>{isDragging=false;window.orbHost?.endDrag()});
// Click/drag distinction is owned by the host; clicks open the chat bubble.
canvas.addEventListener('contextmenu',e=>{e.preventDefault();window.orbHost?.showMenu()});
// Audio is strictly user-initiated. No microphone or unsolicited synthesis.
const fileInput=document.getElementById('audioFile'),playButton=document.getElementById('playAudio'),stopButton=document.getElementById('stopAudio'),audioMeta=document.getElementById('audioMeta');
fileInput.addEventListener('change',async e=>{let f=e.target.files&&e.target.files[0];if(!f)return;stopAudio();if(playedObjectUrl)URL.revokeObjectURL(playedObjectUrl);playedObjectUrl=URL.createObjectURL(f);if(audioSrc){audioSrc.disconnect();audioSrc=null}if(analyser){analyser.disconnect();analyser=null;dataArray=null}audioEl=new Audio(playedObjectUrl);audioEl.preload='auto';audioEl.addEventListener('ended',stopAudio);loadedAudioName=f.name;audioMeta.textContent='LOADED: '+f.name;playButton.disabled=false;stopButton.disabled=true});
async function initAudio(){if(!audioEl)return false;const A=window.AudioContext||window.webkitAudioContext;if(!A){audioMeta.textContent='WEB AUDIO UNAVAILABLE';return false}try{if(!audioCtx)audioCtx=new A();if(!analyser){analyser=audioCtx.createAnalyser();analyser.fftSize=2048;dataArray=new Uint8Array(analyser.frequencyBinCount);audioSrc=audioCtx.createMediaElementSource(audioEl);audioSrc.connect(analyser);analyser.connect(audioCtx.destination)}await audioCtx.resume();return true}catch(err){audioMeta.textContent='AUDIO ERROR: '+(err.message||String(err));return false}}
playButton.addEventListener('click',async()=>{if(!await initAudio())return;try{await audioEl.play();isAudioPlaying=true;setState('speaking');audioMeta.textContent='REAL AUDIO SIGNAL · '+(loadedAudioName||'LOCAL FILE');playButton.disabled=true;stopButton.disabled=false;window.orbHost?.audioStarted();}catch(e){audioMeta.textContent='PLAYBACK FAILED: '+(e.message||String(e));window.orbHost?.audioFailed(e.message||String(e))}});
function stopAudio(){if(audioEl){audioEl.pause();audioEl.currentTime=0}isAudioPlaying=false;playButton.disabled=!audioEl;stopButton.disabled=true;audioMeta.textContent=audioEl?'AUDIO STOPPED · READY TO PLAY':'SIMULATED VOICE MOTION · NO AUDIO PLAYING'}
stopButton.addEventListener('click',stopAudio);
window.addEventListener('pagehide',()=>{if(playedObjectUrl)URL.revokeObjectURL(playedObjectUrl);if(audioCtx)audioCtx.close()});

// Desktop host hooks — visual drawing remains the approved V1 implementation.
function loadAudioFromDesktop(dataUrl, filename) {
 stopAudio();
 if (playedObjectUrl) { URL.revokeObjectURL(playedObjectUrl); playedObjectUrl=null; }
 if (audioSrc) { audioSrc.disconnect(); audioSrc=null; }
 if (analyser) { analyser.disconnect(); analyser=null; dataArray=null; }
 loadedAudioName=String(filename||'LOCAL FILE');
 audioEl=new Audio(dataUrl);
 audioEl.preload='auto';
 audioEl.addEventListener('ended',()=>{stopAudio();window.orbHost?.audioEnded();});
 audioMeta.textContent='LOADED: '+loadedAudioName;
 playButton.disabled=false;stopButton.disabled=true;
 return true;
}
window.renegadeCore={
 setState(s){setState(s);},
 setExpandedSize(px){expandedDiameter=clamp(Number(px)||220,120,320);},
 setBrightness(value){brightness=clamp(Number(value)||75,20,100)/100;},
 setReducedMotion(v){reduced=Boolean(v);},
 loadAudio:loadAudioFromDesktop,
 playAudio(){playButton.click();},
 stopAudio(){stopAudio();},
 getState(){return state;}
};
window.orbHost?.onCommand((message)=>{
 if(!message||typeof message!=='object')return;
 switch(message.type){
 case 'state': window.renegadeCore.setState(message.value);break;
 case 'size': window.renegadeCore.setExpandedSize(message.value);break;
 case 'brightness': window.renegadeCore.setBrightness(message.value);break;
 case 'motion': window.renegadeCore.setReducedMotion(message.value);break;
 case 'audio-load':window.renegadeCore.loadAudio(message.dataUrl,message.name);break;
 case 'audio-play':window.renegadeCore.playAudio();break;
 case 'audio-stop':window.renegadeCore.stopAudio();break;
 case 'speech-begin':window.renegadeSpeechQueue?.begin(message.requestId,message.total);break;
 case 'speech-chunk':window.renegadeSpeechQueue?.chunk(message.requestId,message.index,message.dataUrl);break;
 case 'speech-finish':window.renegadeSpeechQueue?.finish(message.requestId);break;
 case 'speech-stop':window.renegadeSpeechQueue?.stop();break;
 }
});
window.orbHost?.rendererReady();
})();
