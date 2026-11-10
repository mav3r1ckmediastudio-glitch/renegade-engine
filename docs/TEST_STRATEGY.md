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

The configured CTest graph contains many native/headless executables plus
source/resource, Python and Node checks. Import/reimport, Lua, project identity,
dependencies, standalone staging/integrity and companion persistence tests already
exist; they are not merely planned. Registration and counts depend on build options.

RenegadeBridgeTests remains a headless authority and aggregate build dependency.
Renderer-dependent scene factories and Scene::Update require an initialized device;
use pure state or blueprints in headless tests and explicit native/GPU proof
executables for resource/render checks. A proof target being compiled by CI is
not evidence that its manual graphics path executed there.

The latest prefab candidate passed 60 source contracts and eleven focused checks
locally, plus the geometry and scripted-brazier native proofs. Actual Studio
Save/Reopen and package-relative Runtime were observed. See CURRENT_STATUS.md and
evidence/scene-prefab-functional/ for exact commands, source/binary hashes and
boundaries. Counts describe that candidate, not a permanent suite size.

Assistant/manual checks must retain fail-closed source fingerprints, test
sensitivity to changed source and explicit publication/owner-acceptance limits.

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
