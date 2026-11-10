# Particle Picker and layered effects

Integrated in main through PR #192, retained and tested with functional prefabs in PR #193. Current presets use schema 7 (readers retain earlier versions); see Shapes/Timing below and CURRENT_STATUS.md for merged validation and remaining independent acceptance.

## Creator workflow

Add > Particle opens the native Particle Picker. A saved effect is a named collection of emitter layers with independent textures, spritesheet animation, colour, opacity, emission and transforms. Whole-effect controls are relative to the authored setup at 100%. The full effect is previewed privately before placement. Layer add, duplicate, remove, enable and solo operations edit only the draft until Apply or Place.

Saved project presets live in Content/Particles as versioned .rparticle documents. Save As New establishes a new preset identity. Update Preset is explicit and does not change placed instances. Existing single emitters can be opened without resetting settings. A placed effect owns a native parent transform and native emitter children. Commands retain stable instance identities on edit and Undo/Redo; duplicated instances acquire independent identities through the existing native duplication service.

Keep placing defaults on. RMB finishes and consumes the held gesture before viewport camera navigation resumes.

## Stationary animation

Owner feedback requires a stationary candle flame, rather than a stream of moving flame particles. Each layer supports Continuous emitter or Stationary / looping sprite. Stationary layers use a native scene Sprite with a camera-facing transform and controlled atlas frame animation. They have no spawning stream, gravity, particle expiration or lifetime fade. The native emitter/material authoring proxy retains the original emitter settings as metadata, allowing the creator to switch modes without losing them. Inapplicable particle controls are disabled in stationary mode.

The bridge updates only native sprite components with the stationary metadata marker. It performs direct component lookups, binds the governed material texture after reload/duplication and advances the sprite frame using gameplay time. Runtime pause supplies zero time. World placement, parent transforms, depth testing and serialization remain native Wicked responsibilities. No upstream change is needed for stationary sprites.

## Texture and serialization boundaries

All five governed material texture slots are preserved per layer: base colour, normal/distortion, surface, emissive and occlusion. CreatorTextureWorkflowService remains the import authority. Presets reference stable texture asset IDs; native placed material bindings provide Test Game and Build Game texture closure. Native scene resources are retained across command snapshots and duplication.

Schema version 1 stores base colour texture only; version 2 adds four companion texture slots; version 3 adds playback mode. Current readers retain compatibility with the earlier implementation fixtures.

Mesh emission presets currently require a reusable mesh binding before saving; rejecting an unresolved scene-local mesh is preferable to writing a broken cross-scene reference. Existing mesh-driven emitters are preserved when edited in their original scene.

## Preview and controls

The private native 3D preview owns its own scene and advances at 30 Hz. The main Level retains its previous frame while the modal owns authoring. The preview camera supports unrestricted quaternion orbit, Shift-drag roll, RMB pan, wheel zoom and Fit/Reset. Camera inspection never changes the authored effect transform.

Owner rejected generic slider ranges. Each control now has explicit practical limits, increments and numeric precision. Atlas rows, columns, frames and FPS use integer steps. The available frame range follows the actual sheet cells. Numeric entry remains available.

## Historical picker validation checkpoint

Release Studio and Runtime builds completed. Seven focused CTest checks passed: ParticleEffectTests, LightPickerTests, LightTests, S3GovernedLuaRuntimeTests, MarkerIconsSourceContract and both particle emitter source contracts. Native visual checks verified stationary candle preview and mouse orbit, roll, pan, zoom and Fit/Reset. The separate castle QA scene cold reopened with its placed candle retained (1166 entities, five effects). Native repeated placement after the marker-input correction, parent editing, exact numeric input, duplication and real Test Game still require final interactive checks. The owner is actively using the QA picker; leave their mouse and current draft undisturbed. Owner working binaries/project remain unchanged. Procedural local fog is a separate renderer integration after the current picker workflow; a mist spritesheet preset is not claimed as procedural fog. No weather-wide fog coverage or release gate closure is implied.


## Movement fields and natural variation

The field and variation additions are integrated through PR #192. The original native-QA limits remain scoped to their receipt; integration alone is not blanket visual acceptance. In the picker, Fields opens effect-owned Swirl, Turbulence, Updraft and Directional controls. Add creates a shared field. Select a layer and set Layer influence to control that layer's response; zero excludes it. Position / Size and Rotation place the field inside the effect. Show field displays its boundary and directional arrows only in the private preview.

For mixed drifting leaves, add one layer per supplied flipbook, choose 4 columns, 4 rows and 16 frames, and use Alpha blending. Use Variation to enable Spawn box, widen its half width/depth, vary its height/centre and reduce emission. Size variation, Speed variation and Life variation break up uniform particles. Use different FPS and base sizes on the layers. Add a gentle Swirl plus Turbulence, then adjust each layer's influence. Save As New retains all layers, fields and variation; editing a placed effect uses the same controls.

A field applies throughout the particle lifetime. Swirl follows the field's local Y axis; Directional follows local +Z, so Rotation changes its direction. Turbulence evolves with Animation speed and has spatial detail controlled by Noise scale. Stationary sprites retain their authored field weights but do not move.

Schema 4 adds shared fields and per-layer influence; schema 5 adds independent size/speed variation. Seven compiled variants are staged with Studio and Runtime and included by Build Game. The existing field-free/variation-free shader path is preserved.


## Composite scene selection

Placed layered effects are one selectable scene object. Studio binds its selection service to the current native Scene; selecting any internal emitter or stationary sprite resolves to its effect root before gizmo, inspector, duplication or deletion actions. Internal effect layers are omitted from Studio hierarchy rows, while their native components and saved transforms remain available to the Particle Picker. Ordinary scene entities retain their original selection behaviour. Layer edits belong in Edit Effect, rather than dragging individual children out of the effect in the scene.

## Lifetime controls

The native Lifetime page adds opt-in per-layer colour, size and opacity curves, with 2-8 editable points. Schema 6 retains earlier preset compatibility. Governed Lua exposes get_lifetime/set_lifetime through the same bridge authority. See PARTICLE_LIFETIME_CONTROLS.md for creator steps, limits and current verification.


## Shapes, timing and current integration

[Shapes and effect timing](PARTICLE_SHAPES_TIMING.md) describes schema-7 shapes,
loop/play-once timing, delay, duration and bursts. [Lifetime controls](PARTICLE_LIFETIME_CONTROLS.md)
describes schema-6 curves. [Particle Lua](PARTICLE_LUA_API.md) covers the matching
runtime API. Historical schema 1–5 notes above describe evolution, not the current
writer version. Earlier particle budgets and unsupported mesh-bound preset/fog
limitations are not removed by the general-prefab work.

PR #193's full brazier proof checks native light behaviour, three particle layers,
shader/material state, texture identities, fields, curves, shapes, independent
instances, script references, save/reopen, Unpack and packaged startup. It does not
prove every possible artistic effect or configuration. See
[evidence](evidence/scene-prefab-functional/README.md).
