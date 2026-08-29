# Phase 5 Gate 5 — Post-processing, HDR, Exposure and Anti-aliasing

## Status

Gate 5 starts from post-Gate-4 `main` at `7d6e5f2164993c84588fea3b44ef265ddc1db545`.

Production branch: `phase5/scene-render-gate5-post-processing`.

This gate exposes Wicked Engine's existing `RenderPath3D` post-processing and image-quality controls through Renegade Studio. Renegade does **not** introduce a parallel renderer or duplicate the native post-process implementation.

## Architectural rule

Unlike decals, probes and materials, Wicked post-process state is owned by `wi::RenderPath3D`; it is not a serializable scene component. Renegade therefore needs one small scene-owned, versioned render-settings document/state seam whose only job is to persist creator intent and apply it to the native Wicked render path.

The same persisted state must be consumed by:

- Renegade Studio's scene render path,
- Test Level Runtime,
- packaged Runtime.

Studio-only preview state must not be allowed to diverge from Runtime. A scene with no Renegade Gate 5 settings must fall back to the documented Gate 5 defaults rather than inherit arbitrary editor session state.

The persisted state must travel with the level/scene, because different levels may require different exposure, grading and anti-aliasing choices. It must survive ordinary WISCENE save/reopen and Test Level snapshot/package flows.

All creator edits must use Renegade's command/Undo/Redo architecture. Renderer setters themselves are the execution target, not the persistence model.

## Creator-facing workspace

Gate 5 introduces a native Renegade right-hand workspace/tab named:

`RENDER`

This becomes the durable home for global rendering controls added by Gates 5–9. It must use Renegade-owned chrome and controls; the stock Wicked editor post-process window is not surfaced.

### IMAGE // TONEMAPPING

Expose the pinned Wicked native controls:

- Tonemap: ACES / Reinhard / Uchimura
- Exposure
- Brightness
- Contrast
- Saturation
- HDR calibration

`wi::renderer::Tonemap::ACES` remains the default tonemapper unless an existing project/scene explicitly carries another Gate 5 value.

HDR calibration is exposed as Wicked's native render-path calibration value. Platform/OS HDR-output switching, swap-chain policy and monitor capability management are not invented by this gate; those remain a platform/runtime graphics-settings concern if Renegade later requires explicit output-mode selection.

### BLOOM // AUTO EXPOSURE

Expose:

- Bloom enabled
- Bloom threshold
- Eye adaptation / auto exposure enabled
- Eye adaptation key
- Eye adaptation rate

These operate directly on Wicked's native `RenderPath3D` implementation.

### ANTI-ALIASING // NATIVE WICKED

Expose a governed creator-facing anti-aliasing mode rather than independent incompatible checkboxes.

Required modes:

- OFF
- FXAA
- TAA
- MSAA 2X
- MSAA 4X
- MSAA 8X

Implementation rules:

- FXAA uses `RenderPath3D::setFXAAEnabled()`.
- TAA uses Wicked's native `wi::renderer::SetTemporalAAEnabled()`.
- MSAA uses the native render path sample-count control.
- Selecting a mode must establish a deterministic compatible state; for example, selecting MSAA must not silently leave FXAA/TAA enabled from a previous choice.
- Unsupported sample counts/capabilities must fail safely to a supported documented state rather than breaking render-target creation.

FSR/FSR2 and dynamic resolution are native Wicked capabilities but are **not** conflated with anti-aliasing in this first Gate 5 acceptance surface. They are performance/upscaling policy and may be added as a bounded Gate 5B or later graphics-quality pass without changing the persisted Gate 5 core contract.

### POST FX // CAMERA IMAGE

Expose the high-value native post effects that are independent of Gate 6 lighting/GI/reflection work:

- Depth of Field enabled
- Depth of Field strength
- Motion Blur enabled
- Motion Blur strength
- Sharpen enabled
- Sharpen amount
- Chromatic Aberration enabled
- Chromatic Aberration amount
- Dither enabled

Depth of Field deliberately reuses the camera focal distance/aperture authoring introduced in Phase 5 Gate 2; Gate 5 only owns whether the post effect runs and its native render-path strength.

