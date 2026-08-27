# Renegade Engine

Renegade is the working title for a Windows-first game engine and authoring
environment built on [Wicked Engine](https://github.com/turanszkij/WickedEngine).
Wicked provides the renderer, ECS, physics integration and low-level engine
foundation; Renegade owns its Studio editor, project and asset workflows,
runtime/player, build lifecycle, UX, documentation and higher-level gameplay
framework.

> **Status:** Phase 4 — Project and asset pipeline. Renegade Studio and the
> standalone Runtime are separate applications. Project/scene persistence,
> Undo/Redo, environment, terrain, native lights, GLB/GLTF import and placement,
> Runtime Screens/Story Flow, unsaved Test Level launch, deterministic dependency
> extraction, stable asset identity/source provenance and safe standalone Windows
> builds are all established foundations. LP06 is accepted and merged: Studio's
> **BUILD > BUILD WINDOWS GAME...** action can produce a named Release executable,
> smoke-test the staged package on DX12, validate its exact contents and safely
> promote it without destroying the previous successful build. The project owner
> then launched the promoted executable directly from Explorer and confirmed the
> Runtime screen and Story Flow entered Level One successfully. Renegade is still
> a development project, not a distribution-ready v1 game engine.

## Current baseline

- Current main baseline: `628cf26574a4a2a6e8eb0a5a522d94966ad8917e`
  (Scene UI Gate 4 Asset Browser and placement recovery, PR #104)
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

The immediate programme is **LP07 — Reusable Project Asset Workflow**. Its goal
is to turn model import into a real reusable project-asset workflow while also
broadening Renegade's accepted model-format surface.

**FBX is the P0 format for LP07, including skinned and animated FBX.** GLB/GLTF
remains a required regression/secondary path. OBJ and PLY are planned behind the
same common import contract where the pinned Wicked converter seams prove clean;
VRM/VRMA receives an exact seam audit before Renegade claims support.

Target lifecycle:

**import once -> stable project asset -> browse -> place repeatedly -> detect a
source change -> reimport safely -> reopen/build without broken identity.**

The exact pinned Wicked FBX importer already uses the bundled `ufbx` loader and
converts meshes, materials/textures, armatures/skinning, morph data and animation
stacks into native Wicked scene components. LP07 will first expose and prove that
existing path through Renegade-owned services rather than adding a second FBX
stack. External libraries such as Assimp remain an evidence-driven fallback only
when a required format/FBX capability is proven missing.

See [`docs/LP07_REUSABLE_PROJECT_ASSET_WORKFLOW.md`](docs/LP07_REUSABLE_PROJECT_ASSET_WORKFLOW.md).

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
4. For the latest completed lifecycle proof, read
   [`docs/LP06_GATE5_SAFE_PROMOTION_CLOSEOUT.md`](docs/LP06_GATE5_SAFE_PROMOTION_CLOSEOUT.md).
5. For the next programme, read
   [`docs/LP07_REUSABLE_PROJECT_ASSET_WORKFLOW.md`](docs/LP07_REUSABLE_PROJECT_ASSET_WORKFLOW.md).
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
