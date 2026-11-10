# Particle lifetime controls

Select a layer in the Particle Picker, open **Lifetime**, then tick **Use lifetime
curves**. The colour strip and graphs show how that layer changes as a particle
ages. Existing presets start with this option off.

Use **Add** to insert a point. Choose a point from the list or click its position
on the graph, then edit **Age %**, **Size %**, **Opacity %** and the three colour
channels. The first and last points stay at 0% and 100%; you can add up to eight
points and remove any interior point. Values blend linearly between points.

Size is a percentage of the particle's initial size. Opacity replaces the old
fade-peak profile while the curve is enabled. RGB values tint the layer's colour
and texture; they do not replace colours baked into the image. Use a white layer
tint if you want the full range of curve colours.

For a quick grow-and-fade profile:

| Age | Size | Opacity |
| --- | --- | --- |
| 0% | 20% | 0% |
| 20% | 100% | 100% |
| 100% | 0% | 0% |

Turn **Use lifetime curves** off to restore the original size/fade controls.
The curve points remain saved, ready to enable again. **Reset curve** returns
the points to a constant profile. Advanced size/fade controls that are replaced
by the active curve are disabled with an explanatory tooltip.

Stationary looping sprites have no particle lifetime; their curve controls are
disabled while the stored points remain intact. Native **Simple (debug)** shading
draws flat grey, so it displays the size curve only. Use Soft or Soft lit shading
for visible colour and opacity. Heat distortion uses its normal-map shading.

Save As New and Update Preset retain curves with every layer. Applied instances,
duplication, Undo/Redo and cold scene reopening use the same authored metadata.
Saving a preset does not propagate changes to other placed effects.

## Lua

Use `get_lifetime(effect, layer)` and `set_lifetime(effect, layer, patch)` through
`renegade.particles`. Layer selectors accept a unique name or a 1-based index.

```lua
assert(renegade.particles.set_lifetime(effect, "Smoke", {
    enabled = true,
    keys = {
        {time=0,   size=.2, red=1, green=.8, blue=.4, opacity=0},
        {time=.2,  size=1,  red=1, green=.8, blue=.4, opacity=1},
        {time=1,   size=2,  red=.4,green=.4, blue=.4, opacity=0}
    }
}))
```

Lua uses normalized time/opacity/RGB (0-1), and size multipliers (0-4), rather
than percentages. Keys need explicit time; omitted size/colour/opacity default
to 1. A keys patch replaces the entire ordered key array. An enabled-only patch
retains the current points. Arrays must be dense and contain 2-8 points with
strictly increasing times and fixed 0/1 endpoints. Unknown keys, wrong types,
nonfinite numbers and invalid ranges fail atomically with false,error.

[LifetimeEffect.lua](examples/particles/LifetimeEffect.lua) is a creator example.
See [PARTICLE_LUA_API.md](PARTICLE_LUA_API.md) for the full particle API.

## Implementation and verification

Preset schema 6 reads earlier schemas 1-5 with neutral disabled curves. Scene
metadata carries authored keys. A derived 256-sample two-slice RGBA float texture
is rebuilt after native reload; production does not read particle data back to
the CPU. The optional simulation variants share the existing movement integration
and cover sorted/unsorted and depth-collision combinations. Existing native
shaders are selected when no field or lifetime curve is active. The bounded core
decision is recorded in PARTICLE_LIFETIME_CORE_DECISION.md.

Local Release checks: 11 focused tests pass, including validation, interpolation,
Lua atomicity/isolation, old-preset migration, scene/preset cold read and composite
Undo/Redo/duplication. Five real GPU cases capture red/green/blue age stages,
growth/shrink and opacity decay; all four sorting/depth combinations agree.
Rendered PNGs are tone-mapped, so hue dominance rather than linear RGB ratios
is used for the image checks. Midlife covered area exceeds the early area by
14x, and the opacity case's late RGB energy is about 26% of its early energy.
Native picker numeric editing and point addition were exercised. The owner
confirmed that the controls work. The standalone player enabled/disabled curves across two gameplay phases and
exited cleanly, with zero Lua diagnostics. Source checkpoint details are in HANDOFF.md. No CI/release gate is closed by these checks.
