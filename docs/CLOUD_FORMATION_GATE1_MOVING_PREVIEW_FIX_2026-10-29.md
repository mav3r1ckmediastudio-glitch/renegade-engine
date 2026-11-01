# Gate 1 moving cloud preview — corrected review — 29 October 2026

Status: **MOVING-CLOUD REVIEW NOW AVAILABLE on DESIGN**, Gate 1 still open and not accepted or merged.

## Root cause confirmed
The earlier Cloud Body Studio Preview scene serialized `weathers[0].volumetricCloudParameters.animationMultiplier = 0`. This is not the normal cloud behaviour: Wicked defaults this field to 2.0, and volumetricCloudsHF.hlsli computes animation = animationMultiplier * GetFrame().time. The previous QA capture routine under Studio/src/StudioApplication.cpp::CaptureCloudBodyReplay intentionally writes animationMultiplier=0 when `--cloud-studio-replay-audit` is used. A frozen scene was reused for the initial interactive preview. The same frozen weather is supplied to Test Game, so both appeared static. No renderer code change was required for wind motion.

## Corrected saved scene (DO NOT touch earlier preview)
Clone: BUILD/cloud-body-motion-preview/CloudBodyMotionPreview.renegade
Scene: BUILD/cloud-body-motion-preview/Content/Scenes/Main.wiscene
Project ID: e2d3e3f4-73a2-452a-b6c3-1d894f281302 (matches cloned AssetRegistry.renegade-assets)
The separate RenegadeNightSkyTests --restore-preview-cloud-motion command has saved and reopened this scene using Wicked native archive. Verified animationMultiplier=2, layerFirst.windSpeed=15, coverageWindSpeed=30. The initial BUILD/cloud-body-studio-preview/Content/Scenes/Main.wiscene hash was verified unchanged. No changes to owner's other projects or windows.
A cloned project initially failed validation because PowerShell wrote a UTF-8 BOM and the registry still referenced the old project ID; both were corrected. A bad intermediate registry write also left a literal trailing backslash-n; fixed and Python JSON validation passed. The current corrected project passed startup; the 29 October Studio log shows scene deserialization after relaunch and no further project validation failure.

## Running review
- Binary: BUILD/gate1/Studio/Release/RenegadeStudioMovingCloudsG1Oct29.exe
- Cmdline: --cloud-motion-preview --dx12
- Running process PID on launch: 22248 (may change after restart).
- Window caption set to **Renegade Studio - Gate 1 MOVING CLOUDS [DX12]** for ease of finding via Alt+Tab.
- Relaunch shortcut: BUILD/Run-Gate1-Moving-Clouds.cmd
- The original frozen preview was deliberately left alive (at launch PID 15116), separate executable RenegadeStudioCloudReviewG1Oct29.exe. It has NOT been overwritten.
- This moving preview is an isolated real Studio scene, not yet owner-visually approved. Cloud displacement is expected to be gradual because wind speed is 15 metres/sec across kilometre-scale bodies: watch a distinctive cloud edge against the sky for 20–60 sec and check Test Game. We validated the serialized animation and shader path, not a frame-by-frame optical flow analysis of the foreground desktop.
- Two review Studio windows exist; the user should inspect the **MOVING CLOUDS** title, not the old frozen Cloud Body preview.

## Source / validation
- QA-only source edits: Studio/src/StudioApplication.cpp recognizes --cloud-motion-preview, chooses clone and sets Environment tab; Tests/NightSkyStateTests.cpp adds isolated clone scene repair / disk roundtrip mode.
- Tests RenegadeNightSkyTests and RenegadeStudio Release linked successfully; BUILD/cloud-motion-tool-build-v2.log and BUILD/cloud-motion-studio-release-build.log.
- BUILD/cloud-motion-focused-tests.log: 7/7 green focused CTest.
- BUILD/cloud-motion-preservation.log: 45 original authored protected files untouched; git diff --check exit0; accepted Wicked river fork ceb816b844b4ab2fa56a159ac60a4f65c931bdfc unchanged. No push, PR, merge or change to Weather Gates 2–5 / Snow V2.
- Do not run the QA freeze `--cloud-studio-replay-audit` in ordinary interactive previews. For future preview fixtures, ensure animationMultiplier>0 before opening and verify motion in real Studio and Runtime. The editor currently has no exposed cloud wind control or VSync OFF toggle; those are separate usability gaps.

Next: owner visual check in MOVING CLOUDS Studio; assess natural movement, day/night lighting, shape and performance. Gate1 remains OPEN.
