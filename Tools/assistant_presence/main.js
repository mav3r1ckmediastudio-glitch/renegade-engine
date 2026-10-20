'use strict';
// Renegade Engine Presence: standalone Windows host for the LOCKED V1 visual renderer.
// No connection to model, networking, editor mutation, or the shipped Renegade runtime.
const {app,BrowserWindow,ipcMain,Tray,Menu,nativeImage,screen,dialog} = require('electron');
const fs=require('fs');
const path=require('path');
const {VOICES,generate}=require('./voice/synth');
const {WarmDaniel}=require('./voice/warm-daniel');
const {splitSpeech}=require('./voice/segments');
const {speakableExcerpt}=require('./chat/model-client');
const {askQwenStream}=require('./chat/stream-client');
const {LiveSpeech}=require('./chat/live-speech');
const {bubbleLayout}=require('./chat/bubble-layout');
const {VoiceConversation}=require('./voice/conversation');
const {StartupGuide}=require('./chat/startup-guide');
const {readName}=require('./chat/studio-profile');
const startupGuide=new StartupGuide();
const warmDaniel=new WarmDaniel();
let bubbleHideTimer=null,bubbleMoveTimer=null,quitting=false;
const conversation=new VoiceConversation({
 onUpdate:()=>{updateChat();updateMenu();send({type:'conversation',...conversation.snapshot()});},
 onTurn:text=>askChat(text)
});
function reconcileConversation(){
 const voice=conversation.snapshot();
 const phase=currentSpeechStatus.phase;
 const outputActive=['generating','starting','speaking'].includes(phase)||Boolean(chatVoiceSession)||config.state==='speaking';
 conversation.setListening(voice.enabled&&!voice.draft&&!chatBusy&&!outputActive&&!config.muted);
 if(conversation.listening&&config.state!=='listening')selectState('listening');
}
function hideChatPanel(){
 if(!chatWindow||chatWindow.isDestroyed())return {ok:true};
 clearTimeout(bubbleHideTimer);
 chatWindow.webContents.send('renegade:chat:visibility',false);
 bubbleHideTimer=setTimeout(()=>{bubbleHideTimer=null;chatWindow?.hide();},180);
 return {ok:true};
}
function toggleChatPanel(){
 if(chatWindow&&!chatWindow.isDestroyed()&&chatWindow.isVisible()&&!bubbleHideTimer)return hideChatPanel();
 openChatPanel();return {ok:true};
}
function positionChatBubble(){
 clearTimeout(bubbleMoveTimer);
 if(!chatWindow||chatWindow.isDestroyed()||!win)return;
 const layout=bubbleLayout(currentCenter(),workareaNear(currentCenter()),config.bubbleSize,config.state==='idle'?84:config.expandedDiameter);
 chatWindow.setBounds(layout.bounds,false);
 chatWindow.webContents.send('renegade:chat:anchor',layout.tail);
}
function animateBubbleAnchor(){
 clearTimeout(bubbleMoveTimer);
 if(!chatWindow||chatWindow.isDestroyed()||!chatWindow.isVisible()||!win)return;
 const from=chatWindow.getBounds();
 const target=bubbleLayout(currentCenter(),workareaNear(currentCenter()),config.bubbleSize,config.state==='idle'?84:config.expandedDiameter);
 chatWindow.webContents.send('renegade:chat:anchor',target.tail);
 const start=Date.now(),duration=config.reducedMotion?0:300;
 const frame=()=>{
  if(!chatWindow||chatWindow.isDestroyed())return;
  const t=duration?Math.min(1,(Date.now()-start)/duration):1;
  const ease=1-Math.pow(1-t,3);
  chatWindow.setPosition(Math.round(from.x+(target.bounds.x-from.x)*ease),Math.round(from.y+(target.bounds.y-from.y)*ease),false);
  if(t<1)bubbleMoveTimer=setTimeout(frame,16);
 };
 frame();
}
function setConversation(enabled){
 if(enabled){if(config.muted)setMuted(false);config.chatAutoSpeak=true;saveConfigSoon();conversation.start();}
 else {conversation.stop();if(config.state==='listening')selectState('idle');}
 updateChat();return conversation.snapshot();
}
app.setAppUserModelId('Renegade.Studio.LivingPresence');
// Playback is requested by the user in a separate voice window, then sent to
// the orb renderer through a trusted IPC channel. Allow that handoff to play.
app.commandLine.appendSwitch('autoplay-policy','no-user-gesture-required');
// Keep one tray companion per Windows session; launching again restores the original.
const hasSingleInstanceLock = app.requestSingleInstanceLock();
if (!hasSingleInstanceLock) app.quit();
const legalStates=['idle','listening','thinking','speaking','success','warning'];
const defaults={state:'idle',expandedDiameter:220,brightness:75,reducedMotion:false,alwaysOnTop:false,center:null,voice:'daniel',muted:false,chatAutoSpeak:true,chatBounds:null,bubbleSize:null};
let config={...defaults}, win=null, tray=null, ready=false, dragging=null, lastPerformance={fps:null,drawMs:null};
let savePending=null, hitIgnored=false;
let busyAudio=false;
let voiceWindow=null,speechGeneration=0,speechAbort=null;
let chatWindow=null,chatHistory=[],chatBusy=false,chatEpoch=0,chatAbort=null;
let chatStatus='Renegade: verified knowledge available; general conversation uses local Qwen. Editor observation is not connected.';
let chatPartial='',chatFirstTokenMs=null,chatVoiceSession=null,chatUpdateTimer=null;
let currentSpeechStatus={phase:'ready',status:'Voice ready. Press Speak to hear Renegade.'};
app.on('second-instance',()=>{if(win&&!win.isDestroyed())win.showInactive();openChatPanel();});

