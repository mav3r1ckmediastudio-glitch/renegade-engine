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

## Completed slice — Model Import: Scene Placement

Committed and built by the project owner. `ADD > IMPORT MODEL...` merge into
the active scene, hierarchy placement, and Undo removing the whole imported
hierarchy were first confirmed against `crate_box` in a packaged build; the
project owner has since reported the full
`docs/PHASE4_MODEL_IMPORT_PLACEMENT.md` acceptance checklist passing on both
DX12 and Vulkan (Redo, Save/Reopen persistence, Runtime comparison,
busy-guard, malformed-file, and the scale-specific checks below). See
`docs/PHASE4_MODEL_IMPORT_PLACEMENT.md` for the full scope and design
rationale. Summary:

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

### Follow-on: import scale correction

Added after the project owner reported an imported `crate_box` landing
"VERY large." Committed, built, and packaged-accepted on both DX12 and
Vulkan — see "Packaged evidence" below. See "Import scale correction" in
`docs/PHASE4_MODEL_IMPORT_PLACEMENT.md` for full design rationale and the
GameGuru MAX comparison it is modeled after.

- Added `ImportService::ModelScaleMode` (`Original`/`Meters`/`Centimeters`/
  `Inches`/`Automatic`) and `ImportService::ResolveScaleFactor()` to
  `EngineBridge/include/renegade/bridge/ImportService.h` /
  `EngineBridge/src/ImportService.cpp`. `Automatic` normalizes the union of
  every mesh's local vertex-position bounds to a 2 m target extent; the
  others are fixed literal unit-correction multipliers (or a 1.0 no-op for
  `Original`/`Meters`, which are always identical for a glTF source).
- Added `PreparedModelImport::PeekScene()` so a caller can resolve a scale
  factor against the still-isolated prepared scene before
  `ReleaseScene()` hands it to `PlaceImportedModelCommand`.
- Added a `scaleFactor` constructor parameter (default `1.0f`, backward
  compatible) to `PlaceImportedModelCommand`; applied as the import root's
  uniform `Scale` alongside its placement position, non-destructively —
  never baked into vertex data — so it is captured by the same
  `Entity_Serialize` snapshot Undo/Redo already uses.
- `CompleteModelImportPlacement()` in `Studio/src/StudioApplication.cpp` now
  always resolves `ModelScaleMode::Automatic` and passes it through; the
  status bar reports the applied factor (`AUTO SCALE x0.XXX`) as packaged
  evidence.
- A manual override picker was deferred out of this original follow-on (see
  the next, separate section below) rather than risk a blind edit to the
  Inspector's hand-rolled, hardcoded absolute-pixel layout with no way to
  verify the result without a packaged build.
- Added `ResolveScaleFactor` unit coverage (literal multipliers, empty-scene
  fallback, and a synthetic 20-unit-cube bounding-box normalization) and
  extended the existing `PlaceImportedModelCommand` Execute/Undo/Redo test
  to assert the scale factor is applied and survives Redo, in
  `Tests/ImportTests.cpp`.

Changed files (this follow-on only):

- `EngineBridge/include/renegade/bridge/ImportService.h`
- `EngineBridge/src/ImportService.cpp`
- `Studio/src/StudioApplication.cpp`
- `Tests/ImportTests.cpp`
- `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md`
- `docs/FEATURE_MATRIX.csv`
- `docs/ROADMAP.md`
- `HANDOFF.md`

**Packaged evidence (project owner):** the full
`docs/PHASE4_MODEL_IMPORT_PLACEMENT.md` acceptance checklist passed on
packaged DX12 and Vulkan, including the scale-specific steps: an imported
model lands at a plausible size next to existing scene content instead of
"VERY large," the status bar reports `AUTO SCALE x0.XXX`, the imported
root's Scale X/Y/Z in the Inspector read that same value uniformly, Undo/
Redo preserve the applied scale, and it survives Save/close/Reopen. This
closes out the Model Import: Scene Placement slice's full acceptance
checklist, not just the crate_box merge/hierarchy/Undo check noted earlier.

### Further follow-on, uncommitted: manual Import Scale panel

Requested by the project owner as the deferred piece from the section
above. Not yet committed or built. See "Manual override: the Import Scale
panel" in `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md` for full design rationale.

