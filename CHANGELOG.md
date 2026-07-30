# Changelog

## Unreleased — Phase 3 Studio foundation

- Added the first holographic Renegade Project Hub.
- Added versioned `.renegade` project descriptors.
- Added project create, open, validation, recent-project, and relaunch flows.
- Added a persistent recent-project registry.
- Added the first Renegade-owned smoked-black/cyan `wiGUI` theme.
- Added permanent toolbar, hierarchy, inspector, viewport, and content regions.
- Added direct viewport object selection synchronised with the hierarchy,
  inspector, and transform gizmo.
- Added a cyan editor-only silhouette around the selected scene object.
- Added right-mouse freelook with WASD/QE movement, Shift acceleration, and
  wheel-adjustable camera speed inside the 3D viewport.
- Replaced the presentation cube with a generated live Proving Ground starter
  scene using PBR materials, emissive geometry, lights, shadow, fog, and
  environment colour.
- Added automated project lifecycle and recent-project persistence coverage.
- Renamed the continuing Windows Studio workflow and evidence paths so they no
  longer describe all future builds as Phase 2.

## Phase 2 — interaction and display proof

- Added the first standalone `RenegadeRuntime` target.
- Added scene hierarchy projection and selection-bound transform inspection.
- Added command-based translation editing with undo and redo.
- Added automated bridge tests and Runtime packaging to Windows CI.
- Restored the Wicked Tests project `SolutionDir` when invoked directly.
- Added a command-backed viewport translation gizmo.
- Added WISCENE Save As and reopen through `SceneService`.
- Added repeated ten-step Undo/Redo command coverage and no-op filtering.
- Added DX12/Vulkan package launchers and visible display diagnostics.
- Accepted `wiGUI` as the production integration foundation while retaining
  Renegade ownership of UX, styling, docking, layout, and components.
- Added per-monitor DPI resize handling to Studio and Runtime.
- Fixed graphics-backend arguments being parsed after device creation.
- Fixed the Windows title-bar encoding defect.
- Fixed repeated scene-save cleanup crashing after a valid file was written.

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