function configFile(){return path.join(app.getPath('userData'),'presence-window.json');}
function loadConfig(){
 try{const value=JSON.parse(fs.readFileSync(configFile(),'utf8'));
 if(legalStates.includes(value.state))config.state=value.state;
 if(Number.isFinite(value.expandedDiameter))config.expandedDiameter=Math.max(120,Math.min(320,Math.round(value.expandedDiameter)));
 if(Number.isFinite(value.brightness))config.brightness=Math.max(20,Math.min(100,Math.round(value.brightness)));
 if(typeof value.reducedMotion==='boolean')config.reducedMotion=value.reducedMotion;
 if(typeof value.alwaysOnTop==='boolean')config.alwaysOnTop=value.alwaysOnTop;
 if(VOICES.some(v=>v.id===value.voice))config.voice=value.voice;
 if(typeof value.muted==='boolean')config.muted=value.muted;
 if(typeof value.chatAutoSpeak==='boolean')config.chatAutoSpeak=value.chatAutoSpeak;
 if(value.bubbleSize&&Number.isFinite(value.bubbleSize.width)&&Number.isFinite(value.bubbleSize.height))config.bubbleSize=value.bubbleSize;
 if(value.chatBounds&&['x','y','width','height'].every(k=>Number.isFinite(value.chatBounds[k])))config.chatBounds=value.chatBounds;
 if(value.center&&Number.isFinite(value.center.x)&&Number.isFinite(value.center.y)) config.center={x:value.center.x,y:value.center.y};
 }catch(_){}
 // Starting quiet is intentional, even after an app was quit during speech.
 config.state='idle';
}
function saveConfigSoon(){clearTimeout(savePending);savePending=setTimeout(()=>{try{fs.mkdirSync(path.dirname(configFile()),{recursive:true});fs.writeFileSync(configFile(),JSON.stringify({...config,state:'idle'},null,2)+'\n');}catch(err){console.error('Save settings failed:',err.message)}},260);}
// Reserve the FULL transparent window footprint immediately. The approved V1
// renderer animates the pixels inside this stable HWND. Never resize or re-anchor
// the native window merely because the orb changes idle/listening/speaking state.
// This also eliminates the desktop-space jump visible in the owner's recording.
function windowSize(){return Math.ceil(config.expandedDiameter*1.60+18);}
function workareaNear(point){return screen.getDisplayNearestPoint({x:Math.round(point.x),y:Math.round(point.y)}).workArea;}
function clampCenter(point,size){const w=workareaNear(point);const pad=4;const half=size/2;
 return {x:Math.min(w.x+w.width-half-pad,Math.max(w.x+half+pad,point.x)),y:Math.min(w.y+w.height-half-pad,Math.max(w.y+half+pad,point.y))};}
