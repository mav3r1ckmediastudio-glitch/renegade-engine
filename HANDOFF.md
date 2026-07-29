# Current Handoff

**Date:** 2026-07-29
**Project:** Renegade Engine (working title)
**Phase:** 0 — charter and frozen baseline
**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

## Current state

- The repository exists and is intentionally public.
- Wicked Engine is pinned as the `/WickedEngine` submodule at
  `3a800b7134aafe58461093c8abb2e274d4e64033`.
- The canonical master plan, architecture boundary, roadmap, feature ledger,
  testing strategy, licensing policy, and AI workflow are present.
- Product-specific implementation has not started.
- No Windows build has yet been reproduced from this repository.

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

## Verification status

**NOT YET VERIFIED**

The initial commit must be independently checked for:

- Clean recursive clone.
- Correct Wicked submodule SHA.
- Internal documentation links.
- Feature-matrix schema completeness.
- No accidental modification of Wicked source.

## Next bounded task

Reproduce the pinned Wicked baseline on a Windows x64 build machine:

1. Record Visual Studio, MSVC, Windows SDK, CMake, GPU, and driver versions in
   `docs/TOOLCHAIN.md`.
2. Build Wicked Engine and the original editor in Debug and Release.
3. Run the original editor with DirectX 12.
4. Check Vulkan on Windows.
5. Capture exact commands, logs, sample scenes, screenshots, and timings.
6. Update this handoff and request independent verification.

## Known risks

- The supplied ZIP snapshot is no longer present in this workspace, so a
  file-by-file attachment comparison remains outstanding.
- The repository does not yet have CI or a verified Windows build.
- Renegade is a placeholder name.
- A licence for Renegade-authored code has not yet been selected.
