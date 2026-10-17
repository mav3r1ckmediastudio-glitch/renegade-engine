# Renegade Studio — Night Sky & Aurora Borealis
## Feature specification v1.0 — 16 October 2026
**Status:** Design proposal; NOT implemented or tested.
**Scope:** One dedicated NIGHT SKY editor pane, shared native rendering path for Studio and standalone runtime.

## 1. Intent and visual target
The existing midnight scene retains appealing procedural stars but its realistic atmosphere appears near-black over much of the sky. Author a restrained deep-blue/indigo night atmosphere, with a slightly lifted blue-grey/violet horizon, while preserving dark terrain and water, native stars and their existing time-based flicker. Add an optional flowing Aurora Borealis. Night effects must respond automatically to the direction of Renegade's existing serialized Sun and must not change daylight, sunset, weather or legacy scenes unexpectedly.

Visual reference: owner's October 16 midnight screenshot showing sparse stars over a nearly black sky and dark-blue water. Target is atmospheric, not a saturated fantasy sky. Do not solve this by adding unrestricted global ambient illumination.

## 2. UI placement
Add **NIGHT SKY** as a sibling workspace/pane to SCENE, ENVIRONMENT, TERRAIN and RENDER, respecting Renegade Studio chrome, inspector visibility, resizable panels, existing keyboard navigation, and save/undo conventions. It is a single pane, NOT two new top-level tabs.

Sections, in this order:
1. **NIGHT ATMOSPHERE** — Enable natural night, Tint, Brightness, Horizon Glow.
2. **STARS** — existing Stars intensity control, moved or linked here rather than duplicated with a second backing value. Preserve native twinkling; do not add a separate Twinkle slider in v1.
3. **AURORA BOREALIS** — Enable, Intensity, Coverage, Animation Speed, Palette.
All widgets have tooltips and in-context status: e.g. "Visible after dusk"; "Requires Realistic Sky". Reuse existing Renegade sliders, numeric entry, colour control conventions, and command-backed drag transactions. No new pop-up wizard, per-object component, or artist-facing shader jargon.

## 3. Initial authoring controls
The numbers below are *proposed artistic starting values*, to be tuned on an actual packaged GPU build; they are not verified rendering values.

| Control | Proposed range / choices | Initial value for NEW scenes | Effect |
| --- | --- | --- | --- |
| Natural Night | Toggle | On | Enable procedural night sky colour contribution |
| Night Tint | sRGB colour | #0E1832 (dark indigo-blue) | Upper sky colour bias; internally convert sRGB -> linear |
| Night Brightness | 0.00–1.00 | 0.12 | Scales the faint nighttime atmospheric radiance, not global exposure |
| Horizon Glow | 0.00–1.00 | 0.20 | Gentle blue/grey/lavender atmospheric lift near the horizon |
| Stars | Existing 0.00–1.00 | Preserve scene's existing value (currently new-scene base may be 0) | Directly edits the existing WeatherComponent.stars |
| Aurora Enabled | Toggle | Off | Zero Aurora GPU rendering work when off |
| Aurora Intensity | 0.00–1.00 | 0.60 | Visible curtain luminance, night-weighted |
| Aurora Coverage | 0.00–1.00 | 0.40 | Horizontal/angular span and number of curtains |
| Aurora Speed | 0.00–2.00 | 0.35 | Rate of shader flow / folding; 0 freezes animation |
| Aurora Palette | Natural Green / Arctic Cyan / Violet | Natural Green | Controlled colour ramp; no additional colour sliders in v1 |

Render output must be tuned so a 0.12 Brightness setting produces a subtle, recognisable hue without washing out the existing stars. A black/nightless look remains achievable by turning Natural Night off or setting Brightness and Horizon Glow to zero. Aurora renders independently of Natural Night, but remains night-gated.

## 4. Time-of-day response
Use SunService's existing canonical WeatherComponent.sunDirection, not a second clock or arbitrary wall-clock time. Evaluate the visible sun elevation from the normalized direction, with robust conventions verified against the current renderer.

- **Night atmosphere:** smoothly ramps from zero at sun elevation >= -3 degrees to full at <= -12 degrees. Must avoid a visible discontinuity or jump at sunset.
- **Aurora:** smoothly ramps from zero at >= -6 degrees to full at <= -15 degrees, multiplied by Enabled/Intensity. Continue flowing throughout a paused static midnight sky.
- At sunrise, reverse the transition smoothly using the same gates.
- Editor PLAY DAY / PAUSE continues to move SunService only; sky and aurora observe the same live sun direction. No separate timeline, no duplicated sun animation, no editor-only sky change invisible to gameplay.
- Future scripted runtime day/night cycles should work by changing the same sun direction, without separate integration code.

## 5. Renderer / compositing contract
Current evidence (checked in local repo):
- WickedEngine/shaders/skyAtmosphere.hlsli: GetSunLuminance() contains procedural stars, density/visibility and a GetTime()-driven flicker.
- WickedEngine/wiScene_Components.h: native WeatherComponent.stars; EngineBridge WeatherState and Studio's Stars slider already author it.
- EngineBridge/src/SunService.cpp: single bridge for sun direction + directional light and current time-of-day authoring.
- EngineBridge/src/SceneService.cpp: one proving-ground environment currently explicitly sets weather.stars = 0.0f.
- Studio/src/StudioApplication.cpp: existing Environment weather / stars controls and preview-transaction conventions.

