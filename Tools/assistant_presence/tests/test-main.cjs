'use strict';
const assert=require('node:assert/strict');
const {EventEmitter}=require('node:events'),Module=require('node:module'),os=require('node:os'),fs=require('node:fs'),path=require('node:path');
const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'renegade-qwen-chat-test-'));
class FakeWindow extends EventEmitter{
 constructor(opts){super();this.opts=opts;this.bounds={x:opts.x||250,y:opts.y||180,width:opts.width,height:opts.height};this.visible=false;this.ignored=false;
 this.calls=[];this.webContents=new EventEmitter();this.webContents.send=(...args)=>this.calls.push(args);}
 setVisibleOnAllWorkspaces(){}setMenuBarVisibility(){}setAlwaysOnTop(){}focus(){this.focused=true}
 loadFile(p){assert(fs.existsSync(p),'missing '+p);setImmediate(()=>this.webContents.emit('did-finish-load'));return Promise.resolve()}
 getBounds(){return {...this.bounds}}setBounds(v){this.bounds={...v};this.emit('moved')}
 setPosition(x,y){this.bounds.x=x;this.bounds.y=y;this.emit('moved')}
 show(){this.visible=true}showInactive(){this.visible=true}hide(){this.visible=false}
 isVisible(){return this.visible}isDestroyed(){return false}setIgnoreMouseEvents(v){this.ignored=v}
}
class FakeTray extends EventEmitter{setToolTip(){}setContextMenu(){}isDestroyed(){return false}}
const app=new EventEmitter();app.getPath=()=>tmp;app.setAppUserModelId=()=>{};app.commandLine={appendSwitch:()=>{}};
app.requestSingleInstanceLock=()=>true;app.quit=()=>{};
const ipcMain=new EventEmitter(),windows=[];ipcMain.handles=new Map;ipcMain.handle=(k,fn)=>ipcMain.handles.set(k,fn);
function BrowserWindow(opts){const w=new FakeWindow(opts);windows.push(w);return w}
let micChild=null,micLaunches=0;
function fakeMicLaunch(){
 micLaunches++;const child=new EventEmitter();micChild=child;
 child.stdout=new EventEmitter();child.stdout.setEncoding=()=>{};
 child.stderr=new EventEmitter();child.stdin=new EventEmitter();child.commands=[];
 child.stdin.write=command=>{
  child.commands.push(command);
  if(command.startsWith('listen ')){const epoch=Number(command.trim().split(' ')[1]);setImmediate(()=>child.stdout.emit('data',JSON.stringify({type:'listening',epoch})+'\n'));}
 };
 child.stdin.end=()=>{};child.kill=()=>{child.killed=true};
 setImmediate(()=>child.stdout.emit('data',JSON.stringify({type:'ready',language:'en'})+'\n'));
 return child;
}
let cursor={x:1700,y:930};
const areas=[{x:0,y:0,width:1920,height:1080},{x:-1920,y:0,width:1920,height:1080}];
const screen=new EventEmitter();screen.getPrimaryDisplay=()=>({workArea:areas[0]});screen.getDisplayNearestPoint=({x})=>({workArea:x<0?areas[1]:areas[0]});screen.getCursorScreenPoint=()=>cursor;
const fake={app,BrowserWindow,ipcMain,Tray:FakeTray,Menu:{buildFromTemplate:tpl=>({tpl})},
 nativeImage:{createFromPath:()=>({isEmpty:()=>false,resize:()=>({isEmpty:()=>false})})},screen,dialog:{}};
