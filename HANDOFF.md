# Renegade Engine — Current Handoff

**Date:** 2026-08-02

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Active branch:** `phase4/model-import-v1`

**Branch base:** `e515172` (`Plan and prove light material authoring bridge (#12)`)

**Pull request:** Not opened yet

**Gate 1 remote commit:** `38c9f24`

**Gate 2 remote commit:** `32351d9`

**Add Light remote commit:** `b81c4bb`

**Model Import V1 serialization correction:** `e0427e9`

**Model Import V1 write-crash and round-trip fingerprint correction:**
`550d6d7`, `4e78e1b`, `50bb1eb`

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current truth

PR #12 is merged into `main` at `e515172`. The project owner confirmed the
complete light workflow works as intended in packaged DX12 and Vulkan,
including creation, placement, grouped hierarchy discoverability and
selectable editor-only markers.

The active milestone is Model Import V1 Gate 1. `ImportService` compiles only
Wicked's standalone GLB/GLTF conversion unit into EngineBridge while the stock
Wicked Editor target remains disabled. It now uses an explicit two-stage
boundary: conversion and summary produce a heap-backed `PreparedModelImport`
on Wicked's job system; WISCENE write, reload and comparison consume it at
`EVENT_THREAD_SAFE_POINT`. It does not merge an asset into the active Studio
scene and adds no visible importer UI.

The pinned Wicked converter creates mesh/material render data and calls
`Scene::Update()`, so it requires an initialized graphics device. The service
rejects calls without one.

Two further defects were found from a packaged desktop-exit report against
`C:/Users/paulw/Downloads/crate_box/scene.gltf` and fixed at `550d6d7`,
`4e78e1b`, and `50bb1eb`:

1. **Write-path crash (`550d6d7`).** `WriteScene()` called `archive.Close()`
   explicitly mid-function to flush the file before reopening it for
   round-trip validation, then let the same `wi::Archive` local go out of
   scope. `~Archive()` unconditionally calls `Close()` again, and
   `Archive::Close()` is not idempotent: in write mode it re-invokes
   `SaveFile()` every time, and the second call ran with an already-cleared,
   null `data_ptr` and a stale non-zero `pos`, writing megabytes from address
   0. That is an access violation, not a C++ exception, so it skipped every
   `try/catch` and took the whole process down below `wiscene_write_complete`
   with no further breadcrumb — exactly matching the `.import.log` evidence.
   Fixed by writing once via `archive.SaveFile(path)` and then disarming the
   archive (`archive = wi::Archive();`) before its destructor can run,
   mirroring the existing pattern already used in
   `SceneDocumentService.cpp`.
2. **Round-trip false failure (`4e78e1b`, `50bb1eb`).** Once the crash was
   fixed, Gate 1 produced a controlled `FAIL` instead: "Imported WISCENE
   structure changed during save and reload," with identical object/mesh/
   material/transform/hierarchy counts on both sides. Root cause:
   `MaterialComponent::Serialize` in Wicked writes texture names relative to
   the WISCENE's own directory (`wi::helper::GetPathRelative(dir, ...)`) but
   does not restore them to absolute on load (unlike embedded resource data
   in `wiResourceManager.cpp`, which is explicitly re-prefixed with
   `archive.GetSourceDirectory()`). A freshly imported scene therefore holds
   an absolute source texture path, and the same scene reloaded from disk
   holds a path relative to `Saved/Validation/ModelImport/` — two different
   strings for the same file. `ImportService::Summarize()`'s structural
   fingerprint was hashing the full texture path, so it flagged this
   legitimate, intentional Wicked behaviour as a structural regression on
   every textured model. Fixed by hashing only
   `wi::helper::GetFileNameFromPath(texture.name)`, which still catches a
   texture slot pointing at a genuinely different file but is invariant to
   the relative/absolute rewrite. `4e78e1b` also added a field-by-field
   diff to the failure message so future mismatches are diagnosable from the
   dialog/log instead of guessed at.

**Packaged evidence (project owner, both renderers, commit `50bb1eb`):**

- `BUILD > VALIDATE GLB/GLTF IMPORT...` against
  `C:/Users/paulw/Downloads/crate_box/scene.gltf` reports
  `PASS // MODEL IMPORT V1 GATE 1` in both DX12 and Vulkan, with identical
  counts on both: 1 object, 1 mesh, 1 material, 2 texture references, 7
  transforms, 8 hierarchy links, 0 armatures, 0 animations.
- `BUILD > VALIDATE GLB/GLTF IMPORT...` against
  `C:/Users/paulw/Downloads/windmill_in_soviet_village.glb` reports
  `PASS // MODEL IMPORT V1 GATE 1` in both DX12 and Vulkan, with identical
  counts on both: 27 objects, 27 meshes, 2 materials, 6 texture references,
  70 transforms, 109 hierarchy links, 1 armature, 1 animation.

The active scene, selection, and dirty state were unaffected across all four
runs per the on-screen hierarchy/inspector state.

Between the two assets this proves the crash fix and the round-trip
fingerprint fix hold for both a minimal single-object case and a multi-node,
multi-material, skinned, animated case, with matching counts across DX12 and
Vulkan on both. **Model Import V1 Gate 1 acceptance from
`docs/PHASE4_MODEL_IMPORT_V1.md` is satisfied.** Local C++17 syntax checks
pass for the service and tests, but this Linux container has no CMake and
cannot itself execute the Windows DX12/Vulkan import round trip — all
packaged evidence above came from the project owner's machine.

