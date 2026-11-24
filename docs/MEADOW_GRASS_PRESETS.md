# Stock vegetation presets

In Terrain > Vegetation, choose a Grass preset, then Paint. Eight approved choices use seven original RGBA sheets, each with six cropped variations.

| Preset | Height (m) | Nominal tufts / square metre |
|---|---:|---:|
| Meadow - Open | 0.50 | 50 |
| Meadow - Full | 0.50 | 25 |
| Meadow - Very Full | 0.50 | 50 |
| Short grass | 0.25 | 25 |
| Dry grass | 0.45 | 25 |
| Seedhead grass | 0.65 | 15 |
| Low clover | 0.12 | 20 |
| Small wildflowers | 0.30 | 4 |

Selecting a stock choice changes the brush only. Painting adds that plant alongside existing plants. The sliders edit that stock choice across this terrain, including its existing painted layers and subsequent strokes. Choosing clover therefore does not resize meadow grass or flowers. Full and Very Full have separate layer identities and densities. Delete removes all vegetation under the brush, and Undo/Redo restores the affected species.

Custom plant mix retains its existing multi-thumbnail atlas import and replacement painting: a Custom stroke replaces other vegetation within its footprint. Custom sliders affect custom and legacy terrain vegetation, leaving the new independent stock layers unchanged. Previously saved meadow layers retain their original appearance and shared settings; no automatic migration is applied.

The approved stock defaults use width 0.9, two segments/two crossed cards, length randomness 0.25, seed 17, drag 0.12, gravity power 0.02 and 100-metre view distance. Density is the native multiplier (nominal tufts / 3); exact counts depend on terrain sampling, painted boundaries and ground coverage. Ground-material Grass Coverage suppresses/restores each species using its own density.

Source adoption verifies SHA-256 and writes a versioned project-owned PNG through ProjectDocumentTransaction. Native hair geometry, atlas crop rectangles, material, masks and density metadata persist in WISCENE. Terrain metadata saves per-stock-choice edits, including settings changed before the first stroke. Material alpha clipping is 0.5, roughness 0.9, reflectance 0.02 and emissive strength zero. Broad meadow keeps the approved linear RGB multiplier (0.670896, 0.660796, 0.778305); the other stock sheets use white. PNG bytes and crops are unchanged from the approved artwork.

The registered grass-palette proof now also runs StockPlantProfilesProof.h: eight overlapping stock choices, independent edits, Custom isolation, paint and erase history, ground coverage and native cold reopen. FoliageNightReferenceVerify optionally compares a painted stock scene against the six-species approved reference's geometry, dynamics, material and crop values. These checks do not imply a populated-world performance or release gate.

Large-field timing and historical motion limitations below remain scoped to their original tests. The later native-breeze and six-species appearance approvals are recorded in HANDOFF.md. The approved night scene remains Examples/FoliageNightReference; it is preserved unchanged. Automatic world-moonlight tracking remains separate work.

## Validation correction

Earlier review emitters omitted their mesh subset. Native CreateFromMesh therefore found no usable triangle distribution, and repeated buffer reconstruction prevented sustained wind. Those earlier motion claims and timing measurements are invalidated. The review and field authors now declare a six-index mesh subset, and the prairie author repairs/validates its loaded emitter before use. The corrected 4096-square-metre test measured near-view total GPU Render medians of 1.85 ms bare, 3.30 ms Wicked at 25 tufts/m2, 4.75 ms Meadow Open at 50, 3.71 ms Meadow Full at 25 and 5.53 ms Meadow Very Full at 50. The committed CSV contains the corrected run, including P95 and distance samples. Existing terrain painting uses native terrain subsets and was not affected by this test-fixture defect.

## Stock selection and current acceptance

Stock now includes the three meadow choices plus the five temperate additions approved on 24 November. The thirty earlier grass, weed, freshwater, marine and alien PNGs are removed from shipping content and UI discovery. Packaging retires those exact bundled filenames from existing build directories; project Custom folders and previously painted Mixes are untouched. Custom PNG and sheet import remain supported. Choose a meadow preset explicitly; Custom plant mix requires at least one selected custom thumbnail.

The owner's 23 November recording shows texture flicker/shimmer and stuttering grass movement. These are unresolved. Grass appearance is approved, but prairie wind is not visually accepted. Advancing native forces, GPU readback and FPS measurements do not override that failure. Earlier handoff motion claims must not be treated as acceptance.
