'use strict';
const assert=require('node:assert/strict');
const {EventEmitter}=require('node:events');
const {VoiceConversation}=require('../voice/conversation');
const {bubbleLayout}=require('../chat/bubble-layout');
let launched=0,child,turns=[],updates=[];
function launch(){
 launched++;child=new EventEmitter();child.stdout=new EventEmitter();child.stdout.setEncoding=()=>{};
 child.stderr=new EventEmitter();child.stdin=new EventEmitter();child.commands=[];
 child.stdin.write=x=>child.commands.push(x);child.stdin.end=x=>child.commands.push(x);
 child.kill=()=>{child.killed=true};return child;
}
const c=new VoiceConversation({launch,onTurn:t=>turns.push(t),onUpdate:s=>updates.push(s)});
assert.equal(launched,0);assert(!c.snapshot().enabled);
c.start();assert.equal(launched,1);c.start();assert.equal(launched,1);
child.stdout.emit('data','{"type":"ready","language":"en-GB"}\n');
c.setListening(true);const epoch=c.epoch;
c.receive({type:'listening',epoch});
c.receive({type:'result',epoch,text:'How do I import a character?',confidence:.9});
assert.deepEqual(turns,['How do I import a character?']);assert(!c.listening);
c.receive({type:'result',epoch,text:'Daniel echoed answer',confidence:.9});
assert.equal(turns.length,1,'reply echo/stale result rejected');
c.setListening(true);c.receive({type:'result',epoch:c.epoch,text:'Uncertain words',confidence:.2});
assert.equal(turns.length,1);assert.equal(c.snapshot().draft,'Uncertain words');assert(!c.listening);
c.clearDraft();c.setListening(true);const oldEpoch=c.epoch,oldChild=child;c.stop();
assert(oldChild.killed);assert(!c.snapshot().enabled);
c.receive({type:'result',epoch:oldEpoch,text:'After stop',confidence:.99});assert.equal(turns.length,1);
c.start();c.receive({type:'ready',language:'en-GB'});c.setListening(true);
oldChild.stdout.emit('data','{"type":"result","epoch":'+c.epoch+',"text":"Old worker","confidence":1}\n');
assert.equal(turns.length,1,'previous worker cannot submit');
c.receive({type:'error',message:'No microphone'});assert(!c.snapshot().enabled);assert.equal(c.snapshot().phase,'error');
const small={x:0,y:0,width:800,height:600};
for(const work of [small,{x:-1920,y:0,width:1920,height:1080},{x:0,y:-1080,width:1920,height:1080}]){
 for(const center of [{x:work.x+200,y:work.y+200},{x:work.x+work.width-200,y:work.y+work.height-200}]){
  const {bounds:b,tail}=bubbleLayout(center,work,null);
  assert(b.x>=work.x&&b.y>=work.y&&b.x+b.width<=work.x+work.width&&b.y+b.height<=work.y+work.height);
  assert(tail.x>=28&&tail.x<=b.width-28);
 }
}
console.log('PASS: explicit voice opt-in/off, continuous turn, echo/stale rejection, uncertain transcript review, worker failure cleanup, multi-monitor bubble containment');