## Active slice — Model Import: Scene Placement (uncommitted)

Not yet committed or built. This is local, unverified work following Gate 1
closure — see `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md` for the full scope,
design rationale, and packaged acceptance checklist. Summary:

- Added `PreparedModelImport::ReleaseScene()` so a caller can take ownership
  of the converted scene without going through the Gate 1 WISCENE round
  trip.
- Added `PlaceImportedModelCommand` (`ICommand`) in
  `EngineBridge/include/renegade/bridge/ImportService.h` /
  `EngineBridge/src/ImportService.cpp`: merges a prepared scene into the
  active Studio scene via `Scene::Merge()`, locates the new root the same
  way stock Wicked Editor does ("imported models always have a root
  transform entity" — the first newly added transform), positions it five
  metres in front of the camera, and snapshots it with
  `Scene::Entity_Serialize(..., RECURSIVE)` for Undo/Redo, mirroring the
  existing `DuplicateEntityCommand`/`DeleteEntityCommand` pattern.
- Added `ADD > IMPORT MODEL...` to `RenegadeStudioChrome`
  (`Studio/src/RenegadeStudioChrome.h`/`.cpp`) and wired it through
  `StudioApplication`'s existing deferred `EditorAction` queue
  (`ImportModel()` / `RunModelImportPlacement()` /
  `CompleteModelImportPlacement()`), following the same file-dialog →
  worker-thread conversion → `EVENT_THREAD_SAFE_POINT` completion shape as
  `ValidateModelImport()`/`RunModelImportProof()`, and the same
  select-and-reveal UX as `PlaceLight()`.
- Added `PlaceImportedModelCommand` Execute/Undo/Redo coverage to
  `Tests/ImportTests.cpp` using a synthetic scene (no real GLTF conversion,
  since that needs a graphics device this test harness does not have).
- This does not register a reusable project asset and does not write any
  asset file; the model persists only as part of the active scene through
  the existing Save path. `BUILD > VALIDATE GLB/GLTF IMPORT...` is
  untouched.

Changed files:

- `EngineBridge/include/renegade/bridge/ImportService.h`
- `EngineBridge/src/ImportService.cpp`
- `Studio/src/RenegadeStudioChrome.h`
- `Studio/src/RenegadeStudioChrome.cpp`
- `Studio/src/StudioApplication.h`
- `Studio/src/StudioApplication.cpp`
- `Tests/ImportTests.cpp`
- `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md` (new)
- `docs/FEATURE_MATRIX.csv`
- `HANDOFF.md`

**No local validation has run against this yet** — no syntax check, no
build, no packaged test. It has not even been committed. Before treating any
part of this as working:

1. Run the same kind of local C++17 syntax check used for prior slices
   (see the Gate 1 commands below) against the seven changed source files.
2. Commit and push, then run Windows CI.
3. Package Release, then run every step in
   `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md`'s packaged acceptance checklist
   on both DX12 and Vulkan. A crash, a failure to merge, a broken Undo/Redo,
   or a loss of the imported model on Save/Reopen all stop this gate, same
   as every other gate in this repository.

## Completed slice — Model Import V1 Gate 1

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
- `docs/PHASE4_MODEL_IMPORT_V1.md`
- `docs/FEATURE_MATRIX.csv`
- `HANDOFF.md`

Local validation:

```text
git diff --check
g++ -std=c++17 -fsyntax-only <SDL declaration shim> \
  -IEngineBridge/include -IWickedEngine/WickedEngine -IWickedEngine/Editor \
  EngineBridge/src/ImportService.cpp Tests/ImportTests.cpp
g++ -std=c++17 -fsyntax-only <SDL declaration shim> \
  -IEngineBridge/include -IStudio/src \
  -IWickedEngine/WickedEngine -IWickedEngine/Editor \
  Studio/src/StudioApplication.cpp
g++ -std=c++17 -fsyntax-only <SDL declaration shim> \
  -IWickedEngine/Editor -IWickedEngine/WickedEngine \
  WickedEngine/Editor/ModelImporter_GLTF.cpp
```

All three syntax commands pass. The shim only supplies SDL declarations absent from
this container and is not a repository file. CMake is unavailable locally.

Risks and next task:

The representative windmill and a simple control GLTF both crashed in earlier
packaged attempts, and a corrected build still crashed on `crate_box`. Both
root causes are now understood and fixed (see above): a double-`Close()`
null-pointer write, then a false-positive round-trip comparison. `crate_box`
(single object, no armature/animation) and `windmill_in_soviet_village.glb`
(27 objects/meshes, 2 materials, 1 armature, 1 animation) both now pass DX12
and Vulkan with matching counts. **Gate 1 acceptance is closed.**

1. Run fresh Windows CI to prove MSVC compiles `550d6d7`/`4e78e1b`/`50bb1eb`
   in the Renegade target; this has not yet run in CI, only on the project
   owner's packaged local builds.
2. Wicked's importer reports malformed-file errors through its
   reference-editor message box; production structured error capture remains
   a later hardening gate and must not be hidden.
3. Scene placement (merging an imported model into the active scene with
   Undo/Redo) is drafted in the new active slice above, but is uncommitted
   and has not run in packaged Studio yet. The full **Asset Browser > Add
   Asset** workspace — project asset registration and a reusable browser
   entry, so a model can be instanced without re-converting — remains
   further out and is intentionally not part of that draft; see
   `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md`.

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
