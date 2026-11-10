# Particle shapes and effect timing

> **Current-main reconciliation — 10 November 2026:** Implemented source is merged through PR #192 and retained in #193. The original local observations below remain scoped evidence, not current branch/merge instructions. See [CURRENT_STATUS](CURRENT_STATUS.md); owner/independent limits are not automatically closed.

Select a layer in the particle picker and press **Shapes** to choose Point, Box, Sphere, Cone, Disc, Ring or Cylinder. Radius, height, cone angle and ring thickness are enabled where relevant. Sphere can emit from its surface. Box uses the layer half widths. Shapes use local +Z as their forward axis; Pitch, Yaw and Roll aim them. Outward speed adds velocity along the shape normals. Motion velocity and movement fields still work alongside this. Stationary looping sprites stay at their placed position and do not use emission shapes.

Press **Timing...** beside Whole Effect. Enable timing, choose Loop or Play once, then set Duration, Delay and Burst / layer. Delay comes before each emission window. Duration stops new emission while existing particles finish their lifetimes. Loop repeats delay + duration. Burst adds the chosen number at each window start, in addition to the authored per-layer burst. Set emission rate to zero for a burst-only effect. Restart replays from the initial delay; pause/resume freezes and continues the clock. Stationary layers show during the window and restart their flipbook at its start.

All controls are saved in project Content/Particles presets and native scene metadata. Each placed composite keeps its own clock and settings. Versions 1-6 remain readable with timing off and their existing point/box behaviour. Schema 7 stores shapes and timing. Older executables cannot read new schema-7 presets.

Lua uses renegade.particles. Shape types: 0 Point, 1 Box, 2 Sphere, 3 Cone, 4 Disc, 5 Ring, 6 Cylinder.
Example:
    local p = renegade.particles
    assert(p.set_layer(effect, 1, {shape_type=3, shape_radius=.15, shape_angle=25, normal_factor=2}))
    assert(p.set_timing(effect, {enabled=true, loop=true, duration=1, delay=.5, burst=20}))
    local timing = assert(p.get_timing(effect))
    assert(p.pause(effect))
    assert(p.resume(effect))
    assert(p.restart(effect))

Shape fields also include shape_height, shape_thickness and boolean shape_surface. Existing layer quaternion fields aim the shape. get_layer returns the same settings. Timing setter accepts partial tables and validates the whole candidate atomically. Flags require booleans, burst requires an integer, and numeric settings reject NaN/infinity. Runtime edits affect only that instance, not its saved preset.

Technical scope: the seven existing optional DX12 particle movement shader variants now include geometric emission. A bounded float4 extends the existing particle constant buffer; the core pin stays unchanged. Shape members are derived from metadata after archive loading. The clock runs once per root in the existing emitter update, using native membership and hierarchy lookups. Large hitches queue at most one bounded burst event per layer/frame.

## Individual layer size
Select a layer and use Layer size % in Basic settings, below Emission. 100% is that layer size when the preset was loaded; 50% halves its particles and 200% doubles them. Percentages stay with their layers when switching selection. Whole Effect / Overall size scales the full composite. Advanced absolute particle size stays synchronized. Resizing updates only the selected preview emitter, and selecting layers does not restart the effect. This changes particle size; motion, lifetime and emission volume retain their settings.
