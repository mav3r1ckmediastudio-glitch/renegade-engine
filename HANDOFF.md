# Current Handoff

**Date:** 2026-07-30

**Project:** Renegade Engine (working title)

**Active phase:** 3 — Studio foundation

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Published branch before this increment:** `main` at
`b1ce1482a7c642017975ba115cd7b67fa474b932`

**Prepared viewport implementation:** `dc32684` — Add Phase 3 viewport
interaction

**Prepared viewport specification:** `46f382b` — Define Phase 3 viewport
interaction gate

**Pinned Wicked commit:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Accepted baseline

Phase 2 remains closed with:

```text
DX12 PASS / VULKAN PASS / DPI PASS / INPUT PASS / HDR NOT AVAILABLE
```

The project owner launched the exact Phase 3 Project Hub build on an NVIDIA
GeForce RTX 4070 Ti. The Hub created a project and entered the workspace with
33 scene entities. The live counter remained at 75 FPS, consistent with VSync
limiting the renderer to the display's 75 Hz refresh rate. The apparent
single-digit value inferred from an earlier screenshot was not the running
frame rate.

The Hub and Proving Ground are functional baselines. Their live visual
presentation is not accepted as matching the approved holographic mockups yet.

## Prepared viewport outcome

The next Studio increment adds:

- direct left-click selection of rendered scene objects in the 3D viewport;
- empty-space click deselection;
- one shared `SelectionService` state for the viewport, hierarchy, inspector,
  and transform gizmo;
- a cyan editor-only silhouette around the selected object;
- restoration of the object's previous stencil before Save As or scene reload;
- right-mouse freelook initiated only inside the viewport;
- W/A/S/D movement while freelooking;
- Q/E vertical movement;
- Shift acceleration;
- mouse-wheel movement-speed adjustment over the viewport; and
- explicit viewport bounds so panel input cannot select geometry or move the
  camera behind the UI.

Wicked remains exact and unmodified. The change is confined to Renegade Studio
and its existing bridge selection boundary.

## Validation completed in this workspace

The following commands passed:

```text
g++ -std=c++17 -D__SCE__ -I WickedEngine/WickedEngine \
  -I WickedEngine/Editor -I EngineBridge/include \
  -fsyntax-only Studio/src/StudioApplication.cpp

g++ -std=c++17 -D__SCE__ -I WickedEngine/WickedEngine \
  -I EngineBridge/include -fsyntax-only Tests/BridgeCommandTests.cpp

git diff --check
```

Every `docs/FEATURE_MATRIX.csv` row still has 16 fields. The pinned Wicked
submodule pointer is unchanged.

The `-D__SCE__` syntax check only bypasses Wicked's unavailable Linux SDL
platform declarations. It does not represent a PlayStation build.

CMake, MSVC, the Windows SDK, and a render-capable Windows environment are
unavailable here. GitHub Actions and the project owner's GPU remain the compile
and visual authorities.

## Windows publication and verification

1. Publish all prepared commits to `main`.
2. Require the Renegade Studio and Windows baseline workflows to pass Debug and
   Release.
3. Download the Release Studio artifact for the exact published commit.
4. Extract `RenegadeStudio-Release.zip` into a fresh folder.
5. Follow `README-FIRST.txt`.
6. Report:

```text
DX12 VIEWPORT SELECT PASS / OUTLINE PASS / CAMERA PASS /
SAVE ISOLATION PASS / VULKAN VIEWPORT PASS
```

Visual or behavioural failure overrides green CI.

## Known limits and risks

- The new selection silhouette has not yet been compiled or visually inspected
  on Windows. DX12 and Vulkan must both pass.
- The silhouette applies to rendered object geometry. A hierarchy-only entity
  such as a light can still be selected and transformed but does not yet
  receive a mesh silhouette.
- Camera speed is session-only. Input rebinding, persisted camera preferences,
  orbit navigation, focus-on-selection, and orthographic navigation remain
  Phase 3 work.
- The live holographic shell and Proving Ground still require substantial
  visual refinement against the approved editor concept.
- Recent-project settings still live beside the portable Studio package.
- Scene tabs, formal dirty-state tracking, explicit Save, docking, and layout
  persistence remain open.
- This increment is editor-only. `RenegadeRuntime` is unchanged and does not
  require a new gameplay verification run.

## Next bounded work after this increment passes

1. Correct any Windows compile, selection, outline, camera, or save-isolation
   failure.
2. Add focus-on-selection and complete transform-tool switching.
3. Continue the scene-tab, dirty-state, Save, and unsaved-change workflow.
4. Refine the live Proving Ground and holographic workspace against the
   approved visual target.
