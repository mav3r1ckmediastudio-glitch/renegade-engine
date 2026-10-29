# Weather V2 Gate 1 — Cloud Movement Speed control (29 October 2026)

Status: Current isolated Studio build exists for owner visual evaluation. **Gate 1 still OPEN; no visual acceptance, push, PR or merge.**

## What changed
User observed no perceptible cloud movement and requested an in-game speed slider. Native Wicked volumetric clouds use `animationMultiplier * GetFrame().time` for layer wind and coverage animation. Before this session, the earlier isolated preview was frozen with animationMultiplier=0, and the subsequently cloned scene has animation=2 but motion can be subtle due km-scale cloud bodies.
The original bridge weather model did not expose animationMultiplier through Studio UI.

In current isolated source:
- `EngineBridge/include/renegade/bridge/CommandService.h`: WeatherState cloudMovementSpeed (default 2).
- `EngineBridge/src/CommandService.cpp`: capture/application of animationMultiplier, bounded 0..12, command changed-state detection, updates runtime scene.weather.
- `Studio/src/StudioApplication.h`, `Studio/src/StudioApplication.cpp`, `Studio/src/NightSkyStudioIntegration.cpp`: new MOVEMENT SPEED slider (0–12, 2 native) under Environment > Clouds, preview/commit Undo/Redo, refresh and enabled/visible handling.
- `Tests/BridgeCommandTests.cpp`: direct weather command 7.0 speed, Undo/Redo restoration, active runtime weather synchronization, and scene save/reopen 6.5 speed.
- `Tests/CloudFormationGraphicsProof.h`: explicit GPU live-wind-visible-proof with +60s elapsed time for types 1 and 2, separate from frozen morphological test modes.

## Evidence
- Release RenegadeStudio + RenegadeNightSkyTests build exited 0: `BUILD/live-cloud-movement-control-release-build.log`.
- Target RenegadeBridgeTests compiled and RUN successfully, exit 0: `BUILD/cloud-movement-bridge-build.log`, `BUILD/cloud-movement-bridge-run.log`.
- Focused Release CTest 7/7 passed: `BUILD/cloud-movement-focused-ctest.log`.
- Explicit DX12 GPU before/after 60 simulated seconds, same 80% 2280/3190 weather and 1920x1080: `BUILD/cloud-live-wind-measured-dx12`. Opacity changed >5 byte in 51.4% for Cumulus and 56.7% for Towering Cumulus. This proves shader motion when frame time advances; it does NOT establish a perceptible motion rate from every live Studio vantage.
- New Studio executable `BUILD/gate1/Studio/Release/RenegadeStudioCloudSpeedG1Oct29.exe` created separately, leaving old running preview binaries intact. New review PID on launch 8632. It loaded isolated Cloud Body Motion Preview (same editor project), and actual screenshot `BUILD/cloud-speed-review-inspector.png` confirms slider visible directly below Coverage, Cumulus 80%, cloud base 2280, depth 3190. Slider reads 6.1 in observed session and scene marked dirty; do not overwrite owner-edited state.
- `BUILD/Run-Gate1-Cloud-Speed.cmd` relaunches uniquely named review safely. Use Cloud Body Motion Preview project in Project Hub and its Main Level if not auto-open.
- 45 original authored files preserved, `git diff --check` 0 and accepted river fork gitlink/checkout `ceb816b844b4ab2fa56a159ac60a4f65c931bdfc` intact. No commit, push, PR, merge, Weather Gates 2–5 or Snow V2.

## Visual motion caveat
Two 15-second apart desktop screenshot captures while working through remote terminal were byte-identical across entire screenshot, including static inspector and stars. Later Win32 foreground check identified Edge as foreground, not this Studio. Therefore desktop captures are inconclusive as proof of Studio being frozen: inactive windows/remote capture may not advance or return newly rendered pixels. Do not claim a confirmed Studio-time bug based on those captures. StudioRenderPath::Update(dt) calls RenderPath3D::Update(dt); sceneUpdateEnabled defaults true; RenderPath3D::PreRender calls UpdatePerFrameData with scene.dt; renderer advances frameCB.time by delta time * GameSpeed, default GameSpeed=1. If user still sees no drift while this newly built Studio has focus and Movement Speed is high, instrument frame-time/scene animation values, measure its real cloud render output, then fix root cause.
The previous project's **animationMultiplier=0** was a real fixture bug, corrected only in the cloned preview. The *cloud speed slider* is functional, saveable and propagated to Test Game; user visual confirmation still outstanding.

**Next**: user inspect new build with Time of Day fixed and Cumulus/ Towering Cumulus at Movement Speed about 8–10, watching a distinct cloud edge against stars for 20–30 seconds. Compare speed 0; then return to a natural speed (around 2) and save. The original Gate1 cloud morphology defects are still not owner-accepted.
