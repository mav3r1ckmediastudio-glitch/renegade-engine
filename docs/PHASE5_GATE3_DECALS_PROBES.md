# Phase 5 Gate 3 — Decals and Environment Probes

## Status

Preparation only. This contract is staged on `prep/phase5-gate3-decals-probes` from current `main` (`97598cf86f5c9c51a2dbc11c1a331c00773f0f2e`) while Phase 5 Gate 2 / PR #110 completes. Production implementation must be rebased onto post-Gate-2 `main` before coding begins.

## Purpose

Expose Wicked Engine's native decal and environment-probe systems through Renegade-owned services and Studio UI. Gate 3 must not introduce a parallel decal renderer, cubemap system, reflection volume model or serialization format.

The creator outcome is simple:

- place a decal into the level, assign its decal material inputs, move/rotate/scale it and see the projected result immediately;
- place an environment probe, define the reflection volume by transform scale, refresh/capture it and see reflections respond inside the probe bounds;
- save, reopen, Undo/Redo and package the same scene without editor/runtime disagreement.

## Pinned Wicked capability

### Decals

Wicked already exposes native `DecalComponent` authoring. Its editor exposes:

- placement mode;
- `SetBaseColorOnlyAlpha()`;
- `slopeBlendPower`;
- decal material participation through the associated Material component.

Wicked documents decal material support for:

- base colour;
- base-colour texture;
- emissive strength;
- normal-map texture;
- normal-map strength;
- surface-map texture;
- texture tiling / `TexMulAdd`.

### Environment probes

Wicked already exposes native `EnvironmentProbeComponent` authoring. Probes capture a 360-degree scene panorama and act as a reflection fallback where a better reflection method is unavailable. Their transform is meaningful: moving, rotating and scaling a probe defines the parallax-correct reflection influence volume.

Native probe controls include:

- refresh / mark dirty;
- real-time update mode;
- real-time update interval;
- MSAA;
- capture/view distance;
- resolution;
- imported cubemap identity where used;
- generated cubemap render data.

## Gate 3 creator-facing scope

### 1. ADD menu

Add two Renegade-owned creation entries:

- `ADD -> DECAL`
- `ADD -> ENVIRONMENT PROBE`

Both create native Wicked scene entities/components, become normal hierarchy items and participate in the existing selection, transform gizmo, duplicate/delete, dirty-state and save/open systems.

### 2. Decal placement workflow

Renegade should support two useful creation paths:

- direct creation at a sensible point in front of the editor camera;
- placement mode that projects onto the scene under the mouse, matching the creator ergonomics already used for placed lights/assets where practical.

A newly placed decal must have a usable default transform and must be visibly identifiable in the editor even when its projected texture is subtle or missing.

### 3. Decal Inspector

Selecting a decal exposes a dedicated `DECAL // NATIVE WICKED` section with, at minimum:

- Base-colour-only alpha toggle;
- Slope Blend;
- decal material selection / access to the existing governed Material workflow;
- clear readout that transform scale controls projection volume/size.

Gate 3 does not duplicate the Material editor. Texture and material controls already owned by Renegade MaterialService should be reused.

### 4. Decal editor marker / volume aid

Provide an editor-only decal marker or projection-volume aid so the creator can find and orient decals in 3D space. This must not render in Runtime or packaged builds.

### 5. Environment probe creation and hierarchy

`ADD -> ENVIRONMENT PROBE` creates a native Wicked probe entity with Transform plus `EnvironmentProbeComponent`.

The probe is selectable in the hierarchy and has an editor-only world marker/volume visualization. The probe transform's position/rotation/scale is authoritative for its capture location and parallax-correct influence bounds.

### 6. Environment Probe Inspector

Selecting a probe exposes `ENVIRONMENT PROBE // NATIVE WICKED` with:

- Refresh Probe;
- Resolution: 32 / 64 / 128 / 256 / 512 / 1024 / 2048 where supported by the pinned Wicked component;
- Real-time toggle;
- Update Interval;
- MSAA toggle;
- View Distance;
- generated/imported state readout where meaningful;
- GPU-memory / resolution information only if it can be presented cheaply and accurately.

Any control that Wicked cannot safely support in the pinned runtime must not be faked or shown disabled without purpose.

### 7. Probe preview

If practical within the bounded gate, show the selected probe's cubemap preview in the Inspector after it has rendered. This is useful but secondary to correct world reflections and may be deferred if it creates disproportionate UI/render-path complexity.

### 8. Import/export cubemap policy

Wicked's stock editor supports importing and exporting DDS cubemaps. Renegade must not simply expose an arbitrary filesystem path workflow if that bypasses the governed resource pipeline.

For Gate 3:

- generated scene probes are required;
- Refresh is required;
- arbitrary DDS import/export is deferred unless it can be routed through the existing governed resource/asset model without introducing path-owned content.

### 9. Commands, persistence and Undo/Redo

Persistent mutations must use Renegade `CommandService`.

Required command coverage:

- create decal;
- decal property edits;
- create environment probe;
- probe property edits;
- transform changes continue through the existing central transform command;
- duplicate/delete continue through existing scene commands where compatible.

Undo/Redo must restore the actual native component state, not only Studio widget values.

Both native components must survive normal WISCENE save/reopen with no sidecar representation.

### 10. Runtime / packaged parity

Gate 3 is not accepted on editor appearance alone.

A Release owner test must prove:

- a visible decal in Studio remains visible after save/reopen;
- the decal remains visible in Test Level / packaged Runtime;
- a probe changes reflections in a controlled test area;
- changing the probe's scale changes its parallax-correct influence volume;
- Refresh produces an observable updated reflection after changing nearby scene content;
- save/reopen preserves probe settings and transform;
- packaged Runtime sees the same decal/probe scene state.

## Explicitly deferred

Gate 3 does not claim:

- camera/vehicle attachment or runtime camera switching;
- scripted/dynamic decal spawning such as bullet-hole gameplay systems;
- vehicle tyre-mark systems;
- runtime probe-placement APIs;
- probe blending policy beyond Wicked's native behaviour;
- screen-space reflections, ray-traced reflections, DDGI or path tracing;
- global post-processing;
- arbitrary unmanaged DDS import/export;
- a new material/shader editor.

Those belong to later Phase 5 rendering gates or gameplay/runtime phases.

## Architecture rules

- Renegade Studio owns the UX; no stock Wicked Editor windows.
- Use native Wicked `DecalComponent` and `EnvironmentProbeComponent`.
- Do not create a second reflection/cubemap world.
- Use existing Transform, Material, Selection, Hierarchy, Command, Scene and Project services.
- Editor markers/volumes are overlays only and never serialize as gameplay geometry.
- Persistent creator state must have Undo/Redo + Save/Open proof.
- Owner visual/behavioural validation overrides green CI.

## Planned implementation shape

Expected bridge surface:

- `DecalProbeService.h/.cpp` or two small focused services if separation is cleaner after implementation audit;
- native state capture/apply helpers;
- create/edit commands;
- source/headless regressions;
- Studio ADD-menu integration;
- Inspector sections;
- editor-only markers/volume bounds;
- documentation and packaged acceptance notes.

The final implementation branch should be created from post-PR-#110 `main`, not from this prep base.
