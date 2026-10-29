# CODEX HANDOVER — Renegade Weather V2 / Gate 1: realistic volumetric clouds
**Date:** 28 October 2026 | **Machine:** DESIGN | **Owner decision:** STOP ad hoc cloud-noise tuning; transfer architectural investigation and implementation to Codex.

## 0. Mandatory first actions
1. Read this entire document, then:
   - `C:\Users\paulw\source\repos\renegade-moonlit-clouds-20261028\docs\CLOUDS_V2_GATE1_2026-10-28.md` (all sections, including late updates).
   - `...\docs\MOONLIT_CLOUDS_V1.md`, `...\docs\MOON_AUTHORING_V1.md`, `...\docs\SHOOTING_STARS_V1.md`.
   - Relevant `HANDOFF.md` entries; study current source paths listed below.
   - `C:\Users\paulw\source\repos\renegade-inventory-core-20261026\docs\CODEX_INVENTORY_RIVER_INTEGRATION_HANDOVER_2026-10-28.md` for the accepted river/Wicked fork integration and build environment, if this document is present.
2. Inventory *all* existing worktree and WickedEngine submodule changes before editing. Do not reset, clean, stash, overwrite or delete source or unsaved editor scenes.
3. Create a **new, independent Codex worktree**; never work directly in the current moon, clouds, main DESIGN or inventory integration worktrees. Refresh `origin/main` safely (`git fetch`, never `git reset --hard`).
4. Present a precise diagnosis, implementation plan and first visual acceptance slice. Stop trying random scalar changes to Perlin/Worley constants.

## 1. What the owner needs — not what the shader currently does
A high-quality **truly volumetric** cloud system, detailed enough for close viewing and consistent from all camera angles:
- Real 3D cumulus, towering cumulus, stratocumulus, overcast/stratus and high wispy cirrus-like shapes. Do not fake large clouds with skybox images or ordinary billboard sprites.
- Broad connected cloud *bodies* with coherent formation scale; small 3D noise is **surface erosion/detail**, not a field of thousands of miniature detached cloud pieces.
- Cloud Coverage independent of formation type/height/erosion. Coverage 0 means clear; increasing coverage gives a meaningful, progressive increase in sky occupancy. Coverage 1 must **permit full overcast**, preserving height/detail and lighting; 0.8 must NOT be a noisy featureless ceiling or a nearly clear sky merely because of the camera position.
- Distinct cloud-top billows, believable bases, variable vertical extent, daylight scattering, moonlit silver edges, cloud self-shadowing, terrain shadows, moving volume and temporal stability.
- Simple editor: a handful of selectable formation presets (Cumulus, Towering Cumulus, Stratocumulus, Overcast, Cirrus) and Coverage; optional Advanced section, avoid 30 extra sliders. Existing Base/Depth and accepted Moon/Environment controls stay usable and save/reload correctly.
- No unrelated Weather G2–G5 work yet; **Snow V2 is explicitly postponed**.

## 2. Exact current state at handover (observed on DESIGN)
**Prototype (UNAPPROVED):**
- `C:\Users\paulw\source\repos\renegade-moonlit-clouds-20261028`
- Branch `feature/moonlit-clouds-v1`, HEAD `11bbb90`, extensive **uncommitted and untracked** Moon/Night Sky/Shooting Stars/Clouds V2 changes. Source and generated Wicked shader changes are not safely represented by HEAD alone.
- This old worktree uses WickedEngine gitlink `3a800b7134aafe58461093c8abb2e274d4e64033` + **dirty submodule source**. Do NOT copy the entire submodule tree or change its remote.
- `origin/main` in that worktree was last locally observed at `150eca4` (stale until fetched), with the **accepted river-enabled** Wicked gitlink `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc`; after fetching, confirm the *actual* latest main SHA/fork pin before any integration.
- Another integration worktree `C:\Users\paulw\source\repos\renegade-inventory-river-integration-20261028` at `f4053e9` includes owner-approved Inventory G1–G5 merged with river/waterfall, uses the newer fork pin. It is **not a scratch workspace**.
- Original Moon worktree `C:\Users\paulw\source\repos\renegade-moon` at `11bbb90` contains distinct uncommitted Moon work. Main local checkout `C:\Users\paulw\source\repos\renegade-engine` is heavily dirty with unrelated UI/assistant changes. **Protect them all.**
- At handover, multiple isolated Studio programs were open, including `RenegadeStudio.exe` and `RenegadeStudioCloudsV2Balanced.exe`; owner may have unsaved `Main.WISCENE *`. **Do not terminate, switch, relaunch, save or discard their scenes on behalf of the owner**.
- No clouds PR, push, owner acceptance or merge. Any prior “5 green” claims mean only scoped shader/source tests, not cloud visual acceptance or full GitHub CI.

