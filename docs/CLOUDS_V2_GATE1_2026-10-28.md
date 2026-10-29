# Renegade Clouds V2 / Weather roadmap Gate 1 — technical checkpoint
**28 October 2026. Status: isolated functional shader candidate, not owner accepted.**

## Boundary and preservation
- Worktree: `C:\Users\paulw\source\repos\renegade-moonlit-clouds-20261028`, branch `feature/moonlit-clouds-v1`.
- This worktree already contains separate uncommitted Moon/Night Sky, Milky Way,
  aurora and shooting-stars work and was previously used for Moonlit Clouds V1.
  Do NOT reset or discard those changes.
- It is currently based on WickedEngine submodule `3a800b7`; main's accepted
  river/waterfall implementation uses a newer pinned river fork. **Do not merge
  the moon branch wholesale into main.** Integrate the compact Clouds V2 shader
  overlay into the river-enabled Wicked revision in a dedicated integration
  worktree only after approval and verify all accepted water features.
- Original `renegade-engine`, inventory/river integration, and original Moon
  worktrees have not been modified by this Gate 1 work. Snow V2 deferred.
- No pushes, PRs, or merges authorized or performed.

## True volumetric verification
Wicked's actual shader already uses 64x64x64 3D shape noise, 32x32x32 3D
Worley detail noise, curl distortion, 2D *weather distribution* maps,
3D density samples along the camera ray, and true volumetric integration
with directional scattering and cloud self-shadow samples.
2D weather maps do not make the *cloud body* a billboard.
Volumetric shadow pass uses the same SampleCloudDensity model.

## New Gate 1 implementation
- Source-of-truth: `Tools/night_sky/renegadeCloudsV2.hlsli`.
- Wiring: `cmake/RenegadeNightSkyOverlay.cmake`; the idempotent overlay
  adds the helper to `volumetricCloudsHF.hlsli` shared by cloud raymarch
  and shadow passes.
- `RenegadeSculptCloudHeight`: uses existing low/middle-frequency 3D
  Worley channels and cloud-type weight to vary vertical height in the
  upper cloud volume. Relatively even cloud bases, irregular rising tops.
- `RenegadePreserveCloudErosionDetail`: preserves 3D fine erosion at
  partially transparent cloud edges, not throughout the dense interior.
- No new GPU weather fields, structs, raymarch passes, textures or texture
  fetches. Existing moonlight, sun illumination, aerial perspective and
  aurora sky calculations are unchanged.
- First conservative sculpt coefficient 0.30 / detail 0.16 produced a
  small but measurable A/B delta; tuned candidate is 0.56 / detail 0.24.
  Do not describe this as a complete rendering replacement.

## Verification evidence
- Kitware CMake VS2022 x64 Release successfully configured.
- WickedEngine embedded HLSL6 / SPIR-V shader target compiled.
- `RenegadeCloudsV2VolumetricContract`, MoonlitCloudSourceContract and
  NightSky native tests passed.
- Actual DX12 captures: `BUILD/clouds-v2-gate1-tuned-billows-dx12/`.
- Matching Vulkan captures: `BUILD/clouds-v2-gate1-tuned-billows-vulkan/`.
- Focused camera/viewpoint tests include scattered, thick, upward-looking,
  overcast and two camera offsets to demonstrate cloud parallax.
- Controlled baseline (identical source except the two new helper functions
  return their original values): `BUILD/clouds-v2-gate1-controlled-baseline-dx12/`.
  This test temporarily replaced ONLY the local generated Wicked shader
  include. The authored header was restored, the embedded shader dump
  force-regenerated and the native GPU test relinked. No user project touched.
- Frame-times.csv reports 960x540 CPU+GPU serialized synthetic proof time,
  **not** production game GPU time or FPS. Do not claim an exact performance
  gain from it.

## Findings still open / owner acceptance gate
1. True volumetric status is established and 3D sculpt + edge erosion are
   functional. However the visual A/B difference remains modest for some
   camera viewpoints; it is not yet a dramatic improvement in realism.
2. Overcast coverage can produce an overly uniform bright ceiling. Need
   properly varied, weather-appropriate cloud formation presets, not just
   adjusting coverage intensity.
3. Need distinct detailed cumulus / towering cumulus / stratus / high cirrus
   forms, cross-altitude cloud layers and realistic cloud-base variation.
4. Profile moving weather, temporal stability/ghosting and cloud GPU timings
   at 1080p and 1440p on representative scenes, DX12 and Vulkan.
5. Verify real Studio inspector, save/reopen, Undo/Redo, world/cloud shadows,
   scene probes/reflections and the Moon/Aurora/Milky Way remain intact.
6. Carry candidate onto accepted river-enabled Wicked fork in another
   isolated integration worktree; verify river, waterfall and inventory.
7. No changes to rain, snow, season scheduling or weather presets unrelated
   to Gate 1. The five-gate roadmap remains in force.

