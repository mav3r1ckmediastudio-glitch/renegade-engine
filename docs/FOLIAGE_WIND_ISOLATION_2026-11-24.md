# Foliage wind isolation, 24 November 2026

The owner's prairie recording remains a failed motion and shimmer acceptance check.
This diagnostic keeps the same 204800 strands, atlas, seed, geometry, material,
camera and resolution while changing the wind source. It changes no production
renderer, shader, stock palette or importer code.

## Corrected comparison

The earlier WindIsolation ordinary-wind case copied speed 1, randomness 5 and
direction (0,0,0) from the original field. The pinned sample_wind shader multiplies
the sampled wind by direction, so that case supplied no native grass wind. It
cannot establish ordinary moving-wind quality.

Tools/WindIsolationReview.cpp compares travelling gusts (five forces/five
animations), no wind, one constant ambient force without animation, and genuine
native weather wind with direction (0.08,0,0), speed 1 and randomness 5.
Every scene is saved and cold reopened; the proof compares strand count, atlas
rectangles, geometry and dynamics settings and force/animation counts.

Windows x64 Release compilation and native DX12 execution passed all four modes.
The local compiler/link response files and complete build log are retained with
the owner's existing wind QA. Invocation:

    WindIsolationReview.exe <prairie-source.wiscene> <fresh-output-directory>

The existing native dependency/object cache was used; this is not a clean build,
hosted CI run, standalone package performance gate or independent verification.

## Timing result

Each mode has 180 warmup frames and 900 measured frames at a 75 Hz application
limit, 1920 by 1009. No screenshots, GPU readbacks or file writes occur during
the measured window. Thirty captures are produced afterward in a separate phase.

| Mode | Median ms | P95 ms | P99 ms | Maximum ms | Frames over 33.33 ms |
|---|---:|---:|---:|---:|---:|
| Travelling gusts | 13.334 | 15.794 | 18.470 | 44.250 | 2 |
| No wind | 13.334 | 15.977 | 38.016 | 89.950 | 10 |
| Steady force | 13.334 | 15.363 | 20.726 | 41.432 | 7 |
| Native breeze | 13.334 | 16.158 | 20.882 | 46.402 | 8 |

Raw summary: evidence/foliage-wind-isolation-2026-11-24.csv.
This single isolated run does not show worse frame pacing from travelling
gusts. All modes have outliers; other work was active on the same machine.
No present-time trace or whole-level performance conclusion is implied.
Visible motion stepping and moving cutout aliasing remain separate possibilities.

The older 1280 by 720 sequences have effectively static grass with gusts off:
mean absolute successive RGB change in a fixed grass region is 0.000084/255
for no wind versus 6.287/255 for travelling gusts. Ground and sky are unchanged.
This distinguishes static stability from changing grass pixels; it does not
measure visible stutter, prove a cause, or compare equal amounts of motion.

## Playable comparison and outstanding review

Five private projects preserve the same field: travelling gusts, no wind, steady
force, native breeze, and native breeze with TAA. Each owns a fresh project and
scene document identity. Original scenes are retained.

The original review scene's persisted anti-aliasing value is Off (0).
The additional native-breeze TAA trial writes and cold reopens the existing
render setting TAA (2). It disables depth of field and motion blur in all five
playable comparisons. This is a scene setting trial, not a changed engine
default or a certified shimmer fix.

Normal Runtime captures and final owner review are recorded in HANDOFF.md.
Next: judge native breeze versus travelling gusts and compare the two native
breeze AA settings while stationary and walking. If stepping remains, collect
present intervals and sampled tip trajectories in the actual Runtime. Validate
near and distant cutouts before accepting the five additional temperate plants
or preparing hosted CI. No push or merge is authorized by these results.

Normal unmodified Runtime DX12 captures succeeded for travelling gusts, no wind,
native breeze and native breeze with TAA on an isolated Windows desktop. Each
reached the actual player-camera capture at frame 60 and remained alive afterward.
The controller then terminated its own test processes (exit 1); clean shutdown
was not verified. Static captures were visually inspected: TAA smooths cutout
edges but noticeably softens grass detail. A still capture does not certify
motion or remove the outstanding owner video failure.
