# Five temperate foliage candidates

Private visual comparison, 24 November 2026. The owner said the grass in the
Native Breeze review looks great. Retain that meadow/native-wind combination as
the approved appearance baseline. This is not explicit acceptance of every
motion, mip or performance condition.

These are the five existing v3 generated atlases recovered unchanged from the
earlier foliage work. Each RGBA PNG is 1024 by 1536, arranged as six 512-square
cells. manifest.json retains exact SHA-256 values and source-alpha coverage.
No image was repainted, resampled or sharpened in this pass.

The .png.rects sidecars define per-cell alpha >= 128 bounding boxes, with one
pixel of padding inside the source cell. Native UV crops remove unused root
padding. Materials use the existing 0.5 cutout threshold. Low-alpha source halos
remain in the original bytes; this trial does not certify alpha-blended use.

## Matched native review

Tools/TemperateFoliageReview.cpp loads the approved native-breeze field and
authors six labelled plots. Every emitter is 6 by 12 metres. All use two hair
segments, two cards, seed 17, six atlas variants and the same native breeze:
direction (0.08,0,0), speed 1, randomness 5, with no force animations.
Anti-aliasing stays Off as in the approved review. Motion blur and depth of
field remain disabled.

| Plot | Atlas | Nominal height m | Tufts per square metre | Tufts |
|---|---|---:|---:|---:|
| Front left: approved meadow | Unchanged source material/atlas | 0.50 | 50 | 3600 |
| Front centre: short grass | short_grass.png | 0.25 | 25 | 1800 |
| Front right: dry grass | dry_grass.png | 0.45 | 25 | 1800 |
| Rear left: seedhead grass | seedhead_grass.png | 0.65 | 15 | 1080 |
| Rear centre: low clover | low_clover.png | 0.12 | 20 | 1440 |
| Rear right: small wildflowers | small_wildflowers.png | 0.30 | 4 | 288 |

The approved plot inherits its existing material calibration. New atlases use
white base colour, roughness 0.9, reflectance 0.02 and alpha cutoff 0.5.
The new short and seedhead grasses are visibly brighter/greener than the approved
meadow. Colour consistency and density need owner review before stock adoption.

## Local verification

Windows x64 Release compiled the author and linked against the existing matching
native dependency/object cache. The first harness link used an incorrect
directory for two cached bridge objects; the corrected link completed. The
failed log remains separate from the successful resumed build.

The author saved and cold reopened all six plots, comparing strand counts,
length/width, atlas variant counts, emitter subset/index counts, native wind,
and loaded texture resources. It rendered and visually inspected six close-up
captures and checked that all emitter distributions stayed populated.

Successful output:

    TEMPERATE_SIX_PLOTS_COLD_REOPEN_PASS
    TEMPERATE_SIX_NATIVE_CAPTURES_PASS
    TEMPERATE_FOLIAGE_NATIVE_REVIEW_PASS

Invocation:

    TemperateFoliageReview.exe <approved-native-breeze.wiscene> <review-project-directory>

The unmodified normal Runtime also reached the saved review in DX12 and captured
the real player camera at frame 60. Its capture shows the six-plot layout and
labels. The controller terminated only its own isolated test afterward; clean
shutdown was not verified by that check. The owner's existing Native Breeze
Runtime remained open.

Reproduction scripts and complete native/Runtime logs are retained under the
existing TemperateFoliageQA owner workspace: prepare-v3-review.py,
build-v3-review.py, resume-v3-review.cmd, verify-v3-runtime.py, and
open-temperate-review.py. The owner Desktop launcher is Temperate Foliage Review.

This is an artwork review, not stock palette integration, automatic LOD setup,
a whole-level benchmark, a full clean build, hosted CI, or merge approval.
Judge the five additions from standing height and up close, including clover
from above. Check distant cutouts and motion before adopting them into the
production brush. Existing stock choices and custom project plants are unchanged.
