# G4 dense-cloud performance investigation - 30 October 2026

Owner has now visually/audibly approved clouds, lightning and thunder: no horizon blur, cloud jitter or grid. Recording: Desktop 2026-10-30 21-22-54.mp4. Preserve this appearance. Performance is NOT signed off. Owner reports 58-60 FPS with high-coverage Storm and independently confirmed 48-50 FPS during the editor investigation below.

## Confirmed results

DESIGN / RTX 4070 Ti / DX12 Release. Temporary opt-in diagnostics only; no approved shader, geometry, scene, rain/snow asset or audio implementation changed. Seven matched standalone cloud-only runs used the isolated owner-scene snapshot, fixed camera at (0,1.5,-4), fixed yaw, Storm base 750 m and thickness 6500 m, rain and lightning off, VSync-off request, 1080 host samples after 120 warmup frames. Effective coverage, height/thickness, camera direction and zero flash energy were validated from per-frame CSV; no rendered-scene settings were inferred from stale inspector labels. Cloud motion remained enabled. A 35-degree pitch was the only camera change in the two upward cases.

| Coverage / view | Host average FPS | GPU median / P95 ms | Capture median ms | Visible raymarch median ms |
|---|---:|---:|---:|---:|
| 50%, horizon | 95.91 | 10.049 / 12.303 | 4.510 | 3.513 |
| 80%, horizon | 73.04 | 13.327 / 15.319 | 5.713 | 5.233 |
| 90%, horizon | 72.75 | 13.294 / 16.060 | 6.259 | 4.447 |
| 95%, horizon | 79.49 | 11.957 / 14.139 | 4.861 | 4.521 |
| 98%, horizon | 107.48 | 8.931 / 10.310 | 3.951 | 2.462 |
| 80%, up 35 degrees | 69.91 | 13.872 / 15.759 | 5.717 | 5.846 |
| 95%, up 35 degrees | 75.18 | 13.268 / 14.311 | 4.805 | 6.247 |

An earlier 50%-80%-98%-50% repeat gave GPU medians 10.057, 13.321, 8.885, 10.038 ms: the repeated 50% result differs by under 0.2%. Cost is NOT monotonic with the coverage slider. The code admits more whole cloud bodies as coverage increases, then begins a coalescing base above 94%; once opaque, rays terminate at the existing transmittance threshold. That is a plausible explanation for the 98% speedup, not a measured per-ray occupancy/shadow-instruction count.

## Actual editor reproduction, primary display

The first editor capture reproduced about 50 FPS with the loaded owner scene (actual coverage 95%, rain/lightning still authored). It revealed the temporary preset override was applied before the later scene load; it is NOT a cloud-only control. Keep it separate: CPU frame median/P95 18.616/20.568 ms, GPU frame 14.029/16.666 ms.

Corrected scene-revision-aware diagnostic then reproduced the owner's **48 FPS**, with actual coverage 90%, base 750 m, thickness 6500 m, eye (0,2,0), direction (0,0.573576,0.819152). Rain and lightning were explicitly disabled in the disposable scene after its load. Inspector labels were not refreshed by this test-only override and can still display prior authored Rain values; they are not the diagnostic source of truth. Last 1000 scene frames (approximately 20 seconds), warm scene:

| Range | Median ms | P95 ms |
|---|---:|---:|
| CPU Frame | 18.916 | 20.775 |
| GPU Frame | 17.567 | 21.031 |
| GPU visible cloud raymarch | 8.478 | 12.099 |
| GPU all-sky cloud capture | 6.578 | 7.777 |
| CPU Studio Update Total | 12.580 | 13.949 |
| CPU native RenderPath3D Update | 1.415 | 1.751 |
| CPU Player Camera Preview check | 2.246 | 2.955 |
| CPU icons / brush checks | 2.214 | 2.448 |
| CPU assistant tick | 0.0003 | 0.0006 |
| CPU precipitation refresh | 0.0006 | 0.0011 |

CPU and GPU frame times overlap and must NOT be added. GPU time alone exceeds the 13.333 ms budget for 75 FPS, and editor CPU work also exceeds it. This explains why optimizing only audio or reconstruction cannot solve this case. The initial broad Update timing was narrowed; assistant and precipitation CPU processing are not the measured CPU bottleneck. The remaining uninstrumented editor Update work is not yet fully attributed.

Monitor inventory: primary DISPLAY1 1920x1080 at75 Hz; left DISPLAY2 (-1920,0)-(0,1080) at60 Hz. The first new test editor restored onto DISPLAY2. The captures above were moved to the primary display, maximized, and the corrected diagnostic requested VSync off. It still showed48 FPS. Therefore the 60-Hz display is a confound to control, NOT the explanation for the reproduced slow editor case. Initial automation touched windows too early and sent Alt; stack capture showed startup waiting in USER32/uxtheme. Those launches were discarded, runner was corrected to wait for completed startup and avoid Alt, and they are not performance samples.