Proposed rendering path:
1. Evaluate a sky-only night colour contribution in the realistic-sky shader, e.g. dark zenith bias plus a soft horizon-weighted contribution. Composite with the physical atmosphere prior to tone mapping; preserve atmospheric transmittance and natural view-dependent fading.
2. Apply the night elevation weight to the new contribution only; never clamp the entire framebuffer, ambient illumination, material output or post-process black levels. Do not replace the native star calculation, threshold, rotation, or native twinkle.
3. For aurora v1, implement a sky-space procedural curtain effect using bounded coherent/fBM-style noise, slow advection and a vertical folded-ribbon mask. Use controlled emission colours, smooth feathered edges, small variation and restrained peaks. Aurora behind nearer clouds where the active cloud compositing path supports occlusion.
4. Aim for a native single sky contribution; do not instantiate many meshes, world particles, or a looping video/skybox. Shader modification must be included in the pinned Wicked Engine source/build/package for DX12 and Vulkan; do not rely on an editor-only shader override.
5. Existing realistic sky / realistic-with-clouds modes support the pane. SKYBOX TEXTURE mode disables the procedural night/aurora controls with a clear explanation, leaving original skybox visuals unchanged. Stars continue to obey their native realistic-sky constraint.
6. No aurora-emitted light affecting terrain/water in v1. Avoid the misleading appearance of genuine global illumination from the aurora. This can be a later, opt-in expansion.

## 6. Source-of-truth, persistence and command safety
- Extend a Renegade-owned **NightSkyState** service / renderer binding rather than introducing a separate scene entity or editor-only memory state.
- Store namespaced, versioned night/aurora parameters on the existing Environment weather entity's serialized MetadataComponent (existing Renegade pattern); e.g. `renegade.nightsky.schema=1`. Do not overwrite Wicked's native WeatherComponent.stars.
- **Legacy backward compatibility:** missing NightSky metadata means legacy visual behaviour, without silently re-colouring existing levels. Offer an explicit **Natural Night** preset/enable operation for legacy levels. New Environment creation may author the curated default settings. Validate creation paths for blank levels and proving ground.
- Implement Capture / Validate / Apply operations, clamping NaN/out-of-range parameters, command-backed setting changes, one Undo step per drag, Redo parity, save/reopen, and standalone scene load. Source of truth and rendering must be identical in Studio and runtime.
- On switching workspaces and closing the editor, no unintended authoring commits or hidden temporary changes. If package/runtime lacks the feature shader, fail gracefully without corrupting saved data.

## 7. Acceptance criteria
1. At 00:00 with Natural Night enabled, a restrained dark-blue/indigo sky and gentle gradient are visible. The upper sky is no longer uniformly black; horizon fade is smooth, not a distinct band.
2. With Natural Night disabled (or on legacy scenes lacking new metadata), output matches baseline within expected shader tolerances. Daylight and sunset screenshots match the baseline.
3. Existing Stars 0..1 behaviour, density and flicker are preserved. 0 still disables stars; 1 works alongside the new night colour.
4. Existing Clear / Scattered / Overcast / Storm presets, fog, clouds, precipitation, exposure and SunService continue to behave correctly. At dawn/dusk there are no sudden brightness or colour jumps.
5. With Aurora enabled at midnight, moving curtains clearly animate at low speed, without flickering artifacts, popping edges, visible seams, or distracting neon noise. Aurora 0 intensity and Enabled Off render nothing.
6. Changing SunService's time, azimuth and preview speed drives correct effect visibility and direction without dependence on editor viewport-only state. Gameplay/standalone uses the same scene data.
7. For SKYBOX TEXTURE mode, unsupported procedural controls are explicitly disabled; do not silently modify the skybox.
8. Save/close/reopen/Undo/Redo preserve all settings including the native star parameter and colour swatches. Existing scenes load without migration surprises.
9. Compare paired baseline/new screenshots at midday, dusk, midnight and dawn under clear/cloudy weather; include the October 16 owner screenshot as a before reference.
10. Profile feature OFF versus ON in Studio and standalone at 1080p and 1440p, including DX12 and Vulkan, recording frame-time delta and GPU cost. Optimization/quality reduction required if impact is disproportionate. No performance claims without measurements.
11. Run relevant bridge/editor tests and all required Windows CI configurations, then owner visual acceptance from a packaged Release build.

## 8. Delivery sequence
**NS-01 — Atmosphere foundation:** NightSkyState metadata + native command/UI bridge; implement dark indigo zenith/horizon floor only; preserve day sky and stars; verify save/undo/runtime.

**NS-02 — Aurora first slice:** Optional procedural aurora shader behind sky; five simple controls in the same pane, night gating and predictable animation; compare art direction on the owner's level.

**NS-03 — Validation and polish:** Clouds/transmittance/compositing, legacy compatibility, shader permutations, DX12/Vulkan, GPU profiling and packaged owner acceptance.

### Explicitly out of scope for v1
Moon phases, location/latitude/seasonal astronomy, real world star maps, Milky Way, meteors, aurora weather forecasts, physics-driven volumetric aurora, sky-to-world GI, new runtime clock, or a large night-sky preset library.

## 9. Decision
One dedicated **NIGHT SKY** workspace with NIGHT ATMOSPHERE, STARS and AURORA BOREALIS grouped sections. Atmospheric night floor and aurora are independent, non-destructive and scene-persistent. Confirm look and performance on the actual user's sky before calling either feature done.
