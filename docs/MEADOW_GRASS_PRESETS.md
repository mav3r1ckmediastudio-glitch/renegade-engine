# Meadow grass presets

In Terrain > Vegetation, choose a **Grass preset**, then Paint. The three reviewed options use native GPU hair grass.

| Preset | Artwork | Nominal tufts per square metre |
|---|---|---:|
| Meadow - Open | Narrow meadow clumps; six variations | 50 |
| Meadow - Full | Broad bases made of fine blades; six variations | 25 |
| Meadow - Very Full | Same broad clumps at higher density | 50 |

These are starting points. The preset changes the terrain's shared grass settings, including density, height, width, segments and cards. Existing painted textures remain assigned, but their geometry/density follows those shared controls. Use the sliders to customise the result. Choosing a plant thumbnail returns the brush to Selected plant mix, preserving the existing multi-species workflow.

The reviewed geometry uses length 0.5, width 0.9, two segments and two crossed cards, length randomness 0.25, seed 17 and 100-metre view distance. Density is the existing terrain multiplier: 25/3 or 50/3. The supported multiplier range now reaches 20. Native masks and terrain sampling make the precise count depend on the painted footprint and boundary vertices.

The preset adopts its PNG into the project's Content/Vegetation/Mixes through a document transaction. Its versioned content identity also identifies the six crop rectangles and material colour calibration when Undo/Redo recreates a painted layer. The native hair rectangles, material, emitter references and masks are serialized in WISCENE; Runtime does not need the original review atlas sidecar. Existing square-cell mixed atlases retain their layout and appearance.

The broad artwork's linear RGB material multiplier is (0.670896, 0.660796, 0.778305), matching the earlier accepted grass colour. Both source sheets retain low-alpha fringe, so these presets use native alpha clipping at 0.5, roughness 0.9 and reflectance 0.02. They are not alpha-blended foliage.

Large-area validation uses Tools/GrassFieldReview.cpp: a 64 x 64 metre field, 102400 or 204800 tufts, matched native settings and near/25m/60m views at 1920 x 1009. GPU timestamp intervals surround the complete native Render stage. Reported intervals include the scene and terrain, rather than grass alone; serial-frame wall times include a GPU wait and do not represent overlapping gameplay throughput. This test does not establish a populated woodland performance guarantee.

Tools/StockGrassPresetProof.cpp exercises adoption, actual painting, global-setting history, paint Undo/Redo, crop/material restoration, six-variation GPU readback and cold reopen for all three options. The local Studio probe invokes the real selector callbacks and checks history and the existing plant filter.

Tools/PrairieWindReview.cpp is a separate motion demonstration. It authors a gentle directional ambient force and four broad, animated gust volumes across the large field. Native animations and force components persist with the scene. It does not change the existing weather presets or add a new general-purpose wind editor.

## Validation correction

Earlier review emitters omitted their mesh subset. Native CreateFromMesh therefore found no usable triangle distribution, and repeated buffer reconstruction prevented sustained wind. Those earlier motion claims and timing measurements are invalidated. The review and field authors now declare a six-index mesh subset, and the prairie author repairs/validates its loaded emitter before use. The corrected 4096-square-metre test measured near-view total GPU Render medians of 1.85 ms bare, 3.30 ms Wicked at 25 tufts/m2, 4.75 ms Meadow Open at 50, 3.71 ms Meadow Full at 25 and 5.53 ms Meadow Very Full at 50. The committed CSV contains the corrected run, including P95 and distance samples. Existing terrain painting uses native terrain subsets and was not affected by this test-fixture defect.
