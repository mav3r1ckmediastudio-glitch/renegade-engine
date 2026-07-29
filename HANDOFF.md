# Current Handoff

**Date:** 2026-07-29
**Project:** Renegade Engine (working title)
**Phase:** 1 — reproducible Windows build
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`
**Branch:** `agent/phase-1-windows-baseline`
**Starting commit:** `1f887e9b60d76bf83668e94c88f03597837a6cab`
**Implementation commit:** `ba6ff0b2b968c99e57429734b0ef2fab894d9ebd`

## Current state

- Phase 0 is committed on `main`.
- Wicked Engine remains pinned as the `/WickedEngine` submodule at
  `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Phase 1 automation verifies the pin, records the Windows toolchain, builds
  and packages x64 Debug/Release reference targets, hashes outputs, and captures
  logs.
- Windows CI runs the same build for both configurations.
- A guided smoke script records human-observed DX12 and Vulkan results.
- No Windows build or graphics smoke result has yet been observed for this
  implementation commit.

## Decisions in force

- Windows x64 and DirectX 12 are the v1 release target.
- Vulkan on Windows is checked during development.
- Wicked remains the low-level foundation.
- Renegade owns `Studio`, `EngineBridge`, `Runtime`, project/asset workflows,
  UI, branding, documentation, and release packaging.
- WISCENE remains the native scene format for v1.
- Lua remains the first gameplay scripting language.
- The production UI toolkit remains undecided until the HDR/DPI/input proof.
- Release-gate work requires verification by a different AI or human.

## Changed files

- `.github/workflows/windows-baseline.yml`
- `README.md`
- `Tools/Build-Windows.ps1`
- `Tools/Collect-Windows-Baseline.ps1`
- `Tools/Run-Windows-Smoke.ps1`
- `Tools/Windows-Build.Common.ps1`
- `Tools/README.md`
- `docs/BUILD_WINDOWS.md`
- `docs/evidence/PHASE1_WINDOWS_BASELINE_TEMPLATE.md`
- `docs/ROADMAP.md`
- `docs/TOOLCHAIN.md`
- `CHANGELOG.md`
- `HANDOFF.md`

## Commands and observed results

- `git status --short --branch`: clean before implementation; starting commit
  matched `origin/main`.
- `git submodule status`: Wicked resolved to
  `3a800b7134aafe58461093c8abb2e274d4e64033`.
- Inspected the pinned solution and upstream Windows workflow: target names and
  shader sequence match the scripts.
- Windows PowerShell/MSBuild execution: **NOT RUN** in the Linux implementation
  environment.
- DX12 and Vulkan visual smoke: **NOT RUN**; requires a Windows GPU/display.

## Verification status

**NOT YET VERIFIED**

The implementation requires a Windows CI result, local DX12/Vulkan evidence,
and review by a different AI or human using
`docs/evidence/PHASE1_WINDOWS_BASELINE_TEMPLATE.md`.

## Next bounded task

Publish this branch, then:

1. Confirm both Windows CI matrix jobs.
2. On the Windows development machine, run
   `.\Tools\Collect-Windows-Baseline.ps1`.
3. Run `.\Tools\Build-Windows.ps1 -Clean`.
4. Run `.\Tools\Run-Windows-Smoke.ps1` and add DX12/Vulkan screenshots.
5. Have a different AI or human verify the exact commit.
6. Record the observed evidence here before marking Phase 1 complete.

## Known risks

- The supplied ZIP snapshot is no longer present in this workspace, so a
  file-by-file attachment comparison remains outstanding.
- The Windows CI definition has not yet run on this implementation commit.
- GitHub-hosted CI cannot prove interactive DX12/Vulkan rendering.
- The pinned solution requires the `v145` toolset; availability must be
  confirmed on the selected Windows runner and development machine.
- Renegade is a placeholder name.
- A licence for Renegade-authored code has not yet been selected.
