'use strict';
const {contextBridge,ipcRenderer}=require('electron');
contextBridge.exposeInMainWorld('voiceHost',Object.freeze({
 speak(text,voice){return ipcRenderer.invoke('renegade:voice:speak',{text,voice})},
 stop(){return ipcRenderer.invoke('renegade:voice:stop')},
 setMuted(value){return ipcRenderer.invoke('renegade:voice:mute',Boolean(value))},
 getSettings(){return ipcRenderer.invoke('renegade:voice:settings')},
 onStatus(fn){if(typeof fn==='function')ipcRenderer.on('renegade:voice:status',(_ev,v)=>{if(v&&typeof v.status==='string')fn(v)})}
}));