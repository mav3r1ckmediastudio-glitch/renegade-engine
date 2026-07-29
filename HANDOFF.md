# Current Handoff

**Date:** 2026-07-29
**Project:** Renegade Engine (working title)
**Active phase:** 2 — architecture/UI proof
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`
**Published branch before this handoff:** `main` at `11678cdfc75597fc34b47d11d2c1856bdb63f381`
**Published test-harness attempt:** `a45f9ed86e481f3de46849726bbc540637d2997b`
**Published proven-test restoration:** `11678cdfc75597fc34b47d11d2c1856bdb63f381`
**Prepared repeated-history correction:** `9804e63ab390ef748e2097b97e10c84646353662`

## Published evidence

- Wicked remains pinned and unmodified at
  `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Phase 2 workflow
  [30482144721](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30482144721)
  passed Windows x64 Debug and Release at `044b371`.
- That run built and packaged Studio and Runtime and ran the bridge tests.
- Windows baseline run
  [30482144741](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30482144741)
  passed Windows x64 Debug and Release.
- Phase 2 workflow
  [30484886814](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30484886814)
  compiled and packaged the gizmo/persistence increment at `8d59b25`, but
  `RenegadeBridgeTests` crashed while serializing the scene in its headless
  process.
- Phase 2 workflow
  [30486528042](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30486528042)
  again compiled Studio, Runtime, EngineBridge, the gizmo, and the test in
  Debug and Release at `a45f9ed`. Its checkpoints proved the crash occurs
  inside full-scene save, which requires the renderer-backed application
  environment rather than only the Wicked job system.
- Phase 2 workflow
  [30489808616](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30489808616)
  passed Windows x64 Debug and Release at `11678cd`; Windows baseline run
  [30489808638](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30489808638)
  also passed.
- Human Windows GPU inspection proved that the Release artifact launches,
  loads `Content/cube.wiscene`, renders the cube, selects the hierarchy entity,
  displays the translation gizmo, and applies axis drags.
- A single Undo and Redo initially worked. Repeated manipulation exposed a
  behavioural failure: the status reached `Undo 10 / Redo 0`, while history
  actions no longer produced an obvious scene change. The transform also
  contained floating-point drift (`z = -2.15203e-07`), proving that
  microscopic gizmo releases could enter history.

## Published increment

Commit `16cf3f5` adds:

- the pinned Wicked translation gizmo to the Renegade viewport proof;
- adapter logic that converts each completed drag into a
  `SetTranslationCommand`;
- Undo/Redo support for both inspector and gizmo translation;
- WISCENE Save As and Reopen through `SceneService`; and
- a Save As and Reopen workflow that requires Windows GPU acceptance.

The gizmo is the pinned Wicked translation utility, not the Wicked Editor. The
Studio shell, hierarchy, inspector, commands, persistence workflow, and UI
remain Renegade-owned. The proof still does not select the production UI
toolkit; ADR 0002 remains open.

## Published proven-test restoration

The standalone bridge test was restored to the hierarchy, selection, execute,
Undo, and Redo coverage that passed in Phase 2 workflow
[30482144721](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30482144721).
It performs no renderer-backed serialization.

Full WISCENE Save As and Reopen remain implemented through `SceneService`, but
their end-to-end acceptance is explicitly a Windows GPU Studio check. No
Studio, Runtime, bridge, gizmo, or persistence production code changed in this
correction.

## Prepared repeated-history correction

Code commit `9804e63ab390ef748e2097b97e10c84646353662` is based directly
on the green published commit `11678cd` and:

- prevents a focused `wiGUI` widget and the viewport gizmo from consuming the
  same mouse operation;
- allows a drag that began in the viewport to finish if the pointer crosses a
  panel before release;
- rejects transform commands whose total change is only floating-point noise;
  and
- expands `RenegadeBridgeTests` from one Undo/Redo cycle to ten ordered Undo
  operations, ten ordered Redo operations, history-count checks, and
  microscopic-change filtering.

The correction changes only `Studio`, `EngineBridge`, and `Tests`. Wicked
remains pinned and unmodified.

## Validation performed before the rerun

- `git diff --check` passed.
- Both workflow YAML files parse.
- Every feature-matrix row has the expected 16 fields.
- The pinned Wicked submodule is exact and unmodified.
- The repeated-history correction passes `git diff --check`.
- Source inspection confirms `wiGUI` callbacks run inside
  `RenderPath3D::Update()` before the Renegade gizmo update.
- This Linux workspace has no CMake or PowerShell. Windows CI remains the
  compile and command-test authority.

## Publication and verification

1. Publish code commit `9804e63ab390ef748e2097b97e10c84646353662`
   and this handoff update on top of published
   `main` at `11678cd`.
2. Watch `Phase 2 Studio shell` and `Windows baseline`.
3. Fix any compile, command-test, or packaging failure before adding scope.
4. Download the new Release artifact.
5. On a Windows GPU machine:
   - select an entity in the hierarchy;
   - make ten clearly different translation drags;
   - click Undo ten times and confirm each status count and transform change;
   - click Redo ten times and confirm each status count and transform change;
   - click and release a gizmo without moving it and confirm the Undo count does
     not increase;
   - click Undo and Redo while the gizmo is visually near a panel;
   - use Save As;
   - make another edit;
   - use Reopen and confirm the saved transform returns;
   - launch Runtime and confirm the scene renders without editor controls.
6. Give another ChatGPT conversation, Claude, or a human reviewer the exact
   published commit, workflow URLs, artifacts, this file, and
   `docs/VERIFICATION_CHECKLIST.md`.

## Next bounded work after green CI and visual acceptance

- perform DPI, keyboard/mouse, HDR, SDR, and Windows Vulkan checks;
- record the ImGui Docking versus `wiGUI` evidence;
- close or defer ADR 0002 from measured results; and
- define the project metadata format before Phase 3 project-hub work.
