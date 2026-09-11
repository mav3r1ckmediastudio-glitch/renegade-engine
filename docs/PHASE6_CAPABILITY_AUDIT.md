# Phase 6 — Playable-Core Capability Audit

**Original audit baseline:** `90ebea4b9a9ec41cd7c92d86224d1484ec55d70b`  
**Final merged baseline:** PR #148 / `50b3b3663ce42b7093e4d6c367cfe8dd863d3a9e`  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`  
**Programme outcome:** a packaged test project containing a controllable character, collisions, audio and a scripted objective.  
**Status:** **ACCEPTED — PHASE 6 CLOSED, 11 SEPTEMBER 2026**

## Closure evidence

All bounded Phase 6 gates are now complete.

- Gate 1 — Player Start, possession and movement: accepted.
- Gate 2 — gameplay input and play-session lifecycle: accepted.
- Gate 3 — native Wicked spatial audio and mixing: accepted; audio-only zones remain deliberately deferred to one future shared ZoneService.
- Gate 4 — governed Lua gameplay lifecycle: completed through the accepted S1-S7 scripting programme.
- Gate 5 — objective and interaction slice: accepted through the reusable Objective Counter plus stock Action composition.
- Gate 6 — native Wicked navigation and actor path queries: merged in PR #148 using Wicked `VoxelGrid`, `PathQuery` and `CharacterComponent` rather than a competing navigation runtime.
- Gate 7 — integrated acceptance: owner built and ran the Phase 6 mini-game acceptance build successfully after PR #148's final physics/navigation repair.

The final PR #148 head `69650ee02ffe9e6e0853bdb6511d795760ad41a7`
passed the required Windows baseline and Renegade Studio GitHub Actions workflows.
Owner verification also passed the repaired grounded Box/Cylinder terrain-contact
behaviour before merge.

Phase 6 must not be reopened for feature expansion. Future animation, specialist
simulation, richer actor/AI behaviour and remaining scene-component exposure now
belong to Phase 7 or later unless they are repairing a regression in an already
accepted Phase 6 capability.

## Architectural decision

Phase 6 built gameplay on the accepted Wicked/Jolt, Scene, project, Runtime and
packaging foundations. It did not create a second physics world, a second scene
format or a Studio-owned gameplay loop. Studio owns authoring; stable EngineBridge
services own product semantics; Runtime owns execution; Wicked owns the low-level
systems.

The phase proceeded as complete vertical slices. A UI control was not complete
until its state survived Save/Open, executed in Test Level and survived the
packaged standalone path where applicable.

## Final capability state

| Area | Accepted Phase 6 result |
|---|---|
| Physics | JP01 exposes native Wicked/Jolt rigid bodies and the accepted physics foundation. PR #148 repaired generated-terrain HEIGHTFIELD root inheritance plus grounded Box/Cylinder creator auto-fit by storing true local bounds size and native rigid-body offset. |
| Player | One governed Player Start with Runtime first-person possession. |
| Character control | Native Wicked/Jolt character capsule, movement, mouse look, sprint and jump. |
| Runtime camera | Player-owned first-person Runtime camera rather than the original fixed spectator-only path. |
| Input | Persisted gameplay action map with keyboard/mouse/controller-shaped bindings plus Pause/Resume and deterministic Reset. |
| Play lifecycle | Supervised Test Level Runtime, deterministic reset and Build Game standalone parity. |
| Lua | Governed project-owned Lua lifecycle through S7 with ACTION, SCRIPT and GLOBAL SCRIPT authoring, typed properties/references and bounded gameplay APIs. |
| Audio | Native Wicked global/2D and movable positional 3D audio authoring/runtime lifecycle with accepted preview and gameplay behaviour. |
| Navigation | Renegade adapter over native Wicked `VoxelGrid`, `PathQuery` and `CharacterComponent`, including creator grid/test-agent authoring and Runtime following. |
| AI/gameplay | Reusable Creator Library Actions plus Objective Counter prove a bounded interaction/objective gameplay loop without a second event bus or objective runtime. |
| Packaging | Existing LP06/Build Game dependency and Runtime boundaries carry the accepted Phase 6 content into an independently packaged Windows game. |

## Completed gate sequence

### Gate 1 — Player Start, possession and movement — ACCEPTED

One governed Player Start resolves to a Runtime Wicked/Jolt character capsule,
player movement and camera ownership. Save/Open, Test Level and packaged paths
were accepted.

### Gate 2 — Gameplay input and play-session lifecycle — ACCEPTED

The Gate 1 defaults were promoted into persisted gameplay actions with
Pause/Resume and deterministic reset while keeping simulation inside Runtime.

### Gate 3 — Spatial audio and mixing — ACCEPTED

Native Wicked sound sources are exposed through Renegade authoring. Global/2D
and positional 3D playback were owner-proven. Audio-only trigger zones were
removed and deferred to one shared future ZoneService.

### Gate 4 — Lua gameplay lifecycle — ACCEPTED THROUGH S1-S7

The scripting programme delivered governed project scripts, deterministic
Runtime lifecycle, creator metadata/properties, entity references, gameplay
entity/transform/player/input/audio/event/UI seams, diagnostics, Creator Library
package adoption and six accepted stock Actions.

### Gate 5 — Objective and interaction slice — ACCEPTED

A reusable Objective Counter composes with the accepted Interaction Switch,
Proximity Pickup and Sliding Door Actions. No second objective runtime, event bus
or special-purpose zone implementation was introduced.

### Gate 6 — Navigation and actor path queries — ACCEPTED

PR #148 exposes the pinned Wicked voxel-grid/path-query stack through a stable
Renegade service and proves native Runtime actor following. Manual REFRESH PATH
rebuilds the native grid before querying; authored obstacle routing remains a
Wicked PathQuery result.

### Gate 7 — Phase 6 integrated acceptance — ACCEPTED

The owner built and ran the Phase 6 mini-game acceptance build successfully after
the final PR #148 repair. Combined with the previously accepted gate evidence,
this satisfies the Phase 6 exit requirement:

- controllable player;
- collisions;
- audio;
- scripted objective/interaction behaviour; and
- independently packaged standalone Runtime execution.

## Deliberate deferrals carried forward

- Shared ZoneService / reusable trigger volumes.
- Player arms, weapon sockets, melee/projectile/magic combat and production enemy AI.
- UDP networking beyond documented programmer-tier exposure unless explicitly promoted.
- Advanced animation and specialised simulation, now governed by Phase 7.
- Commercial redistribution/release packaging clearance.