- Added a self-contained popup (`importScalePanel_` and related widgets in
  `Studio/src/StudioApplication.h`/`.cpp`) that appears automatically right
  after `CompleteModelImportPlacement()` places a model, reporting the
  Automatic factor already applied and offering **Original/Meters (x1.0)**,
  **Centimeters (x0.01)**, and **Inches (x0.0254)** as one-click overrides.
  Deliberately not `Automatic` again (would need a bounding box scoped to
  just the imported entity's descendant subtree inside the live, merged
  scene — separate, not-yet-built work) and deliberately not a new row in
  the Inspector (see the risk this was deferred for above) — it is a fully
  independent `wi::gui::Window`, positioned on its own in `ResizeLayout`
  rather than inside the Transform section's hardcoded pixel chain.
- `ApplyImportScaleMode()` resets the imported root's Scale to the chosen
  literal multiplier through the existing `SetTransformCommand`/
  `CaptureTransform` path — the same Undo/Redo-backed mechanism
  `ApplySelectedTransformValue()` already uses for manual Transform edits —
  so each APPLY is its own Undo step, independent of the import's own
  Undo/Redo entry.
- Guards against the imported entity having been removed (import undone, or
  a different scene opened) while the panel is still open: `ApplyImportScaleMode()`
  checks the entity still has a `TransformComponent` and dismisses the panel
  instead of resolving against a stale entity.
- `ApplyRenegadeTheme()` reasserts `importScalePanel_`'s background the same
  way it already does for `inspectorPanel_`, and folds the two new labels
  into the existing `ownLabel` pass, so the popup matches the rest of the
  chrome's owned styling instead of only the global theme. The panel keeps
  its own drop shadow (unlike the flush-docked Inspector) since it floats
  over the viewport.

Changed files (this follow-on only):

- `Studio/src/StudioApplication.h`
- `Studio/src/StudioApplication.cpp`
- `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md`
- `docs/FEATURE_MATRIX.csv`
- `HANDOFF.md`

**No local validation has run against this yet** — no syntax check, no
build, no packaged test. It has not even been committed. This is new
`wi::gui` widget wiring (a window, two labels, a combo box, two buttons)
positioned independently in `ResizeLayout`, which is lower-risk than
touching the existing Inspector chain but still entirely unverified. Before
treating any part of this as working:

1. Run the same kind of local C++17 syntax check used for prior slices
   against the two changed source files.
2. Commit and push, then run Windows CI.
3. Package Release, then run `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md`'s
   packaged acceptance checklist steps 6-9 (the new Import Scale panel
   steps) on both DX12 and Vulkan, in addition to re-confirming the earlier
   steps still pass. In particular: the panel is fully on-screen at
   different window sizes, each of the three modes applies the correct
   literal Scale value and is its own Undo step, CLOSE is a true no-op, and
   the "entity removed while panel open" guard does not crash. A crash, a
   wrong Scale value, a broken Undo/Redo, or a panel that clips off-screen
   all stop this gate.

## Active slice — Collision Authoring (uncommitted, service layer only)

Started while the project owner packages and tests the two slices above,
continuing the same "materials, scale, animations, collision" request that
started the Model Import scale work. Not yet committed or built, and
deliberately does not touch Studio/`StudioApplication.cpp` at all yet --
this is a UI-independent bridge library addition only, following the same
sequencing already used for `ImportService::ResolveScaleFactor` (prove the
service layer with unit tests first, wire UI in a following slice).

- Added `CollisionService` (`EngineBridge/include/renegade/bridge/
  CollisionService.h` / `src/CollisionService.cpp`), mirroring
  `MaterialService`/`LightService` exactly: a curated `CollisionState`
  (shape, mass, friction, restitution, and shape-specific dimensions) over
  Wicked's native `wi::scene::RigidBodyPhysicsComponent`, with
  `CaptureCollision`/`SanitizeCollisionState`/`HasCollisionStateChange`/
  `ApplyCollision`, and three commands: `CreateCollisionCommand`,
  `SetCollisionCommand`, `RemoveCollisionCommand`.
