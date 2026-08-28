# Phase 5 Gate 3 — Decals and Environment Probes

## Status

Implementation active on `phase5/scene-render-gate3-decals-probes`, based on post-Gate-2 main `cde15e2809730df32ec234592de30963f59debb6`.

## Purpose

Expose Wicked Engine's native decal and environment-probe systems through Renegade-owned services and Studio UI. Gate 3 must not introduce a parallel decal renderer, cubemap system, reflection volume model or serialization format.

The creator outcome is simple:

- place a decal into the level, assign its decal material inputs, move/rotate/scale it and see the projected result immediately;
- place an environment probe, define the reflection volume by transform scale, refresh/capture it and see reflections respond inside the probe bounds;
- save, reopen, Undo/Redo and package the same scene without editor/runtime disagreement.

## Pinned Wicked capability

### Decals

Wicked already exposes native `DecalComponent` authoring. Its editor exposes placement mode, `SetBaseColorOnlyAlpha()`, `slopeBlendPower`, and decal material participation through the associated Material component.

Wicked documents decal material support for base colour, base-colour texture, emissive strength, normal-map texture, normal-map strength, surface-map texture and texture tiling / `TexMulAdd`.

### Environment probes

Wicked already exposes native `EnvironmentProbeComponent` authoring. Probes capture a 360-degree scene panorama and act as a reflection fallback where a better reflection method is unavailable. Their transform is meaningful: moving, rotating and scaling a probe defines the parallax-correct reflection influence volume.

Native probe controls include refresh / mark dirty, real-time update mode, real-time update interval, MSAA, capture/view distance, resolution, imported cubemap identity where used and generated cubemap render data.

## Gate 3 creator-facing scope

### 1. ADD menu

Add two Renegade-owned creation entries:

- `ADD -> DECAL`
- `ADD -> ENVIRONMENT PROBE`

Both create native Wicked scene entities/components, become normal hierarchy items and participate in the existing selection, transform gizmo, duplicate/delete, dirty-state and save/open systems.

### 2. Decal placement workflow

Renegade should support direct creation at a sensible point in front of the editor camera, plus placement mode that projects onto the scene under the mouse where practical. A newly placed decal must have a usable default transform and must be visibly identifiable in the editor even when its projected texture is subtle or missing.

### 3. Decal Inspector

Selecting a decal exposes `DECAL // NATIVE WICKED` with Base-colour-only Alpha, Slope Blend, decal material access through the existing governed Material workflow, and a clear readout that transform scale controls projection volume/size. Gate 3 does not duplicate the Material editor.

### 4. Decal editor marker / volume aid

Provide an editor-only decal marker or projection-volume aid so the creator can find and orient decals in 3D space. This must not render in Runtime or packaged builds.

### 5. Environment probe creation and hierarchy

`ADD -> ENVIRONMENT PROBE` creates a native Wicked probe entity with Transform plus `EnvironmentProbeComponent`. The probe is selectable in the hierarchy and has an editor-only world marker/volume visualization. The probe transform's position/rotation/scale is authoritative for its capture location and parallax-correct influence bounds.

### 6. Environment Probe Inspector

Selecting a probe exposes `ENVIRONMENT PROBE // NATIVE WICKED` with Refresh Probe, Resolution 32/64/128/256/512/1024/2048 where supported, Real-time, Update Interval, MSAA, View Distance, generated/imported state readout where meaningful, and cheap accurate memory/resolution info where practical.

### 7. Probe preview

If practical within the bounded gate, show the selected probe's cubemap preview in the Inspector after it has rendered. This is secondary to correct world reflections and may be deferred if it creates disproportionate UI/render-path complexity.

### 8. Import/export cubemap policy

Generated scene probes and Refresh are required. Arbitrary DDS import/export is deferred unless it can be routed through the existing governed resource/asset model without introducing path-owned content.

### 9. Commands, persistence and Undo/Redo

Persistent mutations must use Renegade `CommandService`. Required command coverage: create decal, decal property edits, create environment probe, probe property edits. Transform changes continue through the existing central transform command; duplicate/delete continue through existing scene commands where compatible. Undo/Redo must restore actual native component state. Both native components must survive normal WISCENE save/reopen with no sidecar representation.

### 10. Runtime / packaged parity

Gate 3 is not accepted on editor appearance alone. Release owner acceptance must prove a visible decal persists through save/reopen and Runtime, a probe visibly changes reflections in a controlled area, probe scale changes influence/parallax behaviour, Refresh updates reflection after nearby content changes, probe settings survive save/reopen, and packaged Runtime sees the same scene state.

## Explicitly deferred

Gate 3 does not claim camera/vehicle attachment or runtime camera switching; scripted/dynamic decal spawning; tyre-mark systems; runtime probe-placement APIs; probe blending policy beyond Wicked native behaviour; SSR, ray-traced reflections, DDGI or path tracing; global post-processing; unmanaged DDS import/export; or a new material/shader editor.

## Architecture rules

- Renegade Studio owns the UX; no stock Wicked Editor windows.
- Use native Wicked `DecalComponent` and `EnvironmentProbeComponent`.
- Do not create a second reflection/cubemap world.
- Use existing Transform, Material, Selection, Hierarchy, Command, Scene and Project services.
- Editor markers/volumes are overlays only and never serialize as gameplay geometry.
- Persistent creator state must have Undo/Redo + Save/Open proof.
- Owner visual/behavioural validation overrides green CI.

## Planned implementation shape

Expected bridge surface: `DecalProbeService.h/.cpp` or two focused services if separation is cleaner; native state capture/apply helpers; create/edit commands; source/headless regressions; Studio ADD-menu integration; Inspector sections; editor-only markers/volume bounds; documentation and packaged acceptance notes.
