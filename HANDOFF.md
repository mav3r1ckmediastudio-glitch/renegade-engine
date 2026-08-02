# Renegade Engine — Current Handoff

**Date:** 2026-08-02

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Active branch:** `phase4/model-import-v1`

**Branch base:** `e515172` (`Plan and prove light material authoring bridge (#12)`)

**Pull request:** Not opened yet

**Gate 1 remote commit:** `38c9f24`

**Gate 2 remote commit:** `32351d9`

**Add Light remote commit:** `b81c4bb`

**Model Import V1 Gate 1 commit:** Working tree; not committed or pushed

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current truth

PR #12 is merged into `main` at `e515172`. The project owner confirmed the
complete light workflow works as intended in packaged DX12 and Vulkan,
including creation, placement, grouped hierarchy discoverability and
selectable editor-only markers.

The active milestone is Model Import V1 Gate 1. `ImportService` compiles only
Wicked's standalone GLB/GLTF conversion unit into EngineBridge while the stock
Wicked Editor target remains disabled. It validates the source/destination,
imports into an isolated temporary scene, writes WISCENE, reloads it, and
compares native component and texture-reference counts. It does not merge an
asset into the active Studio scene and adds no visible importer UI.

The pinned Wicked converter creates mesh/material render data and calls
`Scene::Update()`, so it requires an initialized graphics device. The service
rejects calls without one. Local C++17 syntax checks pass for the service,
tests, and upstream importer source, but this Linux container has no CMake and
cannot execute the authoritative Windows DX12/Vulkan import round trip.

## Active slice — Model Import V1 Gate 1

Changed files:

- `EngineBridge/include/renegade/bridge/ImportService.h`
- `EngineBridge/src/ImportService.cpp`
- `EngineBridge/CMakeLists.txt`
- `Tests/ImportTests.cpp`
- `Tests/CMakeLists.txt`
- `README.md`
- `EngineBridge/README.md`
- `docs/ARCHITECTURE.md`
- `docs/ROADMAP.md`
- `docs/FEATURE_MATRIX.csv`
- `HANDOFF.md`

Local validation:

```text
git diff --check
g++ -std=c++17 -fsyntax-only <SDL declaration shim> \
  -IEngineBridge/include -IWickedEngine/WickedEngine -IWickedEngine/Editor \
  EngineBridge/src/ImportService.cpp Tests/ImportTests.cpp
g++ -std=c++17 -fsyntax-only <SDL declaration shim> \
  -IWickedEngine/Editor -IWickedEngine/WickedEngine \
  WickedEngine/Editor/ModelImporter_GLTF.cpp
```

Both syntax commands pass. The shim only supplies SDL declarations absent from
this container and is not a repository file. CMake is unavailable locally.

Risks and next task:

1. Run fresh Windows CI to prove MSVC compiles the extracted upstream source and
   temporary Studio proof route in the Renegade target.
2. In packaged Studio, run **BUILD > VALIDATE GLB/GLTF IMPORT...** with the
   same representative textured GLB in DX12 and Vulkan. Require PASS and
   matching component counts; output belongs only under
   `Saved/Validation/ModelImport`.
3. Confirm the active scene, selection, hierarchy, Undo history, and dirty state
   remain unchanged. Do not start the visible importer workspace until both
   renderer proofs pass.
4. Wicked's importer reports malformed-file errors through its reference-editor
   message box; production structured error capture remains a later hardening
   gate and must not be hidden.

## Completed previous slice — Add Light workflow

The next patch completes light authoring before Material UI starts. It:

- adds a permanent `ADD` menu to Renegade's owned top bar;
- offers Point, Spot, Directional, and Rectangle Light;
- calls Wicked's native `Scene::Entity_CreateLight` behind a UI-independent
  `CreateLightCommand`;
- enters click-to-place mode for Point, Spot, and Rectangle, raycasting the
  viewport against terrain and scene geometry;
- previews the surface hit and normal, consumes placement input, and supports
  `Esc` or right-click cancellation;