- Deliberately scoped to `BOX`/`SPHERE`/`CAPSULE`/`CYLINDER` -- the four
  shapes Wicked sizes from explicit dimensions (half-extents/radius/height)
  rather than derived mesh geometry, so any of them can attach to any
  entity with a `TransformComponent`. `CYLINDER` costs nothing extra:
  Wicked stores it in the same `CapsuleParams` (radius/height) as `CAPSULE`
  per the component's own "also cylinder params" comment, confirmed by a
  dedicated `RenegadeCollisionTests` case. `CONVEX_HULL`/`TRIANGLE_MESH`
  need a `MeshComponent`-bearing entity and a `mesh_lod` to derive their
  shape from instead -- a different targeting problem (which entity in a
  multi-node imported hierarchy gets the collider?) that is out of scope
  here, and `TRIANGLE_MESH` is only physically valid for a static body in
  most physics backends. `HEIGHTFIELD` is a regular-grid terrain shape and
  is out of scope entirely -- `TerrainService` already owns that domain,
  it is not a fit for an imported prop's collider. Vehicle and character
  physics are untouched.
- `CreateCollisionCommand`/`RemoveCollisionCommand` are simpler than
  `CreateLightCommand`: they add or remove a single component
  (`ComponentManager<T>::Create`/`Remove`) on an entity that already exists
  and is never created or destroyed by these commands, so Undo/Redo is a
  plain component toggle -- no `wi::Archive` entity-level snapshot needed,
  unlike `CreateLightCommand` (which creates and must be able to fully
  restore a whole new entity) or `PlaceImportedModelCommand` (which merges
  and must restore a whole imported hierarchy).
- `ApplyCollision` calls `RigidBodyPhysicsComponent::SetRefreshParametersNeeded(true)`
  after every edit, per the component's own documented purpose, so a live
  physics simulation rebuilds the shape rather than continuing to use a
  stale one.
- Added `Tests/CollisionTests.cpp` (`RenegadeCollisionTests` in
  `Tests/CMakeLists.txt`, wired the same way as `RenegadeMaterialTests`/
  `RenegadeLightTests`): sanitize-floor coverage, create/duplicate-reject/
  Undo/Redo, edit/no-op-filter/Undo/Redo, remove/Undo (including removing
  from an entity with no rigidbody), and a dedicated Cylinder case proving
  its dimensions reach the native capsule storage -- all against a
  synthetic scene, no graphics device required.

Changed files:

- `EngineBridge/include/renegade/bridge/CollisionService.h` (new)
- `EngineBridge/src/CollisionService.cpp` (new)
- `EngineBridge/CMakeLists.txt`
- `Tests/CollisionTests.cpp` (new)
- `Tests/CMakeLists.txt`
- `docs/FEATURE_MATRIX.csv`
- `HANDOFF.md`

**No local validation has run against this yet** -- no syntax check, no
build, no packaged test, and there is deliberately no Studio UI to exercise
it through yet. Before treating any part of this as working:

1. Run the same kind of local C++17 syntax check used for prior slices
   against the four changed/new source files.
2. Commit and push, then run Windows CI -- `RenegadeCollisionTests` needs to
   pass alongside the existing bridge test suite.
3. This slice intentionally stops at the service layer. The next slice
   wires it into Studio: most likely extending the Import Scale panel into
   a combined Import Setup panel with a collision shape choice, defaulted
   from the same bounding-box data already computed for Automatic scale,
   attached to the import root the same way scale is. That UI work has not
   started and needs its own packaged acceptance pass once it exists.

## Active slice — Import animation autoplay (uncommitted)

Added alongside the Collision Authoring slice, at the project owner's
request to bundle it into the same pending commit. Closes out the
"animations" part of the original "materials, scale, animations, collision"
request -- and turned out to need no new rendering/display capability at
all, just one missing call.

Root cause, confirmed by reading the source rather than assumed: Wicked's
`wi::scene::AnimationComponent` defaults to `LOOPED` but not `PLAYING`
(`_flags = LOOPED`); `Scene::RunAnimationUpdateSystem` only advances an
animation's timer once `IsPlaying()` is true; and
`WickedEngine/Editor/ModelImporter_GLTF.cpp` creates every imported
animation's component but never calls `Play()` on it. An imported model's
armature and keyframe data were never missing or broken (Gate 1 already
proved the windmill's "1 armature, 1 animation" survive conversion and
round-trip intact) -- they were simply frozen at frame zero because nothing
had ever started them, the same as any other native Wicked animation would
be if its `Play()` were never called.

- `PlaceImportedModelCommand::Execute()`'s first-execution branch
  (`EngineBridge/src/ImportService.cpp`) now captures
  `scene_->animations.GetCount()` before `Scene::Merge()`, then calls
  `.Play()` on every newly-added animation afterward, before the
  Undo/Redo snapshot is taken.
