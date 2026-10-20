'use strict';
const assert=require('node:assert/strict');const {normaliseForDaniel}=require('../voice/warm-daniel');
const examples=[
 ['45 m/s, 0.133333 s and 75 fps.','forty-five metres per second, zero point one three three three three three seconds and seventy-five frames per second.'],
 ['1,000 objects, 101 steps and -2.5.','one thousand objects, one hundred and one steps and minus two point five.'],
 ['1 ms; 2 cm; 90°; 25%.','one millisecond; two centimetres; ninety degrees; twenty-five per cent.'],
 ['Mav3r1ck, Qwen3.1, v3.1, FPS and NEW.','Maverick, Qwen3.1, v3.1, FPS and new.'],
 ['007 and 0.05.','zero zero seven and zero point zero five.'],
 ['https://example.com/v3/45 and 45.','https://example.com/v3/45 and forty-five.']
];
for(const [input,expected] of examples)assert.equal(normaliseForDaniel(input),expected,input);
console.log('PASS speech numbers, decimal precision, units, negatives, grouping and protected names/versions/links');