function primaryStartPosition(){const w=screen.getPrimaryDisplay().workArea;const size=windowSize();return {x:w.x+w.width-size/2-24,y:w.y+w.height-size/2-28};}
function currentCenter(){if(!win)return config.center||primaryStartPosition();const b=win.getBounds();return {x:b.x+b.width/2,y:b.y+b.height/2};}
// Window size is allowed to change only when the USER explicitly chooses a new
// expanded diameter, not when Renegade changes state. Preserve the centre.
function resizeForUserSizeChange(){
 if(!win)return;
 const size=windowSize(), center=clampCenter(currentCenter(),size), b=win.getBounds();
 if(b.width!==size||b.height!==size||Math.abs((b.x+b.width/2)-center.x)>.5||Math.abs((b.y+b.height/2)-center.y)>.5){
  win.setBounds({x:Math.round(center.x-size/2),y:Math.round(center.y-size/2),width:size,height:size},false);
 }
 config.center=center;saveConfigSoon();
}
function relocateIfOffscreen(){
 if(!win)return;
 const b=win.getBounds(), center=clampCenter(currentCenter(),b.width);
 if(Math.abs(b.x+b.width/2-center.x)>.5||Math.abs(b.y+b.height/2-center.y)>.5)
  win.setPosition(Math.round(center.x-b.width/2),Math.round(center.y-b.height/2),false);
 config.center=center;saveConfigSoon();
}
function send(message){if(ready && win&&!win.isDestroyed()) win.webContents.send('orb:command',message);}
function selectState(next){if(!legalStates.includes(next))return;const changed=config.state!==next;config.state=next;send({type:'state',value:next});if(changed)animateBubbleAnchor();updateMenu();saveConfigSoon();}
function toggleState(){toggleChatPanel();if(!win.isVisible())win.showInactive();}
function setSize(diameter){config.expandedDiameter=Math.max(120,Math.min(320,Math.round(diameter)));resizeForUserSizeChange();send({type:'size',value:config.expandedDiameter});saveConfigSoon();updateMenu();}
function setBrightness(v){config.brightness=v;send({type:'brightness',value:v});saveConfigSoon();updateMenu();}
function resetPosition(){if(!win)return;const size=windowSize(),center=clampCenter(primaryStartPosition(),size);config.center=center;win.setBounds({x:Math.round(center.x-size/2),y:Math.round(center.y-size/2),width:size,height:size},false);saveConfigSoon();win.showInactive();}
function setAlwaysOnTop(v){config.alwaysOnTop=v;win.setAlwaysOnTop(v,'floating');chatWindow?.setAlwaysOnTop(v,'floating');saveConfigSoon();updateMenu();}
function setMotion(v){config.reducedMotion=v;send({type:'motion',value:v});saveConfigSoon();updateMenu();}
function menuTemplate(){const metric=lastPerformance.fps===null?'Pending':`${lastPerformance.fps} FPS, render ${lastPerformance.drawMs} ms`;
 return [
 {label:'Renegade Studio Living Core — V1',enabled:false},
 {label:`Renderer: ${metric}`,enabled:false},
 {type:'separator'},
 {label:'Show orb',click:()=>win.showInactive()},
 {label:'Hide orb (restore from tray)',click:()=>win.hide()},
 {type:'separator'},
 {label:'Engine state',submenu:legalStates.map(s=>({label:s.charAt(0).toUpperCase()+s.slice(1)+(s==='speaking'?' (demonstration)':''),type:'radio',checked:config.state===s,click:()=>selectState(s)}))},
 {label:'Expanded size',submenu:[120,160,180,220,260,320].map(v=>({label:v+' px',type:'radio',checked:config.expandedDiameter===v,click:()=>setSize(v)}))},
 {label:'Energy intensity',submenu:[45,65,75,85,100].map(v=>({label:v+'%',type:'radio',checked:config.brightness===v,click:()=>setBrightness(v)}))},
 {label:'Reduced motion',type:'checkbox',checked:config.reducedMotion,click:item=>setMotion(item.checked)},
 {label:'Always on top',type:'checkbox',checked:config.alwaysOnTop,click:item=>setAlwaysOnTop(item.checked)},
 {type:'separator'},
 {label:'Load voice recording (local file)',click:loadVoiceFile},
 {label:'Play loaded audio',click:()=>{send({type:'audio-play'});selectState('speaking')}},
 {label:'Stop audio',click:()=>send({type:'audio-stop'})},
 {type:'separator'},
 {label:'Open Renegade conversation',click:openChatPanel},
 {label:conversation.snapshot().enabled?'End voice conversation (microphone off)':'Start voice conversation',click:()=>setConversation(!conversation.snapshot().enabled)},
 {label:'Open voice laboratory',click:openVoicePanel},
 {label:'Hear voice test phrase',click:()=>speakText('Renegade is online. If you need a hand, just ask. I promise to refrain from becoming a paperclip.',config.voice)},
 {label:'Voice',submenu:VOICES.map(v=>({label:v.label,type:'radio',checked:config.voice===v.id,click:()=>{config.voice=v.id;saveConfigSoon();updateMenu();broadcastVoiceStatus()}}))},
 {label:'Mute voice',type:'checkbox',checked:config.muted,click:item=>setMuted(item.checked)},
 {label:'Stop speaking',click:stopSpeech},
 {type:'separator'},
 {label:'Reset to primary lower-right',click:resetPosition},
 {label:'Quit Renegade Presence',click:()=>app.quit()}
 ];}
