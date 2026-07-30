# Current Handoff

**Date:** 2026-07-30

**Project:** Renegade Engine (working title)

**Active phase:** 3 — Studio foundation

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Published branch before this increment:** `main` at
`207d098a040f36fe0f19e4939a9b03d750615627`

**Prepared Phase 3 code:** `30c5d3c` — Build the Phase 3 project hub

**Prepared Phase 3 architecture/docs:** `1f9c0ee` — Define the Phase 3 project
foundation

**Pinned Wicked commit:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Phase 2 closure

The project owner ran the exact `207d098` Release Studio package on an NVIDIA
GeForce RTX 4070 Ti and confirmed both the DX12 and Vulkan launch paths appeared
to work correctly. The Vulkan log identified `GraphicsDevice_Vulkan`.

Together with the earlier human-observed persistence and Runtime checks, the
accepted Phase 2 result is:

```text
DX12 PASS / VULKAN PASS / DPI PASS / INPUT PASS / HDR NOT AVAILABLE
```

Phase 2 is closed. `wiGUI` remains the accepted native UI integration and
rendering foundation under ADR 0002.

## Prepared Phase 3 outcome

The first production-direction Studio increment now provides:

- `ProjectService` behind `EngineBridge`;
- versioned `.renegade` project descriptors under ADR 0003;
- create, open, validate, recent-project, select, launch, and return-to-Hub
  workflows;
- persistent recent-project ordering in `Saved/RenegadeStudio.ini`;
- a generated live Proving Ground inherited by new projects;
- a Renegade-owned smoked-black/cyan holographic theme;
- permanent toolbar, hierarchy, inspector, viewport, and content regions;
- preserved selection, translation gizmo, Undo/Redo, Save As, and Reopen
  paths; and
- project lifecycle and recent-project persistence test coverage.

The Studio title now identifies Phase 3 and the real DX12/Vulkan backend.

Wicked remains exact and unmodified.

## Project descriptor

Each project starts with:

```text
<Project Name>/
├── <Project Name>.renegade
├── Content/
│   └── Scenes/
│       └── Main.wiscene
└── Saved/
```

The descriptor stores only versioned project-relative metadata. It does not
replace WISCENE.

## Validation completed in this workspace

- `git diff --check` passed.
- Every `docs/FEATURE_MATRIX.csv` row has 16 fields.
- The pinned submodule still resolves to
  `3a800b7134aafe58461093c8abb2e274d4e64033`.
- GNU C++17 syntax checks passed for all changed EngineBridge sources,
  `StudioApplication.cpp`, and `BridgeCommandTests.cpp` against the pinned
  Wicked headers.
- The syntax check used `-D__SCE__` only to bypass Wicked's unavailable Linux
  SDL platform declarations; it does not represent a PlayStation build.
- CMake, MSVC, the Windows SDK, and a render-capable Windows environment are
  unavailable here. GitHub Actions and the project owner's GPU remain the
  compile and visual authorities.

## Windows publication and verification

1. Publish the prepared commits to `main`.
2. Require the renamed **Renegade Studio** workflow and **Windows baseline**
   workflow to pass Debug and Release.
3. Download the Release Studio artifact for the exact published commit.
4. Extract `RenegadeStudio-Release.zip` into a fresh folder.
5. Follow `README-FIRST.txt`.
6. Report:

```text
DX12 HUB PASS / CREATE PASS / RECENTS PASS / REOPEN PASS / VULKAN PASS
```

A visual or behavioural failure overrides green CI.

## Known limits

- The live holographic UI and Proving Ground have not yet been visually
  inspected on Windows.
- Recent-project settings currently live beside the portable Studio package.
  A later `SettingsService` increment will move user state to the final
  per-user location.
- The Project Hub has no thumbnails, search, sorting, or filters yet.
- The Identity Handshake, iris scan, voice welcome, and split reveal are not
  implemented in this increment.
- The workspace regions are fixed. Renegade-owned docking and layout
  persistence remain Phase 3 work.
- Save As does not yet update the project's startup-scene setting; scene tabs,
  dirty-state tracking, and explicit Save are the next scene-workflow unit.
- The return-to-Hub warning uses command history as a conservative proxy for
  unsaved state until formal dirty tracking exists.

## Next bounded work after this increment passes

1. Correct any CI, project-lifecycle, or live visual failures.
2. Build the skippable Identity Handshake transition that reveals this real
   Project Hub, including profile name and reduced-motion/disable controls.
3. Begin the scene-tab, dirty-state, Save, and unsaved-change workflow.
