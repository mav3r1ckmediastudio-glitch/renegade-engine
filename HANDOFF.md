# Renegade Engine — Current Handoff

**Date:** 11 September 2026  
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`  
**Merged baseline:** PR #148 — native Wicked navigation plus terrain/rigid-body contact repair  
**Merged commit:** `50b3b3663ce42b7093e4d6c367cfe8dd863d3a9e`  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Programme state

**Phase 6 — Playable Core is accepted and closed.**

PR #148 completed the remaining native-navigation slice and repaired the two
owner-visible physics failures discovered during acceptance:

- generated Wicked terrain heightfields now inherit the translated terrain root
  correctly instead of remaining at local `bottomLevel`;
- creator auto-fit primitive colliders now use the real local bounds half-size
  plus Wicked's native rigid-body local offset, so grounded Box and Cylinder
  shapes no longer begin below one-sided terrain and dropped bodies no longer
  stop several metres early; and
- manual **REFRESH PATH** rebuilds the native Wicked VoxelGrid before PathQuery,
  while passive previews remain read-only.

The final PR #148 head `69650ee02ffe9e6e0853bdb6511d795760ad41a7`
passed both GitHub Actions workflows required for the PR: Windows baseline and
Renegade Studio.

Owner verification then passed the repaired crate/barrel terrain-contact path.
The owner also built and ran the Phase 6 mini-game acceptance build successfully.
That satisfies the Phase 6 exit requirement already defined in
`docs/PHASE6_CAPABILITY_AUDIT.md`: a packaged standalone project with a
controllable character, collisions, audio and a scripted objective.

Do not reopen Phase 6 merely to add more gameplay breadth. New work now belongs
to Phase 7 unless a genuine regression is found in an accepted Phase 6 feature.

## What Phase 6 now provides

### Playable/runtime foundation

- governed Player Start and first-person Runtime possession;
- Wicked/Jolt character capsule, movement, mouse look, sprint and jump;
- persisted gameplay input map plus Pause/Resume and deterministic Reset;
- native global/2D and positional 3D audio authoring/runtime lifecycle;
- JP01 Jolt rigid-body authoring/runtime foundation and repaired terrain contact;
- native Wicked voxel-grid navigation, PathQuery preview and Runtime following;
- Build Game packaging and standalone Runtime parity; and
- live Studio/Runtime diagnostics and Test Level process supervision.

### Governed scripting/gameplay foundation

- durable `.rscripts` documents and governed Lua lifecycle;
- ACTION, SCRIPT and GLOBAL SCRIPT authoring;
- typed script properties and governed entity references;
- generation-safe entity/transform/player/input/audio/event/UI seams;
- reusable Creator Library packages with transactional project adoption;
- six accepted stock Actions: Sliding Door, Interaction Switch, Player Trigger
  Zone, Proximity Pickup, Activation Relay and Play Sound; and
- reusable Objective Counter composition proving switch -> pickups -> door.

### World/effects already available before Phase 7

- finite native Wicked terrain authoring and sculpting;
- native vegetation/grass painting;
- native Wicked particle emitter authoring including sprite-sheet animation;
- environment, ocean and the accepted Phase 5 rendering stack; and
- editor-only marker overlays for creator entities.

## Next engineering objective

Begin **Phase 7 — Animation, Terrain and Advanced Simulation** with a fresh audit
of the pinned Wicked source before implementing another creator-facing gate.

The master plan defines Phase 7 coverage as:

- skeletal animation, animation data and timelines;
- armatures, humanoids, retargeting, IK, expressions and morph animation;
- remaining terrain layers/props/virtual-texture authoring not already exposed;
- hair/grass, force interaction, ocean and fluid effects still missing from the
  Renegade creator surface;
- splines, video components, Gaussian splats and remaining scene components; and
- paint tooling plus component-specific debug visualisation.

Because terrain, vegetation, particles and ocean are already substantially ahead
of the original master-plan schedule, the Phase 7 audit must classify what is
actually missing on the current Renegade baseline rather than blindly rebuilding
those systems.

The highest-value first audit is the **native Wicked character-animation stack**:
armatures, AnimationComponent data/timelines, humanoid mapping, retargeting, IK,
expressions/morphs, and the original Wicked Editor workflows that expose them.
The audit should identify native data ownership, serialization, Runtime behaviour,
existing Lua bindings and the minimum Renegade-owned creator workflow before a
new implementation branch is opened.

## Deferred boundaries that remain deliberate

- One future shared ZoneService for reusable trigger volumes; do not reintroduce
  audio-only or objective-only zone implementations.
- Creator-facing VSync control.
- Player arms, weapons, combat and production enemy AI until deliberately scoped.
- Physical controller owner evidence where hardware is unavailable.
- Commercial redistribution/release packaging clearance.

## Canonical references

- [`README.md`](README.md) — product/build entry point.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — current programme sequence.
- [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md) — long-range programme and Phase 7 scope.
- [`docs/PHASE6_CAPABILITY_AUDIT.md`](docs/PHASE6_CAPABILITY_AUDIT.md) — Phase 6 gate/exit contract.
- [`docs/PHASE6_NATIVE_NAVIGATION_STAGING.md`](docs/PHASE6_NATIVE_NAVIGATION_STAGING.md) — accepted native-navigation architecture and owner test.
- [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) — capability evidence ledger.
- [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) — implementation/handover rules.

Historical repair detail belongs in the gate documents and Git history. This
file intentionally records the current handoff only.