const old=Module._load;Module._load=function(name,...rest){
 if(name==='electron')return fake;
 if(name==='./voice/conversation'){
  const {VoiceConversation}=old.call(this,name,...rest);
  return {VoiceConversation:class extends VoiceConversation{constructor(opts){super({...opts,launch:fakeMicLaunch})}}};
 }
 if(name==='./voice/warm-daniel')return{WarmDaniel:class{start(){return Promise.resolve(this)}stop(){}generate(){return Promise.resolve({data:Buffer.from('test-wav'),elapsedMs:1,engine:'test'})}}};
 if(name==='./chat/model-client')return{ speakableExcerpt:text=>({text,wasTruncated:false}) };
 if(name==='./chat/stream-client')return{askQwenStream:async (_history,{onDelta})=>{
   onDelta('Renegade is a game ');
   await new Promise(r=>setTimeout(r,35));
   onDelta('development engine, not a game.');
   return {answer:'Renegade is a game development engine, not a game.',adapter:'v31',policy:null,elapsedMs:40,firstTokenMs:2,streaming:true};
 }};
 return old.call(this,name,...rest);
};
require('../main.js');app.emit('ready');
(async()=>{
 await new Promise(r=>setTimeout(r,130));
 assert.equal(windows.length,2,'orb plus detached chat');
 const [orb,chat]=windows;
 assert(orb.visible&&chat.visible);
 assert(orb.opts.transparent&&!orb.opts.frame&&!orb.opts.webPreferences.nodeIntegration);
 assert(!chat.opts.webPreferences.nodeIntegration&&chat.opts.webPreferences.contextIsolation);
 const initial=orb.getBounds();
 ipcMain.emit('orb:toggle-state');assert.deepEqual(orb.getBounds(),initial);
 ipcMain.emit('orb:toggle-state');assert.deepEqual(orb.getBounds(),initial);
 const caller={sender:chat.webContents};
 const get=k=>ipcMain.handles.get('renegade:chat:'+k);
 assert.throws(()=>get('snapshot')({sender:{}}),/Unauthorized/);
 let v=get('snapshot')(caller);
 assert(v.autoSpeak&&!v.muted&&!v.busy&&!v.history.length&&!v.conversation.enabled,'Daniel output on; microphone off by default');
 get('auto-speak')(caller,false);
 ipcMain.emit('orb:audio-started',{},null);
 const beforeMinimise=orb.calls.length;
 get('minimise')(caller);
 await new Promise(r=>setTimeout(r,210));
 assert(!chat.visible,'bubble minimises');
 assert(!orb.calls.slice(beforeMinimise).some(c=>c[1]?.type==='speech-stop'||c[1]?.type==='audio-stop'),'minimise preserves playback');
 ipcMain.emit('orb:toggle-state');assert(chat.visible,'orb reopens bubble');
 ipcMain.emit('orb:audio-ended',{},null);
 const invalid=await get('ask')(caller,' '.repeat(2));assert(invalid.error);
 const pending=get('ask')(caller,'What is Renegade?');
 await new Promise(r=>setTimeout(r,12));
 const partial=get('snapshot')(caller);assert(partial.busy&&partial.partial.startsWith('Renegade is a game'), 'live text appears before model finishes');
 const r=await pending;assert(r.ok,'local streamed model response');
 v=get('snapshot')(caller);
 assert.equal(v.history.length,2);assert.equal(v.history[0].role,'user');assert.equal(v.history[1].role,'assistant');
 assert(v.history[1].content.includes('game development engine'));
 const denied=await get('speak')(caller,'Fake assistant reply');assert(denied.error,'cannot speak fabricated reply');
 get('auto-speak')(caller,true);v=get('snapshot')(caller);assert(v.autoSpeak);
 get('mute')(caller,true);v=get('snapshot')(caller);assert(v.muted);
 get('mute')(caller,false);assert(!get('snapshot')(caller).muted);
 assert.equal(micLaunches,0,'typing never opens microphone');
 assert.throws(()=>get('conversation')({sender:{}},true),/Unauthorized/);
 get('conversation')(caller,true);
 await new Promise(r=>setTimeout(r,30));
 v=get('snapshot')(caller);assert(v.conversation.enabled&&v.conversation.phase==='listening');
 const epoch=Number(micChild.commands.at(-1).trim().split(' ')[1]);
 micChild.stdout.emit('data',JSON.stringify({type:'result',epoch,text:'What is Renegade?',confidence:.99})+'\n');
 await new Promise(r=>setTimeout(r,100));
 assert.equal(get('snapshot')(caller).history.length,4,'voice turn uses same conversation');
 assert.equal(micChild.commands.at(-1),'pause\n','microphone pauses for Daniel');
 ipcMain.emit('orb:audio-started',{},null);
 get('minimise')(caller);await new Promise(r=>setTimeout(r,200));
 assert(get('snapshot')(caller).conversation.enabled,'minimising keeps voice mode enabled');
 assert.equal(micChild.commands.at(-1),'pause\n','hidden bubble cannot activate capture during playback');
 ipcMain.emit('orb:audio-ended',{},null);
 await new Promise(r=>setTimeout(r,30));
 assert(micChild.commands.at(-1).startsWith('listen '),'listening resumes after Daniel finishes');
 get('conversation')(caller,false);
 assert(micChild.killed&&!get('snapshot')(caller).conversation.enabled,'End voice releases microphone worker');
 ipcMain.emit('orb:toggle-state');
 get('clear')(caller);v=get('snapshot')(caller);assert.equal(v.history.length,0);
 // Cross-screen dragging preserves native V1 window shape.
 const oldBubble=chat.getBounds();
 cursor={x:1700,y:930};ipcMain.emit('orb:drag-start');cursor={x:-1720,y:600};
 await new Promise(r=>setTimeout(r,90));assert.notDeepEqual(chat.getBounds(),oldBubble,'bubble follows during drag before release');assert(chat.bounds.x<0,'bubble follows orb to other monitor');ipcMain.emit('orb:drag-end');
 assert(orb.bounds.x<0&&orb.bounds.x>=-1920,'cross-monitor');
 cursor={x:orb.bounds.x+2,y:orb.bounds.y+2};await new Promise(r=>setTimeout(r,90));assert(orb.ignored);
 cursor={x:orb.bounds.x+orb.bounds.width/2,y:orb.bounds.y+orb.bounds.height/2};
 await new Promise(r=>setTimeout(r,90));assert(!orb.ignored);
 console.log('PASS: V1 geometry, detached read-only chat, progressive Qwen text, history, IPC guard, opt-in speech, mute/clear, cross-screen drag');
 process.exit(0);
})().catch(e=>{console.error(e);process.exit(1)});
