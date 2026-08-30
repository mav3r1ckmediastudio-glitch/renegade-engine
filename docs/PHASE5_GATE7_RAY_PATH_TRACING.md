# Phase 5 Gate 7 — Ray Tracing and Path Tracing

## Status

Gate 7 starts from accepted Gate 6 `main` at `a0b7e1289b5e345d9990b49732ae36a5f3cef7be`.

Production branch: `phase5/scene-render-gate7-ray-path-tracing`.

Gate 7 extends the accepted Renegade `RENDER` workspace and shared scene-owned render-settings state for real-time ray-tracing effects, while exposing Wicked's path tracer as a Renegade-owned Studio reference-preview mode. It does not expose the stock Wicked Editor, create a second scene model, or persist an offline renderer as a gameplay setting.

## Boundary

### REAL-TIME RAY TRACING

Creator-facing authored state:

- RTAO as the fifth Ambient Occlusion mode when hardware ray tracing is available;
- Ray-traced shadows enabled;
- Ray-traced reflections enabled;
- Ray-traced reflections range — 1.0 to 10000.0;
- Ray-traced reflections quality — LOW / MEDIUM / HIGH;
- Ray-traced diffuse enabled;
- Ray-traced diffuse range — 1.0 to 100.0;
- Ray-traced diffuse quality — LOW / MEDIUM / HIGH;
- Surfel GI enabled.

RTAO, ray-traced shadows, ray-traced reflections and ray-traced diffuse are hardware-capability gated. Authored state remains serialized if a project is opened on unsupported hardware, but the live renderer must fail closed rather than pretending the effect is active. Opening the same scene on capable hardware must restore the authored effect without rewriting the scene.

Surfel GI follows Wicked's own global ray-traced surface-cache path. Its debug visualizers remain Gate 9 diagnostics.

### PATH TRACE // REFERENCE PREVIEW

Wicked's native `RenderPath3D_PathTracing` is exposed inside Renegade Studio as an advanced reference-preview mode:

- START PATH TRACE PREVIEW / RETURN TO REALTIME;
- target sample count — 1 to 2048, default 1024;
- raytrace bounce count — 1 to 10;
- live sample/progress readout;
- denoiser availability/progress readout when Wicked was built with Open Image Denoise;
- RESTART accumulation action.

Path tracing is deliberately editor/tool state rather than WISCENE render state. It is an offline/reference renderer, not a gameplay mode. Test Level and packaged Runtime therefore retain the scene's real-time render path while matching all persisted real-time Gate 7 ray-tracing settings.

Wicked's path tracer can use a fallback ray-tracing path on hardware without native ray-tracing support, but that fallback can be very slow. Renegade must state this explicitly rather than disabling reference preview entirely.

## Architecture

- `RenderSettingsState` advances from schema v2 to schema v3.
- Schema v1 and v2 carriers migrate losslessly; all Gate 5/6 values survive and Gate 7 values receive deterministic defaults.
- Real-time Gate 7 state continues through the existing `SetRenderSettingsCommand` / Metadata carrier / `ApplyRenderSettingsToPath()` seam.
- Studio, Test Level and packaged Runtime consume the same persisted real-time state.
- No second real-time renderer state object is introduced.
- Studio's existing render-path class gains Wicked path-tracing capability while remaining the owner of Renegade chrome, GUI and editor lifecycle. Entering reference preview switches only the native 3D render implementation; it does not activate the stock Wicked Editor or a parallel scene.
- Leaving RENDER or leaving path-trace preview restores the normal raster buffers and re-applies the authored real-time state.

## Native Wicked mapping

Real-time:

- RTAO → `RenderPath3D::AO_RTAO`;
- ray-traced shadows → `wi::renderer::SetRaytracedShadowsEnabled`;
- ray-traced reflections → `setRaytracedReflectionsEnabled`, `setRaytracedReflectionsRange`, `setRaytracedReflectionsQuality`;
- ray-traced diffuse → `setRaytracedDiffuseEnabled`, `setRaytracedDiffuseRange`, `setRaytracedDiffuseQuality`;
- Surfel GI → `wi::renderer::SetSurfelGIEnabled`.

Reference preview:

