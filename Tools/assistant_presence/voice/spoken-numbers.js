'use strict';
// Speech-only expansion. Keep names, version identifiers and displayed text intact.
const small=['zero','one','two','three','four','five','six','seven','eight','nine','ten','eleven','twelve','thirteen','fourteen','fifteen','sixteen','seventeen','eighteen','nineteen'];
const tens=['','','twenty','thirty','forty','fifty','sixty','seventy','eighty','ninety'];
function integerWords(n){
 if(n<20)return small[n];
 if(n<100)return tens[Math.floor(n/10)]+(n%10?'-'+small[n%10]:'');
 if(n<1000)return small[Math.floor(n/100)]+' hundred'+(n%100?' and '+integerWords(n%100):'');
 for(const [value,name] of [[1e9,'billion'],[1e6,'million'],[1000,'thousand']])
  if(n>=value){const remainder=n%value;return integerWords(Math.floor(n/value))+' '+name+(remainder?(remainder<100?' and ':' ')+integerWords(remainder):'');}
}
function numberWords(raw){
 const negative=raw.startsWith('-');const clean=raw.replace(/^-|,/g,'');const [whole,fraction]=clean.split('.');
 const n=Number(whole);
 const words=(whole.length>1&&whole.startsWith('0'))||!Number.isSafeInteger(n)||n>=1e12
  ? [...whole].map(d=>small[Number(d)]).join(' '):integerWords(n);
 return (negative?'minus ':'')+words+(fraction!==undefined?' point '+[...fraction].map(d=>small[Number(d)]).join(' '):'');
}
function spokenNumbers(text){
 // Protect identifiers and links before interpreting any standalone quantity.
 const protectedParts=[];
 const protect=s=>{protectedParts.push(s);return '\uE000'+String.fromCharCode(0xE100+protectedParts.length-1)+'\uE001';};
 let value=String(text).replace(/https?:\/\/\S+|\b[\p{L}_][\p{L}\p{N}_.-]*\d[\p{L}\p{N}_.-]*\b/gu,protect);
 const units={'m/s':'metres per second','cm':'centimetres','mm':'millimetres','m':'metres','ms':'milliseconds','s':'seconds','fps':'frames per second','Hz':'hertz','%':'per cent','°':'degrees'};
 value=value.replace(/(?<![\p{L}\p{N}_.])(-?\d+(?:,\d{3})*(?:\.\d+)?)\s*(m\/s|cm|mm|ms|fps|Hz|m|s|%|°)(?![\p{L}\p{N}_])/gu,(_,n,u)=>{
  let unit=units[u];if(Number(n.replace(/,/g,''))===1&&!['m/s','fps','Hz','%'].includes(u))unit=unit.replace(/s$/,'');
  return numberWords(n)+' '+unit;
 });
 value=value.replace(/(?<![\p{L}\p{N}_.])(-?\d+(?:,\d{3})*(?:\.\d+)?)(?![\p{L}\p{N}_]|\.\d)/gu,(_,n)=>numberWords(n));
 return value.replace(/\uE000([\uE100-\uEFFF])\uE001/g,(_,id)=>protectedParts[id.charCodeAt(0)-0xE100]);
}
module.exports={spokenNumbers,numberWords};
