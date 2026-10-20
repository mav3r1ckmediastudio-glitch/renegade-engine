'use strict';
const assert=require('node:assert/strict');const {isEngineQuestion}=require('../chat/verified-knowledge');
for(const q of ['How was your day, Renegade?','Renegade, how was your day?','Is that something you actually did, or part of your character?','Who are you, Renegade?'])assert.equal(isEngineQuestion(q),false,q);
for(const q of ['How do I import a character, Renegade?','Renegade, how do I import a character?','Tell me about your Character importer','Is that real animation support in the editor?'])assert.equal(isEngineQuestion(q),true,q);
console.log('PASS orb character conversation, leading/trailing address and mixed technical questions');