## Safety / preview caveat
The previously opened isolated Moonlit Clouds Studio process can still be
running with the older Moonlit Clouds V1 executable. DO NOT label that
running process as Clouds V2 until Studio has been incrementally relinked
and reopened safely after confirming no unsaved owner scene changes.
The Gate 1 shader tests are native executable GPU proofs, independently
verified, without opening or changing a user project.

## Separately named local owner preview builds
Native C++ application relinks using the tested embedded Clouds V2 renderer:
- `BUILD/moonlit/Studio/Release/RenegadeStudioCloudsV2.exe` built Release,
  startup-smoke (literal argument `startup-smoke`) exited 0.
- `BUILD/moonlit/Runtime/Release/RenegadeRuntimeCloudsV2.exe`
  built Release and linked successfully (full gameplay test outstanding).
- `Tools/night_sky/LaunchCloudsV2Preview.cmd` opens the separately named
  Studio without touching the previous `RenegadeStudio.exe` process.
  Use the separately owned preview project under
  `BUILD/moonlit-preview-projects/Renegade Moonlit Clouds Preview/`.
- The old running `RenegadeStudio.exe` was **not** closed, relinked,
  overwritten or switched. Save work there before opening the new preview.
- The tuned shader's real DX12 and Vulkan GPU proof ran successfully,
  all 5 focused native/source checks passed and `git diff --check` should
  be part of the final source verification.
- Visual Gate 1 approval and game-level performance tests are still required.

## Gate 1 coverage slider regression and fix — owner screenshot (22:06)
**Problem:** At Coverage 1 the majority of the sky was empty, with
disconnected clouds confined to the horizon. The real implementation of
Wicked's `SampleWeather` multiplied raw 2D weather-map noise by the
0–1 Coverage amount. Multiplying by 1.0 preserved the sparse patches rather
than creating full overcast coverage. This was a semantic flaw of the slider,
not a lack of true volumetric rendering.

**Fix:** `Tools/night_sky/renegadeCloudsV2.hlsli` now defines
`RenegadeResolveCloudCoverage`, wired idempotently into Wicked's shared
`volumetricCloudsHF.hlsli` `SampleWeather` through
`cmake/RenegadeNightSkyOverlay.cmake`. At 0–75% it respects original
weather-map distributions, then smoothly blends toward a continuous weather
coverage distribution (95–100% heavily/fully overcast, respectively).
At exactly 1.0 the weather distribution is 1 everywhere, while the **actual
3D density fields, cloud height gradients, erosion, raymarch, self shadows,
lighting and moonlight remain unchanged**. It affects both main rays and
cloud-shadow samples and doesn't add GPU resources or ABI changes.

**Proof:** Wicked's embedded HLSL6/SPIR-V target compiles successfully.
`RenegadeNightSkyTests`, `RenegadeCloudsV2VolumetricContract`, and
`RenegadeMoonlitCloudSourceContract` all pass. Real GPU DX12 and Vulkan
captures both exited 0; see:
- `BUILD/clouds-v2-coverage-fullsky-dx12/`
- `BUILD/clouds-v2-coverage-fullsky-vulkan/`

Frames `clouds-v2-day-up35-coverage80/95/100.png` demonstrate a transition
from separated clouds to textured overcast covering the entire view;
`clouds-v2-day-up70-coverage100.png` confirms zenith coverage;
`clouds-v2-night-up35-coverage100.png` checks night sky under dense clouds.
Extreme overcast appears fairly uniform at the zenith; more varied cloud
types, textures and shadows remain open Gate 1 polish, not a justification
to regress full-sky coverage.

Separate Release builds:
- `BUILD/moonlit/Studio/Release/RenegadeStudioCloudsV2Coverage.exe`
- `BUILD/moonlit/Runtime/Release/RenegadeRuntimeCloudsV2Coverage.exe`
Native Studio start-up smoke exited 0.
Launcher `Tools/night_sky/LaunchCloudsV2CoveragePreview.cmd` opens a fresh
application; the owner's previous Clouds V2 Studio process is not closed
or overwritten. Save/close the old preview before opening the same project
in the new executable. Owner visual signoff, performance and river-fork
integration remain pending. Do not push/merge automatically.

## Gate 1 — Unwanted fine cloud fragmentation (owner image 22:16)
Owner reported too many tiny, disconnected cloudlets between the large,
moonlit clouds. This is not the target visual. In that screenshot Cloud
Base and Depth were both 10,000, placing all cloud formations exceptionally
high above the viewer and amplifying their apparent small scale.

Investigation:
- Wicked's generated 2D weather map used 100% Worley for coverage
  (`coveragePerlinWorleyDifference=1.0`), then subtracted a full fine
  Perlin octave. This created many small separated weather-map regions
  before the true 3D cloud-density/erosion pass.
- UniStorm reference lesson: broad 2D weather distribution and macro-shape
  are separate from fine 3D density, curl erosion and actual lighting.
  Do not copy UniStorm shader/assets; independently use this architectural
  separation in Renegade.
- Candidate A (retained): blend broad Perlin and Worley coverage
  (`coveragePerlinWorleyDifference=0.35`) and subtract half rather than
  all of the extra Perlin breakup octave. Implemented via idempotent CMake
  overlay into Wicked's weather-map generator; no new GPU ABI changes.