function updateMenu(){if(tray&&!tray.isDestroyed())tray.setContextMenu(Menu.buildFromTemplate(menuTemplate()));}
async function loadVoiceFile(){if(busyAudio)return;busyAudio=true;
 try{
  const result=await dialog.showOpenDialog(win,{title:'Load local speech recording',properties:['openFile'],filters:[{name:'Audio recording',extensions:['wav','mp3','ogg','m4a','flac']}]});
  if(result.canceled||!result.filePaths.length)return;
  const filename=result.filePaths[0],stat=fs.statSync(filename);
  if(stat.size>24*1024*1024){dialog.showErrorBox('Recording too large','Please choose an audio file under 24 MB for this prototype.');return;}
  const ext=path.extname(filename).toLowerCase();
  const mime=({'.wav':'audio/wav','.mp3':'audio/mpeg','.ogg':'audio/ogg','.m4a':'audio/mp4','.flac':'audio/flac'})[ext];
  if(!mime){dialog.showErrorBox('Unsupported format','Use WAV, MP3, OGG, M4A or FLAC.');return;}
  const dataUrl=`data:${mime};base64,`+fs.readFileSync(filename).toString('base64');
  send({type:'audio-load',dataUrl,name:path.basename(filename)});
  selectState('speaking');
  // HTML audio playback is initiated explicitly with the tray's Play command.
 }catch(err){dialog.showErrorBox('Could not load recording',String(err.message||err))}
 finally{busyAudio=false;}
}
function voiceSnapshot(){return {...currentSpeechStatus,voice:config.voice,muted:config.muted,voices:VOICES};}
function broadcastVoiceStatus(values={}){
 currentSpeechStatus={...currentSpeechStatus,...values};
 if(voiceWindow&&!voiceWindow.isDestroyed())voiceWindow.webContents.send('renegade:voice:status',voiceSnapshot());
 updateChat();
}
function openVoicePanel(){
 if(voiceWindow&&!voiceWindow.isDestroyed()){voiceWindow.show();voiceWindow.focus();return;}
 voiceWindow=new BrowserWindow({
  title:'Renegade - Voice Laboratory',width:500,height:475,minWidth:410,minHeight:410,
  backgroundColor:'#071020',show:false,autoHideMenuBar:true,
  webPreferences:{preload:path.join(__dirname,'voice-preload.js'),contextIsolation:true,nodeIntegration:false,sandbox:true,webSecurity:true}
 });
 voiceWindow.setMenuBarVisibility(false);
 voiceWindow.on('closed',()=>voiceWindow=null);
 voiceWindow.webContents.on('did-finish-load',()=>broadcastVoiceStatus());
 voiceWindow.loadFile(path.join(__dirname,'voice-panel.html')).then(()=>voiceWindow?.show()).catch(err=>console.error('Voice console failed:',err));
}
function setMuted(value){
 config.muted=Boolean(value);
 if(config.muted)stopSpeech();
 saveConfigSoon();updateMenu();
 broadcastVoiceStatus({phase:'ready',status:config.muted?'Voice muted. Text mode remains available.':'Ready. Press Speak to hear Renegade.'});
 updateChat();
 return voiceSnapshot();
}
function stopSpeech(){
 if(chatVoiceSession){chatVoiceSession.cancel();chatVoiceSession=null;}
 speechGeneration++;
 if(speechAbort){speechAbort.abort();speechAbort=null;}
 send({type:'speech-stop'});
 send({type:'audio-stop'});
 if(win&&!win.isDestroyed())selectState('idle');
 broadcastVoiceStatus({phase:'ready',status:config.muted?'Voice muted.':'Stopped. Ready for another sentence.'});
 return voiceSnapshot();
}
async function speakTextWhole(text,voice=config.voice){
 if(config.muted){broadcastVoiceStatus({phase:'ready',status:'Voice is muted; unmute to hear speech.'});return {error:'Voice is muted.'};}
 // Validate before interrupting previous speech. The synthesis implementation
 // has the same strict input and voice validation on its subprocess boundary.
 if(typeof text!=='string'||!text.trim()||text.trim().length>900||!VOICES.some(v=>v.id===voice))
   return {error:'Enter 1 to 900 characters and select a supported voice.'};
 stopSpeech();
 const token=speechGeneration;
 const controller=new AbortController();speechAbort=controller;
 config.voice=voice;saveConfigSoon();updateMenu();
 selectState('thinking');
 broadcastVoiceStatus({phase:'generating',status:'Preparing '+VOICES.find(v=>v.id===voice).label+' locally...'});
 try {
  const result=voice==='daniel'?await warmDaniel.generate(text,controller.signal):await generate({text,voice,signal:controller.signal});
  if(token!==speechGeneration||controller.signal.aborted)return {canceled:true};
  speechAbort=null;
  if(!ready){broadcastVoiceStatus({phase:'error',status:'Orb window is not ready for speech playback.'});selectState('idle');return {error:'Orb is not ready'};}
  // The renderer decodes/plays the WAV via WebAudio and uses that same audio
  // signal to animate the central waveform, with no simulated phonemes.
  send({type:'audio-load',dataUrl:'data:audio/wav;base64,'+result.data.toString('base64'),name:'Renegade voice'});
  selectState('speaking');
  send({type:'audio-play'});
  broadcastVoiceStatus({phase:'starting',status:'Speech ready ('+result.engine+', '+result.elapsedMs+' ms). Starting playback...'});
  return {ok:true,engine:result.engine,elapsedMs:result.elapsedMs};
 }catch(err){
  if(token!==speechGeneration)return {canceled:true};
  speechAbort=null;selectState('idle');
  const message='Speech generation failed: '+String(err.message||err);
  broadcastVoiceStatus({phase:'error',status:message});
  console.error(message);
  return {error:message};
 }
}
// Incremental user-directed conversation playback. Generate the first complete
// sentence, start playback, then prepare remaining sentences while it speaks.
// The original whole-utterance path remains available for Windows fallbacks.
async function speakText(text,voice=config.voice){
 if(voice!=='daniel')return speakTextWhole(text,voice);
 if(config.muted){broadcastVoiceStatus({phase:'ready',status:'Voice is muted; unmute to hear speech.'});return {error:'Voice is muted.'};}
 let pieces;
 try{pieces=splitSpeech(text);}catch(err){return {error:err.message};}
 stopSpeech();
 const token=speechGeneration,controller=new AbortController();
 speechAbort=controller;config.voice='daniel';saveConfigSoon();updateMenu();
 if(!ready){speechAbort=null;return {error:'Orb renderer is not ready'};}
 send({type:'speech-begin',requestId:token,total:pieces.length});
 selectState('thinking');
 broadcastVoiceStatus({phase:'generating',status:'Daniel: preparing first sentence ('+pieces.length+' part'+(pieces.length===1?'':'s')+')...'});
 let totalSynthesisMs=0;
 try{
  for(let i=0;i<pieces.length;i++){
   const started=Date.now();
   const result=await warmDaniel.generate(pieces[i],controller.signal);
   if(token!==speechGeneration||controller.signal.aborted)return {canceled:true};
   totalSynthesisMs+=Date.now()-started;
   send({type:'speech-chunk',requestId:token,index:i,dataUrl:'data:audio/wav;base64,'+result.data.toString('base64')});
   // Audio plays immediately from the first ready segment; subsequent parts
   // are queued on the same WebAudio clock to minimise dead air.
   if(i===0)broadcastVoiceStatus({phase:'starting',status:'Starting Daniel. Preparing the rest in parallel with playback...'});
   else broadcastVoiceStatus({phase:'speaking',status:'Daniel speaking ('+(i+1)+'/'+pieces.length+' segments prepared).'});
  }
  if(token!==speechGeneration||controller.signal.aborted)return {canceled:true};
  send({type:'speech-finish',requestId:token});
  return {ok:true,engine:'Kokoro / Daniel (incremental)',chunks:pieces.length,totalSynthesisMs};
 }catch(err){
  if(token!==speechGeneration||controller.signal.aborted)return {canceled:true};
  stopSpeech();
  const message='Speech generation failed: '+String(err.message||err);
  broadcastVoiceStatus({phase:'error',status:message});
  console.error(message);
  return {error:message};
 }
}
// Detached chat is a read-only consumer of the existing local Qwen experiment.
// It has NO editor commands, file/network access in the renderer or scene tools.
// Conversation history lives in memory only for this test session.
function chatSnapshot(){
 const voicePhase=currentSpeechStatus.phase;
 const status=(!chatBusy && (voicePhase==='speaking'||voicePhase==='starting'||voicePhase==='generating'))
  ? currentSpeechStatus.status : chatStatus;
 return {history:chatHistory.map(m=>({...m})),busy:chatBusy,autoSpeak:config.chatAutoSpeak,
         muted:config.muted,status,voice:'daniel',partial:chatPartial,firstTokenMs:chatFirstTokenMs,conversation:conversation.snapshot()};
}
function updateChat(){
 reconcileConversation();
 send({type:'conversation',...conversation.snapshot()});
 if(chatWindow&&!chatWindow.isDestroyed())chatWindow.webContents.send('renegade:chat:update',chatSnapshot());
}
function throttleChat(){
 if(chatUpdateTimer)return;
 chatUpdateTimer=setTimeout(()=>{chatUpdateTimer=null;updateChat();},65);
}
function openChatPanel(){
 clearTimeout(bubbleHideTimer);bubbleHideTimer=null;
 if(chatWindow&&!chatWindow.isDestroyed()){
  positionChatBubble();chatWindow.show();chatWindow.focus();
  chatWindow.webContents.send('renegade:chat:visibility',true);return;
 }
 const layout=bubbleLayout(currentCenter(),workareaNear(currentCenter()),config.bubbleSize,config.state==='idle'?84:config.expandedDiameter);
 chatWindow=new BrowserWindow({
   ...layout.bounds,minWidth:320,minHeight:320,frame:false,transparent:true,hasShadow:false,
   title:'Renegade - Living Core Chat',show:false,backgroundColor:'#00000000',autoHideMenuBar:true,
   alwaysOnTop:config.alwaysOnTop,
   webPreferences:{preload:path.join(__dirname,'chat-preload.js'),contextIsolation:true,nodeIntegration:false,sandbox:true,webSecurity:true}
 });
 chatWindow.setMenuBarVisibility(false);
 chatWindow.on('resized',()=>{
  if(!chatWindow||chatWindow.isDestroyed())return;
  const b=chatWindow.getBounds();config.bubbleSize={width:b.width,height:b.height};saveConfigSoon();
 });
 chatWindow.on('close',event=>{if(!quitting){event.preventDefault();hideChatPanel();}});
 chatWindow.on('closed',()=>{chatWindow=null;});
 chatWindow.webContents.on('did-finish-load',()=>{updateChat();positionChatBubble();chatWindow.webContents.send('renegade:chat:visibility',true);});
 chatWindow.loadFile(path.join(__dirname,'chat.html'))
   .then(()=>chatWindow?.show()).catch(e=>console.error('Chat bubble failed:',e));
}
function cancelChat(){
 chatEpoch++;
 if(chatAbort){chatAbort.abort();chatAbort=null;}
 if(chatVoiceSession)stopSpeech();
 chatPartial='';chatBusy=false;chatFirstTokenMs=null;
 selectState('idle');
 chatStatus='Request cancelled. The model may finish its current token internally.';
 updateChat();return chatSnapshot();
}
async function askChat(message){
 if(chatBusy)return {error:'Renegade is still answering the previous question.'};
 if(typeof message!=='string'||!message.trim()||message.trim().length>900)
   return {error:'Please enter 1 to 900 characters.'};
 const question=message.trim(),turnId=++chatEpoch;
 const controller=new AbortController();chatAbort=controller;
 // Interrupt previous playback when a new question is submitted. The next
 // message's speaking state is then owned by this chatEpoch.
 chatBusy=true;conversation.setListening(false);
 stopSpeech();
 conversation.clearDraft();chatPartial='';chatFirstTokenMs=null;
 chatHistory.push({role:'user',content:question});
 chatStatus='Waiting for the first Qwen3 words...';
 selectState('thinking');
 updateChat();
 const started=performance.now();
 let voiceSession=null;
 if(config.chatAutoSpeak&&!config.muted&&ready){
  const token=speechGeneration;
  const voiceAbort=new AbortController();speechAbort=voiceAbort;
  voiceSession=new LiveSpeech({
   requestId:token,signal:voiceAbort.signal,
   generate:(text,signal)=>warmDaniel.generate(text,signal),
   send,valid:()=>turnId===chatEpoch&&token===speechGeneration&&!config.muted,
   onError:err=>{chatVoiceSession=null;speechAbort=null;selectState('idle');chatStatus='Speech interrupted by generator error: '+String(err.message||err).slice(0,150);updateChat();}
  });
  chatVoiceSession=voiceSession;
 }
 try{
  const startupAnswer=startupGuide.answer(question);
  if(startupAnswer&&voiceSession)voiceSession.deliver(startupAnswer);
  const result=startupAnswer?{answer:startupAnswer,adapter:'startup-guide',elapsedMs:0,firstTokenMs:0}:await askQwenStream(chatHistory,{
   signal:controller.signal,maxTokens:180,
   onMeta:meta=>{
    if(turnId!==chatEpoch)return;
    if(meta.fallback){chatStatus='Streaming model unavailable; using the slower local Qwen fallback.';updateChat();}
   },
   onDelta:chunk=>{
    if(turnId!==chatEpoch||controller.signal.aborted)return;
    if(chatFirstTokenMs===null&&chunk)chatFirstTokenMs=Math.round(performance.now()-started);
    chatPartial+=chunk;
    if(voiceSession&&!voiceSession.cancelled)voiceSession.deliver(chunk);
    chatStatus='Streaming Qwen3 V3.1'
      +(chatFirstTokenMs!==null?' · first text '+(chatFirstTokenMs/1000).toFixed(2)+' s':'')
      +' · '+chatPartial.length+' characters received';
    throttleChat();
   }
  });
  if(turnId!==chatEpoch||controller.signal.aborted)return {cancelled:true};
  if(result.correction){
   // Never speak an unverified claim of editor access.
   if(voiceSession)stopSpeech();
   voiceSession=null;
  }else if(voiceSession){
   // Do not block chat completion while Daniel finishes preparing queued WAVs.
   void voiceSession.finish().catch(err=>console.error('Daniel stream finalization failed:',err.message));
  }
  const conversational=['social-flow','conversation-check','startup-guide'].includes(result.adapter);
  chatHistory.push({role:'assistant',content:result.answer,conversational,sources:Array.isArray(result.sources)?result.sources:[]});
  if(chatHistory.length>36)chatHistory.splice(0,chatHistory.length-36);
  chatBusy=false;chatAbort=null;chatPartial='';
  const ttft=(result.firstTokenMs!==null&&result.firstTokenMs!==undefined)?(result.firstTokenMs/1000).toFixed(2)+' s':'n/a';
  chatStatus=conversational?'Ready.':result.verified
    ? 'Source-verified Renegade answer · '+(result.elapsedMs/1000).toFixed(2)+' s'
    : (result.fallback?'Standard local Qwen reply. ':'Stream: ')+
      'first words '+ttft+'; full reply '+(result.elapsedMs/1000).toFixed(1)+' s. Experimental answer.';
  if(!voiceSession)selectState('idle');
  updateChat();
  return {ok:true,elapsedMs:result.elapsedMs,firstTokenMs:result.firstTokenMs,streaming:result.streaming};
 }catch(err){
  if(turnId!==chatEpoch)return {cancelled:true};
  chatBusy=false;chatAbort=null;chatPartial='';
  if(voiceSession)stopSpeech();
  selectState('idle');
  if(chatHistory.at(-1)?.role==='user'&&chatHistory.at(-1).content===question)chatHistory.pop();
  chatStatus='Local Qwen unavailable: '+String(err.message||err).slice(0,230);
  updateChat();return {error:chatStatus};
 }
}
function clearChat(){
 cancelChat();chatHistory=[];stopSpeech();chatStatus='New session. Messages are kept only while this test app is running.';
 updateChat();return chatSnapshot();
}
async function speakKnownReply(text){
 if(typeof text!=='string'||!chatHistory.some(m=>m.role==='assistant'&&m.content===text))
   return {error:'Only actual replies in this session can be played.'};
 if(config.muted)return {error:'Voice is muted.'};
 const excerpt=speakableExcerpt(text);
 if(excerpt.wasTruncated)chatStatus='Playing first 900 characters only. Full text remains in the conversation.';
 updateChat();
 return speakText(excerpt.text,'daniel');
}
function createWindow(){
 const size=windowSize();
 const center=clampCenter(config.center||primaryStartPosition(),size);config.center=center;
 win=new BrowserWindow({x:Math.round(center.x-size/2),y:Math.round(center.y-size/2),width:size,height:size,
  minWidth:140,minHeight:140,show:false,frame:false,transparent:true,hasShadow:false,backgroundColor:'#00000000',resizable:false,fullscreenable:false,skipTaskbar:true,
  alwaysOnTop:config.alwaysOnTop,autoHideMenuBar:true,webPreferences:{preload:path.join(__dirname,'preload.js'),contextIsolation:true,nodeIntegration:false,sandbox:true,webSecurity:true}});
 // Avoid bouncing in taskbar and competing with the user's editor focus.
 win.setVisibleOnAllWorkspaces(false);
 win.setMenuBarVisibility(false);
 win.on('closed',()=>{win=null;ready=false});
 win.on('moved',()=>{positionChatBubble();if(!dragging){config.center=currentCenter();saveConfigSoon();}});
 win.webContents.on('did-finish-load',()=>{
  ready=true;
  send({type:'state',value:config.state});send({type:'size',value:config.expandedDiameter});send({type:'brightness',value:config.brightness});send({type:'motion',value:config.reducedMotion});
  win.showInactive();
  updateMenu();
 });
 win.webContents.on('render-process-gone',(_e,details)=>console.error('Renderer exit',details.reason));
 win.loadFile(path.join(__dirname,'renderer.html')).catch(err=>console.error('Loading renderer',err));
}
function createTray(){let icon=nativeImage.createFromPath(path.join(__dirname,'assets','orb-tray.png'));
 if(icon.isEmpty()){console.error('Could not load tray icon');icon=nativeImage.createEmpty()}
 tray=new Tray(icon.resize({width:20,height:20}));
 tray.setToolTip('Renegade Studio Living Core (developer preview)');
 tray.on('double-click',()=>{if(win.isVisible())win.hide();else win.showInactive()});
 tray.on('click',()=>{if(!win.isVisible())win.showInactive()});
 updateMenu();
}
function interactiveTick(){if(!win||win.isDestroyed()||!win.isVisible())return;
 if(dragging){const p=screen.getCursorScreenPoint();const dx=p.x-dragging.origin.x,dy=p.y-dragging.origin.y;
  win.setPosition(Math.round(dragging.bounds.x+dx),Math.round(dragging.bounds.y+dy),false);positionChatBubble();return;}
 const p=screen.getCursorScreenPoint(), b=win.getBounds();const x=p.x-(b.x+b.width/2),y=p.y-(b.y+b.height/2);
 const r=(config.state==='idle'?84:config.expandedDiameter)/2;
 const inOrb=(x*x+y*y)<=Math.pow(r*1.25,2)||(Math.abs(x)<110&&y>r+8&&y<r+38);
 // Transparent corners must not consume mouse input meant for the editor.
 if(hitIgnored===inOrb){hitIgnored=!inOrb;win.setIgnoreMouseEvents(hitIgnored,{forward:true});}
}
function repairMissingMonitor(){relocateIfOffscreen();positionChatBubble();}
app.on('ready',()=>{
 if (!hasSingleInstanceLock) return; // Do not race the primary app's browser profile.
 loadConfig();createWindow();createTray();openChatPanel(); // Qwen conversation starts text-only. Voice lab stays optional.
 warmDaniel.start().then(()=>broadcastVoiceStatus({phase:'ready',status:'Daniel fully loaded ('+warmDaniel.loadMs+' ms, one time). Press Speak.'})).catch(err=>{console.error('Daniel warmup failed:',err.message);broadcastVoiceStatus({phase:'error',status:'Daniel voice unavailable; choose Alan or a Windows fallback.'});});
 ipcMain.on('orb:renderer-ready',()=>{if(!ready) return;send({type:'state',value:config.state});send({type:'size',value:config.expandedDiameter});send({type:'brightness',value:config.brightness});send({type:'motion',value:config.reducedMotion})});
 ipcMain.on('orb:drag-start',()=>{if(!win)return;const p=screen.getCursorScreenPoint();dragging={origin:p,bounds:win.getBounds()};win.setIgnoreMouseEvents(false);hitIgnored=false;});
 ipcMain.on('orb:drag-end',()=>{if(!dragging||!win)return;const click=Math.hypot(screen.getCursorScreenPoint().x-dragging.origin.x,screen.getCursorScreenPoint().y-dragging.origin.y)<5;dragging=null;const c=clampCenter(currentCenter(),win.getBounds().width);const b=win.getBounds();win.setPosition(Math.round(c.x-b.width/2),Math.round(c.y-b.height/2),false);config.center=c;saveConfigSoon();if(click)toggleChatPanel()});
 ipcMain.on('orb:toggle-state',toggleState);
 ipcMain.on('orb:show-menu',()=>{if(win)Menu.buildFromTemplate(menuTemplate()).popup({window:win})});
 ipcMain.on('orb:audio-ended',(_event,requestId)=>{
  if(Number.isInteger(requestId)&&requestId!==speechGeneration)return;
  if(config.state==='idle')return;
  chatVoiceSession=null;speechAbort=null;selectState('idle');broadcastVoiceStatus({phase:'ready',status:'Speech finished. Ready for another sentence.'});
 });
 ipcMain.on('orb:audio-started',(_event,requestId)=>{
  if(Number.isInteger(requestId)&&requestId!==speechGeneration)return;
  selectState('speaking');broadcastVoiceStatus({phase:'speaking',status:'Speaking now. Audio drives the waveform.'});
 });
 ipcMain.on('orb:audio-failed',(_event,message,requestId)=>{
  if(Number.isInteger(requestId)&&requestId!==speechGeneration)return;
  stopSpeech();broadcastVoiceStatus({phase:'error',status:'Audio playback failed: '+String(message).slice(0,300)});
 });
 ipcMain.on('orb:performance',(_event,values)=>{if(Number.isFinite(values?.fps)&&Number.isFinite(values?.drawMs))lastPerformance={fps:values.fps,drawMs:values.drawMs};});
 const fromVoicePanel=e=>voiceWindow&&!voiceWindow.isDestroyed()&&e.sender===voiceWindow.webContents;
 ipcMain.handle('renegade:voice:settings',e=>{if(!fromVoicePanel(e))throw new Error('Unauthorized window');return voiceSnapshot()});
 ipcMain.handle('renegade:voice:speak',async(e,v)=>{if(!fromVoicePanel(e))throw new Error('Unauthorized window');return speakText(v?.text,v?.voice)});
 ipcMain.handle('renegade:voice:stop',e=>{if(!fromVoicePanel(e))throw new Error('Unauthorized window');return stopSpeech()});
 ipcMain.handle('renegade:voice:mute',(e,v)=>{if(!fromVoicePanel(e))throw new Error('Unauthorized window');return setMuted(Boolean(v))});
 ipcMain.handle('orb:conversation',e=>{
  if(!win||e.sender!==win.webContents)throw Error('Unauthorized window');
  return setConversation(!conversation.snapshot().enabled);
 });
 // Trust boundary: ONLY the detached local chat renderer can call these.
 const fromChat=e=>chatWindow&&!chatWindow.isDestroyed()&&e.sender===chatWindow.webContents;
 ipcMain.handle('renegade:chat:snapshot',e=>{if(!fromChat(e))throw Error('Unauthorized window');return chatSnapshot();});
 ipcMain.handle('renegade:chat:ask',(e,v)=>{if(!fromChat(e))throw Error('Unauthorized window');return askChat(v);});
 ipcMain.handle('renegade:chat:clear',e=>{if(!fromChat(e))throw Error('Unauthorized window');return clearChat();});
 ipcMain.handle('renegade:chat:cancel',e=>{if(!fromChat(e))throw Error('Unauthorized window');return cancelChat();});
 ipcMain.handle('renegade:chat:speak',(e,v)=>{if(!fromChat(e))throw Error('Unauthorized window');return speakKnownReply(v);});
 ipcMain.handle('renegade:chat:stop-voice',e=>{if(!fromChat(e))throw Error('Unauthorized window');return stopSpeech();});
 ipcMain.handle('renegade:chat:auto-speak',(e,v)=>{if(!fromChat(e))throw Error('Unauthorized window');config.chatAutoSpeak=Boolean(v);if(!config.chatAutoSpeak&&chatVoiceSession)stopSpeech();saveConfigSoon();updateChat();return chatSnapshot();});
 ipcMain.handle('renegade:chat:mute',(e,v)=>{if(!fromChat(e))throw Error('Unauthorized window');setMuted(Boolean(v));updateChat();return chatSnapshot();});
 ipcMain.handle('renegade:chat:minimise',e=>{if(!fromChat(e))throw Error('Unauthorized window');return hideChatPanel();});
 ipcMain.handle('renegade:chat:conversation',(e,enabled)=>{if(!fromChat(e))throw Error('Unauthorized window');return setConversation(Boolean(enabled));});
 ipcMain.handle('renegade:chat:window',(e,mode)=>{
  if(!fromChat(e))throw Error('Unauthorized window');
  if(!chatWindow||chatWindow.isDestroyed())throw Error('Chat window is unavailable');
  if(!['left','right','float'].includes(mode))throw Error('Invalid placement');
  positionChatBubble();
  return {ok:true,mode};
 });
 setInterval(interactiveTick,40).unref();
 setInterval(()=>{
  if(!ready||chatBusy||chatVoiceSession||['generating','starting','speaking'].includes(currentSpeechStatus.phase))return;
  const introduction=startupGuide.poll();
  if(!introduction)return;
  openChatPanel();
  chatHistory.push({role:'assistant',content:introduction,conversational:true,sources:[]});
  updateChat();
  if(config.chatAutoSpeak&&!config.muted)void speakText(introduction,'daniel').catch(err=>{chatStatus='Voice unavailable: '+err.message;updateChat()});
 },400).unref();
 screen.on('display-removed',repairMissingMonitor);
 screen.on('display-metrics-changed',repairMissingMonitor);
 screen.on('display-added',()=>{});
});
app.on('window-all-closed',()=>{}); // only tray Quit ends the companion
app.on('before-quit',()=>{quitting=true;conversation.stop();clearTimeout(bubbleHideTimer);clearTimeout(bubbleMoveTimer);if(chatAbort)chatAbort.abort();warmDaniel.stop();if(speechAbort)speechAbort.abort();clearTimeout(savePending);try{fs.mkdirSync(path.dirname(configFile()),{recursive:true});fs.writeFileSync(configFile(),JSON.stringify({...config,state:'idle'},null,2)+'\n');}catch(_){} });
