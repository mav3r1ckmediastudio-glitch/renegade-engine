# Changelog

All notable user-facing changes are recorded here. Newest first.

## Unreleased — Phase 3 Studio foundation

### Project document transaction (LF02)

- Added `ProjectDocumentTransaction`, a UI-free, format-agnostic disk
  transaction: same-directory staged writes validated before commit,
  previous-version backup, deterministic path-ordered atomic replacement,
  rollback on partial failure, and a durable journal with a separate
  `Recover()` path for an interrupted transaction on next project open.
- `ProjectService::WriteProject` (including the LF01 legacy `project_id`
  migration), `WriteFlowDocument`, and `WriteScreenDocument` now commit
  through this transaction instead of a direct, unprotected
  `wi::config::File` write. A migrated legacy project descriptor now
  produces a real `.bak.renegade` of its pre-migration content.
- Added `RenegadeProjectDocumentTransactionTests` (16 checks) and
  `RenegadeProjectServiceTransactionTests` (4 checks), and extended
  `RenegadeFlowTests`/`RenegadeScreenTests` with transactional-update
  coverage for Story Flow and Runtime screen documents. Full suite: 19/19.
- Accepted PASS WITH LIMITATIONS: editor-owned Flow and Screen dirty-state
  tracking remains deferred until mutable editor document models and
  authoring workspaces exist. See
  `docs/LF02_PROJECT_DOCUMENT_TRANSACTIONS.md` for the full record.

### Native ocean authoring

- Added `OceanService` and an `OCEAN // NATIVE FFT` section to the Environment
  workspace, exposing every field of Wicked's pinned `OceanParameters`:
  FFT displacement-map resolution, patch length, simulation time scale, wave
  amplitude, ocean wind direction/speed/dependency, choppy scale, water RGBA,
  extinction RGB, water height, surface detail, and screen-edge displacement
  tolerance.
- Added Calm, Coastal, Storm, and Alien ocean presets.
- Added `SetOceanCommand` so every preset, toggle, resolution change, and
  completed slider edit is one Undo/Redo transaction with live drag preview.
- The camera-relative FFT surface reflects the current scene and sky through
  Studio's existing renderer reflections. Foam, caustics, refraction, and
  terrain/shoreline intersection are explicitly not built.

### Sun and time-of-day authoring

- Added `SunService` and a `SUN // TIME OF DAY` section to the Environment
  workspace, authoring Wicked's serialized `sunDirection` and the scene's
  primary directional-light transform together.
- Added a 00:00–24:00 time slider and numeric field, independently editable
  azimuth and elevation, and Dawn/Midday/Golden Hour/Dusk/Midnight presets.
- Added `PLAY DAY`, a preview transport from 0.001 to 24.000 hours per second,
  and `PAUSE`, which commits the preview's final position as one Undo/Redo
  command. Both route through Studio's deferred `EditorAction` queue rather
  than mutating state inside Wicked's button callback.
- The solar path is deterministic and art-directable (sunrise 06:00, solar
  noon 75°, sunset 18:00); geographic simulation, moon phases, seasons, and
  Lua runtime progression remain future work.

### Environment workspace and precipitation

- Moved Environment out of the Scene Hierarchy into a dedicated top-right
  `SCENE / ENVIRONMENT` workspace switch that resolves the primary weather
  entity without discarding the creator's current object selection.
- Added native rain and a functional snow visual profile over Wicked's native
  GPU precipitation emitter, with intensity, fall speed, particle size, wind
  direction, wind strength, and turbulence controls.
- Added `SetPrecipitationCommand` with live slider preview and one Undo step
  per completed drag. Unsurfaced native particle values are captured and
  restored rather than overwritten.
- Snow accumulation, footprints, temperature, and material coverage remain
  explicitly deferred rather than represented by dummy controls.

### Renegade-owned Studio chrome — functional slice and stabilization

- Reconnected the real hierarchy, tool selection (Select/Translate/Rotate/
  Scale), Transform and Environment Inspector controls, and a
  collapsed-by-default bottom drawer (Asset Browser, Console, Output,
  Diagnostics) onto Renegade-rendered controls, retaining Wicked's native
  input mechanics while owning every visible pixel.
- Added functional File/Edit/View menus; states without a backend are marked
  unavailable rather than rendered as plausible dummy controls.
- Added the official wordmark asset, an opaque Inspector, persistent
  Hierarchy/Inspector/drawer splitters, and owned Environment slider-plus-
  number controls with live preview and one Undo command per drag.
- Added drawer close by chevron, active-tab click, Escape, or outside click,
  with drawer open state, last tab, and grid visibility persisted through
  `ProjectService` editor preferences.
- Disabled the stock FPS/error overlay, which collided with the wordmark;
  diagnostics now have an owned drawer destination.

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
