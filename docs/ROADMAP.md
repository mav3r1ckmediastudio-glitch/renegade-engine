# Renegade Engine Roadmap

**Current programme:** Phase 6 — Playable Core  
**Merged implementation baseline:** S6 — Library Adoption & Package Closure,
merged as PR #143 (`708030996ea748e5251958a995e1bd69192ae10f`).
**Active candidate:** S7 — six Stock Actions and creator interaction UX in
PR #144.
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`
**Latest integrated CI:** the rebased S7 baseline `3e679ec` passed all four
Windows Debug/Release checks before the final owner-feedback repair; exact-head
checks remain required after the single completion push.

## Current state

Renegade has moved substantially beyond the old Phase 6 Gate 3 roadmap. Spatial
audio is merged, the first-person/player/input lifecycle is established, and the
project now contains a governed creator-facing Lua scripting stack through S6,
including live diagnostics, Studio/Test Level IPC and reusable script-library
package adoption.

The Phase 6 exit gate remains unchanged: author and package a small interactive
game with a controllable character, collisions, audio and a scripted objective
in the standalone Runtime.

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

## Active S7 candidate

PR #144 deliberately contains six representative Lua Actions rather than a
large native catalogue: Sliding Door, Interaction Switch, Player Trigger Zone,
Proximity Pickup, Activation Relay and Play Sound. The completion candidate also
repairs packaged library discovery after native file dialogs, makes ADD assign
missing imported-entity identity through Undo/Redo, gives the source picker a
full-width non-overlapping layout and adds the generic Runtime prompt seam used
by nearby E/Interact Actions.

Creator setup and exact acceptance steps are in
[`SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md`](SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md).

The original Wicked Editor remains the parity oracle. Accepted Renegade features
continue to use Renegade-owned UI and stable EngineBridge boundaries rather than
embedding Wicked's stock Editor windows.

## Playable-core foundations now available

### Player, physics, input and audio

- one governed Player Start and first-person Runtime possession;
- Wicked/Jolt character collision and movement;
- persisted gameplay action maps;
- Pause/Resume and deterministic Reset;
- Renegade-owned Jolt physics authoring and Runtime integration;
- global/2D and movable positional 3D audio; and
- scene/runtime audio lifecycle, mixing and reverb foundations.

### Governed scripting stack

The scripting programme now provides a complete path from creator authoring to
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
10. structured diagnostics and Studio/Test Level IPC; and
11. reusable Creator Library packages with deterministic dependency closure,
    transactional adoption and conflict-safe updates.

After S6 adoption, project-owned script copies are authoritative. Test Level and
Build Game consume those same project-owned files through the established
snapshot/dependency graph. Runtime does not search the installed Creator Library
or enable unrestricted filesystem/package lookup.

See [`SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md`](SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md).

## Next bounded sequence

### 1. S7 owner-visible acceptance

Finish PR #144 only after the packaged Studio lists all six Actions, the picker
and buttons remain usable on imported objects, direct Door/Switch interaction
shows a prompt and responds to E, save/reopen persists the setup, and Test Level
and packaged Runtime agree. Use
[`SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md`](SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md).

### 2. Objective and interaction vertical slice

Build one reusable scripted objective loop on the governed scripting stack rather
than another special-case gameplay path. The slice should exercise creator
properties/references, events, player interaction, save/reopen, Test Level and
packaged Runtime behaviour.

A shared viewport-placeable trigger/volume system may be introduced here if the
objective genuinely needs it, but it must be a single ZoneService suitable for
audio, weather, objectives and later systems.

### 3. Navigation and actor path queries

Expose a stable Renegade service over Wicked voxel/pathfinding facilities and
prove at least one Runtime actor path query/navigation behaviour without creating
a competing navigation world.

### 4. Integrated Phase 6 acceptance

Create the Phase 6 reference playable slice and prove:

- project reopen and authored-state persistence;
- controllable player and collisions;
- audio;
- scripted objective/interaction behaviour;
- navigation where used;
- Test Level parity; and
- independently packaged Windows Runtime parity.

Passing this closes the current Playable Core phase and allows planned Phase 7
animation/advanced-simulation work to become the primary programme.

## Deferred boundaries

- **Shared zones:** no audio-specific replacement. One future ZoneService must
  serve multiple systems.
- **VSync control:** Renegade still has no creator-facing VSync toggle; the 75 Hz
  figure is a capped owner baseline, not uncapped maximum throughput.
- **Controller physical evidence:** controller code has automated/source coverage
  but owner hardware evidence remains unavailable where previously recorded.
- **Player arms, weapons, combat and production enemy AI:** later gameplay work,
  not prerequisites for the current scripting foundation.
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
- Exact gate evidence belongs in the gate/script contract documents; this roadmap
  records programme state rather than reproducing every implementation log.
