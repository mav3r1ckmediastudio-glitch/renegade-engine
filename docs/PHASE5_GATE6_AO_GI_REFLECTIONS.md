# Phase 5 Gate 6 — Ambient Occlusion, Global Illumination and Reflections

## Status

Gate 6 starts from accepted Gate 5 `main` at `0b3a162b88a951097413b029d262487d892d7a6a`.

Production branch: `phase5/scene-render-gate6-ao-gi-reflections`.

Gate 6 extends the existing Renegade `RENDER` workspace and scene-owned render-settings carrier. It does not create a second renderer, a second persistence model, or stock Wicked Editor UI.

## Boundary

Gate 6 exposes the high-value non-ray-tracing AO/GI/reflection controls already present in the pinned Wicked `RenderPath3D` and renderer globals:

### AMBIENT OCCLUSION

- OFF
- SSAO
- HBAO
- MSAO
- AO Power — 0.25 to 8.0
- AO Range — 1.0 to 100.0, enabled only for SSAO
- AO Sample Count — 1 to 16, enabled only for SSAO

Wicked RTAO is deliberately deferred to Gate 7 because it is ray-tracing authoring.

### GLOBAL ILLUMINATION

- SSGI enabled
- SSGI Depth Rejection — 0.1 to 100.0
- GI Boost — 1.0 to 10.0

Surfel GI, DDGI and ray-traced diffuse are not silently folded into this gate. Surfel/ray-traced diffuse belong with Gate 7 hardware-aware ray-tracing exposure; DDGI requires a separate scene/probe authoring contract rather than pretending it is only a RenderPath toggle.

### REFLECTIONS

- Planar reflections enabled
- Planar reflection resolution scale — 0.25 to 2.0
- Planar reflection MSAA — 1X / 2X / 4X / 8X
- SSR enabled
- SSR quality — LOW / MEDIUM / HIGH
- Reflection roughness cutoff — 0.0 to 1.0

Existing Environment Probes from Phase 5 Gate 3 remain the authored probe component workflow. Gate 6 completes the global RenderPath reflection policy around those scene components; it does not duplicate probe authoring.

Ray-traced reflections remain Gate 7.

## Persistence and schema migration

Gate 6 extends `RenderSettingsState` rather than adding another hidden carrier.

The render-settings schema advances from v1 to v2. Compatibility is mandatory:

- v2 writes all Gate 5 and Gate 6 state;
- a v1 Gate 5 carrier must load losslessly, preserving every Gate 5 value and supplying documented Gate 6 defaults;
- unknown future/invalid schema versions fail safely to deterministic defaults;
- the same state is applied by Studio, Test Level Runtime and packaged Runtime;
- creator edits remain command-backed with Undo/Redo and normal WISCENE dirty/save behaviour.

## Native Wicked defaults retained for fresh scenes

- AO mode: OFF
- AO range: 1.0
- AO samples: 16
- AO power: 1.0
- SSGI: OFF
- SSGI depth rejection: 8.0
- GI Boost: 1.0
- Planar reflections: ON
- Planar reflection resolution scale: 0.25
- Planar reflection MSAA: 4X
- SSR: OFF
- SSR quality: MEDIUM
- Reflection roughness cutoff: 0.6

These defaults are part of the scene-level Renegade authoring contract once Gate 6 is present, so a newly loaded scene cannot inherit arbitrary values from a previously edited level.

## Runtime/native application rules

`ApplyRenderSettingsToPath()` remains the single native application seam.

- AO maps to `RenderPath3D::setAO`, `setAOPower`, `setAORange` and `setAOSampleCount`.
- SSGI maps to `setSSGIEnabled` and `setSSGIDepthRejection`.
- GI Boost maps to the pinned renderer global `wi::renderer::SetGIBoost`; scene switching must reassert the authored value.
- Planar reflections map to `setReflectionsEnabled` and `setPlanarReflectionQuality`.
- SSR maps to `setSSREnabled`, `setSSRQuality` and `setReflectionRoughnessCutoff`.

Renderer resource allocation remains Wicked-owned. Renegade does not allocate parallel AO/GI/reflection buffers.

Interactive slider preview should call only the exact corresponding native setter, then collapse the completed drag into one `SetRenderSettingsCommand` history entry, matching the accepted Gate 5 pattern.

## Creator UX

Gate 6 adds three sections below the existing Gate 5 controls in the same scrollable `RENDER` workspace:

- `AMBIENT OCCLUSION // NATIVE WICKED`
- `GLOBAL ILLUMINATION // SCREEN SPACE`
- `REFLECTIONS // SCREEN + PLANAR`

Controls that have no effect in the selected mode must be visibly disabled rather than remaining interactive and misleading.

The Gate 5 Window-construction rule remains mandatory: every child is attached while the RENDER Window is enabled/visible, and the completed Window is hidden only after all Gate 5/6 children have been added.

## Explicitly deferred

- RTAO — Gate 7
- ray-traced reflections — Gate 7
- ray-traced diffuse / hardware RT GI — Gate 7
- path tracing — Gate 7
- DDGI volume/probe authoring — later bounded world/rendering gate unless Gate 7 establishes a suitable scene contract
- lightmap/baking — Gate 8
- render diagnostics/debug visualizers — Gate 9

## Acceptance

Gate 6 is not complete until one owner Release build proves:

1. Existing Gate 5 controls and LUTs remain intact and interactive.
2. AO OFF/SSAO/HBAO/MSAO switch live without instability.
3. SSAO Power/Range/Sample controls visibly affect a suitable contact-shadow scene; Range/Samples disable outside SSAO.
4. SSGI can be toggled and produces visible bounced screen-space lighting in a suitable scene.
5. SSGI Depth Rejection changes the screen-space result without destabilising the frame.
6. GI Boost visibly changes indirect-light contribution where GI is present.
7. SSR can be toggled, Quality can be changed, and Roughness Cutoff changes which reflective materials receive SSR.
8. Planar reflections can be toggled; resolution scale and planar MSAA apply without affecting the main AA mode.
9. Representative Gate 6 slider/toggle/mode edits Undo/Redo correctly.
10. Save/reopen restores the same Gate 6 state.
11. Test Level matches Studio.
12. Packaged Runtime matches Studio.
13. A fresh scene receives deterministic Gate 6 defaults rather than leaked prior-level renderer state.
14. A v1 Gate 5 settings carrier migrates to v2 without losing any Gate 5 authored values.

Green compilation alone is not acceptance; owner visual/behavioural evidence overrides CI.

## CI and regression requirements

Gate 6 automated coverage must lock:

- schema-v1 to schema-v2 migration;
- Gate 6 default and sanitization rules;
- Metadata round-trip / native entity serialization;
- Undo/Redo and no-op filtering with Gate 6 fields;
- non-resource-owning native getter/readback coverage where possible without a graphics device;
- exact Wicked setter presence for AO, SSGI, GI Boost, SSR and planar reflections;
- Studio/Runtime continuing to consume the same `RenderSettingsState` and `ApplyRenderSettingsToPath()` seam;
- the RENDER Window child-enable construction regression from Gate 5;
- four required Windows checks before owner testing.
