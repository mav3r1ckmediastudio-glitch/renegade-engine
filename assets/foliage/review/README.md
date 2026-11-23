# Open meadow grass trial

Six original grass cutouts in a 1024 x 1536 RGBA atlas, arranged in 512 x 512 cells. The PNG is the unmodified generated source; the adjacent `.rects` file contains six visible-content crop rectangles in pixels (left, top, right-exclusive, bottom-exclusive). The author maps these rectangles directly onto native hair cards so transparent cell padding does not lift the roots or change the intended blade proportions.

This is a local visual trial, not the production painting palette. The owner preferred this artwork to the matched Wicked original and requested dense coverage next. It retains native hair rendering, wind and shared texture use. Broad-leaf mesh packs remain a separate visual experiment.

`Tools/HairGrassAtlasReview.cpp` accepts source scene, destination scene, stock terrain folder, atlas PNG, and optional `--dense`. Its default compares two 300-strand plots. Dense mode creates a Wicked 1800-strand reference and new 1800- and 3600-strand plots, each 72 square metres. Common settings: length 0.5, width 0.9, two segments, two cards, randomness 0.25, seed 17. Stock sprite size ratios are preserved but normalized to their mean. Both materials use white base colour, roughness 0.9, reflectance 0.02 and alpha cutoff 0.5. These controls differ from the untouched stock defaults.

The generated source has low-alpha fringe/halo outside the visible plants; native alpha clipping at 0.5 is part of this trial. Do not assume it is ready for alpha-blended use. Mipmapped distance quality and large woodland overdraw still need representative testing.

Atlas SHA-256: c083bbb89d84d7df9e7ac353dcbd4b6ed131f81dc9b51afc51d0d50321b77113.

## Reproduction on DESIGN

The local QA directory `E:\RenegadeAssets\HairGrassRebuild` preserves `build-review.py`, `build-dense.py`, their `.cmd` wrappers, response files and logs. The existing x64 Release native dependency libraries are reused; upstream source and pins are unchanged. Both authors save, cold reopen, assert hair settings and write native 1280 x 800 close-ups. `open-dense-review.py` starts the playable dense comparison with the staged Release DX12 Runtime. Preserve the project-local reference textures and sign PNGs in Content.

The playable projects are `HairGrassReview/HairGrassReview.renegade` and `DenseGrassReview/DenseGrassReview.renegade` under that QA directory. Dense layout: Wicked left X-8, new dense centre X0, new very dense right X8. Each patch spans Z2 to Z14.

## Artwork prompt

Create a production-quality photorealistic RGBA grass sprite atlas, using the attached Wicked grass sheet as the botanical realism and subdued colour reference. Exactly 1024 by 1536 pixels, a 2-column by 3-row arrangement with six separate 512x512 cells and ONE small isolated meadow grass plant per cell. This is diffuse/albedo photography for a videogame cutout, NOT a drawing, NOT an engraving. Six distinct silhouettes: two fine green grass tufts, one low mixed green and dry tuft, one sparse meadow grass with a tiny brown seed stem, one wiry dry-green tuft, one slender meadow tuft with two delicate pale seed stems. Each plant has about 12-20 visibly solid slender leaves, natural irregular bends, open gaps, unequal heights and pointed tips. Roots sit near the bottom of each cell, all plants fully inside their cells. Match the reference's dark desaturated forest/grey greens, brown lower stems, occasional neutral straw leaves, natural longitudinal veins, uneven tones and subtle folded-leaf shading. Visibly separate DARK green, mid grey-green and brown leaves; avoid a uniform yellow-green wash. No luminous green, lime, yellow edging, ambient glow, soft halo, fog, mist, ground, cast shadows, vignette or backdrop. The essential technical requirement: blades and stems are solid opaque alpha=255 inside, with clean antialiased edges; every gap, margin and surrounding background is fully transparent alpha=0, with NO blurred transparency clouds. Neutral soft even studio illumination. Extremely realistic natural plant material with varied local colour and readable blade detail; no outline illustrations, no labels or grid.
