'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const p=require('../chat/studio-profile'),{StartupGuide,readRequest}=require('../chat/startup-guide');
const root=fs.mkdtempSync(path.join(os.tmpdir(),'renegade-profile-')),file=path.join(root,'preferences.cfg');
try{
 assert.equal(p.readName(file),null);
 fs.writeFileSync(file,'version=1\nother=retained\n');
 assert.equal(p.saveName('  Mav3r1ck  ',file),'Mav3r1ck');
 assert.equal(p.readName(file),'Mav3r1ck');assert.match(fs.readFileSync(file,'utf8'),/other=retained/);
 assert.equal(p.saveName("Zoë O'Neil",file),"Zoë O'Neil");
 for(const s of ['', 'x'.repeat(33),'bad\nname'])assert.throws(()=>p.saveName(s,file));
 fs.writeFileSync(file,'version=1\ndeveloper_identity=\n');assert.equal(p.readName(file),null);
 let request={stage:'name',pid:1,key:'1:name'},saved=null,writes=0;
 const g=new StartupGuide({read:()=>request,name:()=>saved,save:n=>{writes++;saved=n;return n}});
 assert.match(g.poll(),/I'm Renegade/);assert.equal(g.poll(),null);
 assert.match(g.answer('How do I import?'),/name/);assert.equal(writes,0);
 assert.match(g.answer('Call me Zoë'),/Is that right/);assert.equal(writes,0);
 assert.match(g.answer('no'),/prefer/);assert.equal(writes,0);
 g.answer("Zoë O'Neil");assert.match(g.answer('yes'),/saved/);assert.equal(writes,1);
 request={stage:'handshake',pid:1,key:'1:handshake'};assert.match(g.poll(),/ENTER HUB/);
 assert.equal(g.answer('yes'),null);request=null;assert.equal(g.poll(),null);
 const marker=path.join(root,'request.cfg');fs.writeFileSync(marker,'stage=name\npid='+process.pid+'\n');
 assert.equal(readRequest(marker).stage,'name');
 fs.utimesSync(marker,new Date(0),new Date(0));assert.equal(readRequest(marker),null);
 console.log('PASS profile save/reopen, Unicode, invalid names, preserved settings, confirmation, native handshake handoff, stale request rejection');
}finally{fs.rmSync(root,{recursive:true,force:true})}
