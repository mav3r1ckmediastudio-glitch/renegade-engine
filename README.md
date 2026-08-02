# Renegade Engine

Renegade is the working title for a Windows-first game engine and authoring
environment built on [Wicked Engine](https://github.com/turanszkij/WickedEngine).
Wicked provides the rendering and low-level engine foundation; Renegade will
provide its own editor, project system, asset workflow, runtime, user experience,
documentation, and release process.

> **Status:** Phase 3 — Studio foundation. Phase 2 passed on the project
> owner's Windows GPU. The holographic Project Hub, workspace, and generated
> Proving Ground run on Windows. Viewport selection, navigation, Inspector
> transforms, gizmos, duplicate/delete, save, and Undo/Redo have passed
> packaged DX12 testing. Renegade now draws its own infinite shader grid, and
> a curated Environment inspector authors sky, fog, volumetric clouds, and
> cloud shadows through undoable commands — both accepted on packaged DX12 and
> Vulkan. Terrain Authoring V1 creates the standard 1,716 x 1,716 m native
> terrain, sculpts across chunk seams, and preserves all 169 sculpted chunks
> through packaged DX12 and Vulkan Save/Open round trips. Light and material
> bridge contracts have passed the first PR gate. A Renegade-owned Light
> Inspector is visible in the packaged editor and its Windows CI is green.
> The active gated slice adds a Renegade-owned Add menu for creating all four
> native Wicked light types with surface click placement for local lights and
> a selectable editor-only Directional icon, plus Undo/Redo, Delete, selection,
> and WISCENE persistence; packaged behaviour remains pending owner acceptance. Material UI
> and asset authoring are not built. This repository is not yet a usable game
> engine release.

## Baseline

- Wicked upstream: `https://github.com/turanszkij/WickedEngine.git`
- Pinned branch: `master`
- Pinned commit: `3a800b7134aafe58461093c8abb2e274d4e64033`
- Snapshot date: `2026-07-29`
- Initial release target: Windows x64 with DirectX 12
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

## Product layers

| Path | Responsibility |
|---|---|
| `/WickedEngine` | Pinned upstream engine foundation |
| `/Studio` | Renegade editor application |
| `/EngineBridge` | Stable services and adapters around Wicked APIs |
| `/Runtime` | Standalone game/player executable |
| `/Tools` | Import, shader, packaging, and validation tools |
| `/Templates` | Starter projects and examples |
| `/Tests` | Automated, integration, visual, and sample-project tests |
| `/docs` | Canonical plan, architecture, roadmap, and verification records |
| `/assets` | Renegade-owned editor assets |

The original Wicked Editor remains available inside the submodule as a parity
reference. It is not the planned Renegade editor.

## Start here

1. Read [`docs/PROJECT_CHARTER.md`](docs/PROJECT_CHARTER.md).
2. Read [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md).
3. Check [`docs/ROADMAP.md`](docs/ROADMAP.md) and [`HANDOFF.md`](HANDOFF.md).
4. Follow [`docs/AI_WORKFLOW.md`](docs/AI_WORKFLOW.md) for Codex, ChatGPT,
   Claude, or human handovers.
5. Do not claim feature parity without updating
   [`docs/FEATURE_MATRIX.csv`](docs/FEATURE_MATRIX.csv) and recording evidence.

## Licensing

Wicked Engine is MIT licensed and retains its original copyright and licence.
Renegade's own project-wide licence has not yet been selected. See
[`docs/LICENSING.md`](docs/LICENSING.md) before redistributing any build.
