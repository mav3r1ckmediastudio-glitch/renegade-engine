# Renegade Weather V2 Gate 1 - MOVING CLOUD PERFORMANCE OWNER ACCEPTED
Date: 29 October 2026
Worktree: C:\Users\paulw\source\repos\renegade-cloud-formations-g1-20261028
Branch: codex/cloud-formations-g1-20261028
Root HEAD: 150eca4b761d3f62f122df3ab5e8536ec084f926
Accepted Wicked river fork (unchanged): ceb816b844b4ab2fa56a159ac60a4f65c931bdfc

## Scope of owner confirmation
Owner explicitly replied **"confirmed"** to the 75 FPS review at Cloud Movement Speed = 8, with moving clouds on RTX 4070 Ti / DX12 Studio. This is acceptance **of that performance and motion check only**. Do not infer that cloud morphology, illumination, moon presentation, daylight, rain, Weather Gates 2-5, Snow V2, Test Game performance under all settings, or final Gate 1 are approved. Weather Gate 1 remains OPEN.

## Exact approved test candidate
- Review executable (copy, preserving other builds): BUILD/gate1/Studio/Release/RenegadeStudioCloudPerfNear64G1Oct29.exe
- SHA-256: 87025817AC89DFF9AF2B5CC3883402C08AF07653A8F430AC51482B1DC98936DC
- Preview launch args: --cloud-motion-preview --dx12; test project BUILD/cloud-body-motion-preview/CloudBodyMotionPreview.renegade; editor scene Main.WISCENE
- On-screen FPS at speed 8, when three other review processes were briefly paused for an isolated measurement: 75 FPS; BUILD/perf-near64-speed8-solo.png. Earlier speed 2 capture similarly showed 75 FPS: BUILD/perf-near64-isolated.png. Windows display refresh 75 Hz may cap observable FPS; no claim of headroom beyond cap.
- Three older reviewers were resumed with NtResumeProcess exit status 0 after each measurement. Do not close them, save other owners' dirty scenes, or remove these previews automatically.
- The 64-m near adaptive shader build was linked successfully: BUILD/perf64-studio-release-build.log.
- A separate 48-m adaptive + site admission build exists, BUILD/gate1/Studio/Release/RenegadeStudioCloudPerfOptimizedG1Oct29.exe; DO NOT overwrite previously accepted executable. Original moving cloud slider and cloud review binaries are also preserved.

## Implementation details
- Cumulus body admission test in Tools/night_sky/renegadeCloudFormations.hlsli checks cached site admission rank before expensive centre, rotation, phase, scale, and oriented bounds. Rejected sites follow the same result, without redundant calculations.
- cmake/RenegadeCloudQualityOverlay.cmake ensures near-field max ray step 64m and distance-adaptive up to 144m for the approved candidate (with maximum 384 samples), preserving 3D form, moonlight, cloud movement and controls. Changes in cloud opacity vs previous 48m adaptive version were small in six numerical GPU scenes (mean absolute opacity 0.29-0.45 on a 0-255 scale, with 0.47%-2.46% of pixels differing by >5). These are screen-space test metrics, not blanket zero-visual-delta guarantees.
- The rank-first optimization was exactly pixel-identical to its 48m adaptive baseline in six DX12 cloud-opacity tests: BUILD/perf-site-admission-quality.py results. Controlled GPU serialised timings improved, e.g. Cumulus speed2 from 129.8ms original -> 105.8ms rank-first 48m -> 85.3ms with 64m candidate, but GPU simulation timings are NOT Studio FPS.
- Individual scene/camera cloud performance varies. One lower Cumulus heavily covered night view showed 41 FPS with other Studio previews running; minimizing the older preview alone did not solve contention. Real isolated 75 FPS test and owner confirmed are the acceptance for the scoped preview. Do not claim isolated performance across the full game.

## Validation after owner confirmation
- BUILD/perf64-final-focused-ctest.log: 7/7 focused weather CTest green in Release.
- BUILD/perf64-final-bridge-tests.log: PASS weather command, scene save/reload, undo/redo, history and bridge checks.
- BUILD/perf64-final-preservation.json: all 45 originally protected authored files untouched; native density and albedo preserved; legacy DX12/Vulkan RGB unchanged, accepted Wicked river fork pin unchanged.
- git diff --check exit 0. No push, PR, merge, or commit performed.

## Next
Keep the approved movement and speed control working. Gate 1 remains open for the previously documented morphology, repetitive cloud placement, moon/cloud lighting and moon visibility defects pending a separate owner acceptance. Further work must preserve this approved performance baseline on DESIGN and test in Test Game/Runtime before broader signoff. Do not enter Weather Gates 2-5 or Snow V2.
