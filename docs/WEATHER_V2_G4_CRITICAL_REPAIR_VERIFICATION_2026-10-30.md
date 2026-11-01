# Weather V2 G4 critical repair candidate - 2026-10-30

Status: local Release candidate available for owner visual/audio testing. NOT owner accepted; performance defect D remains open in the dense-cloud editor view. No commit, push, merge, source reset, or original scene save.

## Identity and preservation

Root HEAD: f073233a61e8d8814cdc29a610e242689832c37f. Wicked HEAD: 7e2326c0338e625f4405d8fe5150fde9547eaded. Both working trees contained owner-approved, uncommitted G4 work before this repair; preserve it. Exact repair-source and executable hashes: BUILD/g4-critical-source-manifest.json. Source digest: c1761c40f425398e63a988db5dfb92f83eae03e167c18ab9fba4bea413843721.
Baseline dirty diffs, original files, original scene, and evidence have an external dated audit backup. Original unsaved Studio PID 39748 remains open/minimized. Its executable was renamed to allow linking; the process and unsaved scene were not closed or discarded. A normal Test Level snapshot captured the unsaved scene into BUILD/g4-owner-unsaved-snapshot; work in that isolated copy does not overwrite the original. Final hash verification: saved original Main.wiscene unchanged, all nine owner-authored thunder WAVs match their manifest and Runtime copies, including the four defaults.

## Repairs and evidence

A/B: removed the flash-only structural reconstruction bypass. The ordinary four-phase, motion-aware cloud-history path now runs through onset, strokes, gaps and recovery. Cloud density, opacity and base illumination retain normal temporal reconstruction. Rapid lightning illumination is integrated along the same ray samples into a separate current-frame RGB buffer, reconstructed with phase-aware spatial positions and depth rejection, then composited in the existing bilateral upsample. It never enters structural history. The base cloud path no longer switches algorithms at a flash. No cloud time stop was proven; do not describe the reported interruption as a confirmed simulation pause. Sampled actual DX12 captures no longer show the original grid; continuous visible motion and exact approved appearance still require owner's frame-by-frame review. No approved fork geometry, sheet envelope, rain/snow authoring, or scene state was intentionally changed.

C has TWO independently confirmed defects:
- A fresh XAudio2 voice reports callback ended before asynchronous OnBufferStart. RuntimeThunderAudio erased it in the same update. Keep a voice until native BuffersQueued reaches zero (with safety timeout), prewarm all nine sounds at scene revision, and log native sample progress. Metadata sample count alone is not a valid completion test: hardware stopped with final padding differences.
- StartupMediaFoundationPlayer used MR_POLICY_VOLUME_SERVICE / IMFSimpleAudioVolume. Its startup fade set the ENTIRE Windows application audio session to zero, including Wicked/XAudio2, and left it there. Actual Studio session gain was measured as 0 although source samples advanced and native master/submix gains were 1. Restoring only the review session to 1 immediately produced output. The code now uses MR_STREAM_VOLUME_SERVICE / IMFAudioStreamVolume and per-channel stream gain, preserving startup visual/audio fading without muting engine audio. Production code respects Windows application volume.
Camera::At is a LookTo direction, not a world target. Normalize it directly and derive orthogonal listener Up. This corrects the initial investigation report's tentative camera interpretation.

Hardware proof: BUILD/g4-critical-audio-all-nine.log validates immediate nonspatial WAV playback, 100 startup-lifetime iterations, production 3D, all nine variations, samples and voice completion, with WASAPI loopback output. Fresh rebuilt Studio session remained gain=1/muted=0 after startup. Actual 30-second editor capture BUILD/g4-final-studio-loopback.log: peak 0.0981892, RMS 0.00849932. Actual Test Game child of the rebuilt Studio, correct default endpoint/session gain=1: BUILD/g4-final-testgame-loopback.log peak 0.11459, RMS 0.00906254. These are output measurements, not merely playback-call success. Loopback measures the endpoint mix; correlate with source events, backend proof and owner's listening. Foreground sampling was imperfect (editor 239/299, game 23/300); do not claim a fully isolated unattended audiovisual capture.

## Performance: measured, not fully resolved

Native DX12 GPU timestamp ranges and raw CPU timings, warmed comparable Runtime fixture, Storm with rain 0.95 plus both lightning modes at 20/min each; uncapped, 1080-class output. Raw exports buffer in RAM and flush at shutdown so instrumentation does not add periodic disk stalls. Earlier synchronous CSV instrumentation caused 80-96 ms spikes and was corrected before final metrics.

| Final Storm metric | Median ms | P95 ms |
|---|---:|---:|
| CPU Frame | 4.36645 | 10.71 |
| GPU Frame | 12.25216 | 14.36739 |
| Cloud capture | 4.81997 | 5.53472 |
| Cloud raymarch | 4.49229 | 6.69184 |
| Cloud reconstruction | 0.018432 | 0.024576 |
| Cloud upsample | 0.043008 | 0.04608 |
| Cloud shadows | 0.879616 | 1.17043 |
| Rain simulation | 0.088064 | 0.095232 |
| Rain render | 0.079872 | 0.17408 |

