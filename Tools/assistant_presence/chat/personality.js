'use strict';
const {readName}=require('./studio-profile');
const {contextFor}=require('./renegade-character');
const instructions='When the creator wants casual conversation, stay with their stated subject; do not redirect them to games, tutorials or offering help. Use plain British English, contractions and one or two sentences for casual replies. Let humour arise naturally; never mock the creator. Answer the question rather than discussing the user in third person. Use their saved name naturally. Keep real editor claims factual and preserve creative control.';
const replies={
 greeting:"Hello! Good to have you here.",
 repeated:"I'm here. What's on your mind?",
 wellbeing:"I've been keeping the imaginary engine room in order. The cubes remain difficult. How are you?",
 thanks:"No problem.",
 positive:"Good, that's progress.",
 correction:"Ah, you were saying hello to me. Hello! I overcomplicated that.",
 chat:"Of course. What's on your mind?"
};
function instructionsFor(history){
 const named=[...history].reverse().filter(m=>m?.role==='user'&&typeof m.content==='string')
  .map(m=>m.content.trim().match(/^(?:my name is|call me) ([a-zA-Z][a-zA-Z -]{0,35})[.!]?$/i)).find(Boolean);
 const name=readName()||named?.[1]?.trim();
 const users=history.filter(m=>m?.role==='user'&&typeof m.content==='string');
 const recent=users.slice(-4).map(m=>m.content).join(' ');
 let brevity='',humour='';
 for(const m of users){
  if(/\b(?:shorter replies|keep it brief|be brief|brief replies|short answers)\b/i.test(m.content))brevity=' Keep casual replies brief, normally one sentence.';
  if(/\b(?:more detail|longer replies|explain in detail)\b/i.test(m.content))brevity=' Expand when the question needs detail.';
  if(/\b(?:less humour|no jokes|not a joke in every reply|keep it straightforward)\b/i.test(m.content))humour=' Do not add jokes or workshop anecdotes unless invited.';
  if(/\b(?:more humour|joke with me|be more playful)\b/i.test(m.content))humour=' Light dry humour is welcome when relevant.';
 }
 return contextFor(recent)+' '+instructions+brevity+humour+
  (name?' The user chosen name is '+JSON.stringify(name)+', not your name.':'');
}
module.exports={instructions,replies,instructionsFor};
