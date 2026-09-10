# Renegade Engine Roadmap

**Current programme:** Phase 6 — Playable Core  
**Merged implementation baseline:** PR #146 — marker overlays + native Wicked
particle emitter authoring, merged on top of the completed S7 stock Action stack.
**Active candidate:** Objective + Interaction Vertical Slice on
`phase6/objective-interaction-vertical-slice`.
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current state

Renegade has moved substantially beyond the original Phase 6 Gate 3 roadmap.
Player/input/audio, Jolt physics, the governed creator Lua stack through S7,
marker overlays, vegetation and native Wicked particle authoring are now merged.

The Phase 6 exit gate remains unchanged: author and package a small interactive
game with a controllable character, collisions, audio and a scripted objective
in the standalone Runtime.

The current work is therefore intentionally a gameplay vertical slice rather
than another infrastructure programme.

## Merged production baseline

| Programme | Merged result |
|---|---|
| Story Flow Gates 1-10 | Project-home Journey/Graph authoring, Screen lifecycle, Runtime traversal, Build Game and standalone parity through PR #101. |
| Scene UI recovery | Shell/workspace isolation, Hierarchy and Inspector, Asset Browser placement, Environment/Terrain and consolidated whole-editor acceptance through PRs #102-#106. |
| JP01 physics foundation | Wicked/Jolt physics authoring, Physics Lab, serialization, Runtime and packaged parity through PR #107. |
| Phase 5 Gates 1-9 | Scene components, cameras, decals/probes, materials/shaders, post-processing, AO/GI/reflections, ray/path-tracing exposure, lightmap/baking and render diagnostics through PRs #109-#118. |
| WD01 vegetation/frame-loop recovery | Native Wicked grass painting across Terrain chunks, restored editor interaction and the accepted 75 Hz capped baseline through PR #122. |
| Phase 6 Gate 1 | Governed Player Start, Runtime first-person possession, Wicked/Jolt character capsule, movement/look/sprint/jump and packaged parity through PR #123. |
| Phase 6 Gate 2 | Project gameplay input map, Pause/Resume and deterministic Reset lifecycle through PR #124. |
| Phase 6 Gate 3 | Native Wicked global/2D and positional 3D audio authoring, preview, buses/mixing/reverb and Runtime lifecycle through PR #125. Audio-only zones were deliberately removed and deferred to one future shared ZoneService. |
| S1A/S1B | Extensible Inspector section/provider framework and migration of existing Inspector ownership through PRs #127-#128. |
| Test Level runtime handoff | Studio suspends competing 3D ownership while Test Level Runtime is active, reducing duplicate rendering/resource pressure through PR #130. |
| S2 | Durable `.rscripts` script document/source model, source identity, transactions and validation through PR #131. |
| S3 | Governed Runtime-owned Lua lifecycle, live EntityRef validation and restricted standard-library surface through PR #132. |
| S4A-D | Restricted metadata evaluator, ACTION/SCRIPT attachment, typed generated properties, governed references and GLOBAL SCRIPT authoring through PRs #134-#137. |
| S5A/S5B | Generation-safe entity/transform gameplay API and governed gameplay lifecycle through PRs #138-#139. |
| S5C + live diagnostics | Bounded cross-script events plus built-in live Studio/Runtime diagnostics and local diagnostic transport through PR #141. |
| S5D | Structured diagnostics and Studio-Test Level IPC/handshake closure through PR #142. |
| S6 | Installed script-package manifests, deterministic transitive closure, transactional first-use adoption, Creator Library integration, update/conflict safety and Test Level/Build Game closure through PR #143. |
| S7 | Six representative creator-facing stock Actions — Sliding Door, Interaction Switch, Player Trigger Zone, Proximity Pickup, Activation Relay and Play Sound — plus interaction prompt/discovery/identity/UI repairs through PR #144. |
| Marker + particle authoring | Editor marker overlays and native Wicked GPU particle authoring, including separate static-texture and sprite-sheet creator workflows, through PR #146. |

## Active objective + interaction slice

The current branch adds one reusable **Objective Counter** Action as a separate
Creator Library package rather than changing the accepted six-Action S7 package.
The reference loop deliberately composes existing governed capabilities:

1. an **Interaction Switch** sends `start_objective`;
2. three **Proximity Pickup** Actions send `pickup`;
3. **Objective Counter** reports `1/3`, `2/3`, then completion; and
4. completion targets a **Sliding Door** with its existing `open` event.

