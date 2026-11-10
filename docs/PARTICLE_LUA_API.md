# Particle effects from Lua

Save an effect in the Particle Picker, then use `renegade.particles` in a governed
creator script. The whole saved effect is one instance, including every layer and
movement field. Changes affect that instance; they do not overwrite your preset.

```lua
local p = renegade.particles
local fire, err = p.spawn("Brazier Fire", {x=0, y=1, z=4})
assert(fire, err)
assert(p.set_layer(fire, "Flame", {opacity=.8, fps=24}))
assert(p.pause(fire))
assert(p.resume(fire))
assert(p.stop(fire))       -- stop emitting; existing particles finish
assert(p.restart(fire))    -- clear and start again
assert(p.destroy(fire))    -- remove every layer
```

Use your own saved names. `presets()` returns `{id,name}` entries. Prefer stable
preset IDs in shipped scripts. Names must be unique; ambiguous names return an error.

## Calls

| Call | Result / purpose |
| --- | --- |
| `presets()` | Project preset catalog. |
| `spawn(preset [, position])` | New complete effect as an opaque EntityRef. |
| `layers(effect)` / `fields(effect)` | Ordered names; selectors accept a unique name or 1-based index. |
| `get_layer(effect, layer)` / `get_field(effect, field)` | Copy of current authored settings. |
| `set_layer(effect, layer, patch)` / `set_field(effect, field, patch)` | Validate and apply a partial settings table. |
| `get_lifetime(effect, layer)` / `set_lifetime(effect, layer, patch)` | Read/edit enabled and 2-8 normalized lifetime keys; see PARTICLE_LIFETIME_CONTROLS.md. |
| `play(effect)` / `resume(effect)` | Enable emission and resume; per-layer authored paused flags remain authoritative. |
| `pause(effect)` | Freeze the complete effect, including stationary sprites. |
| `restart(effect)` | Clear particles and restart sprite animation. |
| `stop(effect [, clear=false])` | Stop emission, optionally clear immediately; hides stationary sprites. |
| `burst(effect, count [, layer])` | Burst on enabled continuous layers; stationary sprites cannot burst. |
| `attach(effect, parent [, socket="" [, keep_world=false]])` | Attach to an EntityRef or named descendant transform. |
| `detach(effect)` | Detach, retaining the native world transform. |
| `get_transform(effect)` / `set_transform(effect, patch)` | Local x/y/z, qx/qy/qz/qw and scale_x/y/z. |
| `set_texture(effect, layer, slot, asset)` | Governed project texture UUID or AssetRef. |
| `destroy(effect)` | Delete the complete instance. |

Queries and spawn return `nil,error` on failure; mutations return `false,error`.
Successful mutations return true. Unknown keys, invalid ranges, nonfinite numbers,
wrong types and invalid/cross-project identities fail without partially applying
the patch. Quaternion rotations must be nonzero. Scale must be positive.
`contract_version` is 1.

Texture slots are `colour`, `normal`, `surface`, `emissive`, `occlusion`.
Adopt/import textures through the project first. Arbitrary filesystem paths and
texture clearing are not accepted by this version.

`playback`: 0 continuous, 1 stationary loop. Field `type`: 0 swirl, 1 turbulence,
2 updraft, 3 directional. Field and layer transforms are effect-local.
Rotation/velocity units follow the picker/native emitter settings.
`fps` controls flipbook playback; field `strength` controls swirl speed independently.

## Attachment example

[AttachedEffect.lua](examples/particles/AttachedEffect.lua) spawns a saved effect
and attaches it to a tool or NPC. For grinder sparks, author a directional effect
in the picker, align the field with the tool's cut direction, then attach to its
named cut-point transform. The socket option resolves a named transform beneath
the parent.

## Lifetime and packaging

Spawned instances are removed at scene stop. Placed instances can be controlled
through their existing EntityRef and are not deleted when scripting stops.
Deleted/detached layers invalidate subsequent access safely. References from a
previous scene generation cannot be reused.

Test Game snapshots and Build Game dependency discovery include the project's
saved particle catalog and its governed texture products. This supports dynamic
preset selection by name/ID. The installed fifty-texture browser catalog itself
is not automatically bundled: only project-preset references are included.
Unused project presets still contribute dependencies. Missing referenced inputs
fail the build/snapshot rather than falling back to developer-machine paths.

This API covers current picker settings. It does not add new
emitter shapes, delayed sequences, trails, collision/completion callbacks, or
save-game persistence for live instances.

## Settings reference

Numeric bounds are API input limits, not slider track recommendations.
Native emitter sanitization still applies; read back with get_layer after edits.
Boolean values must be actual Lua booleans. Integer values must be whole numbers.

### Layer