## Code findings and ranked causes

1. Confirmed dominant GPU workloads: visible-cloud raymarch plus the separate 256x256 hemispherical all-sky cache. The latter runs every renderer frame and traces one of four phases (all four only on initial fill). The shader executes the same density/lighting kernel for the capture, with no camera opaque-depth culling. It is a real additional workload, not a second visible-camera history and not the earlier grid workaround.
2. Confirmed coverage-dependent work in the density kernel: RenegadeWholeCloudWeather tests a conservative 4x4 body window per sample. Higher admission admits more geometry calculations and density evaluations. Every occupied view sample additionally invokes volumetric self-shadow marching (and ground contribution when applicable), which resamples the field. View steps use the existing adaptive world-space budget up to384. Native GPU timestamps isolate the cost but do not identify register spills, texture bottlenecks or exact per-ray sample counts; those remain hypotheses pending a vendor capture/counters.
3. Confirmed redundant CPU preview work: StudioApplication.cpp UpdatePlayerCameraPreview calls ResolvePlayerStart BEFORE checking whether that entity is selected. Nothing was selected in this capture; the preview remained invisible, yet its check cost2.246 ms. PlayerService.cpp ResolvePlayerStart scans scene.metadatas and resolves player settings. HandlePlayerStartCapsule separately invokes it in the measured icons/brush phase (2.214 ms for the entire phase, not a separately timed resolver claim). Compose and other paths also call it. Repeated whole-scene resolution is a well-supported optimization candidate; the full unmeasured6+ms of Update must not all be blamed on it without additional timers.
4. Rejected explanations: no lightning or thunder was needed to reproduce48 FPS; the fixed cloud reconstruction is only hundredths of a millisecond in the coverage runs. Native Scene Update itself was about1.4ms in the editor phase capture. CPU assistant Tick and precipitation Refresh were negligible. Do not claim the whole regression is unavoidable weather load or blame a60-Hz monitor.

## Repair recommendation and limits

Keep the approved density field, extinction, lighting, motion, temporal reconstruction, sample budgets and effects unchanged. First independent CPU repair: reject an invalid/no selection before expensive preview resolution; share/cache player-marker resolution with proper invalidation on level load, add/remove, metadata changes, Undo/Redo and changed selection. Always read fresh transforms during a live gizmo drag; preserve multiple/missing-start validation and preview behavior. Test no-selection idle CPU time, selected Player Start preview, capsule selection, move, Undo/Redo and new-level load.

Separate GPU repair candidate: precompute exact admission/growth/body transforms/bounds per coverage/thickness/site revision, preferably with the same GPU arithmetic, and share those inputs between primary rays, shadow rays and the all-sky capture. Preserve the proven candidate support, density texture lookup and camera-independent field. Measure identical source/binary camera cases and pixel/motion parity before accepting. Do NOT lower coverage, density, sample counts or resolution as a disguised fix. Prior capture slicing produced no demonstrated benefit and was reverted; it is not an accepted solution. No75-FPS recovery is claimed or guaranteed yet.

The G1 acceptance document scopes75 FPS to a particular moving-cloud preview. It does not establish75 FPS for every later Storm height/thickness/camera. A comparison with the preserved exact G1 executable still needs a truly matching scene/view; current findings explain the current cost, not a quantified historical regression.

## Evidence, restoration and handoff

Raw timings/events and diagnostic source/binaries: external audit coverage-investigation folder, including validated/coverage-summary.json, editor90-gpu/events CSV, editor90-cloudonly-gpu/events CSV, editor-summary.json, editor-cloudonly-summary.json. Local scripts/logs: BUILD/g4-run-cloud-coverage.py, g4-run-cloud-validated.py, g4-cloud-coverage-active-runs.log, g4-cloud-validated-runs.log, g4-cloud-validated-resume.log, g4-studio-cpu-profile-build.log, g4-editor-cloudonly-analysis.log. GPU ranges retain source-frame IDs. CSVs buffer in RAM until normal shutdown. These are native DX12 timestamps, not PIX or a synthetic visual GPU probe.

Temporary diagnostic source was removed with BUILD/g4-restore-approved-after-profiling.py. It verifies every source in BUILD/g4-critical-source-manifest.json matches the exact pre-investigation candidate by SHA256, including all approved shaders, lightning geometry and audio fixes, before rebuilding normal Release Studio/Runtime. Restoration build: BUILD/g4-post-cloud-investigation-restore-build.log. Original saved scene and all9 thunder assets are checked again by the delivery manifest. No original scene was saved or overwritten; only diagnostic copies were closed. The previously preserved owner processes had already been closed externally before the editor tests, which is observed process state, not an assertion about whether their work was saved. No commit, push, merge or performance signoff.

Final cloud-only editor dt median/P95/mean: 19.8594/22.3978/20.0488 ms; average 49.878 FPS.
