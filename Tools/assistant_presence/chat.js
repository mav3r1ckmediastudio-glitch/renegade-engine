'use strict';
const $=id=>document.getElementById(id);
const historyEl=$('history'),prompt=$('prompt'),status=$('status'),send=$('send'),
 cancel=$('cancel'),auto=$('autoSpeak'),mute=$('mute');
let current={history:[],busy:false,autoSpeak:false,muted:false,status:'Ready'};
let lastHistory='',pending=null;
function makeMessage(entry){
 const item=document.createElement('article');item.className='message '+entry.role;
 const head=document.createElement('div');head.className='role';
 const sources=Array.isArray(entry.sources)?entry.sources.filter(s=>typeof s==='string'&&s.trim()):[];
 const leftover=[];
 const answerText=String(entry.content||'').replace(/\s*\[Source:\s*([^\]\r\n]+)\]/g,(_all,path)=>{
   if(path.trim()&&!leftover.includes(path.trim()))leftover.push(path.trim());return '';
 }).trim();
 for(const s of leftover)if(!sources.includes(s))sources.push(s);
 const name=document.createElement('span');name.textContent=entry.role==='user'?'YOU':(sources.length?'RENEGADE / VERIFIED':entry.conversational?'RENEGADE':'RENEGADE / LOCAL MODEL');
 head.append(name);
 if(entry.role==='assistant'){
  const listen=document.createElement('button');listen.type='button';listen.className='listen-reply';listen.textContent='Listen';
  listen.title='Hear this completed response in Daniel voice';
  listen.disabled=current.busy||current.muted;
  listen.addEventListener('click',async()=>{
   listen.disabled=true;
   try{const r=await window.renegadeChat.speak(entry.content);if(r?.error)status.textContent=r.error}
   catch(e){status.textContent='Voice error: '+String(e.message||e)}
   finally{listen.disabled=current.busy||current.muted}
  });head.append(listen);
 }
 const content=document.createElement('div');content.className='message-body';
 window.renegadeTextFormatting.render(content,answerText);
 if(entry.role==='assistant'&&sources.length){
   const view=document.createElement('button');view.type='button';
   view.textContent='View source'+(sources.length===1?'':'s');
   view.title='Show the source files used to verify this answer';
   view.setAttribute('aria-expanded','false');
   const refs=document.createElement('div');refs.className='answer-sources';refs.hidden=true;
   for(const source of sources){
     const line=document.createElement('div');line.textContent=source;refs.append(line);
   }
   view.addEventListener('click',()=>{
     refs.hidden=!refs.hidden;view.setAttribute('aria-expanded',String(!refs.hidden));
     view.textContent=(refs.hidden?'View':'Hide')+' source'+(sources.length===1?'':'s');
   });
   item.append(head,content,view,refs);
 }else item.append(head,content);
 return item;
}
function draw(data){
 const wasNearBottom=historyEl.scrollHeight-historyEl.scrollTop-historyEl.clientHeight<100;
 current=data;
 // Reconstruct previous messages only when the actual completed history changes.
 // The live partial answer updates one existing DOM text node, avoiding flicker.
 const signature=JSON.stringify(data.history);
 if(signature!==lastHistory){
  lastHistory=signature;pending=null;historyEl.replaceChildren();
  if(!data.history.length){
   const p=document.createElement('div');p.className='empty';
   p.textContent='What can I help you build?';
   historyEl.append(p);
  }
  for(const entry of data.history)historyEl.append(makeMessage(entry));
 }
 if(data.busy){
  if(!pending){
   pending=document.createElement('article');pending.className='message assistant stream';
   const label=document.createElement('div');label.className='role';label.textContent='RENEGADE / LIVE';
   const body=document.createElement('div');body.className='partial-body message-body';
   pending.append(label,body);historyEl.append(pending);
  }
  const body=pending.querySelector('.partial-body');
  const text=data.partial||'Renegade is preparing the first words...';
  if(body.dataset.lastText!==text){window.renegadeTextFormatting.render(body,text);body.dataset.lastText=text;}
 }else if(pending){pending.remove();pending=null;}
 status.textContent=data.status||'Ready';
 send.disabled=Boolean(data.busy);cancel.hidden=!data.busy;
 const voice=data.conversation||{enabled:false,status:'Microphone off'};
 $('voiceConversation').textContent=voice.enabled?'END VOICE CONVERSATION':'VOICE CONVERSATION';
 $('voiceConversation').setAttribute('aria-pressed',String(voice.enabled));
 $('conversationStatus').textContent=voice.status;
 if(voice.draft&&prompt.dataset.voiceDraft!==voice.draft){prompt.value=voice.draft;prompt.dataset.voiceDraft=voice.draft;prompt.focus();}
 if(!voice.draft)delete prompt.dataset.voiceDraft;
 auto.checked=Boolean(data.autoSpeak);mute.checked=Boolean(data.muted);
 for(const btn of historyEl.querySelectorAll('.message.assistant button.listen-reply'))btn.disabled=Boolean(data.busy||data.muted);
 if(wasNearBottom)historyEl.scrollTop=historyEl.scrollHeight;
}
window.renegadeChat.onUpdate(draw);
window.renegadeChat.snapshot().then(draw).catch(e=>status.textContent='Chat unavailable: '+e.message);
async function ask(){
 const value=prompt.value.trim();if(!value||current.busy)return;
 prompt.value='';send.disabled=true;
 try{const r=await window.renegadeChat.ask(value);if(r?.error)status.textContent=r.error}
 catch(e){status.textContent='Local model error: '+String(e.message||e)}
}
send.addEventListener('click',ask);
prompt.addEventListener('keydown',e=>{if(e.key==='Enter'&&!e.shiftKey&&!e.isComposing){e.preventDefault();ask()}});
$('new').addEventListener('click',()=>window.renegadeChat.clear().catch(e=>status.textContent=String(e.message||e)));
cancel.addEventListener('click',()=>window.renegadeChat.cancel().catch(e=>status.textContent=String(e.message||e)));
$('stop').addEventListener('click',()=>window.renegadeChat.stopVoice().catch(e=>status.textContent=String(e.message||e)));
auto.addEventListener('change',()=>window.renegadeChat.autoSpeak(auto.checked).catch(e=>status.textContent=String(e.message||e)));
mute.addEventListener('change',()=>window.renegadeChat.mute(mute.checked).catch(e=>status.textContent=String(e.message||e)));
$('minimise').addEventListener('click',()=>window.renegadeChat.minimise());
$('voiceConversation').addEventListener('click',()=>window.renegadeChat.conversation(!current.conversation?.enabled));
window.renegadeChat.onVisibility(visible=>{if(visible)requestAnimationFrame(()=>requestAnimationFrame(()=>document.body.classList.remove('retracted')));else document.body.classList.add('retracted');});
window.renegadeChat.onAnchor(anchor=>{
 document.body.classList.toggle('tail-top',anchor.edge==='top');
 document.documentElement.style.setProperty('--tail-x',anchor.x+'px');
 document.documentElement.style.setProperty('--origin-y',anchor.edge==='top'?'0%':'100%');
});