- aligns Spot and Rectangle emission to the clicked surface normal;
- creates Directional immediately five metres in front of the editor camera;
- draws distinct constant-size, selectable Point, Spot, Directional, and
  Rectangle markers entirely in Studio, with no serialized helper state;
- classifies visible entities from their native Wicked components and presents
  collapsible Lights, Models, Characters, Cameras, Terrain, Effects, Audio,
  and Scene Objects headers only when those categories are present;
- reveals the selected entity by expanding its category and scrolling its row
  into view, while retaining mouse-wheel scrolling for large categories;
- assigns unique creator-facing names such as `Point Light 2`;
- selects the new entity automatically so the existing Inspector opens;
- snapshots the complete native entity for Undo/Redo;
- reuses the existing Delete command and restoration path; and
- adds native component, all-type, naming, rectangle-shape, history, Delete,
  and WISCENE round-trip tests.

No stock Wicked Editor window is embedded. No Wicked file or submodule pointer
is changed. No material UI or terrain material path is added.

## Local validation of the Add Light patch

- `git diff --check` passes.
- Changed `LightService`, `LightTests`, `BridgeCommandTests`,
  `RenegadeStudioChrome`, and `StudioApplication` translation units pass local
  C++17 syntax validation using the pinned Wicked headers and a temporary SDL
  declaration shim required only because this container lacks SDL development
  headers.
- The container has no CMake installation, so Windows compilation and runtime
  behaviour are not claimed locally. PR #12 must run fresh Windows checks.

The `4ebc7a8` correction additionally passes `git diff --check`, preserves the
16-column feature ledger, and passes C++17 syntax validation for
`SceneService.cpp`, `RenegadeStudioChrome.cpp`, `StudioApplication.cpp`, and
`BridgeCommandTests.cpp` against pinned Wicked headers. The bridge test now
asserts native Light, Model, and Humanoid entities resolve to the intended
hierarchy categories. Full Windows build and visual interaction remain the
authoritative gate.

## Required Add Light acceptance

1. Apply and push the Add Light patch to
   `phase3/light-material-authoring` without merging PR #12.
2. Require all four PR checks to pass again.
3. Package Release and launch DX12 Studio.
4. Open `ADD` and create each of Point, Spot, Directional, and Rectangle Light.
5. Confirm every new light appears beneath the collapsible **Lights** header,
   is revealed and selected automatically, and opens the correct type-aware
   Inspector controls. Collapse/expand the header and wheel-scroll a long
   category without selecting or moving a scene entity.
6. Confirm Point, Spot, and Rectangle show a placement preview and appear at
   the clicked surface; Spot and Rectangle face along its normal. Confirm the
   placement click does not also select, sculpt, move a gizmo, or start camera
   navigation, and cancellation creates no light.
7. Confirm every type shows its distinct selectable viewport marker—Point
   burst, Spot cone, Directional sun/direction, and Rectangle panel/direction—
   and that selecting each marker opens its Inspector and reveals its hierarchy
   row. Confirm no editor marker or light visualizer appears in Runtime.
8. Verify Undo removes the new light, Redo restores it, Delete removes it, and
   Undo restores the deletion.
9. Edit representative values, then Save, close, reopen, and confirm all four
   created lights and their appearance remain.
10. Launch Runtime with the saved scene and compare light appearance.
11. Repeat the complete editor visual check with the `vulkan` argument.

A visible or behavioural failure stops the gate even if CI is green. Do not
begin the Material Inspector until the Light Inspector passes the applicable
checks above.

## Following slice

After the complete Add/Edit/Delete light workflow passes DX12, Save/Open,
Runtime, and Vulkan acceptance, wire the already-tested `MaterialService` into
a Renegade-owned Material Inspector for ordinary mesh materials only. Keep
terrain materials behind `TerrainService`, explain ambiguous multi-material
targets, and require a sculpt-preserving packaged regression check.

## Repository rules

- Do not edit Wicked or move its pin without an explicit core-patch/upstream
  task.
- Do not claim behavioural success from compilation alone.
- A visible failure overrides green CI.
- Persistent scene mutations belong in EngineBridge commands and require
  Undo/Redo plus Save/Open evidence.
- PR #12 remains a draft until every Light and Material gate is accepted.
