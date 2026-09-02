# Renegade Engine

Renegade is the working title for a Windows-first game engine and authoring
environment built on [Wicked Engine](https://github.com/turanszkij/WickedEngine).
Wicked provides the renderer, ECS, physics integration and low-level engine
foundation; Renegade owns its Studio editor, project and asset workflows,
runtime/player, build lifecycle, UX, documentation and higher-level gameplay
framework.

> **Status:** Phase 6 — Playable Core. Phase 5 Gates 1-9 and the WD01/editor
> performance recovery are accepted on `main`. Phase 6 Gates 1-3 are accepted;
> Gate 4 Lua gameplay lifecycle is active on
> `phase6/gate4-lua-gameplay-lifecycle`. Renegade is still a
> development project, not a distribution-ready v1 engine.

Scene UI Gate 5 is also accepted and merged. The recovered Scene Editor now has
independent Environment and Terrain ownership, complete realistic-sky/Sun
startup, finite one-metre Terrain creation and non-destructive ring expansion,
cross-chunk sculpting, validated packaged terrain materials and matching Studio
and Runtime weather/terrain resources.

## Current baseline

- Current main baseline: `6a135aa2a2ae15a723235406afec8f7f8b12d2cd`
  (Phase 6 Gate 3 spatial audio and mixing, PR #125)
- Wicked upstream: `https://github.com/turanszkij/WickedEngine.git`
- Pinned branch: `master`
- Pinned commit: `3a800b7134aafe58461093c8abb2e274d4e64033`
- Initial target: Windows x64 / DirectX 12
- Development cross-check: Vulkan on Windows

Wicked Engine is included as a pinned Git submodule at `/WickedEngine`. Clone
with:

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

## What is already proven

- **Studio foundation:** custom Renegade chrome over Wicked subsystems, Project
  Hub, hierarchy/selection, transform gizmos, Inspector workflows, Save/Open and
  command-backed Undo/Redo.
- **World authoring:** environment, precipitation, sun/time-of-day foundations,
  native FFT ocean and Terrain Authoring V1, including packaged DX12/Vulkan
  persistence proof.
- **Native lights and materials foundation:** Renegade-owned service/command
  boundaries over Wicked components rather than stock Wicked Editor windows.
- **Model import:** isolated GLB/GLTF conversion, scene placement, Undo/Redo,
  Save/Open and automatic scale correction. The pinned Wicked source also
  contains dedicated FBX, OBJ and PLY model converters; Renegade has not yet
  promoted those to accepted creator-facing import paths.
- **Runtime lifecycle:** Runtime Screens, stable action dispatch, Story Flow and
  LP04 unsaved Test Level snapshots launched through the real Runtime process.
- **LP05 dependency extraction:** deterministic project dependency closure with
  typed providers and packaged separate-process proof.
- **LC01 asset identity/source tracking:** durable stable asset IDs, transactional
  registry persistence, import provenance and deterministic moved/missing source
  recovery.
- **LP06 standalone build lifecycle:** deterministic plan, staging, named
  executable, package integrity, isolated DX12 smoke, safe rollback/promotion and
  owner-visible Build Windows Game integration.

## What comes next

The active gate is **Phase 6 Gate 4 — Lua Gameplay Lifecycle**. It adds governed
project script attachments, deterministic Start/Update/Pause/Resume/Reset/Stop
callbacks and stable entity/input/player/audio access while retaining the
accepted JP01 physics namespace and Wicked-owned Lua VM.

See
[`docs/PHASE6_GATE4_LUA_GAMEPLAY_LIFECYCLE.md`](docs/PHASE6_GATE4_LUA_GAMEPLAY_LIFECYCLE.md).

## Product layers

| Path | Responsibility |
|---|---|
| `/WickedEngine` | Pinned upstream engine foundation |
| `/Studio` | Renegade editor application and owned UX |
| `/EngineBridge` | Stable Renegade services/adapters around Wicked APIs |
| `/Runtime` | Standalone game/player executable |
| `/Tools` | Import, shader, packaging and validation tools |
| `/Templates` | Starter projects and examples |
| `/Tests` | Automated, integration, packaged and sample-project tests |
| `/docs` | Canonical plan, architecture, roadmap and verification records |
| `/assets` | Renegade-owned editor assets |

The original Wicked Editor remains available inside the submodule as a parity
reference. It is not the Renegade editor and is not embedded as Renegade UI.

## Start here

1. Read [`docs/PROJECT_CHARTER.md`](docs/PROJECT_CHARTER.md).
2. Read [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md).
3. Check [`docs/ROADMAP.md`](docs/ROADMAP.md) and [`HANDOFF.md`](HANDOFF.md).
4. For the current programme sequence, read
   [`docs/PHASE6_CAPABILITY_AUDIT.md`](docs/PHASE6_CAPABILITY_AUDIT.md).
5. For the active gate, read
   [`docs/PHASE6_GATE3_SPATIAL_AUDIO.md`](docs/PHASE6_GATE3_SPATIAL_AUDIO.md).
6. Follow [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) for Codex, ChatGPT,
   Claude or human handovers.
7. Do not claim feature parity from compilation alone; update
   [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) and record behavioural
   evidence for creator-facing work.

## Licensing and distribution

Wicked Engine is MIT licensed and retains its original copyright and licence.
Renegade's own project-wide licence has not yet been selected. LP06's promoted
standalone output deliberately remains `distribution_ready=false`; it is an
owner-visible engineering build, not commercial redistribution clearance. See
[`docs/LICENSING.md`](docs/LICENSING.md) before redistributing any build.
