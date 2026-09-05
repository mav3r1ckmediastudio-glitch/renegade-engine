# Test Strategy

## Evidence principle

A compile is necessary but not sufficient. Visual and behavioural work must be
run, observed, and compared with a known fixture. A visual failure overrides a
nominal automated pass.

## Build layer

- Windows x64 Debug and Release.
- DirectX 12 release-blocking path.
- Vulkan-on-Windows development check.
- Shader compilation and cache validation.
- Warnings monitored and recorded.

## Automated layer

Built and running in `RenegadeBridgeTests`:

- EngineBridge unit tests.
- Command and undo/redo state tests, including repeated history and no-op
  filtering.
- Curated weather command tests: application, undo, redo, preset content, and
  preservation of Wicked weather values the curated state does not cover.
- Generated Proving Ground blueprint structure, including rejection of internal
  helper entities and of duplicate entity names.
- Project metadata, recent-project persistence, and editor-preference round
  trips.
- Headless WISCENE save and reload over a mesh-free scene.

Planned, not yet built:

- Asset import/reimport fixtures.
- WISCENE migration tests.
- Lua API smoke tests.
- Standalone packaging tests.

### Headless constraint

`RenegadeBridgeTests` runs without a graphics device. Anything that reaches
`MeshComponent::CreateRenderData()` or `Scene::Update()` will dereference a null
device and crash the test process, which means Wicked's primitive factories and
any scene reload carrying meshes cannot be exercised there.

Two consequences shape the test design. Scene *generation* is asserted through
the renderer-independent `ProvingGroundBlueprint()` rather than by building the
scene. Save and reload round trips use a mesh-free fixture, and full
generated-scene reload stays a packaged-Release acceptance step. Component
authoring — transforms, weather, and in future lights and materials — needs no
device and is therefore fully testable.

## Visual and behavioural layer

- Golden sample scenes with captured settings and screenshots.
- Reference comparison against the pinned Wicked Editor as a source-level
  parity oracle. `WICKED_EDITOR` is forced `OFF` in the Renegade build graph, so
  the Wicked Editor is read, not built or run.
- Manual checks for HDR, ray tracing, post effects, particles, terrain, and
  animation.
- Save/close/reopen checks for authoring changes.
- Standalone-player checks for gameplay-facing changes.
- DPI, keyboard/mouse/controller, and multi-monitor checks.

## Performance layer

- Editor startup.
- Scene load and save.
- Import and reimport.
- CPU/GPU frame time in fixed scenes.
- VRAM and system memory.
- Packaged-player size and startup.

## Required test record

Every completed task records:

- Exact commit.
- Machine/toolchain configuration.
- Commands used.
- Fixtures/scenes used.
- PASS, PASS WITH LIMITATIONS, or FAIL.
- Logs, screenshots, or other evidence locations.
- Known limitations and next task.
