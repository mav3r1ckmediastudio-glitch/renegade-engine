'use strict';
const conversationButton=document.getElementById('voiceConversation');
conversationButton.addEventListener('click',async()=>{
 try{await window.orbHost.conversation()}catch(e){conversationButton.title=String(e.message||e)}
});
let orbDiameter=220,orbState='idle';
function placeVoiceButton(){
 conversationButton.style.bottom='auto';
 conversationButton.style.top='calc(50% + '+((orbState==='idle'?84:orbDiameter)/2+10)+'px)';
 conversationButton.style.transition='top .3s ease';
}
placeVoiceButton();
window.orbHost.onCommand(message=>{
 if(message.type==='size'){orbDiameter=message.value;placeVoiceButton();return}
 if(message.type==='state'){orbState=message.value;placeVoiceButton();return}
 if(message.type!=='conversation')return;
 conversationButton.textContent=message.enabled?(message.phase==='listening'?'Listening · End':message.phase==='review'?'Check chat · End':'Voice active · End'):'Voice conversation';
 conversationButton.setAttribute('aria-pressed',String(Boolean(message.enabled)));
 conversationButton.title=message.status||'Microphone off';
 conversationButton.style.borderColor=message.enabled?'#80e4c0':'#3d7183';
});
