# Weather V2 Gate 1 — morphology, placement and Moon checkpoint (29 October 2026)

**LATEST CONTINUATION:** Read docs/CLOUD_FORMATION_GATE1_HYBRID_MOON_STUDIO_2026-10-29.md for the subsequently corrected generated C++ cloud placement, hybrid GPU fit and saved-actual-Studio Moon diagnostic. Earlier numerical results in this document predate the source synchronization and are retained as historical comparisons.

**Status: Gate 1 OPEN; NO new owner preview approved or offered.** Both historical previews were rejected. This is an internal Cumulus-only architecture experiment, not a complete Weather V2 cloud implementation. Four other formation types are still unapproved prototypes.

## Read these first
1. `docs/CODEX_CLOUDS_RESUME_2026-10-29.md` (earlier exact checkpoint; this document supersedes its morphology TODO and source/build status).
2. `docs/CLOUD_FORMATION_GATE1_LIVE_REPLAY_2026-10-29.md`, `docs/CLOUD_FORMATION_GATE1_FIELD_METRIC_2026-10-29.md`, `docs/CLOUD_FORMATION_GATE1_BODY_CACHE_DECISION_2026-10-29.md`, `docs/CLOUD_FORMATION_GATE1_WHOLE_BODIES_2026-10-29.md`.
3. Original `C:\Users\paulw\source\repos\renegade-moonlit-clouds-20261028\docs\CODEX_WEATHER_GATE1_CLOUDS_V2_HANDOVER_2026-10-28.md` and referenced preservation requirements.

## Isolation and preservation
- Existing isolated tree: `C:\Users\paulw\source\repos\renegade-cloud-formations-g1-20261028`.
- Branch `codex/cloud-formations-g1-20261028`; root HEAD `150eca4b761d3f62f122df3ab5e8536ec084f926`.
- Accepted river-enabled Wicked fork gitlink AND checkout `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc`.
- Dirty source/overlay expected. No commits, reset, clean, stash, push, PR or merge. Other owner worktrees and potentially unsaved Studio windows untouched. No Weather Gates 2–5 or Snow V2.
- `BUILD/morphology-families-preservation.json`: **45/45 original authored files SHA256 preserved**, native legacy density/albedo preserved, root and river fork pins preserved. Prior evidence paths retained; no previous proof overwritten.
- `git diff --check` exit 0; `BUILD/morphology-final-diff-check.log`.
- C: owner freed additional disk space during this session; last measured ~86.66GB available. Do not delete owner sources, builds or unrelated files.

## Authored changes made this session (NOT only generated Wicked source)
1. `Tools/night_sky/renegadeCloudBodyTemplate.hlsli`: true connected 3D Cumulus thermal hierarchy with coherent condensation base, shoulders, rising thermals and optional crown lobes. Four structurally different cached **Cumulus subfamilies**: broad/low; narrow/high; twin-crown/multicore; asymmetrical/leaning. Crown altitude bounded for atlas support. No third-party shaders/assets.
2. `cmake/RenegadeCloudBodyCacheOverlay.cmake`: deterministic 3km site x/z displacement increased from ±600m to ±1350m, preserving 64×64 cached site buffer, balanced 4×4 rank admission and accepted fork shader overlay.
3. `Tools/night_sky/renegadeCloudFormations.hlsli`: independent per-site X/Z size and height (not isotropic width), expanded rotating **4×4 candidate window**, 16 per-body bits plus sheet bit 65536, conservative 220m empty-space skip cap. No scene/ShaderWeather ABI changes. Correct axis-specific gradient metric. New 17-entry deep/thin admission LUT from 48 raw GPU views per layer, using conservative sampled-view envelope; 80% deep probability .598604269, thin .693615385. 100% probability exactly 1.0. **LUT is statistical, NOT proof of visual coverage in all views.**
4. `Tools/night_sky/renegadeMoonCloudLighting.hlsli`: removed erroneous proportional dependence on primary Sun light intensity 5.5. Moonlight is now independent `float3(.50,.60,.74) * strength` with existing phase, above-horizon, night and authored Moon brightness gates; physically uncalibrated artistic scale. Keeps native volumetric light/shadow pipeline, Moon disc/texture and daylight intact. Earlier authored shader backed up at `BUILD/morphology-pre-lunar-illumination.hlsli`.
5. QA-only `Tests/CloudFormationGraphicsProof.h`: `single-cloud-variant-study` renders each subfamily below/side/top with private sentinel, cached and analytic.
6. QA-only `Tests/NightSkyGraphicsProof.h`: explicit `cloud-moon-visibility-study` in disposable offscreen GPU fixture for clear/overcast on/off, intensity, phase and below-horizon comparisons. Does NOT interact with owner Studio projects.

