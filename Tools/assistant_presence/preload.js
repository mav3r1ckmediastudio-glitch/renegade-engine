'use strict';
const {contextBridge,ipcRenderer}=require('electron');
const allowed=new Set(['orb:state','orb:audio-load','orb:audio-play','orb:audio-stop','orb:size','orb:brightness','orb:motion']);
contextBridge.exposeInMainWorld('orbHost',Object.freeze({
 conversation(){return ipcRenderer.invoke('orb:conversation')},
 startDrag(){ipcRenderer.send('orb:drag-start')},
 endDrag(){ipcRenderer.send('orb:drag-end')},
 toggleState(){ipcRenderer.send('orb:toggle-state')},
 showMenu(){ipcRenderer.send('orb:show-menu')},
 audioEnded(requestId=null){ipcRenderer.send('orb:audio-ended',requestId)},
 audioStarted(requestId=null){ipcRenderer.send('orb:audio-started',requestId)},
 audioFailed(message,requestId=null){ipcRenderer.send('orb:audio-failed',String(message||'Unknown playback error').slice(0,500),requestId)},
 rendererReady(){ipcRenderer.send('orb:renderer-ready')},
 reportPerformance(fps,drawMs){ipcRenderer.send('orb:performance',{fps,drawMs})},
 onCommand(fn){if(typeof fn==='function')ipcRenderer.on('orb:command',(_event,value)=>{
  if(value&&typeof value.type==='string'&&allowed.has('orb:'+value.type))fn(value);
  // state/size/brightness/motion commands are also allowlisted explicitly below.
  else if(value&&['conversation','state','size','brightness','motion','audio-load','audio-play','audio-stop','speech-begin','speech-chunk','speech-finish','speech-stop'].includes(value.type))fn(value);
 })}
}));
