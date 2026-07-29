# Changelog

## Unreleased — Phase 2 interaction proof

- Added the first standalone `RenegadeRuntime` target.
- Added scene hierarchy projection and selection-bound transform inspection.
- Added command-based translation editing with undo and redo.
- Added automated bridge tests and Runtime packaging to Windows CI.
- Restored the Wicked Tests project `SolutionDir` when invoked directly.
- Added a command-backed viewport translation gizmo.
- Added WISCENE Save As and reopen through `SceneService`.
- Extended bridge tests to prove edited transforms survive save and reopen.

All notable user-facing changes will be recorded here.

## Unreleased

### Added

- Initial Renegade Phase 0 repository charter and architecture.
- Pinned Wicked Engine foundation and upstream-sync policy.
- Repository-first AI handoff and independent-verification workflow.
- Initial feature-exposure matrix and test strategy.
- Reproducible Windows x64 Debug/Release build and packaging scripts.
- Windows toolchain and DX12/Vulkan smoke-evidence capture.
- GitHub Actions matrix build for the pinned Wicked reference targets.
- Phase 1 Windows build and verification documentation.
- Renegade-owned CMake graph with an `EngineBridge` library and Windows Studio
  shell.
- Minimal scene, selection, and session services around the pinned Wicked
  scene API.
- Branded `RenderPath3D` viewport proof that loads a packaged WISCENE fixture.
- Phase 2 Windows Studio build, packaging, evidence, and CI workflow.

### Fixed

- Build Wicked's Tests project directly instead of passing the colliding
  `Tests` target name to the entire Visual Studio solution.
