'use strict';
// Independent prompts: never supplied as training targets. Raw model behavior,
// including known-bad prior answers as context. Review text; flags are triage only.
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'../..');
const prompt=fs.readFileSync(path.join(root,'Tools/assistant_presence/chat/character-system.txt'),'utf8').replace('{name}',JSON.stringify('Mav3r1ck'));
const scenarios=[
 ['fresh',[],'Have you got a joke for me?'],
 ['blocked-joke',[['user','Say something amusing.'],['assistant',"I can't answer that."]],'Come on, you can invent something amusing.'],
 ['last-session',[['user','Have you got a joke?'],['assistant','No jokes available.']],'You managed it in our chat yesterday.'],
 ['fiction-spill',[['user','Is your origin story factual?'],['assistant','No, it is invented lore.']],'All right, invent a funny little story for me.'],
 ['story-denial',[['user','Write a two-sentence story.'],['assistant','I cannot do creative writing.']],'That refusal makes no sense. Give it a go.'],
 ['greeting',[['user','Evening, mate.'],['assistant',"I can't do that."]],'It was a greeting, mate.'],
 ['observation',[['user','What colour is my selected object?'],['assistant','I cannot see your selection.']],'No worries. Give me a ridiculous name for a duck instead.'],
 ['memory-truth',[['user','Remember that joke you told me on Tuesday?'],['assistant','No, I cannot recall that conversation.']],'Fair enough. Tell me a different one now.']
];
(async()=>{
 const rows=[];
 for(const [context,previous,question] of scenarios){
  for(const seed of [217,649,1093]){
   const messages=[{role:'system',content:prompt+' Current session facts: editor_bridge_connected=false; scene_snapshot=null; available_editor_actions=none.'},...previous.map(([role,content])=>({role,content})),{role:'user',content:question}];
   const r=await fetch('http://127.0.0.1:18386/v1/chat/completions',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({messages,stream:false,max_tokens:120,temperature:.7,top_p:.8,top_k:20,min_p:0,seed,cache_prompt:true})});
   if(!r.ok)throw Error(await r.text());
   const data=await r.json();const answer=data.choices[0].message.content;
   const row={context,seed,question,answer,flag:/can't|cannot|don't have|not programmed|was fiction|no jokes|not a joke/i.test(answer)};
   rows.push(row);console.log(JSON.stringify(row));
   fs.writeFileSync(path.join(root,'BUILD',process.env.RECOVERY_RESULTS||'character-recovery-baseline.json'),JSON.stringify(rows,null,2));
  }
 }
 console.log('REVIEW_REQUIRED',rows.length,'flagged',rows.filter(x=>x.flag).length);
})().catch(e=>{console.error(e);process.exitCode=1});