This proves creator properties, entity references, bounded events, player
interaction and Runtime presentation in one small reusable gameplay loop. It
introduces no second objective runtime, no competing event bus and no special
trigger-volume implementation.

Exact owner acceptance is in
[`PHASE6_OBJECTIVE_INTERACTION_OWNER_TEST.md`](PHASE6_OBJECTIVE_INTERACTION_OWNER_TEST.md).

The original Wicked Editor remains the parity oracle for native Wicked systems.
Accepted Renegade features continue to use Renegade-owned UI and stable
EngineBridge boundaries rather than embedding stock Wicked Editor windows.

## Playable-core foundations now available

### Player, physics, input, audio and world authoring

- one governed Player Start and first-person Runtime possession;
- Wicked/Jolt character collision and movement;
- persisted gameplay action maps;
- Pause/Resume and deterministic Reset;
- Renegade-owned Jolt physics authoring and Runtime integration;
- global/2D and movable positional 3D audio;
- scene/runtime audio lifecycle, mixing and reverb foundations;
- native Wicked vegetation/grass authoring;
- native Wicked GPU particle emitter authoring; and
- creator-facing editor marker overlays.

### Governed scripting stack

The scripting programme now provides the complete path from creator authoring to
Runtime/package consumption:

1. Inspector provider architecture;
2. durable script document/source identity;
3. governed Lua Runtime lifecycle;
4. restricted metadata evaluation;
5. ACTION, SCRIPT and GLOBAL SCRIPT authoring;
6. typed properties and governed entity references;
7. generation-safe gameplay entity/transform APIs;
8. governed gameplay lifecycle;
9. bounded cross-script events;
10. structured diagnostics and Studio/Test Level IPC;
11. reusable Creator Library packages with deterministic dependency closure,
    transactional adoption and conflict-safe updates; and
12. six owner-tested stock Actions with direct player interaction/prompt seams.

After Creator Library adoption, project-owned script copies are authoritative.
Test Level and Build Game consume those same project-owned files. Runtime does
not search the installed Creator Library or enable unrestricted filesystem or
package lookup.

See [`SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md`](SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md)
and [`SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md`](SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md).

## Next bounded sequence

### 1. Complete objective + interaction vertical slice

Finish the active branch only after:

- Objective Counter is discoverable/adoptable from Creator Library;
- the switch -> three pickups -> door loop works in Test Level;
- save/close/reopen retains all authored properties and references;
- the same loop works in independently packaged Build Game Runtime; and
- exact-head CI is green.

No shared ZoneService is needed for this reference loop.

### 2. Navigation and actor path queries

Expose a stable Renegade service over Wicked voxel/pathfinding facilities and
prove at least one Runtime actor path query/navigation behaviour without creating
a competing navigation world.

### 3. Integrated Phase 6 acceptance

Create the Phase 6 reference playable slice and prove:

- project reopen and authored-state persistence;
- controllable player and collisions;
- audio;
- scripted objective/interaction behaviour;
- navigation where used;
- Test Level parity; and
- independently packaged Windows Runtime parity.

Passing this closes **Phase 6 — Playable Core** and allows the planned Phase 7
animation/advanced-simulation work to become the primary programme.

## Deferred boundaries

- **Shared zones:** no audio-specific or objective-specific replacement. One
  future ZoneService must serve multiple systems.
- **VSync control:** Renegade still has no creator-facing VSync toggle; the 75 Hz
  figure is a capped owner baseline, not uncapped maximum throughput.
- **Controller physical evidence:** controller code has automated/source coverage
  but owner hardware evidence remains unavailable where previously recorded.
- **Player arms, weapons, combat and production enemy AI:** later gameplay work,
  not prerequisites for the current Phase 6 objective slice.
- **Advanced animation/simulation:** remains under later master-plan phases unless
  a narrow Phase 6 acceptance dependency is identified.
- **Commercial distribution:** current Build Game output remains an engineering
  acceptance path until licensing/release packaging is explicitly cleared.

## Verification policy

- Green compilation is necessary but never sufficient for creator-facing work.
- Visual or behavioural owner failure overrides nominal automated success.
- Save/reopen is required wherever authored state is persisted.
- Gameplay-facing state must be exercised in the real Runtime process.
- Packaged parity is required for Phase 6 exit acceptance.
- Exact gate evidence belongs in its gate/script contract documents; this roadmap
  records programme state rather than reproducing every implementation log.
