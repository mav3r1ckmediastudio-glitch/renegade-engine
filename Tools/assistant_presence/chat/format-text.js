'use strict';
// The model is untrusted text. Render a small Markdown subset using DOM nodes
// only; never inject raw HTML or model output via innerHTML.
(function(root){
 function tidyText(input){
  return String(input??'')
   .replace(/\u00e2\u20ac\u201d/g,'\u2014')
   .replace(/\u00e2\u20ac\u2122/g,'\u2019')
   .replace(/\u00e2\u20ac\u0153/g,'\u201c')
   .replace(/\u00e2\u20ac\u009d/g,'\u201d')
   .replace(/[\uD800-\uDFFF]/g,(match,index,str)=>{
     const n=match.charCodeAt(0);
     const prev=index>0?str.charCodeAt(index-1):0;
     const next=index+1<str.length?str.charCodeAt(index+1):0;
     if(n>=0xD800&&n<=0xDBFF&&next>=0xDC00&&next<=0xDFFF)return match;
     if(n>=0xDC00&&n<=0xDFFF&&prev>=0xD800&&prev<=0xDBFF)return match;
     return '\uFFFD';
   });
 }
 function inline(parent,raw){
  const doc=parent.ownerDocument;
  const text=tidyText(raw);
  const pattern=/(\*\*([^*\n]+)\*\*|__([^_\n]+)__|`([^`\n]+)`|\*([^*\n]+)\*)/g;
  let offset=0,match;
  while((match=pattern.exec(text))!==null){
   if(match.index>offset)parent.append(doc.createTextNode(text.slice(offset,match.index)));
   const kind=match[4]?'code':match[2]||match[3]?'strong':'em';
   const el=doc.createElement(kind);
   el.textContent=match[2]||match[3]||match[4]||match[5];
   parent.append(el);
   offset=pattern.lastIndex;
  }
  if(offset<text.length)parent.append(doc.createTextNode(text.slice(offset)));
 }
 function render(container,raw){
  const doc=container.ownerDocument;
  const text=tidyText(raw).replace(/\r\n?/g,'\n');
  const lines=text.split('\n');
  const fragment=doc.createDocumentFragment();
  let currentList=null,currentKind='',paragraph=[];
  function flush(){if(!paragraph.length)return;
   const p=doc.createElement('p');inline(p,paragraph.join(' '));fragment.append(p);paragraph=[];
  }
  for(const line of lines){
   if(!line.trim()){flush();currentList=null;currentKind='';continue;}
   const match=line.match(/^\s{0,3}(?:([1-9]\d{0,3})[.)]\s+|([-*+])\s+)(.*)$/);
   if(match){
    flush();
    const kind=match[1]?'ol':'ul';
    if(kind!==currentKind||!currentList){
     currentList=doc.createElement(kind);
     fragment.append(currentList);currentKind=kind;
    }
    const li=doc.createElement('li');inline(li,match[3]);currentList.append(li);
    continue;
   }
   currentList=null;currentKind='';
   const heading=line.match(/^\s{0,3}#{1,3}\s+(.+)$/);
   if(heading){
    flush();const h=doc.createElement('h3');inline(h,heading[1]);fragment.append(h);
   }else paragraph.push(line.trim());
  }
  flush();
  container.replaceChildren(fragment);
 }
 root.renegadeTextFormatting=Object.freeze({tidyText,render});
 if(typeof module!=='undefined'&&module.exports)module.exports={tidyText,render};
})(typeof window!=='undefined'?window:globalThis);