- This only reaches animations belonging to the just-imported model, never
  a pre-existing one already in the target scene, because Wicked's own
  importer always creates the animation entity and appends it to
  `scene.animations` as part of the same merge.
- Confirmed (not assumed) that `Undo`/`Redo` already cover this for free:
  `ModelImporter_GLTF.cpp` attaches every animation entity under the import
  root via `Component_Attach(entity, state.rootEntity)`, so it is inside
  the same hierarchy subtree that `PlaceImportedModelCommand`'s existing
  `Entity_Serialize(..., RECURSIVE)` snapshot and `Entity_Remove(...,
  recursive = true)` already capture and restore -- calling `Play()` before
  that snapshot is taken means the playing state round-trips through
  Undo/Redo exactly like any other authored value, with no new snapshot
  logic required.
- Extended the existing `PlaceImportedModelCommand` synthetic-scene test in
  `Tests/ImportTests.cpp` with a fixture animation entity attached under the
  imported root (mirroring `ModelImporter_GLTF.cpp`'s exact attachment
  call), asserting it starts paused, is playing immediately after Execute,
  is removed by Undo, and is playing again after Redo.
- Does not add any pause/seek/scrub/blend control, and does not change
  behavior for any animation authored directly in Studio rather than
  imported -- this only affects the moment a GLB/GLTF model with animation
  data is placed into the scene.

Changed files:

- `EngineBridge/src/ImportService.cpp`
- `Tests/ImportTests.cpp`
- `docs/PHASE4_MODEL_IMPORT_PLACEMENT.md`
- `docs/FEATURE_MATRIX.csv`
- `docs/ROADMAP.md`
- `HANDOFF.md`

**No local validation has run against this yet** -- no syntax check, no
build, no packaged test. Before treating any part of this as working:

1. Run the same kind of local C++17 syntax check used for prior slices
   against the two changed source files.
2. Commit and push, then run Windows CI -- `RenegadeImportTests`' extended
   assertions need to pass.
3. Package Release and repeat an import of `windmill_in_soviet_village.glb`
   (or any animated GLB) on both DX12 and Vulkan; confirm the blades (or
   equivalent) visibly animate immediately on placement with no extra
   click, that Undo stops and removes the animation along with the rest of
   the imported hierarchy, that Redo resumes it, and that Save/close/Reopen
   preserves the playing state. A model that imports but does not animate,
   or an Undo that leaves an orphaned animation still playing, both stop
   this gate.

## Bug fix, uncommitted: Import Scale panel dropdown was unselectable

The project owner packaged and ran the already-shipped Import Scale panel
(models import "beautifully" and now animate) and reported the panel opens
but its combo box "won't let me select anything."

Root cause, confirmed by reading `WickedEngine/WickedEngine/wiGUI.cpp`
rather than guessed: `Window::Render` scissor-clips every child widget to
`widget->parent->scissorRect` -- the window's own rectangle -- including a
`ComboBox`'s dropdown list once it opens. `ComboBox::GetDropOffset`'s
auto-flip-upward logic only checks the dropdown against the full canvas
height (`screenheight`), never against the parent window's bounds, so it
never rescues a dropdown that would overflow a *short* parent -- it only
flips if the drop would go off the bottom of the whole screen. The Import
Scale panel is 168px tall with the combo sitting near its bottom edge, so
its three-item dropdown (roughly 28px header + 3 x 28px items = ~112px)
had almost nowhere to render before hitting the panel's own scissor edge
and disappearing. The combo logic itself was never broken -- the options
were just being drawn into a region that gets cut away.

Fixed in `StudioApplication.cpp`'s `ResizeLayout()`: `importScalePanel_`
grew from 168 to 288px tall, and `importScaleApplyButton_`/
`importScaleDismissButton_` moved from y=126 to y=234 to leave the combo's
full dropdown room to render inside the panel's own bounds instead of
immediately crowding it with the button row.

Changed files:

- `Studio/src/StudioApplication.cpp`
- `HANDOFF.md`

**No local validation has run against this yet** -- no syntax check, no
build, no packaged test. Before treating this as fixed:

1. Run the same kind of local C++17 syntax check used for prior slices.
2. Commit and push, then run Windows CI.
3. Package Release, reimport a GLB, and confirm the combo's dropdown is
   fully visible (all three options readable, not clipped) and each is
   actually clickable/selectable, on both DX12 and Vulkan.

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
