# Phase 5 Gate 3 — Decals and Environment Probes

## Status

Implementation active on `phase5/scene-render-gate3-decals-probes`, based on post-Gate-2 main `cde15e2809730df32ec234592de30963f59debb6`.

The native bridge/backend passed the first four-job CI proof at `ace2ce244b4ffe3b93c2775a15100b3b81f4fc33`. The Studio authoring pass is now implemented and awaiting its own Windows/Studio CI plus Release owner acceptance. The probe-refresh Studio compile defect was corrected at `cd57ad63c450de145d8f91b47ae826e51f5f395b`; this documentation commit retriggers normal PR CI from a user-authored head.

## Purpose

Expose Wicked Engine's native decal and environment-probe systems through Renegade-owned services and Studio UI. Gate 3 must not introduce a parallel decal renderer, cubemap system, reflection volume model or serialization format.

The creator outcome is simple:

- place a decal into the level, author its core material colour/opacity, move/rotate/scale it and see the projected result immediately;
- place an environment probe, define the reflection volume by transform scale, refresh/capture it and see reflections respond inside the probe bounds;
- save, reopen, Undo/Redo and package the same scene without editor/runtime disagreement.

## Pinned Wicked capability

### Decals

Wicked already exposes native `DecalComponent` authoring. Its editor exposes placement, `SetBaseColorOnlyAlpha()`, `slopeBlendPower`, and decal material participation through the associated Material component.

Wicked documents decal material support for base colour, base-colour texture, emissive strength, normal-map texture, normal-map strength, surface-map texture and texture tiling / `TexMulAdd`.

### Environment probes

Wicked already exposes native `EnvironmentProbeComponent` authoring. Probes capture a 360-degree scene panorama and act as a reflection fallback where a better reflection method is unavailable. Their transform is meaningful: moving, rotating and scaling a probe defines the parallax-correct reflection influence volume.

Native probe controls include refresh / mark dirty, real-time update mode, real-time update interval, MSAA, capture/view distance, resolution, imported cubemap identity where used and generated cubemap render data.

## Implemented creator-facing capability

### 1. ADD menu

Renegade Studio now exposes:

- `ADD -> DECAL`
- `ADD -> ENVIRONMENT PROBE`

Both use native Wicked scene creation through the Gate 3 bridge commands, become normal hierarchy/selection entities, use the existing transform gizmo, and participate in CommandService Undo/Redo and WISCENE save/open.

New entities are created at a useful point in front of the current editor camera. A decal receives a shallow projection-volume transform; a new probe receives a larger cubic influence transform.

### 2. Decal Inspector

Selecting a decal exposes `DECAL // NATIVE WICKED` with:

- Base-colour-only Alpha;
- Slope Blend;
- core material Base Colour R/G/B;
- material Opacity.

The material values reuse Renegade's existing `MaterialService` and `SetMaterialCommand`; Gate 3 does not create a second decal-material representation. Deeper texture/shader authoring remains part of the later material/shader exposure gate rather than being duplicated here.

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

### 5. Environment probe world marker / influence volume

Every authored probe receives an editor-only world marker. Selecting it displays the transformed native influence volume so creator transform edits have immediate spatial feedback. The marker is clickable and is not packaged as scene geometry.

### 6. Commands, persistence and Undo/Redo

Persistent mutations use Renegade `CommandService`. Gate 3 covers native decal/probe creation and component editing; core decal material colour/opacity continues through the existing Material command. Transform changes remain owned by the central transform command. Native entities are serialized through WISCENE with no Gate 3 sidecar.

## Runtime / packaged acceptance still required

Gate 3 is not accepted on editor appearance alone. Release owner acceptance must prove:

- a visible decal persists through save/reopen;
- transform scale/rotation visibly changes decal projection volume/result;
- decal core material edits and native decal properties survive Undo/Redo and save/reopen;
- a probe visibly changes reflections in a controlled reflective test area;
- probe scale changes its parallax/influence behaviour;
- Refresh produces an observable new capture after nearby scene content changes;
- probe settings and transform persist through save/reopen;
- Test Level / packaged Runtime sees the same serialized decal/probe scene state;
- editor-only markers and volume guides do not appear in Runtime.

## Explicitly deferred

Gate 3 does not claim camera/vehicle attachment or runtime camera switching; scripted/dynamic decal spawning; tyre-mark systems; runtime probe-placement APIs; probe blending policy beyond Wicked native behaviour; SSR, ray-traced reflections, DDGI or path tracing; global post-processing; unmanaged DDS cubemap import/export; or the deeper material/shader editor.

Arbitrary decal texture/shader authoring is intentionally not duplicated in this gate; the existing core MaterialService is reused now, while the broader material/shader exposure remains a later Phase 5 gate.

## Architecture rules

- Renegade Studio owns the UX; no stock Wicked Editor windows.
- Use native Wicked `DecalComponent` and `EnvironmentProbeComponent`.
- Do not create a second reflection/cubemap world.
- Use existing Transform, Material, Selection, Hierarchy, Command, Scene and Project services.
- Editor markers/volumes are overlays only and never serialize as gameplay geometry.
- Persistent creator state must have Undo/Redo + Save/Open proof.
- Owner visual/behavioural validation overrides green CI.
