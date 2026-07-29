# Current Handoff

**Date:** 2026-07-29
**Project:** Renegade Engine (working title)
**Active phase:** 2 — architecture/UI proof
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`
**Published branch before this handoff:** `main` at `baf423647a48bf991929a5ffcf300aa324d78116`
**Prepared implementation:** `16dc6cdcd9960e84e68dd8e275bc1044b077eeb5`
**Prepared baseline-script fix:** `33b7eb22ad516654aecf2e352d61a96076f68d20`

## Published evidence

- Wicked remains pinned at
  `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Phase 2 Studio workflow
  [30480638574](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30480638574)
  passed Windows x64 Debug and Release at `baf4236`.
- Both jobs compiled, linked, packaged, and uploaded `RenegadeStudio`.
- The package-path correction is commit `baf4236`.
- Human visual checks and independent verification remain open.

## Baseline workflow diagnosis

Windows baseline run
[30480638310](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30480638310)
compiled Wicked Engine but Debug failed while compiling the standalone upstream
Tests project. Building the `.vcxproj` directly leaves `$(SolutionDir)` empty,
so its relative include path cannot find `WickedEngine.h`.

Prepared commit `33b7eb2` passes the pinned Wicked root as `SolutionDir`. This
changes only Renegade's build script and does not modify the Wicked submodule.

## Prepared Phase 2 interaction increment

Commit `16dc6cd` adds:

- standalone `RenegadeRuntime`;
- UI-independent hierarchy projection from `SceneService`;
- diagnostic hierarchy selection through `SelectionService`;
- translation inspection and editing;
- `CommandService` and `SetTranslationCommand`;
- one undo/redo proof;
- `RenegadeBridgeTests`; and
- Debug/Release Runtime packaging and CTest execution in Phase 2 CI.

The proof controls use `wiGUI` only to exercise the services. ADR 0002 remains
open and no final editor design or UI toolkit is claimed.

## Validation performed before publication

- `git diff --check` passed.
- Both workflow YAML files parsed successfully.
- Every feature-matrix row has the expected 16 fields.
- The pinned Wicked submodule is exact and unmodified.
- Source/API usage was checked against the pinned Wicked headers and reference
  editor.
- This Linux workspace does not contain CMake or PowerShell, so the prepared
  Windows targets have not yet compiled. CI is the compile authority.

## Publication and verification

1. Fast-forward published `main` from `baf4236` through the prepared commits.
2. Watch `Phase 2 Studio shell` and `Windows baseline`.
3. Fix compiler, test, or packaging failures before extending scope.
4. Download both Debug and Release artifacts.
5. Launch Studio and Runtime on a Windows GPU machine.
6. Perform the human checks in:
   - `docs/PHASE2_STUDIO_SHELL.md`
   - `docs/PHASE2_EDITOR_INTERACTION.md`
7. Give another ChatGPT conversation, Claude, or a human reviewer the exact
   published commit, workflow URLs, artifacts, this handoff, and
   `docs/VERIFICATION_CHECKLIST.md`.

## Next bounded work after this increment is green

- add a viewport transform gizmo through the same command boundary;
- save the edited WISCENE to a new path;
- reopen it and prove the transform persists; and
- continue the DPI, input, HDR, Vulkan, and UI-toolkit decision gates.
