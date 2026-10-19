'use strict';
// Conservative sentence-level incremental speech plan.
// Keep punctuation attached, preserve original wording and prefer sentence boundaries.
// No LLM rewriting or SSML; this is audio transport only.
const segmenter=new Intl.Segmenter('en-GB',{granularity:'sentence'});
function splitSpeech(text,{maxChars=200,targetChars=145}={}){
 if(typeof text!=='string'||!text.trim()||text.trim().length>900)throw new Error('Speech must contain 1-900 characters.');
 const sentences=[...segmenter.segment(text.trim())].map(x=>x.segment.replace(/\s+/g,' ').trim()).filter(Boolean);
 const result=[];let current='';
 function emit(){if(current){result.push(current);current=''}}
 for(const sentence of sentences){
  if(sentence.length>maxChars){
    emit();let tail=sentence;
    while(tail.length>maxChars){
      // Break overly long technical sentences near a clause boundary or space.
      const prefix=tail.slice(0,maxChars+1);
      let cut=Math.max(prefix.lastIndexOf('; '),prefix.lastIndexOf(', ')+1);
      if(cut<65)cut=prefix.lastIndexOf(' ');
      if(cut<30)cut=maxChars;
      const fragment=tail.slice(0,cut).trim();
      if(fragment)result.push(fragment);
      tail=tail.slice(cut).trim();
    }
    if(tail)current=tail;
    continue;
  }
  if(current && current.length+1+sentence.length>targetChars)emit();
  current=current?current+' '+sentence:sentence;
 }
 emit();
 if(result.length>20)throw new Error('Too many speech segments.');
 return result;
}
module.exports={splitSpeech};
