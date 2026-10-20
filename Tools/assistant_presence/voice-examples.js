'use strict';
// Demonstration copy only. These are NOT live Renegade diagnostic findings.
// No audio plays until the user explicitly presses Speak.
window.renegadeDemoScripts=Object.freeze({
  "short": "Good morning. Renegade's voice system is online. What shall we build today?",
  "technical": "If the editor slows down while painting grass, I would begin by measuring frame time rather than guessing. First, check how many foliage instances are visible. Then compare main-thread time, draw calls, and GPU workload. If the frame time spikes when the camera moves, that suggests a rendering issue rather than the painting tools. We can isolate one variable at a time. It is less dramatic than changing everything at once, but remarkably more effective.",
  "conversation": "You asked whether we should change how terrain chunks are managed. I would not rush into that. Chunk boundaries, vegetation updates, collision meshes and render submission each have different performance costs. We should measure them separately before deciding which part needs work. Start with a representative level and record a baseline. Then turn one feature off at a time, repeating the same camera movement. If foliage proves to be the cause, we can optimise its visibility and instance updates without redesigning the terrain system. And if nothing changes, we investigate further. The profiler is usually less persuasive than a confident guess, but its track record is considerably better."
});
