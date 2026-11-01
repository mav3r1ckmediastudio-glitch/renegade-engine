# Weather V2 Gate 1 — exact resume checkpoint, 29 October 2026

**NEWER CHECKPOINT:** Read `docs/CLOUD_FORMATION_GATE1_MORPHOLOGY_MOON_2026-10-29.md` after this file. Later on 29 October, an isolated, still-unapproved Cumulus morphology/placement family and independent lunar cloud-lighting study completed. Both DX12/Vulkan GPU tests and seven focused CTest checks passed, but **the current 80% sky still looks like a broad, soft ceiling**. The real Studio missing-Moon report is **not resolved** by a controlled clear-sky Moon capture. Current authored changes, preservation, evidence, exceptions and rollback are in the newer document; older source/build/LUT status below is historical where superseded.

Owner has rejected two previews ('they look worse'). Gate 1 is OPEN. No new owner candidate is ready. Communicate progress at least every minute; the owner was left waiting and has only 4% usage remaining. Avoid repeating completed investigations and huge tool outputs.

## Mandatory reading and preservation
Read this document, then the original handover IN FULL:
C:\Users\paulw\source\repos\renegade-moonlit-clouds-20261028\docs\CODEX_WEATHER_GATE1_CLOUDS_V2_HANDOVER_2026-10-28.md
Follow its referenced documents and preservation requirements. They were read in the preceding session.
Read docs/CLOUD_FORMATION_GATE1_LIVE_REPLAY_2026-10-29.md and docs/CLOUD_FORMATION_GATE1_FIELD_METRIC_2026-10-29.md in the isolated worktree. HANDOFF.md's first entry is STALE: wind fix and latest metric build have since completed. This checkpoint supersedes those status statements.

Existing isolated worktree (CONTINUE HERE; do not create another redundant build):
C:\Users\paulw\source\repos\renegade-cloud-formations-g1-20261028
Branch codex/cloud-formations-g1-20261028; HEAD 150eca4b761d3f62f122df3ab5e8536ec084f926.
Wicked checkout AND gitlink ceb816b844b4ab2fa56a159ac60a4f65c931bdfc, accepted river fork mav3r1ckmediastudio-glitch/WickedEngine, renegade-rocky-riverbed-v1. Dirty generated overlay changes are expected. No commits/push/PR/merge authorized. No Gates 2–5 or Snow V2.
45 protected original authored files were SHA256 reverified unchanged in latest BUILD/body-field-metric-preservation.json. Original manifest: C:\Users\paulw\source\repos\clouds-gate1-audit-20261028\source-manifest.json. Preserve ALL original worktrees/dirty source and unsaved owner Studio windows. Never reset/clean/stash or terminate owner Studios.

## Findings and completed work
1. Cumulus now consists of complete bounded 3D bodies, separately admitted by coverage, using immutable morphology cache and static site attributes. Unrelated 2D masks no longer cut cylinders through Cumulus. Other FOUR types still use the earlier UNAPPROVED native-profile path; they are not complete.
2. Conservative bounds/empty-space skips passed 48 comparisons across APIs, at most one opacity byte difference. Coverage fit used 192 raw views and 176 fitted/holdout/default/reference views. Still fails visually, especially steep 80% views.
3. Actual Studio scene/camera archive and replay implemented. Owner-like live view was 1920x1009, yaw45/pitch7.6, not assumed 1920x1080/yaw0/pitch35. Replay mean opacity error .50/.55 byte, occupied fraction difference <.002. This demonstrates controlled new-build geometry parity, NOT complete RGB parity or every historical screenshot cause.
4. Fixed actual vertical-wind bug: native wind.y is upward NOISE advection (negative), and moving macro height by it emptied the fixed Base/Depth march layer after 600sec. Now horizontal wind advects formations; original world altitude anchors geometry; vertical wind animates internal fine/water phase only. Actual GPU time/wind readback regression passes both APIs: before zero voxels with upward wind, after ~482k populated voxels for both up0/.5. BUILD/body-height-wind-check.json and body-wind-height-regression-results.json.
5. Expensive analytic geometry is packaged only in explicit RENEGADE_CLOUD_ANALYTIC_REFERENCE permutations. Production kernels do not carry its runtime branch. 808 embedded shaders. Optional cloud-shader-audit logs filename/bytes/hash and creation result; failures always identify filename.
6. QA project registry canonical CRLF/content-hash failure repaired ONLY in owned copies, validation not weakened. Native Studio adopts scenes into StoryFlow, so guarded test routing opens the level via native session.LoadScene and RequestLevelEditor. Requires exact disposable GUID plus THREE test flags. No owner startup bypass. Latest routed Studio selftest captured and exited0; trace has no shader failures.