CRT, custom post-process shaders and custom post-process chains are not part of the normal creator Inspector in this gate.

## Explicitly deferred to later Phase 5 gates

Gate 5 must not absorb these systems:

- AO modes/range/samples/power — Gate 6
- SSR / SSGI / ray-traced reflections / ray-traced diffuse — Gate 6/7
- planar-reflection quality — Gate 6
- path tracing / ray-tracing authoring — Gate 7
- lightmap/baking workflow — Gate 8
- render debug visualizers/profiler/diagnostics — Gate 9
- custom shader/post-process code registration — C++ SDK / later specialist authoring

Light shafts, volumetric-light policy and lens-flare authoring should remain with their lighting/environment ownership unless a later gate proves that render-path-only exposure is required.

## Defaults

A scene with no Gate 5 state must resolve deterministically to Renegade's documented baseline based on Wicked's pinned defaults, including:

- Tonemap: ACES
- Exposure: 1.0
- Brightness: 0.0
- Contrast: 1.0
- Saturation: 1.0
- Bloom: enabled
- Bloom threshold: 1.0
- Eye adaptation: disabled
- FXAA: disabled
- TAA: disabled unless explicitly selected by Renegade
- MSAA: 1X/off
- Motion blur: disabled
- Depth of field: enabled, retaining Wicked's native baseline behaviour until creator changes it
- Sharpen: disabled
- Chromatic aberration: disabled
- Dither: enabled

If Renegade changes a baseline during Gate 5 implementation, the changed value must be deliberate, documented and shared identically by Studio and Runtime.

## Undo/Redo and dirty state

Every creator-facing Gate 5 edit must:

1. capture the prior persisted render state,
2. apply the changed state to the active native Wicked render path,
3. commit through `CommandService`,
4. mark the scene dirty,
5. Undo back to the exact prior state,
6. Redo to the exact changed state.

Continuous sliders may preview live during drag, but one drag must collapse to one owner-facing Undo step just like existing Renegade slider authoring.

## Save/reopen and Runtime parity

Gate 5 acceptance requires the persisted state to be restored before the level is considered visually ready after load.

Test Level and packaged Runtime must apply the exact authored settings to their own `RuntimeRenderPath`; Runtime must not retain the current hardcoded FXAA/post defaults when Gate 5 state exists.

Editor chrome and authoring controls are never serialized into Runtime.

## Owner acceptance

Gate 5 is not complete until one owner build proves all of the following:

1. `RENDER` workspace is present and does not disturb Scene/Environment/Terrain/Physics workspaces.
2. Exposure changes are immediately and visibly reflected in Studio.
3. ACES/Reinhard/Uchimura can be selected and visibly alter an appropriate high-dynamic-range scene.
4. Brightness/contrast/saturation controls work live.
5. Bloom can be disabled/enabled and its threshold visibly changes bright-emissive bloom response.
6. Eye adaptation can be enabled and visibly adapts when moving between materially different lighting levels.
7. OFF/FXAA/TAA/MSAA 2X/4X/8X selection is deterministic and does not leave incompatible previous modes active.
8. Depth of field works with an authored Gate 2 camera focal-distance/aperture setup.
9. Motion blur, sharpen and chromatic aberration can each be enabled/disabled and their strength controls visibly respond.
10. Dither toggle works without destabilising the render path.
11. Undo/Redo works for representative slider, toggle and mode changes.
12. Save/reopen restores the authored render appearance and selected settings.
13. Test Level matches the authored Studio appearance within normal viewport/window/output differences.
14. Packaged Runtime uses the same authored state.
15. A new/fresh scene with no authored Gate 5 changes receives deterministic Renegade defaults rather than settings leaked from the previous editor scene.

## CI / regression requirements

Gate 5 must add headless/source-contract coverage for:

- state sanitize/default rules,
- persistence round-trip,
- command Undo/Redo,
- deterministic anti-aliasing-mode mapping,
- Studio native Wicked setter application,
- Runtime native Wicked setter application,
- absence of the old unconditional Runtime FXAA state overriding authored Gate 5 state,
- Studio/Runtime use of the same state schema/version.

Windows baseline Debug/Release and Renegade Studio Debug/Release remain authoritative compile/startup gates before owner testing.