## 3. Critical source of truth and paths
Current prototype (read these **before** architecture proposals):
- `Tools/night_sky/renegadeCloudsV2.hlsli`: 3D height sculpt (`RenegadeSculptCloudHeight`), fine edge erosion (`RenegadePreserveCloudErosionDetail`), scalar Coverage blending (`RenegadeResolveCloudCoverage`).
- `cmake/RenegadeNightSkyOverlay.cmake`: copies authored shader includes into Wicked and injects all currently uncommitted Moon/Clouds patches. Treat as the source-of-truth overlay, **not** the edited generated Wicked files alone.
- `WickedEngine/WickedEngine/shaders/volumetricCloudsHF.hlsli`: `SampleWeather` (~line 449), 3D `SampleCloudDensity` (~line 480), height gradient, 3D noise sampling and erosion (~line 539).
- `WickedEngine/WickedEngine/shaders/volumetricCloud_weathermapCS.hlsl`: **2D weather distribution map**; now candidate `coveragePerlinWorleyDifference = 0.55`, breakup subtraction `0.65 * perlinNoise4`. These are *experimental*, NOT approved artistic defaults.
- `WickedEngine/WickedEngine/shaders/volumetricCloud_renderCS.hlsl` and `volumetricCloud_shadow_renderCS.hlsl`: actual volumetric raymarch and shadows; preserve moonlight/daylight separation.
- `WickedEngine/WickedEngine/shaders/ShaderInterop_Weather.h`: 3D cloud layers and parameters; changing its layout requires a versioned, consistent CPU/HLSL update, scene serialization, and new tests.
- `Tools/night_sky/renegadeMoonCloudLighting.hlsli`, `renegadeMoon.hlsli`, `renegadeNightSky.hlsli`, `renegadeAurora.hlsli`, `renegadeMilkyWay.hlsli`: accepted/owner-reviewed visual features to preserve.
- `Studio/src/StudioApplication.cpp` near Cloud Coverage, Base, Depth controls (~lines 2007–2032), `Studio/src/NightSkyStudioIntegration.cpp`, existing WeatherState/WeatherField implementation and Undo/Redo paths.
- `Tests/NightSkyGraphicsProof.h`: synthetic GPU capture, many daylight/night/cloud views and coverage values; `Tests/CloudsV2VolumetricContract.cmake`; `Tests/MoonlitCloudSourceContract.cmake`.
- `BUILD/clouds-v2-balanced-midpoint-dx12/`, `BUILD/clouds-v2-balanced-midpoint-vulkan/`: *synthetic* capture images; `frame-times.csv` is serialized 960×540 CPU+GPU loop timing, NOT a production GPU FPS budget.
- Most recent separately named candidate: `BUILD/moonlit/Studio/Release/RenegadeStudioCloudsV2Balanced.exe`; `Tools/night_sky/LaunchCloudsV2BalancedPreview.cmd`. **Do not assume this editor is the one displaying a given screenshot.**

## 4. Confirmed underlying renderer and why this failed
Wicked already uses true 3D volumetric raymarching with roughly 64³ shape texture and 32³ detail texture plus 2D weather distribution. We should **retain and improve** this renderer; 2D weather maps do not make the rendered clouds billboard geometry.