## NEW metric cache — latest actual status
Corrected anisotropic scaling: old scalar field * min(widthScale,heightScale) widened horizontal density transitions incorrectly. Now cache stores field plus local gradient in RGBA16F and evaluates f/length(gradient/inverse shape scale). Four 128-cubed variants are 2x2 tiles in a 256x256x128 texture, ~73MiB with 7 mips, one lookup. Tile samples clamp at both trilinear mip texel centres. Zero surface unchanged; no noise-constant tuning.
Authored source: Tools/night_sky/renegadeCloudBodyTemplate.hlsli, renegadeCloudFormationVolumeCS.hlsl, renegadeCloudFormations.hlsli; cmake/RenegadeCloudFieldMetricOverlay.cmake included by BodyCacheOverlay. Never fix only generated Wicked source.
Initial combined offline compilation crashed in dxcompiler.dll 1.9.2602.24, 0xC0000005. Running HLSL6 and SPIRV separately succeeded without code change, then packaged Release RenegadeNightSkyTests rebuild passed. Intermittent crash NOT root-caused; do not claim fixed. BUILD/body-field-metric-{hlsl6,spirv}-compiler.log and body-field-metric-build-retry.log.
Latest 60 captures COMPLETE, process38188 exited0: cached single6 + analytic single6 + whole18 perAPI. BUILD/body-field-metric-{cached-single,analytic-single,whole}-{dx12,vulkan}. Includes RGB/opacityPNG/320x180PGM/CSV/shader trace. BUILD/body-field-metric-parity.json: all12 cache/oracle mean opacity deltas .028–.041/255, max7–16; occupied fraction differences <.00034. Only DEEP single-body layer tested so far; thin case pending.
Visual inspection: single cloud remains smooth and uniform. Whole image type1-c80-h9-p35-canonical.png shows obvious repetitive rows of similar broad bases, nearly ceiling coverage, weak lobes. DO NOT present this as improved owner candidate. The physical-distance correction is valid but does not solve formation realism.

## Most important next implementation decision
Stop spending whole turns re-proving already-green infrastructure. Main visible problem is the morphology/placement architecture:
- Template currently merges only FOUR large ellipsoidal thermals with width200 smoothing plus native detail alpha billow amplitude300. Fine erosion50m; edge transition100m. Template extent5000x4000x5000, support absXZ2300,Y0..3500. Those four lobes look like one smooth pillow.
- Sites are on 3000m grid with ONLY +/-600m jitter (.4*(random-.5)*3000), widths .8–1.2, heights .85–1.15, four near-identical variants. 4x4 balanced rank admission. Regular rows remain extremely visible. Investigate non-lattice placement/variable coherent thermal hierarchies with proven conservative neighbor bounds, rather than scalar noise tweaks. Do not merely increase jitter without expanding neighbor enumeration/bounds if needed.
- Whole-cloud coverage currently uses fitted deep/thin LUT; latest metric may change opacity coverage. Full coverage adds uniform low sheet (height min350,depth*.2), another flatness risk. Coverage must remain independent of morphology and permit full overcast without losing detail.
- Other four profiles need truly distinct 3D forms; cirrus must not be stretched fluffy Cumulus. Any incremental subset must be explicitly documented. Do not claim all five implemented.
- High-altitude horizon extension currently only Cirrus; Cumulus Base10k not validated.

