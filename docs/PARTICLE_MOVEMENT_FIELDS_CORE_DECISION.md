# Effect-owned particle movement fields: bounded core decision

The owner approved creating our own vector fields inside the Particle Picker, saved with the composite effect and editable after placement. Fields must influence individual GPU particles during their lifetime. Swirl, turbulence, updraft and directional flow are the bounded initial types. No separately placed Level field assets, third-party field imports, ember appearance replacement or colour-curve redesign is included.

Native point/plane force fields are scene-wide and cannot supply effect-owned vortex/turbulence with independent layer weights through the current wrapper. A bounded extension adds nonserialized emitter movement data, an appended GPU constant-buffer region, and four optional simulation shader variants and three optional emission variants for independent size/speed variation. The existing simulation shader path remains selected when no owned field is active. No particle structure, native archive version, submodule pointer or existing weather semantics change.

Renegade owns field definitions, preset schema, scene metadata, hydration, compiled shader staging and picker UI. Core consumes only bounded runtime values and shader handles. The picker and Runtime use the same GPU movement functions. Shader variants cover sorting and depth collision. Missing field shaders must produce an explicit error. Fields are limited to four per effect; existing effects default to none. Simulation performs no CPU particle readback or whole-scene discovery.

Required verification: field mathematics, boundaries and zero influence; schema 1-5 compatibility; saved field and per-layer influence round trip; native edit/duplicate/Undo/Redo/cold reopen; visible native preview and standalone Runtime movement; no-field regressions, marker and point-lookup contracts. Owner acceptance remains required before treating ember appearance as approved.


## Owner feedback: leaf variation and user reproducibility

The owner supplied three 4x4 leaf flipbooks and rejected a uniform, tightly packed circular effect. The revised QA fixture uses three independent layers, wider spawn boxes, lower emission counts, different heights/lifetimes/animation speeds, per-layer size and speed variation, gentle shared swirl and animated turbulence. These are all public picker controls and saved preset values. No fixture-only simulation behaviour is permitted.

A Variation button opens the emitter Variation / spawn page: independent size variation, speed variation, lifetime variation, spawn half width/height/depth, spawn centre X/Y/Z, emission, base size and lifetime. The Spawn box checkbox enables volume spawning. Fields have shared effect-local position/rotation/size, type, strength, lift, edge fade, noise scale, animation speed and enabled state. Each layer has its own influence percentage. Existing Advanced Randomness remains exposed and can additionally vary legacy particle properties; the mixed fixture sets it to zero so the new independent variation controls determine the ranges.

Preset schema 5 adds independent variation and reads schemas 1-4 with neutral defaults. Scene metadata v2 retains compatibility with the earlier field-only runtime payload. The native particle structure and archive format remain unchanged. Initial velocity variation and ongoing flow-speed variation both apply, so field steering does not erase the creator's speed variation. Colour-over-life curves, imported field assets, general cone-emitter redesign and an approved ember appearance remain outside this change.
