# Phase 5 Gate 9 — Render Diagnostics and Debug

## Status

Gate 9 starts from accepted Gate 8 `main` at
`0ac94c24b57b1d7bec29e57e72530d25d608a524`
(`Phase 5 Gate 8: lightmap and baking (#117)`).

Production branch: `phase5/scene-render-gate9-render-diagnostics`.

Pinned Wicked source:
`3a800b7134aafe58461093c8abb2e274d4e64033`.

Gate 9 closes the Phase 5 scene/render exposure programme with creator-facing
renderer diagnostics. These are Studio tools for inspecting an authored scene;
they are not gameplay render settings and must never leak into Test Level,
packaged Runtime or another project.

## Boundary

### DIAGNOSTIC VIEW // REALTIME

Expose the pinned Wicked renderer's native view overrides:

- SHADED — ordinary authored rendering;
- WIREFRAME OVERLAY — shaded scene with native wire overlay;
- WIREFRAME ONLY — native wireframe-only view.

Expose one mutually-exclusive shading diagnostic selector:

- NORMAL;
- NO ALBEDO;
- DIFFUSE ONLY;
- UNLIT.

The selector must drive Wicked's existing `SetDisableAlbedoMaps`,
`SetForceDiffuseLighting` and `SetForceUnlit` flags without allowing
contradictory combinations.

### SCENE / VISIBILITY DEBUG

Expose native, renderer-owned helpers that are directly useful while building a
scene:

- environment probe visualizers;
- camera/frustum visualizers;
- scene spatial-partition tree;
- ray-tracing BVH visualizer;
- freeze culling camera.

These map directly to the pinned `wi::renderer` debug APIs. Renegade must not
build replacement debug geometry for features Wicked already draws through
`DrawDebugWorld`.

### RENDERER DEBUG

Expose high-value diagnostics for Phase 5 systems already authored by Renegade:

- light-culling heatmap;
- TAA history/jitter debug, enabled in the UI only while the authored AA mode is
  TAA;
- Surfel GI debug selector, enabled only while Surfel GI is authored/live:
  - NONE;
  - NORMAL;
  - COLOR;
  - POINT;
  - RANDOM;
  - HEATMAP;
  - INCONSISTENCY.

The Surfel modes are the exact pinned Wicked `SURFEL_DEBUG` values; Gate 9 does
not invent a second GI visualizer.

### PROFILING

Expose Wicked's native CPU/GPU profiler through a Renegade-owned overlay inside
the Scene viewport. Do not instantiate the stock Wicked `ProfilerWindow`.
Renegade's existing status-bar FPS remains authoritative for the compact FPS
readout; Gate 9 must not re-enable the stock `InfoDisplayer` over Renegade
chrome.

The profiler overlay is part of Gate 9 final acceptance but may follow the first
usable visual-diagnostics slice so the owner can exercise the gate before
hardening.

### RESET DIAGNOSTICS

One creator action must restore every Gate 9 native flag to its clean default:

- normal shaded rendering;
- all debug helpers off;
- no frozen culling camera;
- Surfel debug NONE;
- native profiler disabled.

## Native Wicked mapping

The exact pinned `wiRenderer.h` exposes:

- `WIREFRAME_DISABLED`, `WIREFRAME_ONLY`, `WIREFRAME_OVERLAY`;
- `SetWireframeMode` / `GetWireframeMode`;
- `SetToDrawDebugPartitionTree`;
- `SetToDrawDebugEnvProbes`;
- `SetToDrawDebugCameras`;
- `SetDebugLightCulling`;
- `SetTemporalAADebugEnabled`;
- `SetFreezeCullingCameraEnabled`;
- `SetRaytraceDebugBVHVisualizerEnabled`;
- `SetDisableAlbedoMaps`;
- `SetForceDiffuseLighting`;
- `SetForceUnlit`;
- `SetSurfelGIDebugEnabled`.

The normal realtime `RenderPath3D` already invokes `DrawDebugWorld` and
`DrawWireframeOverlay`, so Gate 9 changes diagnostic state only. It does not
create a parallel render path or copy Wicked debug renderer code.

## State and lifecycle

Gate 9 state is deliberately transient Studio state:

- no `RenderSettingsState` schema bump;
- no WISCENE Metadata carrier;
- no Undo/Redo entry;
- no scene dirtying;
- no Test Level or packaged Runtime consumption;
- no project persistence.

All Gate 9 controls default clean/off when Studio starts. Renegade must
explicitly reset the native global debug flags when:

- the RENDER workspace is exited;
- path-trace reference preview is entered;
- the active project/scene is abandoned or replaced; and
- Studio shuts down or releases the render path.

The path tracer also calls Wicked `DrawDebugWorld`, while its render sequence
does not provide the same wireframe-overlay parity as realtime. Gate 9 therefore
keeps diagnostics a realtime inspection mode and disables/resets them during
path-trace reference preview.

## Creator UX

Gate 9 extends the existing scrollable Renegade `RENDER` workspace after Gate 8
with compact sections:

- `DIAGNOSTICS // VIEW`;
- `DIAGNOSTICS // SCENE`;
- `DIAGNOSTICS // RENDERER`;
- `DIAGNOSTICS // PROFILER`.

Controls must visibly reflect the live native state. Unsupported or contextually
inapplicable controls must disable rather than pretending to work.

The Gate 5 Window-construction rule remains mandatory: every Gate 9 child is
attached while the RENDER Window is enabled/visible, and the completed Window
is hidden only after all children have been attached.

## Explicitly deferred

Gate 9 is not a catch-all Wicked graphics menu. The following remain outside
this gate:

- DDGI debug until Renegade has a governed DDGI volume/probe authoring contract;
- VXGI voxel helpers while VXGI itself remains unexposed;
- variable-rate-shading debug while VRS is not part of accepted Renegade render
  state;
- emitter/force-field diagnostics — Effects programme;
- collider diagnostics — JP01/physics tooling;
- bone/spring diagnostics — animation/physics tooling;
- Wicked's stock grid helper — Renegade already owns its editor grid;
- FSR/FSR2/dynamic-resolution authoring — graphics-quality follow-up;
- RenderDoc integration, shader hot reload and raw render-target/resource
  inspectors — developer/tooling follow-up;
- stock Wicked `GraphicsWindow`, `ProfilerWindow` or other Editor UI.

## Delivery order

To avoid repeating Gate 8's late owner-discovery problem, Gate 9 is delivered in
this order:

1. lock this bounded contract against the exact Wicked pin;
2. wire the first usable realtime diagnostics surface;
3. build a real Release and obtain owner visual/interaction feedback;
4. only then add profiler/final lifecycle hardening and regression coverage;
5. run the final four required Windows checks and one final owner Release pass.

Green compilation is never creator acceptance.

## First usable owner test

The first Gate 9 Release should prove only the interaction surface:

1. RENDER opens with all diagnostics clean/off.
2. WIREFRAME OVERLAY visibly overlays geometry.
3. WIREFRAME ONLY visibly replaces shaded rendering.
4. SHADED restores ordinary rendering.
5. NO ALBEDO, DIFFUSE ONLY and UNLIT each visibly change the scene and NORMAL
   restores authored shading.
6. Environment-probe and camera helpers visibly toggle in a suitable scene.
7. Spatial-partition and raytrace-BVH helpers toggle without destabilising the
   renderer.
8. Freeze Culling Camera can be enabled and disabled.
9. Light-culling heatmap toggles.
10. TAA debug is available only when TAA is the authored AA mode.
11. Surfel debug is available only with Surfel GI and its modes change the
    debug output.
12. RESET DIAGNOSTICS returns a clean shaded viewport.
13. Leaving RENDER clears all diagnostics.
14. Entering path-trace reference preview clears/disables diagnostics.

This first test happens before final profiling/hardening work.

## Final acceptance

Gate 9 is complete only when one final owner Release additionally proves:

1. the Renegade-owned native profiler overlay opens/closes in the viewport;
2. no diagnostic state survives save/reopen or project switching;
3. Test Level starts with a clean renderer;
4. packaged Runtime starts with a clean renderer;
5. Gate 5–8 RENDER controls remain interactive and behaviourally intact;
6. diagnostics reset cleanly after error/unsupported-context cases;
7. the final source tree contains no stock Wicked Editor UI integration.

## Regression requirements

Final automated coverage must lock:

- clean transient defaults and reset semantics;
- exact pinned Wicked setter/getter mappings;
- mutually-exclusive diagnostic shading state;
- exact Surfel debug enum mapping;
- TAA and Surfel contextual enable rules;
- no `RenderSettingsState` schema change caused by Gate 9;
- no Runtime/Test Level application seam for Gate 9 state;
- no Wicked `GraphicsWindow`, `ProfilerWindow` or grid-helper UI integration;
- Gate 5 RENDER child-enable construction regression;
- existing Gate 5–8 contracts remain in the build graph;
- four required Windows checks before final owner acceptance.