- Candidate B (rejected/reverted): automatically force cloud-type selection
  toward medium/large at high coverage. GPU proof showed *more* distracting
  fragmented clouds and poorer sky shape. Removed from authored header,
  generated shader and CMake overlay. Coverage must not silently alter type.

Evidence:
- DX12 proof: `BUILD/clouds-v2-gate1-macro-masses-dx12/`.
- Actual Vulkan proof: `BUILD/clouds-v2-gate1-macro-final-vulkan/`.
- Extra 80% coverage captures with Base/Depth 10000/10000:
  `clouds-v2-night-up35-highbase80.png`, matching the problem scenario.
- Extra mid-altitude comparison Base 1600, Depth 4200 illustrates the
  effects of altitude and cloud scale.
- Native Night Sky, Moon, Shooting Stars, Moonlit Cloud and Clouds V2
  contract tests pass 5/5 after reverting candidate B.
- Separate Studio/Runtime Release builds:
  `BUILD/moonlit/Studio/Release/RenegadeStudioCloudsV2Macro.exe` and
  `BUILD/moonlit/Runtime/Release/RenegadeRuntimeCloudsV2Macro.exe`,
  Studio startup-smoke exited 0.
- Isolated preview launcher `Tools/night_sky/LaunchCloudsV2MacroPreview.cmd`.
  Existing running owner Studio processes and preview files were not touched.

**Status**: Some improvement in broader cloud masses is visible, but
fine cloudlets persist at high cloud altitudes and visual quality remains
UNAPPROVED. The durable Clouds V2 solution is to decouple cloud coverage,
formation TYPE (cumulus / towering cumulus / stratocumulus / stratus / cirrus),
height profiles, macro shapes, and high-frequency erosion. Use simple presets
for user-facing controls rather than dozens of scattered sliders. Preserve
full 0–1 sky coverage behaviour and true 3D volumetric rendering.
Do not merge to main, replace accepted river-enabled Wicked fork, or begin
Snow V2 before Weather gates are completed.

## Coverage=0.8 dense sky regression — owner screenshot (22:28)

The real Studio image at Base=1500, Depth=5000, Coverage~0.8 had
noisy, nearly continuous stratocumulus-like cloud texture overhead,
with repetitive small internal patterning. This is **not** accepted as
the intended realistic cumulus quality and is not a Gate 1 sign-off.

Investigation:
- Previous macro weather map version (0.35 Perlin/Worley fraction, 0.50
  fine-cut subtract) merged clouds excessively and produced a ceiling.
- More aggressive separation (0.70 fraction, 0.80 subtract) was too sparse
  in the GPU fixture, with most clouds at the horizon.
- A middle experiment uses 0.55/0.65, and delays forced overcast blending
  from the former 0.75 threshold to 0.88. At Coverage=1.0, full overcast
  is still available, while 0.8 remains in the broken-cloud region.
- Redundant temporary CMake replacements were consolidated to a single
  idempotent weather-map source patch, verified by repeated reconfigure.
- Two sources of difference remain: the *camera/world-space location*
  of 2D weather-map patches and distinct authored cloud-type/shape parameters.
  A fixed synthetic capture cannot certify the exact live Studio viewpoint.
- The primary Coverage slider controls layerFirst; Wicked's default
  layerSecond.coverageAmount is 0.0, so a second default layer is not the
  explanation (an authored scene can potentially override this).
- Higher density at a particular camera location, even for the same nominal
  coverage, can occur: the current slider is not a measured sky-fraction
  estimator, only a parameter controlling weather-map density.

Verification of middle candidate:
- Release embedded shaders, DX12 GPU capture in
  `BUILD/clouds-v2-balanced-midpoint-dx12/`, and genuine Vulkan capture in
  `BUILD/clouds-v2-balanced-midpoint-vulkan/` passed.
- Native/source test subset 3/3 passed during the DX12 build.
- Separate Studio and Runtime Release builds passed, Studio startup smoke
  exited 0:
  `BUILD/moonlit/Studio/Release/RenegadeStudioCloudsV2Balanced.exe`
  `BUILD/moonlit/Runtime/Release/RenegadeRuntimeCloudsV2Balanced.exe`
- Optional isolated launcher:
  `Tools/night_sky/LaunchCloudsV2BalancedPreview.cmd`
  The owner has an *unsaved* Studio scene in the original process.
  Do not terminate or change it. Save and close it manually if using
  the candidate to open the same preview project.
- The earlier macro Studio and other worktrees remain untouched.
  This is an experimental branch; do not merge, push, or mark Gate 1
  accepted.

**Next engineering direction:** Proper cloud formation profiles, separate
from overall Coverage: cumulus, towering cumulus, stratocumulus, overcast,
and high cirrus. Independent weather distribution, 3D shape/height profiles,
and small-scale erosion controls are preferable to further global noise
constant guessing. Keep true volumetric raymarching and moonlight, and
test moving viewpoints at 1080p/1440p.
