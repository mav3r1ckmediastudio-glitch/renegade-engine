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

- EngineBridge unit tests.
- Command and undo/redo state tests.
- Project metadata and migration tests.
- Asset import/reimport fixtures.
- WISCENE save/load/compare tests.
- Lua API smoke tests.
- Standalone packaging tests.

## Visual and behavioural layer

- Golden sample scenes with captured settings and screenshots.
- Reference comparison against the pinned Wicked Editor.
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
