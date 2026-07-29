# Current Handoff

**Date:** 2026-07-29
**Project:** Renegade Engine (working title)
**Active phase:** 2 — architecture/UI proof
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`
**Published branch before this handoff:** `main` at `8d59b25fb28daf371f0d05487709a840984c0e8d`
**Prepared test-harness fix:** `a11eaf914a66f84a93c10d6f9262661d5b58a7d7`

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
- Human visual checks and independent verification remain open.

## Published increment

Commit `16cf3f5` adds:

- the pinned Wicked translation gizmo to the Renegade viewport proof;
- adapter logic that converts each completed drag into a
  `SetTranslationCommand`;
- Undo/Redo support for both inspector and gizmo translation;
- WISCENE Save As and Reopen through `SceneService`; and
- an automated persistence test proving the edited transform survives save and
  reopen.

The gizmo is the pinned Wicked translation utility, not the Wicked Editor. The
Studio shell, hierarchy, inspector, commands, persistence workflow, and UI
remain Renegade-owned. The proof still does not select the production UI
toolkit; ADR 0002 remains open.

## Prepared test-harness fix

The bridge test now initializes and shuts down Wicked's job system around its
scene lifetime. Wicked scene serialization dispatches component work through
that service; Studio and Runtime initialize it through their application
lifecycle, while the standalone test previously did not. Save and reopen
checkpoints were also added so any remaining failure is precisely located in
the Actions log.

No Studio, Runtime, bridge, gizmo, or persistence production code changed in
this correction.

## Validation performed before the rerun

- `git diff --check` passed.
- Both workflow YAML files parse.
- Every feature-matrix row has the expected 16 fields.
- The pinned Wicked submodule is exact and unmodified.
- The test harness follows the pinned `wi::jobsystem::Initialize()` and
  `wi::jobsystem::ShutDown()` lifecycle.
- This Linux workspace has no CMake or PowerShell. Windows CI remains the
  compile and persistence-test authority.

## Publication and verification

1. Fast-forward `main` from `8d59b25` through the prepared test-harness fix.
2. Watch `Phase 2 Studio shell` and `Windows baseline`.
3. Fix any compile, persistence-test, or packaging failure before adding scope.
4. Download Debug and Release artifacts.
5. On a Windows GPU machine:
   - select an entity in the hierarchy;
   - drag each translation axis;
   - verify Undo and Redo;
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