Sequence of failed/partially successful attempts:
1. Moonlit Clouds V1 added phase/direction/altitude-aware moon illumination to existing 3D cloud scattering. Owner's screenshot looked dramatically better than black clouds; do not replace it with a uniform cloud tint. Moonlit test is a **foundation**, not complete day/night approval.
2. V2 height sculpt and fine erosion added actual 3D detail. Native DX12/Vulkan GPU tests and source contracts pass, but visible improvement was modest. Overhead clouds were sometimes sparse at high Coverage.
3. Coverage 1 previously multiplied the weather map by 1, leaving gaps. `RenegadeResolveCloudCoverage` blended to weather coverage 1 and fixed the *ability* to render overcast. Initially blend began at 0.75, which overfilled 80% into a noisy dense ceiling. Latest candidate delays it to `smoothstep(0.88, 1.0, Coverage)`.
4. Full Worley weather map and strong octave subtraction produced too many small fragments. Patch `0.35/0.50` created broad joined cloud patches — **too much textured ceiling** at owner scene 0.8 / Base 1500 / Depth 5000. Aggressive `0.70/0.80` created an **almost empty sky** in the synthetic capture. Intermediate `0.55/0.65` compiles but the synthetic test still shows sparse clouds and **has not resolved owner visual complaints**.
5. Forcing weather-map cloud type towards larger types at higher Coverage made the little fragments WORSE; **reverted**. Do not repeat silently coupling cloud type to Coverage.
6. Fixed synthetic GPU tests often render clear sky / clouds near the horizon while owner camera sees a noisy full ceiling. **Different view/world coordinate, weather map area, parameter defaults or scene history** may explain this. The existing tests are insufficiently representative; investigate that discrepancy before further tuning.
7. At cloud Base 10000 / Depth 10000 small features look even smaller, but at 1500 / 5000 the owner still sees the overcast/fragmentation defect. Altitude alone is NOT the answer.

The key fault may be **architectural coupling** between cloud distribution, formation-type mask, height density and fine 3D erosion, and the absence of stable formation profiles and measurable sky-fraction semantics. Treat this as a hypothesis to prove, not a pre-decided shader implementation.

## 5. Owner visual failures to reproduce
- Screenshot around 22:16: Coverage ~0.8, very high cloud base/depth, innumerable tiny white/grey cloud fragments between larger dark clouds; moonlit sky.
- Screenshot around 22:28: Coverage ~0.8, Base 1500, Depth 5000, **entire top half becomes a mottled, repetitive, nearly continuous dark cloud ceiling**. This triggered the Codex decision.
- Other screenshots: at Coverage 1 previously huge clear gaps; subsequently fully overcast at 1 was possible but could be featureless.
- Owner requests: realistic, detailed cloud bodies; no cartoon clouds, no tiny speckle field, no generic grainy overcast; useful from daylight, dusk and under moonlight.
- At 75 FPS the editor is likely display/refresh-rate limited: **75 FPS is not a GPU performance proof**.

## 6. Codex work phases — work independently, review evidence, do not merge
**Phase A — audit and reproducible evidence (first deliverable):**
- Inspect all related source, actual shader binding, two cloud-layer settings, weather map generation/caching, CPU scene state, saved scene data, camera altitude/pitch, wind/time evolution, exposure, dithering and temporal reconstruction.
- Find why identical Coverage/Base/Depth synthetic and live Studio views differ. Extract the actual scene weather state, ideally via read-only utility, and implement a **deterministic, replayable** scene/camera/seed capture without modifying owner's saved scene.
- Generate daylight, midnight and twilight comparison captures at Coverage 0/0.25/0.5/0.8/0.95/1.0; look level, up 35°, up 70°, and rotate/move through weather field. Capture 80% with Base 1500, Depth 5000 explicitly.
- Establish true sky-pixel coverage metric (exclude moon/objects/ground; avoid using a single image average brightness as proxy), cloud-size distribution and cloud-edge noise. Record source Git SHAs and actual backend command.

**Phase B — coherent cloud-formation system (Gate 1):**
- Propose and implement separate macro cloud distribution, 3D shape/height profile, and micro erosion. Give meaningful formations/presets for cumulus, towering cumulus, stratocumulus, overcast and cirrus or a documented incremental subset with a plan for all five.
- Do not assume just higher frequencies or fewer noise octaves is enough. Distinct heights, coverage distribution, scale, thresholds and erosion are legitimate separate signals. Profile actual texture lookups/march steps.
- Coverage should request intuitive sky occupancy independently of formation type, with overcast at 1; don't depend on sample camera alignment. Preserve volumetric cloud movement, shadows, correct moon/day interplay.
- Keep user UI compact with one Cloud Type selector, Coverage, Base and Depth and Advanced hidden by default. Persist preset/type and undo/redo; do not reinterpret old scenes without a migration/default policy.