| Key | Type | Bounds |
| --- | --- | --- |
| `enabled` | boolean | 0 to 1 |
| `playback` | integer | 0 to 1 |
| `emit_rate` | number | 0 to 1000000 |
| `size` | number | .001 to 10000 |
| `lifetime` | number | .001 to 10000 |
| `life_variation` | number | 0 to 10000 |
| `randomness` | number | 0 to 10000 |
| `normal_factor` | number | -10000 to 10000 |
| `velocity_x` | number | -10000 to 10000 |
| `velocity_y` | number | -10000 to 10000 |
| `velocity_z` | number | -10000 to 10000 |
| `gravity_x` | number | -10000 to 10000 |
| `gravity_y` | number | -10000 to 10000 |
| `gravity_z` | number | -10000 to 10000 |
| `drag` | number | 0 to 10000 |
| `rotation` | number | -10000 to 10000 |
| `motion_blur` | number | 0 to 10000 |
| `mass` | number | .0001 to 10000 |
| `restitution` | number | 0 to 1 |
| `colour_variation` | number | 0 to 1 |
| `red` | number | 0 to 1 |
| `green` | number | 0 to 1 |
| `blue` | number | 0 to 1 |
| `opacity` | number | 0 to 1 |
| `emission` | number | 0 to 100000 |
| `emissive_red` | number | 0 to 10000 |
| `emissive_green` | number | 0 to 10000 |
| `emissive_blue` | number | 0 to 10000 |
| `particle_scale_x` | number | .001 to 10000 |
| `particle_scale_y` | number | .001 to 10000 |
| `fade_start` | number | 0 to 1 |
| `fade_end` | number | 0 to 1 |
| `columns` | integer | 1 to 32 |
| `rows` | integer | 1 to 32 |
| `frames` | integer | 1 to 1024 |
| `start_frame` | integer | 0 to 1023 |
| `fps` | number | 0 to 120 |
| `max_particles` | integer | 1 to 1000000 |
| `burst_on_create` | integer | 0 to 1000000 |
| `fixed_timestep` | number | -1 to 1 |
| `shader` | integer | 0 to 3 |
| `blend` | integer | 0 to 5 |
| `paused` | boolean | 0 to 1 |
| `sorted` | boolean | 0 to 1 |
| `depth_collision` | boolean | 0 to 1 |
| `sph` | boolean | 0 to 1 |
| `spawn_box` | boolean | 0 to 1 |
| `ignore_colliders` | boolean | 0 to 1 |
| `mesh_colour` | boolean | 0 to 1 |
| `blend_frames` | boolean | 0 to 1 |
| `sph_smoothing` | number | .001 to 10000 |
| `sph_pressure` | number | 0 to 10000 |
| `sph_density` | number | .001 to 10000 |
| `sph_viscosity` | number | 0 to 10000 |
| `size_variation` | number | 0 to 1 |
| `speed_variation` | number | 0 to 1 |
| `field_1_weight` | number | 0 to 1 |
| `field_2_weight` | number | 0 to 1 |
| `field_3_weight` | number | 0 to 1 |
| `field_4_weight` | number | 0 to 1 |
| `offset_x` | number | -1000000 to 1000000 |
| `offset_y` | number | -1000000 to 1000000 |
| `offset_z` | number | -1000000 to 1000000 |
| `layer_scale_x` | number | .001 to 10000 |
| `layer_scale_y` | number | .001 to 10000 |
| `layer_scale_z` | number | .001 to 10000 |
| `qx` | number | -10000 to 10000 |
| `qy` | number | -10000 to 10000 |
| `qz` | number | -10000 to 10000 |
| `qw` | number | -10000 to 10000 |
| `name` | string | 1-120 bytes |

### Movement field

| Key | Type | Bounds |
| --- | --- | --- |
| `enabled` | boolean | 0 to 1 |
| `clockwise` | boolean | 0 to 1 |
| `type` | integer | 0 to 3 |
| `strength` | number | 0 to 20 |
| `lift` | number | -20 to 20 |
| `falloff` | number | 0 to 1 |
| `noise_scale` | number | .05 to 20 |
| `animation_speed` | number | 0 to 10 |
| `x` | number | -1000000 to 1000000 |
| `y` | number | -1000000 to 1000000 |
| `z` | number | -1000000 to 1000000 |
| `width` | number | .01 to 10000 |
| `height` | number | .01 to 10000 |
| `depth` | number | .01 to 10000 |
| `qx` | number | -10000 to 10000 |
| `qy` | number | -10000 to 10000 |
| `qz` | number | -10000 to 10000 |
| `qw` | number | -10000 to 10000 |
| `name` | string | 1-120 bytes |


## Shapes and effect timing
See [PARTICLE_SHAPES_TIMING.md](PARTICLE_SHAPES_TIMING.md) for shape fields, get_timing / set_timing, delay, duration and burst semantics.
