# Changelog

All notable user-facing changes are recorded here. Newest first.

## Unreleased — Phase 3 Studio foundation

### Environment authoring

- Added a curated Environment inspector that replaces the Transform inspector
  when the weather entity is selected.
- Added Clear, Scattered, Overcast, and Storm sky presets.
- Added realistic sky, realistic sky with volumetric clouds, and skybox texture
  sky modes.
- Added aerial perspective, sky exposure, and ambient intensity controls.
- Added distance fog, height fog, and fog-layer bounds controls.
- Added volumetric cloud coverage, base height, and thickness controls.
- Added volumetric clouds casting shadows onto the world.
- Added `SetWeatherCommand` so every field edit and preset is a single undoable
  command that leaves unrepresented Wicked weather values untouched.

### Editor grid and visual polish

- Replaced Wicked's fixed 20x20 debug grid helper with a Renegade-owned
  infinite shader grid: adaptive spacing, analytic anti-aliasing, distance
  fade, depth occlusion, and Renegade-chosen colours including the axis lines.
- Added a grid visibility toggle on the command bar and the `G` key.
- Added persisted editor preferences through `ProjectService`, stored beside
  the recent-project registry and never written to a scene or `.renegade`
  descriptor.
- Reduced the transform gizmo to a usable screen-space size and restyled it.
- Thinned the selection outline from double Wicked's default to one pixel.
- Gave every generated Proving Ground entity a unique name.
- Fixed the workspace title clipping and replaced the Content Browser
  placeholder with a real empty state.

### Viewport and Proving Ground visual foundation

- Removed the 22 serialized internal grid entities from generated projects.
- Rebuilt the generated Proving Ground from a renderer-independent blueprint
  with generated ground relief, a shared PBR vocabulary, enclosure, and
  background masses.
- Moved atmosphere onto a serialized weather entity so sky and fog survive save
  and reopen; they were previously lost on reload.
- Removed renderer-dependent `Scene::Update()` calls from every `EngineBridge`
  path.
- Changed scene loading from `wi::scene::LoadModel` to a direct
  `Scene::Serialize` read, making it the exact inverse of saving.

### Project hub and viewport interaction

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

## Phases 0 and 1 — baseline, build, and repository foundation

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
