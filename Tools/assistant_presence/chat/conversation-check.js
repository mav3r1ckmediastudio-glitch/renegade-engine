'use strict';
// Narrow conversational checks; engine questions still use verified knowledge.
function conversationCheck(text){
 const clean=String(text).toLowerCase().replace(/[^a-z0-9\s]/g,' ').replace(/\s+/g,' ').trim();
 if(/^(?:can you hear me|do you hear me|are you hearing me|can you hear what i am saying|mic check|microphone check|testing(?: testing)?(?: one two three| 1 2 3)?|test(?: test)?(?: one two three| 1 2 3)?)$/.test(clean))
  return "I received your message. If you said it aloud, your microphone worked for this turn.";
 return null;
}
module.exports={conversationCheck};
