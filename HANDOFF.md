# Current Handoff

**Date:** 2026-07-30

**Project:** Renegade Engine (working title)

**Active phase:** 3 — Studio foundation

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Published failing milestone:** `c4eb43d` — Record the Phase 3 editor
usability handoff

**Prepared command implementation:** `cb04cf4` — Add undoable scene editing
commands

**Prepared Studio implementation:** `547906a` — Build the Phase 3 editor
usability milestone

**Prepared CI repair:** `08c8f12` — Fix headless editor command tests

**Pinned Wicked commit:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Accepted baseline

Phase 2 remains closed with:

```text
DX12 PASS / VULKAN PASS / DPI PASS / INPUT PASS / HDR NOT AVAILABLE
```

The project owner launched the first Phase 3 Project Hub build on an NVIDIA
GeForce RTX 4070 Ti. Project creation, scene loading, hierarchy population,
the live Proving Ground, and a VSync-limited 75 FPS passed.

Viewport click selection and fly-camera navigation at `8fc89ba` passed on the
project owner's Windows system. Selected-object outlining also works, but its
current silhouette is too thick; that is recorded for a later visual-polish
milestone and does not block this functional repair.

The Hub and Proving Ground remain functional visual baselines, not acceptance
of the final approved holographic mockups.

## Prepared editor-usability outcome

The next substantial Studio milestone adds:

- generated grid helpers hidden from the creator-facing hierarchy;
- a compact FPS readout confined to the viewport;
- an unobstructed command bar;
- Position, Rotation in degrees, and Scale Inspector fields;
- Move, Rotate, and Scale gizmo modes;
- W/E/R tool shortcuts;
- focus-on-selection with F;
- duplicate with Ctrl+D;
- delete with Delete;
- direct Save with Ctrl+S;
- Save As with Ctrl+Shift+S;
- Undo and Redo with Ctrl+Z/Ctrl+Y;
- undoable complete-transform edits;
- undoable recursive entity duplication; and
- undoable recursive entity deletion.

The selection service remains shared between the viewport, hierarchy,
Inspector, outline, and gizmo. Wicked source and the pinned submodule pointer
remain unchanged.

## Windows CI failure and focused repair

GitHub Actions compiled the exact `c4eb43d` Studio milestone successfully in
Windows x64 Debug and Release. `RenegadeBridgeTests.exe` then crashed in both
configurations.

The new duplicate/delete commands called `Scene::Update()` after ECS
mutations. Wicked's scene update requires an initialized graphics device, but
the bridge test is intentionally headless. The hierarchy-filter test would
have reached the same renderer dependency by constructing the full Proving
Ground.

Repair `08c8f12`:

- removes renderer-dependent scene updates from duplicate/delete commands;
- leaves scene advancement with Studio's normal `RenderPath3D` frame loop; and
- replaces the rendered Proving Ground hierarchy fixture with lightweight
  named ECS entities.

The editor features, Wicked source, submodule pin, WISCENE format, and Runtime
remain unchanged.

## Validation completed in this workspace

The following commands passed after implementation:

```text
g++ -std=c++17 -D__SCE__ -I WickedEngine/WickedEngine \
  -I WickedEngine/Editor -I EngineBridge/include \
  -fsyntax-only Studio/src/StudioApplication.cpp

g++ -std=c++17 -D__SCE__ -I WickedEngine/WickedEngine \
  -I EngineBridge/include -fsyntax-only \
  EngineBridge/src/CommandService.cpp EngineBridge/src/SceneService.cpp

g++ -std=c++17 -D__SCE__ -I WickedEngine/WickedEngine \
  -I EngineBridge/include -fsyntax-only Tests/BridgeCommandTests.cpp

git diff --check
```

The same gates passed after repair `08c8f12`. `RenegadeBridgeTests` still
covers complete transform state, duplicate and delete Undo/Redo, generated
hierarchy filtering, prior repeated history, no-op filtering, selection,
hierarchy, and project lifecycle behaviour without constructing a rendered
scene.

Every `docs/FEATURE_MATRIX.csv` row has 16 fields. The pinned Wicked submodule
pointer is unchanged.

The `-D__SCE__` syntax check only bypasses Wicked's unavailable Linux SDL
platform declarations. It does not represent a PlayStation build.

CMake, MSVC, the Windows SDK, and a render-capable Windows environment are
unavailable here. GitHub Actions and the project owner's GPU remain the compile
and visual authorities.

## Publication order

1. Merge and publish the focused CI-repair bundle on top of exact
   `c4eb43d`.
2. Require the Renegade Studio and Windows baseline workflows to pass Debug and
   Release, including `RenegadeBridgeTests`.
3. Download the Release Studio artifact for the exact repaired commit.
4. Extract `RenegadeStudio-Release.zip` into a fresh folder.
5. Follow `README-FIRST.txt`.
6. Report:

```text
DX12 EDITING PASS / HIERARCHY PASS / TRANSFORM PASS / FOCUS PASS /
DUPLICATE-DELETE PASS / HISTORY PASS / SAVE PASS / SHORTCUTS PASS /
RECENTS PASS / VULKAN EDITING PASS
```

Visual or behavioural failure overrides green CI.

## Known limits and risks

- The Studio milestone compiled in MSVC Debug and Release at `c4eb43d`, but the
  repaired bridge test has not yet rerun in GitHub Actions.
- Recursive duplicate/delete restoration relies on Wicked's in-memory entity
  serialization with entity remapping disabled; automated coverage is prepared
  but the Windows test executable remains authoritative.
- Euler rotation fields have the same conversion limitations as Wicked's
  reference Transform window.
- The hierarchy convention hides generated entities whose names start with
  `__renegade_internal_`; creator naming and reserved-name validation are not
  yet exposed.
- Delete has no confirmation because it is undoable. Unsaved-change and
  dirty-state presentation remain open Phase 3 work.
- Camera speed and editor layout remain session-only.
- The live holographic shell and Proving Ground still require substantial
  visual refinement against the approved concept.
- Scene tabs, docking, formal dirty state, crash recovery, asset import, and
  the Identity Handshake remain open.
- This milestone is editor-only. `RenegadeRuntime` is unchanged.

## Next work after this milestone passes

1. Correct any Windows compile or behavioural failure.
2. Add scene tabs, formal dirty-state tracking, unsaved-change prompts, and
   crash-safe recovery.
3. Refine the live Proving Ground terrain, atmosphere, lighting, and materials
   against the approved visual target.
4. Begin the real Identity Handshake only once the Project Hub transition it
   reveals is stable.
