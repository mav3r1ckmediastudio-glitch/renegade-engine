'use strict';
const $=id=>document.getElementById(id);
const speech=$('speech'), voice=$('voice'), speak=$('speak'), stop=$('stop'), mute=$('mute'), status=$('status');
function setStatus(info){
 const phase=info.phase||'ready';
 const busy=phase==='generating';
 speak.disabled=busy||Boolean(info.muted);
 status.textContent=info.status;
 if(typeof info.muted==='boolean')mute.checked=info.muted;
 if(typeof info.voice==='string'&&[...voice.options].some(v=>v.value===info.voice))voice.value=info.voice;
}
window.voiceHost.onStatus(setStatus);
(async()=>{
 try{const v=await window.voiceHost.getSettings();setStatus(v)}
 catch(e){status.textContent='Voice connection error: '+e.message;speak.disabled=true}
})();
speak.addEventListener('click',async()=>{
 speak.disabled=true;
 status.textContent='Preparing speech locally...';
 try{const response=await window.voiceHost.speak(speech.value,voice.value);if(response?.error)status.textContent=response.error;}
 catch(e){status.textContent='Could not speak: '+String(e.message||e);speak.disabled=false}
});
stop.addEventListener('click',async()=>{try{await window.voiceHost.stop()}catch(e){status.textContent=String(e.message||e)}});
mute.addEventListener('change',async()=>{try{await window.voiceHost.setMuted(mute.checked)}catch(e){status.textContent=String(e.message||e)}});
document.querySelectorAll('[data-demo]').forEach(button=>button.addEventListener('click',()=>{
 const demo=window.renegadeDemoScripts?.[button.dataset.demo];
 if(!demo)return;
 speech.value=demo;
 status.textContent='Example loaded ('+demo.length+' characters). Press Speak when ready; this is not a live engine diagnosis.';
 speech.focus();
}));
speech.addEventListener('keydown',e=>{if((e.ctrlKey||e.metaKey)&&e.key==='Enter'){e.preventDefault();speak.click()}});
