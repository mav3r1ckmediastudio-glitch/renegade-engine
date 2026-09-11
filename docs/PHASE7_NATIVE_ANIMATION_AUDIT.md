# Phase 7 — Pinned Wicked Animation and Specialist-System Audit

**Date:** 11 September 2026  
**Renegade baseline:** PR #148 merged at `50b3b3663ce42b7093e4d6c367cfe8dd863d3a9e`  
**Audit parent:** Phase 6 closure head `df6154848adee7b40744b49b92ecd01c7fb1377e`  
**Pinned Wicked revision:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Executive conclusion

Phase 7 must **not** build a new animation runtime.

The pinned Wicked source already owns the complete low-level character-animation
stack needed by Renegade: serialized animation data, skeletal armatures, humanoid
mapping, retargeting, inverse kinematics, root motion, morph-weight animation,
expressions, look-at, ragdolls and Runtime evaluation. Wicked Editor already
contains working authoring surfaces for those systems. Renegade's job is to add a
bounded creator workflow and stable EngineBridge adapters over that native state.

Renegade is also further ahead than the original Phase 7 master-plan schedule
suggests. Native finite terrain/sculpting, ocean, GPU emitters and terrain grass
already exist. The remaining specialist systems are mostly bounded exposure
work: generic hair, force fields, video, splines and Gaussian splats, plus the
still-missing terrain/material tooling identified below.

The highest-value next implementation gate is therefore a **native Animation
Playback Inspector**, followed by humanoid mapping/retargeting. This provides
immediate control of animations that Renegade already imports and plays today,
without first taking on full timeline/keyframe authoring.

## 1. Native Wicked animation data model

The pinned Scene registers both `AnimationComponent` and
`AnimationDataComponent` as normal native Scene component managers. Animation
state therefore already participates in Wicked Scene ownership and WISCENE
serialization; Renegade must not create a parallel clip database.

`AnimationComponent` natively provides:

- `start`, `end` and `timer`;
- `amount` as the blend amount;
- `speed`;
- Playing, Looped, Ping-Pong and Root-Motion flags;
- Play, Pause and Stop;
- loop, ping-pong and play-once modes;
- a root-motion bone plus root translation/rotation output; and
- channel/sampler references into `AnimationDataComponent` keyframe data.

Native sampler interpolation includes Step, Linear and Cubic Spline. Wicked
Editor's Animation window notes that spline data can be imported from GLTF/VRM,
but spline keyframe-data creation is not currently authored by that editor.

The native Scene animation update blends sampled values on top of current target
component values using `AnimationComponent::amount`. Rotation channels use
quaternion interpolation; translation/scale and other vector channels use native
interpolation. Root-motion channels are handled specially rather than requiring
Renegade to infer locomotion from bone transforms.

### Runtime ordering

At the pinned revision, `Scene::Update()` runs the relevant stages in this order:

1. character update;
2. animation update;
3. Jolt physics update;
4. transform update; and
5. hierarchy update.

That ordering is important. A Renegade animation service should author native
state and let Wicked's existing update pipeline evaluate it. It must not manually
pose bones from Studio or create an additional Runtime animation tick.

## 2. Wicked Editor animation authoring already exists

The pinned `AnimationWindow` is a general-purpose timeline/editor, not only a
skeletal clip player. It exposes:

- Step / Linear / Cubic Spline sampling;
- loop, ping-pong and play-once behaviour;
- play forward/backward, start-from-beginning/end, pause/stop;
- timer scrubbing;
- blend amount;
- speed;
- editable start/end range; and
- keyframe recording.

Native keyframe recording can target:

- Transform / position / rotation / scale;
- morph weights;
- light colour, intensity, range and cone values;
- sound play, stop and volume;
- emitter emit count;
- camera FOV, focal length and aperture properties;
- script play/stop; and
- material colour, emissive, roughness, metalness, reflectance and UV
  multiplier/addition.

This means a future Renegade timeline can remain an adapter over Wicked's native
animation channels rather than inventing a proprietary sequencer format.

## 3. Armature and humanoid support

The pinned `ArmatureWindow` already provides:

- armature bone enumeration/selection;
- pose reset through `Scene::ResetPose()`; and
- automatic creation of a Humanoid component from bone names.

The automatic mapper explicitly supports VRM- and Mixamo-style naming and maps
the full main body plus fingers, eyes and jaw where matching bones are present.

The pinned `HumanoidWindow` adds:

- explicit humanoid bone mapping/inspection;
- head and eye look-at controls;
- persisted look-at target entity;
- head/eye angular limits and movement speeds;
- ragdoll enable/disable, 2D lock and sizing controls;
- arm/leg spacing and head size;
- animation testing; and
- native animation import and retargeting.

### Native retargeting

Wicked Editor's **Import animations** workflow accepts:

- WISCENE;
- GLTF / GLB;
- FBX;
- VRM; and
- VRMA.

It imports the source into an isolated Scene, resets the destination humanoid
pose and calls `Scene::RetargetAnimation(...)` for each source animation.

`Scene::RetargetAnimation()` matches source animation channel targets to the
source humanoid bone index and redirects them to the corresponding destination
humanoid bone. With `bake_data=true` it creates new native animation data and
bakes the retargeted result. Renegade therefore already has an upstream-native
retarget solution and does not need a custom bone-retarget solver.

## 4. Native inverse kinematics

The pinned `InverseKinematicsComponent` and Wicked Editor IK window expose a very
small creator surface:

- target Transform entity;
- disabled/enabled state;
- hierarchy chain length; and
- iteration count.

This is a strong candidate for direct EngineBridge wrapping after the humanoid
mapping/retarget gate. It does not justify an independent IK subsystem.

## 5. Expressions, morphs and lip-sync behaviour

Wicked's native expression system already exposes:

- named expression weights;
- binary expressions;
- mouth, blink and look override modes (`None`, `Block`, `Blend`);
- automatic blinking frequency/length/count;
- automatic look-away frequency/length; and
- forced talking.

Wicked Editor also documents that when a Sound component exists on the same
entity, mouth expression can be animated from sound playback. Morph weights are
also a first-class AnimationWindow record target.

Renegade should therefore treat facial animation as another native component
surface, not as a separate custom facial-animation runtime.

## 6. Lua/programmer surface

The pinned Wicked Lua Scene bindings already expose native animation-related
access including:

- Animation component lookup and arrays;
- AnimationData entity arrays;
- InverseKinematics create/get/remove;
- Expression create/get/remove;
- Humanoid create/get/remove;
- `Scene::RetargetAnimation`; and
- `Scene::ResetPose`.

Renegade should continue its existing scripting policy: expose a stable,
creator-safe subset through the governed Renegade scripting API where gameplay
scripts need it, while leaving raw Wicked bindings as programmer-tier capability.

## 7. What Renegade already has

The current Renegade baseline is not starting from zero.

### Imported animation preservation and playback

The Phase 4 model-import path already:

- imports native Wicked armatures/animation data;
- merges the imported hierarchy into the active Scene;
- calls `Play()` on every imported animation because Wicked importers create
  clips looped but paused by default;
- snapshots the imported hierarchy for Undo/Redo;
- survives Save/Reopen; and
- runs the animation in Runtime/packaged gameplay.

The documented gap is explicit: Studio currently has no creator-facing pause,
seek, scrub or blend controls and does not otherwise author imported animations.

### FBX/GLTF foundation

Renegade already routes model import through pinned Wicked importers, including
Wicked's bundled `ufbx` FBX path and native GLTF/GLB path. Phase 7 must reuse this
proven import boundary rather than introducing an animation-only importer.

### Humanoid/ragdoll bridge

JP01 already owns a command-backed `RagdollPhysicsService` over the native
`HumanoidComponent`, including ragdoll enable/disable, 2D lock, fatness/head-size
settings and live ragdoll impulses. Phase 7 humanoid work should extend this
existing native ownership and avoid a second Humanoid/Ragdoll service model.

## 8. Recommended Phase 7 gate sequence

### Gate 7A — Native Animation Playback Inspector

**Goal:** give creators proper control of animations Renegade already imports.

Minimum surface:

- enumerate native AnimationComponents associated with the selected imported
  hierarchy/character;
- clip/name selection;
- Play / Pause / Stop / Play From Start;
- Loop / Ping-Pong / Play Once;
- timer scrub;
- speed;
- blend amount;
- start/end range display/edit;
- root-motion state and root-motion bone inspection, with authoring only if the
  native semantics can be safely bounded; and
- clear current-time/length/status readout.

Architecture:

- new stable EngineBridge `AnimationService`/state adapter;
- native `AnimationComponent` remains authority;
- no parallel clip database;
- no Studio-owned animation update loop;
- Undo/Redo for persisted authoring values;
- transient Play/Pause/Scrub preview commands must not dirty the Scene unless
  they change authored state.

Acceptance:

1. import an animated GLB and an animated FBX;
2. select clips and prove play/pause/stop/scrub/speed/loop modes;
3. Save/Reopen preserves authored settings;
4. Test Level and packaged Runtime use the same native animation data;
5. Undo/Redo works for persisted changes; and
6. no change to the Wicked submodule or animation evaluator.

**Do not include full keyframe/timeline authoring in 7A.** That would turn a
small, testable creator gate into a large sequencer project unnecessarily.

### Gate 7B — Humanoid Mapping and Native Retargeting

Expose a Renegade-owned Character/Rig panel over native Armature/Humanoid state:

