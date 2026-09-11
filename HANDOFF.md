# Renegade Engine — Current Handoff

**Date:** 11 September 2026
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`
**Merged baseline:** PR #146 — marker overlays + native Wicked particle emitter authoring
**Active candidate:** PR #148 native navigation and terrain-contact repair on
`staging/phase6-native-navigation`
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current status

Renegade remains in **Phase 6 — Playable Core**. PR #148 now carries native
Wicked navigation authoring and the finite 19x19 terrain foundation. The first
terrain-contact failure was traced to Wicked's transform-less generated chunk
group: the group broke inheritance from Renegade's translated terrain root, so
Jolt heightfields remained at local `bottomLevel` instead of world Y=0.

Renegade-controlled terrain restarts now recreate that group with an identity
local transform. Generated render chunks and their native Jolt HEIGHTFIELD
bodies therefore inherit the same root translation while retaining the
negative/positive sculpt envelope and the Y=0.02 editor grid. The registered
terrain test creates representative HEIGHTFIELD physics and steps real Jolt.

Owner retest at the four-green commit `249fa0244ccfca08839715cc70d372818f5ea93a`
then proved a second independent failure: creator auto-fit expanded primitive
colliders around a grounded model pivot. The crate and cylindrical barrel debug
shapes extended below their visible meshes. Placed on terrain they began inside
the one-sided heightfield and fell through; dropped from above they contacted
several metres early. This repair commit now fits the true local
bounds half-size and stores the bounds centre in Wicked's native rigid-body
offset. The Jolt regression covers both placement directly on world-zero
terrain and a drop from above; the reusable-asset suite separately covers Box
and Cylinder grounded-pivot fitting.

The same repair makes manual **REFRESH PATH** rebuild the native Wicked
VoxelGrid before querying it. Passive preview refreshes stay read-only. The
native navigation test now voxelizes a normal Jolt rigid-body render mesh during
a real grid rebuild and proves that PathQuery makes a lateral detour around it
before running the existing PathQuery and Character-following checks.

Changed files in this repair commit are `CollisionService.h/.cpp`,
`RenegadeNavigationWorkspace.cpp`, the terrain/reusable-physics/native-navigation
tests and source contracts, `docs/PHASE6_NATIVE_NAVIGATION_STAGING.md`,
`docs/FEATURE_MATRIX.csv`, and this handoff. No Wicked source or submodule
pointer changed.

Local evidence on Linux, using temporary uncommitted executable wrappers around
the registered test sources:

- `RenegadeReusablePhysicsRepairAudit` — passed, including grounded Box and
  Cylinder primitive fit;
- `RenegadeTerrainRepairAudit` — passed after generating world-zero HEIGHTFIELD
  physics and stepping two mass-1 auto-fitted boxes for 600 frames;
- `RenegadeNavigationRepairAudit` — passed real Wicked rigid-body mesh
  voxelization, lateral obstacle routing, native PathQuery and Character
  following;
- `cmake -DRENEGADE_SOURCE_DIR=$PWD -P Tests/JP01HardeningSourceContract.cmake`
  — passed;
- `cmake -DRENEGADE_SOURCE_DIR=$PWD -P Tests/Phase6NativeNavigationSourceContract.cmake`
  — passed; and
- `cmake -DRENEGADE_SOURCE_DIR=$PWD -P Tests/JP01PhysicsLabSourceContract.cmake`
  — passed.

Remaining risk is packaged Windows/DX12 owner behavior. Do not treat the prior
four-green run as evidence for this second repair; one fresh exact-head CI and
the crate/barrel/navigation owner sequence are still required.

## Current objective slice

The active branch adds one reusable Creator Library Action:

**Objective Counter**

It:

- starts immediately or waits for a configured start event;
- counts a configured gameplay event;
- exposes a creator-authored required count;
- reports short progress/completion messages through the existing governed UI
  prompt seam;
- targets an authored entity reference on completion; and
- sends a configured completion event/payload through the existing bounded
  gameplay event API.

The reference owner loop deliberately reuses the already accepted S7 Actions:

1. **Interaction Switch** sends `start_objective`;
2. three **Proximity Pickup** instances send `pickup`;
3. **Objective Counter** reaches `3` and sends `open`; and
4. the target **Sliding Door** opens.

No new objective runtime, competing event bus or trigger-volume implementation
is introduced.

The reusable Action is packaged separately under:

`Content/ScriptLibrary/RenegadeObjectiveActions`

This keeps the accepted S7 contract of exactly six stock Actions frozen rather
than silently changing that package after merge.

## Automated proof in the active branch

`RenegadePhase6ObjectiveSliceRuntimeTests` exercises the real shipped Lua sources
rather than token fixtures. It verifies:

- `ObjectiveCounter.lua` is a valid discoverable Creator Library Action;
- a real Interaction Switch starts the inactive objective;
- three real Proximity Pickup Actions advance progress through targeted governed
  events;
- the objective uses a persisted-style entity reference to target the exit door;
- completion sends the stock Sliding Door its normal `open` event;
- the actual door moves in Runtime; and
- the complete six-instance gameplay loop runs without a disabled script or
  Runtime diagnostic.

The packaged creator acceptance guide is:
[`docs/PHASE6_OBJECTIVE_INTERACTION_OWNER_TEST.md`](docs/PHASE6_OBJECTIVE_INTERACTION_OWNER_TEST.md)

## What is now on main

### Gameplay foundation

- governed Player Start and first-person Runtime possession;
- Wicked/Jolt character capsule, movement, mouse look, sprint and jump;
- persisted gameplay action map, Pause/Resume and deterministic Reset;
- native global/2D and positional 3D audio authoring/runtime lifecycle;
- JP01 Jolt physics authoring/runtime foundation;
- native Wicked vegetation/grass authoring;
- native Wicked particle emitter authoring with static and animated sprite-sheet
  creator workflows; and
- editor-only marker overlays for creator entities.

### Governed scripting stack through S7

- extensible Inspector section/provider architecture;
- durable `.rscripts` document and script-source identity;
- governed Runtime-owned Lua lifecycle;
- restricted metadata evaluation;
- ACTION, SCRIPT and GLOBAL SCRIPT authoring;
- generated typed script properties and governed entity references;
- generation-safe entity/transform gameplay APIs;
- governed gameplay lifecycle;
- bounded cross-script events;
- structured diagnostics and Studio/Test Level IPC;
- reusable Creator Library packages with transactional project adoption,
  deterministic dependency closure and update/conflict protection; and
- six accepted stock Actions: Sliding Door, Interaction Switch, Player Trigger
  Zone, Proximity Pickup, Activation Relay and Play Sound.

## Creator-library contract

Installed packages are immutable inputs. Selecting a compatible Creator Library
entry and pressing **ADD** adopts its deterministic closure into the project
under:

`Content/Scripts/Library/<package-id>/...`

The project-owned copy then becomes authoritative for Test Level and Build Game.
Runtime does not search the installed Creator Library directly.

The objective package follows the same S6 contract; there is no second adoption
or runtime path for objectives.

## Immediate owner-visible verification for this branch

Use the final packaged Studio artifact and follow
[`docs/PHASE6_OBJECTIVE_INTERACTION_OWNER_TEST.md`](docs/PHASE6_OBJECTIVE_INTERACTION_OWNER_TEST.md).
At minimum:

1. confirm **Objective Counter** appears in Creator Library and adopts into the
   active project;
2. wire one Interaction Switch to start it;
3. wire three Proximity Pickups to its `pickup` count event;
4. wire its completion target to a Sliding Door using `open`;
5. save, close and reopen the level and confirm every property/reference remains;
6. prove switch -> 1/3 -> 2/3 -> completion -> door in Test Level; and
7. repeat the same loop in an independently packaged Build Game Runtime.

Compile-only success is not creator acceptance.

## Next engineering work

For PR #148, inspect the one fresh Windows CI cycle for this exact repair head,
then perform packaged owner verification: generate fresh 19x19 terrain; place and
drop both a mass-1 crate and cylindrical barrel on flat and sculpted areas;
rebuild terrain; confirm their debug shapes fit their visible meshes; press
**REFRESH PATH**; and confirm the agent routes around the normal rigid-body
obstacle. Do not merge until that owner proof is accepted.

A viewport-placeable trigger/volume system remains deferred until a real shared
need appears. When introduced it must be one ZoneService usable by objectives,
audio, weather and later systems; do not reintroduce an audio-only zone.

## Known deferred boundaries

- Shared ZoneService / reusable trigger volumes.
- Creator-facing VSync control.
- Physical controller owner evidence where controller hardware is unavailable.
- Player arms, weapons, combat and production enemy AI.
- Advanced animation/simulation work governed by later phases.
- Commercial redistribution/release packaging clearance.

## Canonical references

- [`README.md`](README.md) — product/build entry point.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — programme sequence.
- [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md) — long-range programme.
- [`docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md`](docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md) — Creator Library contract.
- [`docs/SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md`](docs/SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md) — accepted stock Action owner setup.
- [`docs/PHASE6_OBJECTIVE_INTERACTION_OWNER_TEST.md`](docs/PHASE6_OBJECTIVE_INTERACTION_OWNER_TEST.md) — active objective-slice acceptance.
- [`docs/LIVE_DIAGNOSTIC_ACCESS.md`](docs/LIVE_DIAGNOSTIC_ACCESS.md) — live diagnostics architecture/access.
- [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) — capability evidence ledger.
- [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) — implementation/handover rules.

Historical gate narratives belong in their gate/contract documents and Git
history. This file intentionally records the **current** handoff only.
