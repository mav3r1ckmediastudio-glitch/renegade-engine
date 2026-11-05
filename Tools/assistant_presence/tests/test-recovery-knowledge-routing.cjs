'use strict';
const assert = require('node:assert/strict');
const http = require('node:http');
const {isEngineQuestion, answerVerified} = require('../chat/verified-knowledge');
async function run() {
 const questions = ['How do I make a river?', 'Can I add a waterfall?',
  'Why is my riverbed not linked?', 'How do tributaries work?', 'How do I load an OBJ?'];
 for (const question of questions) assert(isEngineQuestion(question), question);
 assert(!isEngineQuestion('Alright?'));
 assert(!isEngineQuestion('Did you enjoy that movie?'));
 const received = [];
 const server = http.createServer((req, res) => {
  assert.equal(req.url, '/chat');
  assert.equal(req.method, 'POST');
  let raw = '';
  req.on('data', chunk => raw += chunk.toString());
  req.on('end', () => {
   received.push(JSON.parse(raw).messages.at(-1).content);
   res.writeHead(200, {'Content-Type': 'application/json'});
   res.end(JSON.stringify({route: 'local_verified_knowledge',
    answer: 'Use LINK / UPDATE BED. [Source: EngineBridge/src/RiverTerrainLayerService.cpp]'}));
  });
 });
 await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
 const port = server.address().port;
 try {
  for (const question of questions) {
   const result = await answerVerified([{role:'user', content:question}], {port});
   assert.equal(result.policy, 'source-verified');
   assert.match(result.answer, /LINK \/ UPDATE BED/);
   assert.deepEqual(result.sources, ['EngineBridge/src/RiverTerrainLayerService.cpp']);
  }
  assert.deepEqual(received, questions);
  assert.equal(await answerVerified([{role:'user',content:'Alright?'}], {port}), null);
 } finally { await new Promise(resolve => server.close(resolve)); }
 const offline = await answerVerified([{role:'user',content:questions[0]}], {port, timeout:300});
 assert.equal(offline.policy, 'verified-service-unavailable');
 assert.match(offline.answer, /won't guess/);
 console.log('PASS: actual Electron river/waterfall/tributary/OBJ classifier, HTTP routing, source separation and offline refusal');
}
run().catch(error => { console.error(error); process.exitCode=1; });
