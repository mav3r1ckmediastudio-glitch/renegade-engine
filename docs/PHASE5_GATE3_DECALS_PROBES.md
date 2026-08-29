# Phase 5 Gate 3 — Decals and Environment Probes

## Status

Implementation active on `phase5/scene-render-gate3-decals-probes`, based on post-Gate-2 main `cde15e2809730df32ec234592de30963f59debb6`.

The native bridge/backend passed its first four-job CI proof at `ace2ce244b4ffe3b93c2775a15100b3b81f4fc33`. The complete Studio decal/probe authoring pass later reached a four-green checkpoint at `7070a254172048acb0ce5406a581c02fd495006e`.

Before owner acceptance, Gate 3 was deliberately extended so a creator can choose a decal image directly from local storage. That image is imported through Renegade's existing governed texture pipeline and bound to the projected decal's native Wicked material. The governed texture-picker head subsequently passed the Windows/Studio CI set at `67d78a4ddc95da0d280b0766cd447e3e80db3550`.

Owner testing then exposed a stale ADD popup hit region: the menu rendered eight entries while only five rows participated in popup hit testing, and the dispatch guard stopped before the final entries. This caused DECAL / ENVIRONMENT PROBE / IMPORT MODEL clicks to fall through into viewport selection instead of invoking their actions. Gate 3 now sizes the ADD popup for all eight items, dispatches items 0–7, and pins both values in the source contract.

Owner testing has since confirmed projected decal creation, multiple creator-supplied decal textures, Slope Blend, Base Colour R/G/B and Opacity all work in Studio. Environment-probe usability exposed the next gap: a small marker alone does not communicate what the probe captured. Gate 3 now enables Wicked's native environment-probe debug visualizer in Studio so probes display as reflective cubemap spheres with their true parallax-correct oriented influence boxes. The visualizer is disabled while Test Level owns preview execution and is never Runtime geometry.

## Purpose

Expose Wicked Engine's native decal and environment-probe systems through Renegade-owned services and Studio UI. Gate 3 must not introduce a parallel decal renderer, cubemap system, reflection volume model, texture store or serialization format.

The creator outcome is simple:

- place a projected decal into the level, choose a local decal texture, author its core material colour/opacity, move/rotate/scale it and see the projected result immediately;
- place an environment probe, see its captured cubemap directly on the editor sphere, define the reflection volume by transform scale, refresh/capture it and see reflections respond inside the probe bounds;
- save, reopen, Undo/Redo and package the same scene without editor/runtime disagreement.

## Pinned Wicked capability

### Decals

Wicked already exposes native `DecalComponent` authoring. Its editor exposes placement, `SetBaseColorOnlyAlpha()`, `slopeBlendPower`, and decal material participation through the associated Material component.

Wicked documents decal material support for base colour, base-colour texture, emissive strength, normal-map texture, normal-map strength, surface-map texture and texture tiling / `TexMulAdd`.

### Environment probes

Wicked already exposes native `EnvironmentProbeComponent` authoring. Probes capture a 360-degree scene panorama and act as a reflection fallback where a better reflection method is unavailable. Their transform is meaningful: moving, rotating and scaling a probe defines the parallax-correct reflection influence volume.

Native probe controls include refresh / mark dirty, real-time update mode, real-time update interval, MSAA, capture/view distance, resolution, imported cubemap identity where used and generated cubemap render data.

Wicked 0.72.55 added ocean rendering to environment-probe captures. Renegade's pinned Wicked commit `ad283cdf10ac4989078c77fc8b02a6d8daec6699` is dated 23 August 2026 and therefore already contains that ocean-aware probe path as well as later multi-camera/envprobe renderer work. Gate 3 deliberately uses Wicked's native capture path rather than a Renegade replacement.

Wicked's native debug renderer exposes `SetToDrawDebugEnvProbes()`, which renders environment probes as reflective spheres using their captured cubemaps and shows their affection/influence range as oriented bounding boxes. Gate 3 uses that native representation in Renegade Studio.

## Implemented creator-facing capability

### 1. ADD menu

Renegade Studio now exposes:

- `ADD -> DECAL`
- `ADD -> ENVIRONMENT PROBE`

Both use native Wicked scene creation through the Gate 3 bridge commands, become normal hierarchy/selection entities, use the existing transform gizmo, and participate in CommandService Undo/Redo and WISCENE save/open.

The ADD popup hit region and dispatch range explicitly cover all eight rendered ADD entries. This is source-contract locked so Decal, Environment Probe and Import Model cannot regress into visually rendered but non-clickable menu rows or leak the click into viewport selection.

New entities are created at a useful point in front of the current editor camera. A decal receives a shallow projection-volume transform; a new probe receives a larger cubic influence transform.

### 2. Decal Inspector

Selecting a decal exposes `DECAL // NATIVE WICKED` with:

- Base-colour-only Alpha;
- Slope Blend;
- core material Base Colour R/G/B;
- material Opacity;
- `SELECT DECAL TEXTURE...` / `CHANGE DECAL TEXTURE...`.