## Verified in this session
- Kitware CMake configured existing `BUILD/gate1` project repeatedly without error; latest embedded HLSL6/SPIR-V Release `RenegadeNightSkyTests` build exit0. `BUILD/morphology-family-final-release-build.log`.
- Release focused native build exit0, then **7/7** CTest passed (NightSky, NightSky source, Moon, Shooting Stars, Moonlit source, CloudsV2 source, Cloud Formation): `BUILD/morphology-family-final-ctest-release.log`.
- Deterministic 9000-position 4×4 bounds/skipping proof: zero omitted valid bodies, zero omitted site within 220m safe skip distance, float-exact 17-bit mask. `BUILD/morphology-window-invariants.json`.
- All four 3D cache template support bounds safe (maximum local support X/Z ±2300m, Y 0..3500m), `BUILD/morphology-template-support-audit.json`.
- **24 cached vs analytic** comparisons (four variants × below/side/top × DX12/Vulkan): largest mean opacity delta .04955/255 bytes, largest pixel delta17, occupied fraction max error .000347. `BUILD/morphology-four-families-parity.json`.
- 96 new raw sky poses (48 deep, 48 thin) used for final family fit: `BUILD/morphology-families-calibration.json`. Full 80% day/night/reference-quality actual GPU capture exited0 on DX12 and Vulkan: `BUILD/morphology-families-final-{dx12,vulkan}`, logs `BUILD/morphology-families-final-{dx12,vulkan}-run.log`.
- Example **current** DX12 80% day p35: `BUILD/morphology-families-final-dx12/type1-c80-h9-p35-canonical.png`. Opacity from `BUILD/morphology-families-occupancy-compare.json`: 88.84% occupied at p35, vs previous 91.03%, still fails visually. P70 now 56.18% vs 61.88% previously. This is not uniform sky-fraction semantics over all camera positions.
- Controlled Moon GPU proof (final source, DX12 AND Vulkan exit0): `BUILD/moon-family-final-{dx12,vulkan}`. `BUILD/moon-family-final-check.json`: fixed above-horizon Moon visible with clear sky; dense 80% Cumulus occludes disc; full Moon produces visibly stronger cloud illumination than quarter, new/below-horizon nearly matches Moon-off. No claim actual Studio disappearance solved.
- Earlier controlled Moon isolation before latest families (same authored lunar formula) `BUILD/moon-independent-metrics.json`: scene with Moon disabled remains unchanged; clear-sky Moon RGB unchanged; new 80% Moon-on mean luminance 12.856 vs previous 30.687/255; fraction above 32 luma 5.86% vs 38.28%. Do not infer physical light calibration.

## Known defects / NOT accepted
1. **Most important visual failure:** 80% Cumulus still forms an overly uniform, near-continuous soft cloud ceiling. At 35° p9, sky occupancy 88.84% in current canonical test; billows appear smooth, and many distant towers retain repetitive silhouettes despite new four subfamilies and varying x/z scale. **DO NOT offer this candidate for owner preview or call Gate1 complete.**
2. Current four Cumulus subfamilies are NOT four other Weather formation types. Towering Cumulus, Stratocumulus, Overcast and Cirrus need distinct appropriate 3D architectures. Cirrus must not be stretched Cumulus. Real full-overcast visuals at 100% need inspection for detail; a 100% body-admission endpoint alone is not enough.
3. **Owner reports Moon absent in actual Studio despite enabled checkbox.** Controlled synthetic GPU clear sky renders Moon when fixed above horizon; same view at 80% cloud can hide Moon through legitimate 3D occlusion. Real Studio Moon checkbox/hydration, Follow Time of Day orbit/horizon, custom skybox status, camera and cloud occlusion NOT reproduced in owner's exact unsaved scene. Do not assert resolved, do not manipulate owner window.
4. Independent lunar irradiance is a promising improvement, but still artistic and needs actual Studio moonlit scattering review, moving viewpoints and lighting/shadow acceptance. Avoid brightness-only scalar tweaking.
5. No current-source Studio/Runtime Debug and Release final app builds, fresh full CTest, packaged real Studio/Runtime DX12/Vulkan, river/fog/ocean/aurora gameplay visual regressions, 1440p/current FPS comparison, Studio save/Undo/Redo or 600sec long-wind validation since final morphology. Prior checkpoint evidence predates this work. Previous unpredictable dxcompiler.dll crash remains unroot-caused, not repaired here.
6. Historical visuals/replay explain controlled NEW-build opacity parity, not every old screenshot or original owner scene mismatch.

## Exact next engineering decision
Avoid more arbitrary noise/coverage constant tweaking. The 4×4 site bound and cache now pass strong proofs, but large neighbouring volumes overlap along shallow sky rays. Need a coherent *formation distribution* model that breaks repeated rows and giant joined undersides without an independent 2D mask cutting cylinder-shaped cloud bodies. Previous 8km regional admission failed with huge variance (43–99% coverage) and independent planar crop cut cylinders; don't repeat those designs blindly. Potential next bounded trial: clustered/intermittent storm-cell groups with full 3D whole-body admission and conservative neighbour enumeration, separately measure canonical and held-out sky occupancy and screenshots. Also consider how camera ray integrates foreground and distance; don't equate admission to view coverage. Verify new preview visually BEFORE full four-app matrix or owner launch.
Retain Moon light independence, explicitly test real Studio Moon visibility in a separate disposable GUID project/window (never touch existing unsaved owner Studio). Only then, if genuinely improved, complete full Release/Debug Studio/Runtime, 1080p/1440p DX12/Vulkan, all environmental/regression checks and a separately named owner preview. **Gate 1 stays OPEN** until owner expressly accepts.

## Rollback and source of truth
- First template checkpoint: `BUILD/morphology-prechange-20261029/renegadeCloudBodyTemplate.hlsli` (before this session's template edit). Note the other two files in that folder were saved AFTER the initial placement edit, so don't treat their contents as pre-session originals.
- Fitted LUT backups: `BUILD/morphology-precalibration.hlsli`, `BUILD/morphology-aniso-precalibration.hlsli`, `BUILD/morphology-families-precalibration.hlsli`.
- Moon original: `BUILD/morphology-pre-lunar-illumination.hlsli`.
- Authored HLSL/CMake are the source of truth. Never restore or edit only generated Wicked files. Never reset the dirty fork or any owner checkout.
