# Moonlit Clouds V1 — isolated shader prototype (28 October 2026)

## Purpose
Correct Renegade's fully black nocturnal volumetric clouds. Reuse the authored
Moon's direction, fixed or automatic orbit, on/off state and phase to illuminate
moon-facing cloud tops and produce view-dependent silver linings by the existing
Wicked volumetric scattering and self-shadow code. Do not brighten clouds uniformly.

## Source and boundary
Worktree: `C:\Users\paulw\source\repos\renegade-moonlit-clouds-20261028`
Branch: `feature/moonlit-clouds-v1`
Base: `feature/moon-phases-v1` at 11bbb90, with its uncommitted moon and
shooting-star source reproduced in this isolated worktree. The original moon
worktree was not altered, staged or reset.

IMPORTANT: Current origin/main (150eca4 / PR #186) has the new river-enabled
Wicked fork (gitlink ceb816b). This candidate uses the older 3a800b Wicked
submodule in order to validate the unmerged Moon feature independently.
DO NOT merge this worktree wholesale into main: integrate its bounded lunar
cloud shader changes onto the current fork and the accepted moon implementation
after the visual acceptance gate.

## Implementation
- `Tools/night_sky/renegadeMoonCloudLighting.hlsli`: no new GPU buffers.
  Reads existing `ShaderWeather.moon_parameters` and `moon_orbit`.
- `cmake/RenegadeNightSkyOverlay.cmake`: idempotent module copy and
  source patch to `volumetricCloud_renderCS.hlsl`.
- Existing `VolumetricCloudLighting` continues using one directional source;
  for deep night and visible, enabled moon, the dominant light becomes moonlight.
  Existing atmospheric transmittance, shadow rays, dual-lobe phase function and
  multiple scattering produce surface highlights/silver linings.
- Output remains exactly native when moon is disabled, its disc brightness is
  zero, or the sun is above -0.09 direction Y. New moon contributes no light.
  Illumination is strongest at full moon; quarter phase has reduced effect.
  Lunar horizon and dusk gates avoid abrupt nighttime glow.
- No new editor control; Moon state remains metadata-backed and undoable.
- The moonlight multiplier is a deliberately small artistic starting point,
  not a physically calibrated radiometric measurement. **GPU images are
  required** to tune silver-edge visibility and nighttime exposure.

## Visual acceptance to run
1. Daylight with moon enabled vs disabled: equivalent cloud pixels.
2. Midnight with full moon overhead: visible moon-facing tops, narrow bright
   edges when backlit, dark cloud undersides; no flat white cloud tint.
3. New moon and moon disabled: same legacy dark clouds.
4. First and last quarter: noticeably less illumination than full moon.
5. Moon below horizon / low on horizon: no light / smoothly diminished light.
6. Fixed position and automatic orbit, dusk and dawn, moving camera and clouds.
7. Test both DX12 and Vulkan; inspect temporal accumulation/ghosting in motion.
8. Confirm aurora, Milky Way, existing stars, cloud shadows, fog, water/reflections
   and environment probe captures are unaffected.
9. Profile cloud GPU time at 1080p and 1440p, not just overall FPS.
10. Studio/Runtime packaged shader variants, save/reopen and Undo/Redo.

## Status
Configured with working Kitware CMake, using VS2022 x64 Release.
NightSkySourceContract and MoonlitCloudSourceContract pass.
WickedEngine embedded HLSL6 + SPIR-V shader package builds successfully, including
the final fix that separates cloud moonlight from atmospheric solar lighting.
Studio and Runtime Release both build successfully (complete MSBuild exit 0).
The native Moon, Night Sky and Shooting Stars tests plus Moonlit Cloud source
contract pass. Actual DX12 and bare-argument Vulkan GPU capture passes are
listed below. Owner visual approval and representative gameplay GPU profiling
remain open; do not infer final acceptance from the builds and source tests.
No push, PR or main merge authorized. Protect the current river, inventory,
night-sky and aurora worktrees.

## Native GPU proof — 28 October 2026

The original conservative moon factor (0.006/0.008/0.011) compiled, but real
night captures were practically black. It was deliberately **not accepted**.

The second candidate uses exposure-compensated, cool values (0.20/0.26/0.35)
multiplied by authored moon brightness (clamped to 2), phase, moon altitude and
solar-night weight. It uses the existing cloud density, light scattering and
self-shadowing. The test fixture uses first/second cloud coverage 0.92/0.55,
base 650, thickness 2200, to make structures visible from the camera.

Captured real GPU frames in:
- `BUILD/moonlit-cloud-gpu-proof-tuned/` (default DX12)
- `BUILD/moonlit-cloud-gpu-proof-real-vulkan/` (Wicked argument `vulkan`, without a leading dash). The earlier `-vulkan` run was default DX12 due to literal token parsing and is not counted as Vulkan verification.

Both runs exited 0 and wrote the requested PNG frames plus frame-times.csv.
At 960x540, the DX12 lower cloud-sky sample (mean RGB value over y=420..495):
- moon disabled 1.722
- new moon 1.736
- full moon below horizon 1.772
- quarter moon 10.954
- full moon 23.089
- daytime moon on 170.962
- daytime moon off 174.008

The numerical change and inspected PNGs show useful phase-graded moonlit
cloud shapes on otherwise dark sky, with moonless clouds remaining nearly black.
**This is not yet owner aesthetic acceptance.** Some cloud shapes near the horizon
still exhibit the stock Wicked band's flattened form; cloud-formation quality
is a distinct Clouds V2 improvement.

The 'frame-times.csv' values are serialized CPU/GPU test-loop timings at only
960x540 in a small synthetic scene, **not** production gameplay GPU times or
an FPS budget. Comprehensive 1080p/1440p runtime and moving-weather profiling
remain open.

Daytime scene pixels differ modestly from moon on/off because the visible lunar
disc and temporal wind-driven cloud movement change between sequential frames.
The shader itself leaves the native daytime lighting branch unchanged.

Source/contract tests and RenegadeNightSkyTests passed in the isolated build.
No merge or push is authorized; this remains a candidate for the user's review.

RenegadeStudio Release successfully linked. The correct Studio startup smoke uses
the bare `startup-smoke` argument, not `--startup-smoke`, and exits 0.
The earlier unrecognized argument opened a normal editor GUI; that isolated
test process was terminated without opening any user project.
