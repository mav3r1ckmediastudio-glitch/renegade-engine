'use strict';
const {spawn}=require('node:child_process');
const path=require('node:path');
// Only start() can create the microphone worker. No persisted enabled setting.
class VoiceConversation {
 constructor({launch=spawn,onUpdate=()=>{},onTurn=()=>{}}={}){
  this.launch=launch;this.onUpdate=onUpdate;this.onTurn=onTurn;
  this.child=null;this.epoch=0;this.listening=false;this.ready=false;
  this.state={enabled:false,phase:'off',status:'Microphone off',draft:''};
 }
 snapshot(){return {...this.state}}
 update(values){Object.assign(this.state,values);this.onUpdate(this.snapshot())}
 start(){
  if(this.child)return this.snapshot();
  this.update({enabled:true,phase:'starting',status:'Starting local voice conversation...',draft:''});
  let child;
  try{child=this.launch(process.env.RENEGADE_STT_PYTHON||path.join(__dirname,'..','..','..','BUILD','assistant-stt-venv','Scripts','python.exe'),['-u',path.join(__dirname,'conversation_worker.py')],{windowsHide:true,stdio:['pipe','pipe','pipe']});}
  catch(e){this.fail(e.message);return this.snapshot()}
  this.child=child;this.ready=false;this.listening=false;
  let buffer='';
  const boot=setTimeout(()=>{if(this.child===child&&!this.ready)this.fail('Speech recogniser startup timed out.');},15000);boot.unref?.();
  child.stdout.setEncoding('utf8');child.stdout.on('data',chunk=>{
   if(this.child!==child)return;
   buffer+=chunk;if(buffer.length>65536){this.fail('Invalid speech worker output.');return}
   let pos;while((pos=buffer.indexOf('\n'))>=0){const line=buffer.slice(0,pos).trim();buffer=buffer.slice(pos+1);
    if(!line)continue;try{this.receive(JSON.parse(line))}catch(_){this.fail('Invalid speech worker response.');break}
   }
  });
  child.stderr.on('data',()=>{}); // Do not log spoken content.
  child.stdin.on('error',()=>{if(this.child===child)this.fail('Speech input connection closed.')});
  child.on('error',err=>{clearTimeout(boot);if(this.child===child)this.fail(err.message)});
  child.on('exit',()=>{clearTimeout(boot);if(this.child===child)this.fail('Voice conversation stopped unexpectedly.')});
  return this.snapshot();
 }
 receive(m){
  if(m.type==='error'){this.fail(String(m.message||'Speech recognition failed.').slice(0,220));return}
  if(m.type==='ready'){this.ready=true;this.update({phase:'paused',status:'Local voice ready ('+m.language+').'});return}
  if(m.type==='listening'&&this.listening&&m.epoch===this.epoch){
   this.update({phase:'listening',status:'Listening'+(m.device?' on '+m.device:'')+' - speak naturally.'});return;
  }
  if(!this.state.enabled||!this.listening||m.epoch!==this.epoch)return;
  if(m.type==='capture'){
   this.update({phase:m.voiced?'hearing':'listening',status:m.voiced?'Hearing speech...':'Listening - waiting for speech.'});return;
  }
  if(m.type==='transcribing'){
   this.update({phase:'transcribing',status:'Turning speech into text...'});return;
  }
  if(m.type==='uncertain'){this.setListening(false);this.update({status:"I didn't catch that. Please try again."});return}
  if(m.type!=='result'||typeof m.text!=='string'||!m.text.trim()||m.text.length>900)return;
  if(!Number.isFinite(m.confidence)||m.confidence<.55){
   this.setListening(false);
   this.update({phase:'review',draft:m.text.trim(),status:'Please check the words I heard before sending.'});
   return;
  }
  this.setListening(false);
  void this.onTurn(m.text.trim());
  this.update({draft:'',phase:'answering',transcriptionMs:m.elapsedMs??null,status:'Answering'+(Number.isFinite(m.elapsedMs)?' · speech recognised in '+(m.elapsedMs/1000).toFixed(2)+' s':'')+'...'});
 }
 setListening(enabled){
  enabled=Boolean(enabled&&this.ready&&this.state.enabled&&this.child);
  if(enabled===this.listening)return;
  this.listening=enabled;this.epoch++;
  if(this.child)this.child.stdin.write(enabled?'listen '+this.epoch+'\n':'pause\n');
  if(!enabled&&this.state.enabled)Object.assign(this.state,{phase:'paused',status:'Listening paused while Renegade replies.'});
 }
 clearDraft(){this.update({draft:'',phase:'paused'})}
 stop(){
  const child=this.child;this.child=null;this.ready=false;this.listening=false;this.epoch++;
  if(child){child.stdin.end('quit\n');child.kill();}
  this.update({enabled:false,phase:'off',status:'Microphone off',draft:''});
  return this.snapshot();
 }
 fail(message){this.stop();this.update({phase:'error',status:'Voice unavailable: '+message})}
}
module.exports={VoiceConversation};