## Tools/build and storage
Remote Desktop Commander device e97d1a13-b11e-4807-8d2f-895b78a873ca (DESIGN). Discover relevant tools through ALL_TOOLS; start_process shell powershell; timeout capped3s, continue read_process_output. Use ConvertTo-Json for process inventory (PowerShell mixed table output hides fields). Native computer APIs unavailable; do not manipulate owner windows.
Explicit UTF-8 all I/O. Windows Python write_text translates LF to CRLF; canonical registry JSON must write_bytes. PowerShell python -c with multiline quoted code loses quotes: WRITE A SCRIPT FILE, then execute it. Scripts can run via powershell -NoProfile -ExecutionPolicy Bypass -File.
Python C:\Python314\python.exe, PIL available, numpy absent. rg absent on remote.
CMake C:\Users\paulw\AppData\Local\Microsoft\WinGet\Packages\Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe\cmake-4.4.4-windows-x86_64\bin\cmake.exe
Build tree BUILD/gate1; CL=/MP4; cmake --build ... --config Release|Debug --target RenegadeStudio RenegadeRuntime RenegadeCloudFormationTests RenegadeNightSkyTests RenegadeMoonTests RenegadeShootingStarsTests --parallel 4 -- /p:LinkIncremental=false
Native v143 toolchain. Embedded HLSL6/SPIRV. Test cwd BUILD/gate1/Tests/Release.
C: last ~1.2GB free: inspect before linking. Clean only owned disposable artifacts with hash manifests. Never delete protected sources/projects. Prior .ilk caches already pruned; /INCREMENTAL:NO now used. Can prune redundant opacityPNG only where matching PGM exists after needed comparisons, or obsolete proof RGB after retaining contact sheet/selected examples+hash manifest.
No reliable production FPS claim: RTX4070Ti, prior default thin DX12~15–16ms, reference~24ms; live Studios can contend timings. 75FPS is not evidence.

## Exact proof commands / helpers
Outside-worktree scripts under C:\Users\paulw\source\repos:
clouds-gate1-field-metric-proof.ps1 — completed60captures, do not overwrite.
clouds-gate1-field-metric-compare.py — writes parityJSON, completed.
clouds-gate1-field-metric-preservation.py — latest45files/pin/native fallback PASS; historical noise/Legacy comparisons labelled historical, not fresh captures.
Example: RenegadeNightSkyTests.exe --formation-proof OUTPUT dx12 1920 single-cloud-body-study cloud-cached-body cloud-reference-quality cloud-stream-metrics cloud-shader-audit (omit cloud-cached-body for analytic).
whole stage macro-body-review; holdouts macro-holdout-review; raw admission macro-admission-study; long-wind-height-study for actual600sec wind regression. cloud-metrics-only emits tiny PGM without PNG.
Cached and analytic thin-layer single study is not yet configurable: current stage hardcodes1500/5000. Add a purposeful thin case if needed.
Runtime-compiled density audit must use NEW binary with NEW atlas source; old preserved baseline executable embeds old format, do not run its runtime density shader against current source.

## Preserved baselines / Studio selftest
Full256-case pre-metric Cumulus matrix at1080/1440 bothAPIs passed numerical checks: BUILD/body-cumulus-baseline-{api}-{1920,2560}, body-cumulus-baseline-matrix-results.json. Baseline executable RenegadeCloudCumulusBaselineTests.exe + binary hashJSON; embedded-only proofs safe. Source checkpoint BUILD/cumulus-live-replay-source-checkpoint/manifest.json (17files). Baseline still visually fails.
Actual latest pre-metric v2 scene/camera capture: BUILD/body-studio-replay-split, saved=1, Cumulus.8 Base1500Depth5000, pitch35/yaw0,1920x1009, animation0. Preserve/archive before reuse. Replay command --studio-cloud-replay OUTPUT API INPUT_FOLDER.
Disposable QA test project BUILD/cloud-body-studio-test/CloudBodyStudioTest.renegade GUID9dc8e035-52f9-4474-a319-7465698ae7cd. Successful routed alias RenegadeStudioCloudRoutedSelfTest.exe PID28812 EXITED0; parent17392 exited0. Process metadata body-routed-self-test-process.json, shader trace body-routed-self-test-shader-trace.log. Older audit3 PID6828 MAY STILL BE OPEN with confirmation/unsaved state: preserve. Do not rerun stale fixed-PID Win32 input scripts. No global mouse/keyboard input authorized for owner windows.

## Remaining validation before owner preview
Current Release NTests passes; current metric Studio/Runtime Debug+Release NOT BUILT. Prior height/wind four app builds passed; routing-only Studio Debug/Release also passed. Prior63 focused/source tests/config passed BEFORE metric and latest routing. Rerun appropriate checks after final source, accepted river/fog/ocean/night/Moon regressions and actual isolated Studio/Runtime bothAPIs. Runtime checker outside clouds-gate1-macro-runtime-check.ps1 must be copied to new output prefix, not overwrite old evidence. Startup-smoke alone does not exercise GPU.
When materially better, build separately named owner alias/project and provide Studio preview. Owner approval remains required. No later gate or integration work.