- bone list and pose reset;
- one-click native auto-humanoid mapping;
- manual correction of missing/incorrect mapped bones;
- import animation file (`WISCENE/GLTF/GLB/FBX/VRM/VRMA`);
- call native `RetargetAnimation(..., bake_data=true, ...)`;
- list the resulting baked native clips; and
- immediately test a retargeted clip through the 7A playback controls.

Acceptance should use at least two differently proportioned humanoid assets and
a source animation not originally authored for the destination rig.

### Gate 7C — IK, Look-at and Expressions

Extend the character surface with the existing native components:

- IK target, chain length and iterations;
- head/eye look-at target and limits;
- expression list and weights;
- blink/look/talk controls; and
- integration with the already accepted JP01 ragdoll controls.

Keep the components native and command-backed. Do not create Renegade-side bone
simulation.

### Gate 7D — Native Timeline / Keyframe Authoring

Only after 7A-7C are owner-proven, expose Wicked's generic AnimationWindow
capabilities in Renegade UX:

- create animation/timeline;
- create/select channels;
- record transform and morph keys first;
- then the already-native light/audio/emitter/camera/script/material channels;
- keyframe editing and close-loop support; and
- interpolation selection.

Cubic-spline authoring must remain honestly bounded by Wicked's capability: the
pinned editor can consume imported spline data but does not create spline data
itself.

### Gate 7E — Remaining specialist component exposure

After the character-animation vertical slice, address the smaller remaining
native components and stale feature-matrix classifications:

- generic HairParticle authoring (distinct from already accepted terrain grass);
- ForceField authoring;
- VideoComponent authoring;
- Spline authoring;
- Gaussian Splat inspection/import path;
- remaining terrain material-paint / heightmap import-export / virtual-texture
  creator gaps; and
- any still-unclassified registered Scene component.

## 9. Specialist-system audit summary

### Already substantially delivered in Renegade

- finite Wicked Terrain creation/expansion/sculpting;
- native terrain grass using per-chunk `HairParticleSystem` masks;
- native GPU emitted-particle authoring;
- native FFT ocean;
- Jolt rigid bodies, constraints, character/vehicle/ragdoll/soft-body Physics
  Lab coverage;
- native voxel-grid navigation; and
- audio.

### Native Wicked systems still lacking a normal Renegade creator surface

**Generic HairParticle:** Wicked supports mesh source, strand count, length,
width, stiffness, drag, gravity, randomness, segments, billboards, seed, view
distance, camera bend and sprite-atlas variants. Renegade terrain grass already
proves the underlying system but does not expose this generic hair workflow.

**ForceField:** bounded native surface — Point/Plane, gravity and range. This is a
small exposure gate.

**Video:** native MP4 open/preview, play/pause/stop, loop and seek. Video output
can replace material base/emissive textures and can drive a light multiplier.
The pinned editor reports H264/H265 decode capability; audio track playback is
not implemented by Wicked's video component. Renegade already knows
`VideoComponent::filename` for dependency extraction but has no creator surface.

**Spline:** native loop/fill/alignment, width/rotation, horizontal and vertical
mesh subdivision, generated corridor/tunnel geometry, terrain deformation,
terrain-texture falloff, terrain push-down and editable node entities. This is a
useful world-building feature, but not a prerequisite for character animation.

**Gaussian Splats:** native Scene component exists at the pin. Wicked Editor's
surface is currently primarily inspection (splat count, SH degree and memory),
so Renegade should not invent a broad editing workflow beyond upstream
capability without a separate product decision.

## 10. Feature-matrix corrections required

`docs/FEATURE_MATRIX.csv` is stale in several relevant rows and must be corrected
before Phase 7 is declared complete:

- `REN-ANI-001` says Animation is "Not started"; actual state is **native import,
  serialization and automatic playback proven; creator animation/rig controls
  missing**.
- `REN-FX-001` says particles/hair/force fields are "Not started"; actual state is
  **GPU emitter authoring and terrain-HairParticle vegetation accepted; generic
  hair and force-field authoring missing**.
- `REN-WLD-001` predates later accepted terrain/vegetation work and should be
  reconciled against the current baseline.
- `REN-PHY-001` is also historically stale after JP01 and PR #148, though physics
  itself is now a closed Phase 6 foundation rather than Phase 7 work.

Do not mechanically mark the complete Phase 7 row green. Split or annotate
capabilities so already-proven native paths are distinguished from missing
creator surfaces.

## Decision

Start implementation with **Gate 7A — Native Animation Playback Inspector**.

It has the strongest value-to-risk ratio because Renegade already imports,
serializes and runs native Wicked animations. 7A only needs to expose native
state that already exists and is already exercised by Runtime. Once 7A is owner
accepted, proceed directly to native humanoid mapping/retargeting in 7B.
