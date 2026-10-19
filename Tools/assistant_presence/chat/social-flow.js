'use strict';
// Social turns precede source routing; mixed editor requests are never swallowed.
const normalise=t=>String(t).toLowerCase().replace(/[’]/g,"'").replace(/[^a-z0-9'\s]/g,' ').replace(/\s+/g,' ').trim();
const {isEngineQuestion}=require('./verified-knowledge');
const {replies}=require('./personality');
const {readName}=require('./studio-profile');
const greeting=/^(?:hello|hi|hey|hiya|alright|all right|good morning|good afternoon|good evening)(?: there| mate| renegade| daniel)?$/;
function socialReply(history){
 const latest=history.at(-1)?.content||'',text=normalise(latest);
 const earlier=history.slice(0,-1).filter(m=>m.role==='user');
 if(isEngineQuestion(latest.replace(/\brenegade\b/ig,'')))return null;
 if(greeting.test(text)){
  const repeated=earlier.slice(-2).some(m=>greeting.test(normalise(m.content)));
  const name=readName();
  return repeated?replies.repeated:name?'Hello, '+name+'. Good to have you here.':replies.greeting;
 }
 if(/^(?:(?:hello|hi|hey|alright)(?: renegade| mate)? )?(?:how are you|how are you doing|how's it going|how are things)(?: today| renegade| mate)?$/.test(text))
  return replies.wellbeing;
 if(/^(?:i'm|i am) (?:good|fine|well|okay|ok|not bad)(?: thanks| thank you| cheers)?$/.test(text))
  return "Good to hear.";
 if(/^(?:thanks|thank you|cheers|ta)(?: very much| a lot| mate| renegade| for that| that's helpful)?$/.test(text))
  return replies.thanks;
 if(/^(?:great|brilliant|excellent|perfect|nice|that's better|that is better|that worked|that's working better)$/.test(text))
  return replies.positive;
 if(/^(?:bye|goodbye|see you|see you later|good night)(?: renegade| mate)?$/.test(text))
  return "See you later.";
 if(/^(?:can we|could we|i want to|i just want to) (?:just )?(?:chat|talk|have a conversation)(?: naturally| for a bit)?$/.test(text))
  return replies.chat;
 if(/\b(?:i was|i am|i'm) (?:just )?talking to you\b/.test(text)&&
    /\b(?:hello|hi|greeting|said|saying)\b/.test(text))
  return replies.correction;
 if(/^(?:i (?:was|am|'m) (?:just )?saying (?:hello|hi)|that was (?:just )?a greeting|i meant (?:hello|a greeting))$/.test(text))
  return "Got you. Hello!";
 const introduction=latest.trim().match(/^(?:my name is|call me) ([a-zA-Z][a-zA-Z -]{0,35})[.!]?$/i);
 if(introduction)return "Hi "+introduction[1].trim()+". I'll use that name in this conversation.";
 if(/^(?:what(?:'s| is) my name|do you remember my name|what did i ask you to call me|what should you call me)$/.test(text)){
  const named=[...earlier].reverse().map(m=>m.content.trim().match(/^(?:my name is|call me) ([a-zA-Z][a-zA-Z -]{0,35})[.!]?$/i)).find(Boolean);
  const name=readName()||named?.[1]?.trim();
  return name?"You asked me to call you "+name+".":"You haven't told me your name yet.";
 }
 return null;
}
module.exports={socialReply};
