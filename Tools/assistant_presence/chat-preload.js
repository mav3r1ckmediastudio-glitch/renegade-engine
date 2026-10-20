'use strict';
const {contextBridge,ipcRenderer}=require('electron');
contextBridge.exposeInMainWorld('renegadeChat',Object.freeze({
 minimise(){return ipcRenderer.invoke('renegade:chat:minimise')},
 conversation(enabled){return ipcRenderer.invoke('renegade:chat:conversation',Boolean(enabled))},
 onVisibility(fn){ipcRenderer.on('renegade:chat:visibility',(_e,v)=>fn(v))},
 onAnchor(fn){ipcRenderer.on('renegade:chat:anchor',(_e,v)=>fn(v))},
 snapshot(){return ipcRenderer.invoke('renegade:chat:snapshot')},
 ask(text){return ipcRenderer.invoke('renegade:chat:ask',text)},
 clear(){return ipcRenderer.invoke('renegade:chat:clear')},
 cancel(){return ipcRenderer.invoke('renegade:chat:cancel')},
 speak(text){return ipcRenderer.invoke('renegade:chat:speak',text)},
 stopVoice(){return ipcRenderer.invoke('renegade:chat:stop-voice')},
 autoSpeak(enabled){return ipcRenderer.invoke('renegade:chat:auto-speak',Boolean(enabled))},
 mute(enabled){return ipcRenderer.invoke('renegade:chat:mute',Boolean(enabled))},
 window(mode){return ipcRenderer.invoke('renegade:chat:window',mode)},
 onUpdate(fn){if(typeof fn==='function')ipcRenderer.on('renegade:chat:update',(_e,data)=>{if(data&&typeof data==='object')fn(data)})}
}));