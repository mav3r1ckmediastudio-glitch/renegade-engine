# Renegade Engine — Current Handoff

**Date:** 2026-09-01

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Authoritative main:**
`90ebea4b9a9ec41cd7c92d86224d1484ec55d70b`
(`Perf/editor frame loop recovery (#122)`).

**Accepted PR #122 source head:**
`ad97b0f631c7cb565fb3c2c209815abd073448e0`.

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`.

## Current status

PR #122 is merged and owner-accepted. Phase 5 Gates 1-9, WD01 native Wicked
vegetation, the editor performance recovery and both reported interaction
repairs are now one product baseline on `main`.

There is no remaining #122 blocker and no active Scene Editor recovery PR.
Superseded PRs #119, #120 and #121 have been closed by the project owner.

## Accepted #122 behaviour

The project owner tested the exact PR head and confirmed:

- an empty Level runs at the 75 FPS VSync cap;
- a populated Level also runs at the 75 FPS VSync cap;
- native grass painting works across all Terrain chunks without the former
  significant FPS loss;
- the full top and bottom Studio chrome works;
- Hierarchy selection and collapse/expand work; and
- with Paint Grass active, RMB look, RMB+WASD movement and repeated
  movement-to-stroke navigation work without leaving the Terrain workspace.

All four required GitHub checks passed before merge:

- Renegade Studio Windows x64 Debug;
- Renegade Studio Windows x64 Release;
- Windows baseline x64 Debug; and
- Windows baseline x64 Release.

## What #122 corrected

The performance recovery removed work that did not belong in ordinary frames:

- governed texture restoration moved out of `CreatorAssetStudioChrome::Update`;
- synchronous PR58 lifecycle-file writes were retired;
- Gate 9 Diagnostics became the sole owner of environment-probe debug drawing;
- Studio and Runtime render/LUT synchronization became revision-gated through
  `SceneService::Revision()`; and
- WD01 vegetation synchronization became terrain-lifecycle gated instead of a
  whole-terrain idle-frame pass.

The interaction recovery then corrected two independent faults:

1. Gate 9 had registered `studioChrome_` twice as a top-level component. One
   physical press was therefore processed twice, opening and immediately
   closing menus or toggling Hierarchy disclosure twice. The accepted tree has
   one authoritative top-level chrome registration.
2. An armed vegetation brush consumed every hovered viewport frame, including
   frames with no paint input. The accepted tree consumes input only for an LMB
   paint/delete action or an already active stroke, preserving RMB navigation
   and keyboard camera movement.

The fixes preserve native Wicked vegetation, Terrain chunks and render quality.
They do not obtain performance by lowering scene fidelity.

## Completed programme state

- Story Flow Gate 10 merged through PR #101.
- Scene UI recovery and consolidated whole-editor acceptance merged through
  PRs #102-#106.
- JP01 Wicked/Jolt physics parity and Physics Lab merged through PR #107.
- Phase 5 Gates 1-9 merged through PRs #109-#118.
- WD01 vegetation and editor recovery merged through PR #122.

The previous handoff incorrectly described Phase 5 Gate 6 and PR #122 as active.
Those instructions are superseded by this file.

## Next required task

Begin planning **Phase 6 / the Heathen playable core**. JP01 already supplies
the physics foundation, so do not build a second physics system or repeat its
authoring work.

Before implementation, audit the current tree and produce a bounded gate plan
for:

- player start/possession, controller and camera;
- keyboard/mouse/controller action mapping;
- Play/Pause/Stop and deterministic reset;
- 3D audio and Runtime persistence;
- Lua gameplay bindings and a simple scripted objective;
- any navigation/path-query capability required by the vertical slice; and
- packaged standalone parity.

The target exit proof is a packaged project containing a controllable
character, collisions, audio and a scripted objective. The first code PR should
be one end-to-end vertical slice with an explicit owner checklist.

## Documentation checkpoint

Documentation branch: `docs/post-pr122-closeout`.

Changed files:

- `HANDOFF.md`;
- `docs/ROADMAP.md`.

This checkpoint changes documentation only. It deliberately does not alter
`docs/FEATURE_MATRIX.csv`, because no feature exposure changed after the
accepted #122 implementation. It must be incorporated into the next
implementation branch/PR so the next normal four-way CI cycle validates the
complete code-and-documentation candidate. Do not open a documentation-only PR
or manually dispatch CI for this checkpoint.

## Verification for this checkpoint

- `git diff --check` — required;
- Markdown local-link validation — required;
- authoritative merge/head values compared with Git history — required;
- Windows build and CTest — not required because this checkpoint changes only
  Markdown and will travel with the next implementation CI cycle.
