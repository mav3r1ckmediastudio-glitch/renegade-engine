'use strict';
const fs=require('node:fs'),path=require('node:path');
function preferencesPath(env=process.env){
 if(env.RENEGADE_PROFILE_PATH)return env.RENEGADE_PROFILE_PATH;
 return env.LOCALAPPDATA?path.join(env.LOCALAPPDATA,'RenegadeStudio','preferences.cfg'):null;
}
function normaliseName(value){
 if(typeof value!=='string')return null;
 const name=value.trim();
 return name&&name.length<=32&&!/[\x00-\x1f\x7f]/.test(name)?name:null;
}
function parseName(text){
 for(const line of String(text).split(/\r?\n/))
  if(line.startsWith('developer_identity='))return normaliseName(line.slice(19));
 return null;
}
function readName(file=preferencesPath()){
 if(!file)return null;
 try{
  const bytes=fs.readFileSync(file);
  if(bytes.length>65536)return null;
  return parseName(new TextDecoder('utf-8',{fatal:true}).decode(bytes));
 }catch{return null}
}
function saveName(value,file=preferencesPath()){
 const name=normaliseName(value);if(!name||!file)throw Error('Enter a name of 1 to 32 characters.');
 fs.mkdirSync(path.dirname(file),{recursive:true});
 let existing='';try{existing=fs.readFileSync(file,'utf8')}catch(e){if(e.code!=='ENOENT')throw e}
 const lines=existing.split(/\r?\n/).filter(l=>l&&!l.startsWith('developer_identity='));
 if(!lines.some(l=>l.startsWith('version=')))lines.unshift('version=1');
 lines.push('developer_identity='+name);
 const temporary=file+'.orb.tmp';fs.writeFileSync(temporary,lines.join('\n')+'\n','utf8');
 fs.renameSync(temporary,file);
 return name;
}
module.exports={preferencesPath,normaliseName,parseName,readName,saveName};