The texture button opens a local image picker for PNG, TGA, DDS, JPG/JPEG, BMP and HDR. The selected image does not remain an unmanaged absolute-path dependency: `CreatorTextureWorkflowService` retains/registers it as a governed Renegade texture, `PrepareMaterialTextureAsset` resolves the project product, and `SetMaterialBaseColorTextureAssetCommand` binds it to the decal's native Wicked `BASECOLORMAP` while persisting its stable asset ID in WISCENE material metadata.

The material values reuse Renegade's existing `MaterialService`, `MaterialTextureAssetService` and CommandService. Gate 3 does not create a second decal-material or texture representation. The texture asset import itself is a governed project operation; the material binding is Undo/Redo-able through the existing material-texture command.

### 3. Decal editor marker / projection volume

Every authored decal receives an editor-only world marker. Selecting it displays its transformed projection-volume wireframe, making position, rotation and scale visually understandable. The marker is clickable and never serializes as Runtime geometry.

### 4. Environment Probe Inspector

Selecting a probe exposes `ENVIRONMENT PROBE // NATIVE WICKED` with:

- Resolution: 32 / 64 / 128 / 256 / 512 / 1024 / 2048;
- Real-time update;
- Update Interval;
- 8x MSAA capture;
- View Distance (`-1` = main-camera distance);
- Refresh Probe.

Persistent changes use `SetEnvironmentProbeCommand`. Refresh uses the native dirty/recapture seam and does not invent serialized refresh state.

### 5. Environment probe reflective preview / influence volume

Renegade Studio enables Wicked's native environment-probe debug rendering while editing. Each authored probe is therefore represented by a reflective sphere displaying that probe's actual captured cubemap together with the native oriented influence/parallax box. The existing Renegade marker remains available as the Studio selection affordance.

The preview is truthful rather than decorative: after Refresh, changes in the captured surroundings should be visible on the sphere. When Wicked ocean is enabled and within the probe's configured view distance, the ocean participates in the cubemap capture and should therefore be visible in the reflective preview where appropriate.

The debug visualizer is explicitly disabled while Test Level owns preview execution. It is a Studio debug representation only and is never serialized or packaged as gameplay geometry.

### 6. Commands, persistence and Undo/Redo

Persistent mutations use Renegade `CommandService`. Gate 3 covers native decal/probe creation and component editing; core decal material colour/opacity continues through the existing Material command, while the governed base-colour/alpha texture binding uses `SetMaterialBaseColorTextureAssetCommand`. Transform changes remain owned by the central transform command. Native entities are serialized through WISCENE with no Gate 3 sidecar.

Governed decal texture bindings persist stable IDs in serializable material metadata. On scene/project load, Renegade's existing material-texture restore path recreates the live Wicked resource from the governed project asset rather than depending on the original local source path.

## Runtime / packaged acceptance still required

Gate 3 is not accepted on editor appearance alone. Release owner acceptance must prove:

- ADD -> DECAL creates and selects a decal without selecting the viewport object behind the menu;
- ADD -> ENVIRONMENT PROBE creates and selects a probe without menu click-through;
- a creator can choose a local decal image and see it projected on scene geometry;
- the chosen governed decal texture persists through save/reopen and restores without needing the original external source path;
- transform scale/rotation visibly changes decal projection volume/result;
- decal core material edits, native decal properties and texture binding survive the intended Undo/Redo workflow and save/reopen;
- a probe appears in Studio as a reflective cubemap sphere with its oriented influence box;
- Refresh produces an observable new capture on the sphere after nearby scene content changes;
- with Wicked ocean enabled and visible to the probe, the probe capture/preview includes the ocean;
- a probe visibly changes reflections in a controlled reflective test area;
- probe scale changes its parallax/influence behaviour;
- probe settings and transform persist through save/reopen;
- Test Level / packaged Runtime sees the same serialized decal/probe scene state and governed decal texture;
- editor-only markers, reflective debug sphere and volume guides do not appear in Runtime.

## Explicitly deferred

Gate 3 does not claim camera/vehicle attachment or runtime camera switching; scripted/dynamic decal spawning; tyre-mark systems; runtime probe-placement APIs; probe blending policy beyond Wicked native behaviour; SSR, ray-traced reflections, DDGI or path tracing; global post-processing; or unmanaged DDS cubemap import/export.

The basic projected decal base-colour/alpha texture is part of Gate 3. Deeper decal material authoring—normal-map textures, surface maps, emissive maps, texture tiling/UV controls and custom shader selection—remains part of the later Phase 5 material/shader exposure gate.

## Architecture rules

- Renegade Studio owns the UX; no stock Wicked Editor windows.
- Use native Wicked `DecalComponent` and `EnvironmentProbeComponent`.
- Use Renegade's existing governed texture import/asset-binding pipeline for decal images; never retain an arbitrary local absolute path as the Runtime dependency.
- Use Wicked's native environment-probe capture and debug-sphere/OBB representation; do not create a second reflection/cubemap preview system.
- Do not create a second reflection/cubemap world or decal texture store.
- Use existing Transform, Material, MaterialTextureAsset, Selection, Hierarchy, Command, Scene and Project services.
- Editor markers/volumes are overlays only and never serialize as gameplay geometry.
- Persistent creator state must have Undo/Redo + Save/Open proof.
- Owner visual/behavioural validation overrides green CI.
