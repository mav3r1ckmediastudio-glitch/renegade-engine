# Current Handoff

**Date:** 2026-07-29  
**Project:** Renegade Engine (working title)  
**Active phase:** 2 — architecture/UI proof, first increment  
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`  
**Published branch:** `main` at `fa79bab88b1d61f607102b7b27bfed5d95ddbd20`  
**Prepared branch:** `agent/phase2-studio-shell`  
**Phase 1 test-target fix:** `55c1894`  
**Phase 2 Studio-shell implementation:** `89ea08d`

## Current state

- Wicked remains pinned as `/WickedEngine` at
  `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Phase 1 Windows CI run
  [30471535806](https://github.com/mav3r1ckmediastudio-glitch/renegade-engine/actions/runs/30471535806)
  checked out the exact pin and captured the Windows/Visual Studio toolchain.
- Both Debug and Release compiled Wicked Engine and Wicked Editor with zero
  compiler errors.
- Both jobs then failed because `/t:Tests` was passed to the solution. MSBuild
  treated `Tests` as a target to invoke on every project instead of building the
  Tests project.
- Commit `55c1894` corrects the invocation by building
  `Samples/Tests/Tests.vcxproj` directly.
- Commit `89ea08d` starts Phase 2 with a Renegade-owned CMake graph,
  `RenegadeEngineBridge`, and a branded Windows `RenegadeStudio` shell.
- The Studio proof loads a packaged `cube.wiscene` through `SceneService` and
  renders it through a real `RenderPath3D`.
- ADR 0002 remains open. The `wiGUI` status label is diagnostic and is not the
  production editor toolkit.

## Publication blocker

The connected GitHub integration can read the repository and Actions logs, but
GitHub rejects contents writes with:

```text
403 Resource not accessible by integration
```

The two prepared commits therefore exist locally but are not yet on `main`.
No CI result may be claimed for them until they are pushed.

## Verification performed

- Inspected both completed Phase 1 job logs.
- Confirmed Debug and Release engine/editor compilation reached `0 Error(s)`.
- Confirmed both jobs failed at the identical Tests target invocation.
- Confirmed the failing jobs still uploaded toolchain and build evidence.
- Checked all prepared diffs with `git diff --check`.
- Parsed `.github/workflows/phase2-studio.yml` as YAML.
- Checked every feature-matrix row retains the header field count.
- Confirmed the pinned fixture exists at
  `WickedEngine/Content/models/cube.wiscene`.
- Confirmed the prepared branch is based on published `main`.

## Verification not yet performed

- The corrected Phase 1 workflow has not run because `55c1894` is unpublished.
- The Phase 2 Windows Studio target has not run in CI because `89ea08d` is
  unpublished.
- Local CMake configuration was unavailable in the current Linux workspace
  because the `cmake` executable and SDL2 development headers are absent.
- DX12/Vulkan rendering, resize, DPI, and input remain human-observed checks on
  a Windows machine with a display/GPU.
- Independent review by another AI or human is still required.

## Decisions in force

- Windows x64 and DirectX 12 are the v1 release target.
- Vulkan on Windows is checked during development.
- Wicked remains the low-level foundation; Renegade code stays outside the
  submodule.
- WISCENE remains the native scene format for v1.
- Lua remains the first gameplay scripting language.
- EngineBridge services remain UI-independent.
- The production UI toolkit remains undecided until the HDR/DPI/input proof.
- Release-gate work requires verification by a different AI or human.

## Exact next steps

1. Publish commits `55c1894` and `89ea08d` to `main`.
2. Watch both `Windows baseline` and `Phase 2 Studio shell` workflows.
3. Fix any compiler or packaging error before extending editor functionality.
4. On Windows, run:

   ```powershell
   .\Tools\Build-Studio-Windows.ps1 -Clean
   ```

5. Launch each package and capture the human-observed acceptance evidence in
   `docs/PHASE2_STUDIO_SHELL.md`.
6. Give another ChatGPT conversation, Claude, or a human reviewer this file,
   the exact published commit SHA, the two workflow URLs, and the artifacts.
   Ask them to verify the acceptance list without changing scope.

## Next bounded implementation

After the Studio-shell build is green:

- add the first buildable Runtime target;
- list scene entities in a minimal hierarchy view;
- bind selection through `SelectionService`;
- edit one transform through a command object; and
- prove one undo/redo operation.

Save/reopen and the final UI-toolkit decision remain later Phase 2 gates.
