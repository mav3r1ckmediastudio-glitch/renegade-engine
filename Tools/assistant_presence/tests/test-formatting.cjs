'use strict';
const assert=require('node:assert/strict');
const {render,tidyText}=require('../chat/format-text');
const {cleanForVoice}=require('../chat/live-speech');
class Node {
 constructor(doc,tag,text=''){this.ownerDocument=doc;this.tag=tag;this.children=[];this._text=text;}
 append(...nodes){for(const n of nodes){if(n.tag==='#fragment')this.children.push(...n.children);else this.children.push(n);}}
 replaceChildren(...nodes){this.children=[];this.append(...nodes)}
 set textContent(value){this._text='';this.children=[new Node(this.ownerDocument,'#text',String(value))]}
 get textContent(){return this._text+this.children.map(n=>n.textContent).join('')}
}
const doc={createElement(name){return new Node(doc,name)},createDocumentFragment(){return new Node(doc,'#fragment')},createTextNode(v){return new Node(doc,'#text',v)}};
function tags(node){return [node.tag,...node.children.flatMap(tags)]}
const container=doc.createElement('div');
const example='1. **Weapon Selection**: Choose a weapon type.\n2. **Animation Setup**: Assign the appropriate animation.\n3. **Hitbox Definition**: Configure collision.\n\nThis setup allows custom behavior.';
render(container,example);
assert.equal(container.children[0].tag,'ol');
assert.equal(container.children[0].children.length,3);
assert(tags(container).includes('strong'));
assert.equal(container.children[0].children[0].textContent,'Weapon Selection: Choose a weapon type.');
assert(container.textContent.includes('Hitbox Definition'));
assert(!container.textContent.includes('**'));
const malformed='Renegade \u00e2\u20ac\u201d '+String.fromCharCode(0xdc9d)+' has no live editor access.';
const expected=tidyText(malformed);
assert(expected.includes('\u2014'),'repair mojibake em dash');
assert(expected.includes('\ufffd'),'replace illegal UTF-16 code unit');
assert(!/[\uD800-\uDFFF]/.test(expected));
assert(!/[\uD800-\uDFFF]/.test(cleanForVoice(malformed)));
assert(!cleanForVoice(malformed).includes('\ufffd'));
render(container,'**Bold** <script>alert(1)</script> 1. Keep safe.');
assert(tags(container).includes('strong'));
assert(!tags(container).includes('script'),'never render model-supplied HTML');
assert(container.textContent.includes('<script>alert(1)</script>'),'literal HTML must be harmless');
const emoji='Unicode check \uD83D\uDE80 allowed';
assert(tidyText(emoji).includes('\uD83D\uDE80'),'preserve valid surrogate pairs in text');
console.log('PASS: ordered list, safe bold, raw HTML treated as text, malformed Unicode display and Daniel speech sanitization, valid emoji preserved');