Host 1080-frame measured window after 120 warmup frames: average 12.837 ms (77.901 FPS), median 12.406, P95 14.529, P99 14.947, maximum 16.419; zero frames over 33 ms. GPU frame max 15.9539 ms. Fork mesh create CPU median 0.2807, max 0.7322 ms. GPU usage about 2013 MiB against 11226 MiB budget. Evidence: BUILD/g4-critical-final-metrics.log and external audit evidence/final CSVs and matrix-summary.json. Earlier six-condition isolation matrix includes clouds/rain/sheet/fork/both/storm. No PIX capture was made; native GPU timings are the evidence, not a claim of PIX validation.

The owner's approximately 56 FPS report is NOT universally resolved: actual dense-cloud editor views still displayed roughly 50-59 FPS. Runtime horizon and editor near/upward cloud views are not comparable; neither proves the original editor performance regression repaired. The minimized original Studio's sampled 3D GPU engine utilization was zero, so do not blame it without sustained evidence. Remaining work requires exact same camera, viewport, weather, warmup and presentation settings, GPU/CPU timing of the editor and accepted reference build, with flash/dark-frame grouping. Cloud capture/raymarch dominate the controlled Runtime, not reconstruction or measured rain passes. Do not attribute the whole regression to lightning. A four-slice cloud-cache experiment had no useful A/B improvement and was reverted. Retained equivalent zero-density early-outs do not justify a performance-fix claim.

## Build and regression evidence

Commands use configured BUILD/g3, Windows x64 Release:
- cmake --build BUILD/g3 --config Release --target RenegadeStudio RenegadeRuntime --parallel 4 : passed, BUILD/g4-critical-final-verified-build.log.
- Focused exact-source regression target rebuild and ctest: BUILD/g4-critical-exact-regression-build.log and BUILD/g4-critical-exact-regression-results.log, 7/7 passed: precipitation, sheet lightning, fork lightning, weather effects transitions, weather transitions, moon, cloud formations.
- After the startup stream-volume correction: Studio rebuild passed, BUILD/g4-startup-stream-audio-build.log.
- Final Studio rebuild after preserving the original search glyph: passed, BUILD/g4-final-ui-preservation-build.log. One existing nodiscard warning remains; no errors.
- git diff --check in root and Wicked: passed.
The test layout assertions were corrected to the existing G4 ShaderWeather layout size 1104 and explicit new lightning offsets; existing Moon offsets remain checked.

## Changed files, rollback and acceptance

Repair-owned files: Runtime/src/RuntimeThunderAudio.h (new), RuntimeWeatherDiagnostic.h (new), RuntimeApplication.cpp, RuntimeForkLightning.h; Studio/src/StartupMediaFoundationPlayer.h/.cpp and opt-in isolated-test launch additions in StudioApplication.cpp; Tests/ThunderAudioTests.cpp (new), CMakeLists.txt, MoonStateTests.cpp, CloudFormationTests.cpp. Wicked: wiAudio.cpp native queued-state query; wiProfiler.cpp opt-in raw timestamps; wiRenderer.h/.cpp resources and bindings; volumetricCloud_renderCS.hlsl, volumetricCloud_reprojectCS.hlsl, volumetricCloud_upsamplePS.hlsl, volumetricCloudsHF.hlsli. The root and Wicked also contain pre-existing owner edits outside this list. Never git reset/checkout the whole files to roll back; use baseline backups and saved dirty diffs to reverse only this repair.

Independent review gates:
1. Cloud structural/transient separation: before/onset/strokes/gaps/recovery frame sequence, fixed and moving camera; no grid, ghost, pop or movement interruption, no regression to signed-off formations/moonlighting/sheet/fork. Check depth edges and above-cloud view.
2. Voice lifetime/listener: immediate nonspatial output, all nine voices complete; then 3D directional test, distance falloff and physical delay in Studio and Test Game. Reopen app after startup and prove session stays audible.
3. Performance: same editor camera/viewport/preset/reference build; CPU/GPU median/P95, per-pass and flash-only peaks, budget/memory. Target the owner's 75 Hz preview with acceptable P95; current dense-editor limitation remains open.
4. Editor preservation: no original scene change; normal save/reload of a disposable copy, rain/snow and transitions still behave; owner listens and inspects actual applications. No G4 signoff until independent human/AI checks exact source identity and binary.

Review launcher: BUILD/Run-Weather-G4-Critical-Repair-Owner-Test.cmd opens the isolated unsaved-scene snapshot. Current review Studio PID 45096 includes the actual audio/cloud repair, with only the original search-glyph restoration differing from the final on-disk executable. Its current edits were deliberately left intact; do not close it for a cosmetic rebuild. Original unsaved Studio PID 39748 is the old executable and cannot test these fixes. The normal final executable is BUILD/g3/Studio/Release/RenegadeStudio.exe; Runtime is BUILD/g3/Runtime/Release/RenegadeRuntime.exe. Nothing was committed, pushed, or merged.
