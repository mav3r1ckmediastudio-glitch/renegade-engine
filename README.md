# Renegade Engine

Renegade is a Windows-first game engine and creator environment built on
[Wicked Engine](https://github.com/turanszkij/WickedEngine). Wicked supplies the
renderer, ECS, Jolt physics integration and other low-level engine systems;
Renegade owns its Studio editor, project and asset workflows, Runtime/player,
build lifecycle, creator UX, diagnostics and higher-level gameplay framework.

> **Status: active development — Phase 6 / Playable Core.** Renegade has moved
> beyond scene-authoring foundations into a working gameplay stack with player
> control, governed input, physics, audio, Lua scripting, creator-facing script
> authoring, gameplay APIs, live diagnostics and reusable script-library package
> adoption. S6 — Library Adoption & Package Closure — is merged on `main`
> through PR #143 after its final integrated Windows CI passed. PR #144 is the
> active S7 candidate for six creator-ready stock Lua Actions. Renegade is not
> yet a distribution-ready v1 engine.

## What Renegade already does

### Studio and project authoring

- Custom Renegade Studio chrome and workspaces over Wicked subsystems rather
  than embedded stock Wicked Editor windows.
- Project Hub, Story Flow/Journey authoring, Scene hierarchy, selection,
  transform gizmos, Inspector workflows and command-backed Undo/Redo.
- Asset Browser placement and creator-owned project/scene lifecycle.
- Environment, Terrain, Render, Physics, Audio and Diagnostics authoring
  surfaces.
- Stable Inspector section/provider architecture used by both existing and new
  creator-facing systems.

### World and rendering

- Finite one-metre Terrain creation, non-destructive ring expansion and
  cross-chunk sculpting.
- Native Wicked grass/vegetation painting across Terrain chunks with normal
  viewport navigation preserved while the brush is armed.
- Environment, realistic sky, sun/time-of-day, precipitation and native FFT
  ocean foundations.
- Lights, materials, decals, probes, post-processing, AO/GI/reflections,
  ray/path-tracing exposure, lightmap/baking workflows and render diagnostics.
- Packaged Studio/Runtime parity checks for the accepted world and rendering
  paths.

### Assets and standalone builds

- GLB/GLTF creator-facing import with placement, automatic scale correction,
  Undo/Redo and save/reopen behaviour.
- Stable asset identity, source provenance and deterministic moved/missing
  source recovery.
- Deterministic dependency extraction and standalone Windows build staging,
  validation, rollback/promotion and isolated Runtime launch.
- Test Level runs through the real Runtime process; Studio yields 3D ownership
  while Test Level is active instead of rendering a competing second world.

### Physics, player, input and audio

- Renegade-owned authoring over Wicked/Jolt physics, including the JP01 physics
  foundation and Physics Lab workflow.
- One governed Player Start, Runtime first-person possession and a Wicked/Jolt
  character capsule with movement, mouse look, sprint and jump.
- Versioned project action maps with governed gameplay input plus Pause/Resume
  and deterministic Reset lifecycle behaviour.
- Native Wicked audio authoring for global/2D and movable positional 3D sources,
  preview playback, buses/mixing/reverb and Runtime Pause/Reset integration.
- Audio-specific trigger zones are intentionally deferred; a future shared
  ZoneService must serve audio, objectives, weather and other gameplay systems
  rather than creating separate incompatible zone implementations.

## Governed Lua scripting

Renegade now has a creator-facing scripting stack rather than relying on ad-hoc
raw Lua execution:

- **S1A/S1B — Inspector foundation:** extensible Inspector sections/providers and
  migration of existing Inspector ownership.
- **S2 — Script document/source model:** durable project `.rscripts` companions,
  transactional edits, source identity and validation.
- **S3 — Governed Lua Runtime:** Runtime-owned Lua lifecycle, live EntityRef
  validation and a deliberately restricted standard-library surface.
- **S4A-D — Creator authoring:** restricted metadata evaluation, ACTION/SCRIPT
  attachments, typed generated properties, governed entity references and
  GLOBAL SCRIPT authoring through normal Undo/Redo and dirty-state ownership.
- **S5A-C — Gameplay APIs and events:** generation-safe entity/transform access,
  governed gameplay lifecycle and bounded cross-script event dispatch.
- **S5D — Structured diagnostics/IPC:** Studio-to-Test-Level handshake and
  structured diagnostics across the creator/runtime boundary.
- **S6 — Creator Library packages:** immutable installed package manifests,
  deterministic transitive dependency closure, transactional first-use adoption
  into project-owned `Content/Scripts/Library/...`, clean update handling,
  creator-edit conflict protection and structured package diagnostics.
- **S7 — Stock Actions candidate:** six installed Lua Actions for doors,
  switches, trigger zones, pickups, relays and sound playback; generic Runtime
  Interact prompts; imported-entity identity repair; and a usable Action picker.

After S6 adoption the **project copy is authoritative**. Runtime never searches
or executes scripts directly from the installed Library. Test Level and Build
Game consume the same project-owned script closure through the existing project
snapshot and dependency-graph paths.

See
[`docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md`](docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md).
The S7 setup and owner test is
[`docs/SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md`](docs/SCRIPTING_S7_STOCK_ACTIONS_OWNER_TEST.md).

## Diagnostics

Renegade has a built-in Diagnostics surface for inspecting the running editor and
Test Level/Runtime rather than relying only on build logs. The current diagnostic
stack includes structured editor/runtime state, script and audio diagnostics,
Runtime heartbeat/liveness semantics, local live diagnostic transport and the
Studio/Test Level IPC handshake.

See [`docs/LIVE_DIAGNOSTIC_ACCESS.md`](docs/LIVE_DIAGNOSTIC_ACCESS.md).

## Current programme

Phase 6 exits when Renegade can author and package a small interactive game with
a controllable character, collisions, audio and a scripted objective.

With the scripting foundation through S6 merged and S7 stock Actions in active
acceptance, the remaining bounded playable-core work is centred on:

1. a reusable scripted **objective and interaction vertical slice** using the
   governed scripting/gameplay APIs;
2. **navigation and actor path queries** over Wicked's voxel/pathfinding
   facilities; and
3. **integrated playable-core acceptance** across reopen, Test Level and an
   independently packaged Windows game.

Shared trigger/volume authoring may be introduced as part of that work only as a
single cross-system ZoneService. Player arms, combat, production enemy AI and
advanced animation remain later work unless a narrow playable-core dependency
requires them.

The detailed programme state lives in [`docs/ROADMAP.md`](docs/ROADMAP.md), not
in a fast-aging branch/status paragraph here.

## Build baseline

- Wicked upstream: `https://github.com/turanszkij/WickedEngine.git`
- Pinned branch: `master`
- Pinned Wicked commit: `3a800b7134aafe58461093c8abb2e274d4e64033`
- Primary target: Windows x64 / DirectX 12
- Development cross-check: Vulkan on Windows

Wicked Engine is included as a pinned Git submodule at `/WickedEngine`.

Clone with:

```bash
git clone --recurse-submodules \
  https://github.com/mav3r1ckmediastudio-glitch/renegade-engine.git
```

For an existing clone:

```bash
git submodule update --init --recursive
```

The Windows reference build and evidence workflow is documented in
[`docs/BUILD_WINDOWS.md`](docs/BUILD_WINDOWS.md).

## Product layers

| Path | Responsibility |
|---|---|
| `/WickedEngine` | Pinned upstream engine foundation |
| `/Studio` | Renegade editor application and owned creator UX |
| `/EngineBridge` | Stable Renegade services/adapters around Wicked APIs |
| `/Runtime` | Standalone game/player executable |
| `/Tools` | Import, shader, packaging and validation tools |
| `/Templates` | Starter projects and examples |
| `/Tests` | Automated, integration, packaged and acceptance tests |
| `/docs` | Canonical architecture, roadmap, gate contracts and verification records |
| `/assets` | Renegade-owned editor assets |

The original Wicked Editor remains available inside the submodule as a parity
reference. It is not the Renegade editor and is not embedded as Renegade UI.

## Start here

1. Read [`docs/PROJECT_CHARTER.md`](docs/PROJECT_CHARTER.md).
2. Read [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md).
3. Check [`docs/ROADMAP.md`](docs/ROADMAP.md) for the current programme state.
4. Check [`HANDOFF.md`](HANDOFF.md) for the current implementation handoff only.
5. For the scripting stack, start with
   [`docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md`](docs/SCRIPTING_S6_LIBRARY_ADOPTION_PACKAGE_CLOSURE.md)
   and follow its references back through S1-S5.
6. For live diagnostics, read
   [`docs/LIVE_DIAGNOSTIC_ACCESS.md`](docs/LIVE_DIAGNOSTIC_ACCESS.md).
7. Follow [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) for Codex, ChatGPT,
   Claude or human handovers.
8. Treat [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) as the capability
   evidence ledger; compilation alone is never proof of creator-facing parity.

## Verification policy

Green compilation is necessary but not sufficient. Creator-facing work must be
behaviourally verified in the editor, save/reopen must be exercised where state
is persisted, and gameplay-facing work must be checked in Test Level and/or an
independently packaged Runtime as appropriate. Visual or behavioural owner
failure overrides nominal automated success.

## Licensing and distribution

Wicked Engine is MIT licensed and retains its original copyright and licence.
Renegade's own project-wide licence has not yet been selected. Current standalone
outputs are engineering/acceptance builds rather than commercial redistribution
clearance. See [`docs/LICENSING.md`](docs/LICENSING.md) before redistributing any
build.
