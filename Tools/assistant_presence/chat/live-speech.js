'use strict';
const {tidyText}=require('./format-text');
// Real-time text -> queued Daniel WAVs. The approved V1 orb renderer stays
// untouched. The same WebAudio clock schedules chunks without overlay jumps.
function cleanForVoice(input){
 // Preserve ordinary wording but prevent illegal Python Unicode on ALL
 // synthesis paths, including mid-token speech segments and manual Listen.
 return tidyText(input)
  .replace(/\uFFFD/g,'')
  .replace(/[\uD800-\uDFFF]/g,'')
  .replace(/[\u0000-\u001f\u007f]/g,' ')
  .replace(/https?:\/\/\S+/g,'a reference link')
  .replace(/[*#`_]/g,'')
  .replace(/\s+/g,' ').trim();
}
class LiveSpeech {
 constructor({requestId,signal,generate,send,valid,onError=()=>{}}){
  this.id=requestId;this.signal=signal;this.generate=generate;this.send=send;this.valid=valid;
  this.onError=onError;this.pending='';this.parts=0;this.open=false;this.finished=false;this.cancelled=false;
  this.tail=Promise.resolve();this.fullyStopped=false;this.totalChars=0;
 }
 active(){return !this.cancelled&&!this.signal?.aborted&&this.valid();}
 deliver(chunk){
  if(!this.active()||typeof chunk!=='string')return;
  this.pending+=chunk;
  // Feed complete sentences promptly, or long clauses when we cannot wait for
  // full punctuation (e.g. model outputs a >100-character sentence).
  while(this.pending.length){
   let cut=-1;
   const punct=this.pending.match(/[.!?](?:\s|$)/);
   if(punct&&punct.index<=180){cut=punct.index+1;}
   // First audible clause should start early. For all later segments prefer
   // longer natural phrases to avoid robotic cadence at every short fragment.
   if(cut<0&&this.parts===0&&this.pending.length>=46){
    const candidate=this.pending.slice(0,78);
    const clause=Math.max(candidate.lastIndexOf(', '),candidate.lastIndexOf('; '),candidate.lastIndexOf(': '));
    if(clause>=33)cut=clause+1;
    else if(this.pending.length>=62)cut=candidate.lastIndexOf(' ');
   }
   if(cut<0&&this.pending.length>=110){
    const candidate=this.pending.slice(0,170);
    const clause=Math.max(candidate.lastIndexOf(', '),candidate.lastIndexOf('; '),candidate.lastIndexOf(': '));
    if(clause>=70)cut=clause+1;
    else if(this.pending.length>=170)cut=candidate.lastIndexOf(' ');
   }
   if(cut<1)break;
   const segment=cleanForVoice(this.pending.slice(0,cut));
   this.pending=this.pending.slice(cut).trimStart();
   this.enqueue(segment);
  }
 }
 enqueue(text){
  if(!text||!this.active()||this.parts>=25)return;
  const remaining=3500-this.totalChars;
  if(remaining<=0)return;
  text=text.slice(0,remaining);
  this.totalChars+=text.length;
  const index=this.parts++;
  if(!this.open){
   this.send({type:'speech-begin',requestId:this.id,total:1});
   this.open=true;
  }
  this.tail=this.tail.then(async()=>{
   if(!this.active())return;
   const wav=await this.generate(text,this.signal);
   if(!this.active())return;
   this.send({type:'speech-chunk',requestId:this.id,index,
    dataUrl:'data:audio/wav;base64,'+wav.data.toString('base64')});
  }).catch(err=>{
   if(!this.active())return;
   this.cancelled=true;
   this.send({type:'speech-stop'});
   this.onError(err);
  });
 }
 async finish(){
  if(this.finished)return;
  this.finished=true;
  if(this.active()&&this.pending.trim())this.enqueue(cleanForVoice(this.pending));
  this.pending='';
  await this.tail;
  if(this.active()&&this.open)this.send({type:'speech-finish',requestId:this.id});
 }
 cancel(){
  this.cancelled=true;this.pending='';
 }
}
module.exports={LiveSpeech,cleanForVoice};