**Phase C — verify before owner preview:**
- Automated source/native tests plus **actual DX12 and Vulkan** shader compilation, real GPU captures and Studio/Runtime Debug and Release when ready.
- Inspect real graphical results and measure temporal ghosting, cloud movement, shadow edges, horizon/zenith consistency, and quality at 1080p and 1440p. Do not infer production performance from 960×540 smoke captures.
- Scene save/reopen, Undo/Redo, legacy scene fallback, packaged shaders, moon/new/full/quarter, night sky/aurora/stars, cloud shadows, fog, ocean and accepted river/waterfall integration regressions.
- Build **new separately named candidate**; no overwriting or closing the owner's running Studio. Owner has to open preview and approve. Gate 1 stays OPEN until owner says yes.

**Phase D — integrate only after approval:**
- In a new isolated integration worktree based on fetched latest `origin/main`, preserve the exact newer **river-enabled Wicked fork pin**. Rebase/port ONLY accepted bounded Moon/Cloud changes; don't replace the entire Wicked submodule and don't copy its old gitlink.
- Bring along Moon source files that are currently untracked where required. Preserve river/waterfall, inventory, original night sky/aurora and projectile systems. Do not reset unrelated fork edits. Run four CI-equivalent builds (Studio/Runtime Debug/Release), source tests and packaged visuals. No GitHub PR, push or merge without owner approval.

## 7. Acceptance criteria (must all be demonstrated)
- Camera sees coherent, differently sized, properly shaped **3D clouds** at 0.8 rather than all speckle OR all blanket; distinct realistic cumulus vs stratocumulus vs overcast vs cirrus silhouettes.
- User can move from scattered to full overcast continuously; coverage semantics repeat across saved scenes, camera positions and day/night. No regression to a clear sky at maximum.
- Surface detail is subordinate to macro structure; no excessive isolated micro-fragments, square edge seams, visible repeating noise tiles, frame-to-frame flicker, or texture swimming.
- Volume responds to sunlight and moon phase/direction, including 3D shadowing, while retaining a realistic nighttime sky.
- Owner preview tested in actual Renegade Studio, not just synthetic offscreen screenshots. Performance measured without 75-FPS VSync ceiling and compared against an original Wicked baseline.
- No loss of previously accepted environmental, river, inventory, UI and night-sky features.

## 8. Tooling / constraints
- Known working CMake is the installed Kitware executable under `C:\Users\paulw\AppData\Local\Microsoft\WinGet\Packages\Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe\cmake-4.4.4-windows-x86_64\bin\cmake.exe`. VS2022 x64 builds work; MSBuild has been available via `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`. Verify what is actually installed rather than assuming VS version from path.
- Example CMake configuration on isolated path:
  `& '<Kitware cmake.exe path>' -S '<new codex worktree>' -B '<new codex worktree>\BUILD\clouds-gate1' -G 'Visual Studio 17 2022' -A x64 -DRENEGADE_EMBED_SHADERS=ON`
- GPU proof entry: `RenegadeNightSkyTests.exe --graphics-proof <output-dir> dx12` or `... vulkan` (**literal `vulkan`, not `-vulkan`**). Studio's smoke argument is literal `startup-smoke`, NOT `--startup-smoke`.
- Do not use unlicensed Unity/UniStorm shaders or textures. Can study cloud architecture and independently implement equivalent techniques; do not import the pack without proven rights.
- C: free was ~23 GB at investigation. Avoid creating numerous huge full builds or re-downloading SDKs; clean ONLY disposable build artifacts you created, and only after checking ownership. Keep source and owners' backups intact.

## 9. Completion/report contract
At each stage, report: exact worktree/branch, source files modified, submodule gitlink and dirty state, visual examples at 80% and 100% day/night, whether owner scene mismatch is resolved, actual tests and their outputs, performance comparisons, unresolved defects and safe rollback path. Never call G1 complete based solely on a shader build, source-contract test or fixed synthetic camera.

**Immediate Codex task:** Start with Phase A and identify the root cause of the live-editor / synthetic-capture mismatch and cloudy-ceiling/micro-fragment distribution. Then design the cloud-formation architecture. Do not just adjust another constant; do not make a PR/merge. Provide a separate owner-test build once evidence shows it is materially better.