- native renderer → `wi::RenderPath3D_PathTracing`;
- target samples → `setTargetSampleCount`, 1–2048;
- progress → `getCurrentSampleCount` / `getProgress`;
- reset → `resetProgress`;
- bounce count → `wi::renderer::SetRaytraceBounceCount`, 1–10;
- denoiser status → `isDenoiserAvailable` / `getDenoiserProgress`.

## Deterministic defaults

- RTAO: not selected;
- ray-traced shadows: OFF;
- ray-traced reflections: OFF;
- ray-traced reflections range: 10000.0;
- ray-traced reflections quality: MEDIUM;
- ray-traced diffuse: OFF;
- ray-traced diffuse range: 10.0;
- ray-traced diffuse quality: MEDIUM;
- Surfel GI: OFF;
- path-trace target samples: 1024 session/tool default;
- raytrace bounce count: 1 session/tool default.

## Creator UX

Gate 7 extends the existing scrollable `RENDER` workspace with:

- `RAY TRACING // REALTIME`;
- hardware capability status;
- RT shadows;
- RT reflections toggle/range/quality;
- RT diffuse toggle/range/quality;
- Surfel GI;
- `PATH TRACE // REFERENCE PREVIEW`;
- start/return button;
- samples;
- bounces;
- restart;
- progress/status.

RTAO is integrated into the existing AO selector. RTAO uses AO Power and AO Range; AO Sample Count remains SSAO-only.

Controls whose effect is unavailable or disabled must be visibly disabled. The Gate 5 window-construction rule remains mandatory: controls are attached while the RENDER window is enabled/visible, and the completed window is hidden only after every Gate 5–7 child has been attached.

## Explicitly deferred

- DDGI volume/probe authoring — requires its own scene/probe contract;
- Surfel/DDGI debug views, BVH visualizers and other renderer diagnostics — Gate 9;
- lightmap baking — Gate 8;
- FSR/FSR2/dynamic resolution — bounded graphics-quality follow-up;
- custom ray/path shaders — SDK/specialist authoring.

## Acceptance

Gate 7 is not complete until one owner Release build proves:

1. Gate 5/6 RENDER controls and LUTs remain intact and interactive.
2. Hardware capability state is reported truthfully.
3. RTAO can be selected on capable hardware and AO Range remains active while AO Samples remain disabled.
4. RT shadows visibly change a suitable shadowed scene.
5. RT reflections enable live and Range/Quality respond on reflective materials.
6. RT diffuse enables live and Range/Quality respond in a suitable indirectly lit scene.
7. Surfel GI can be toggled without destabilising the renderer.
8. Representative real-time Gate 7 edits Undo/Redo and survive save/reopen.
9. Test Level matches the authored real-time Gate 7 state.
10. Packaged Runtime matches the authored real-time Gate 7 state.
11. Unsupported hardware fails closed for hardware-only real-time RT effects without losing authored state.
12. START PATH TRACE PREVIEW keeps Renegade Studio chrome present and renders the same scene/camera through Wicked's native path tracer.
13. Changing the camera restarts accumulation as Wicked intends.
14. Sample target and bounce controls work, RESTART resets accumulation, and progress advances.
15. RETURN TO REALTIME restores the accepted raster RENDER state without scene mutation.
16. Leaving RENDER while path tracing is active safely returns to realtime.
17. A schema-v2 Gate 6 carrier migrates to v3 without losing any Gate 5/6 authored values.

Green compilation alone is not acceptance. Owner visual/behavioural evidence overrides CI.

## CI and regression requirements

Automated coverage must lock:

- v1/v2 → v3 migration;
- Gate 7 defaults and sanitization;
- Metadata round-trip and Undo/Redo/no-op filtering;
- exact Wicked real-time setter/getter mappings;
- capability-gated expected live state;
- Runtime continuing to consume the same `RenderSettingsState` seam;
- RTAO enum and AO dependent-control rules;
- path-trace preview deriving from / invoking the pinned `RenderPath3D_PathTracing` seam without stock Wicked Editor integration;
- sample/bounce ranges and reset/progress hooks;
- Gate 5 RENDER child-enable regression;
- four required Windows checks before owner testing.
