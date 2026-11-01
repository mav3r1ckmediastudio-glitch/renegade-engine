# Weather V2 G4 — lightning horizon blur and silent thunder
Date: 2026-10-30. Branch: feature/weather-v2-all-gates. Owner observations: every strike temporarily blurs/shakes distant horizon with no observed FPS drop; no audible thunder in Studio or Test Game; bolt visuals visually impressive in Test Game.

## Confirmed trace evidence
An isolated DX12 fork preview was launched from the built Runtime with RENEGADE_G4_THUNDER_TRACE and RENEGADE_G4_FORK_LIGHTNING_TRACE enabled.
- All 9 mono samples report LOAD_OK.
- Several FORK_FLASH events and thunder QUEUE events were observed, with distances around 3.3–4.6 km and realistic sound-delays of approximately 9.7–13.4 seconds.
- PLAY events with nonzero logical gains were observed after those delays. This proves event/asset/play-call wiring but not audible output; owner reports silence.
- Evidence: BUILD/g4-horizon-audio-thunder-trace.log, BUILD/g4-horizon-audio-fork-trace.log.

## Remediation
- RuntimeThunderAudio: preserve real strike distance for delay and gain, but use a near-field 3D virtual source along true bearing, avoiding attenuation to zero in X3DAudio's metre-scale falloff curve. Normalize listener front vector. Add gain matching for low-level supplied clips, with original WAVs unchanged.
- Resolve thunder files first relative to the actual executable location, with relative fallback and bounded retries if initial audio-device loading is unsuccessful.
- Add opt-in, noninvasive trace for missing asset loads, queue, delay and Play/failed voices.
- Wicked volumetricCloud_reprojectCS.hlsl: lightning flash uses local cloud-region reconstruction rather than swapping the entire sky/horizon into a coarser flash-time reconstruction path. Approved geometry and sheet-lightning illumination shaders untouched.

## Acceptance status
Build, DX12 live visuals and audio listening remain to be independently verified after changes. Do not claim full resolution based only on compile. No push or merge without owner approval. Preserve owner video/screenshots (video not attached in this conversation yet). Do not overwrite saved G3 scene.

## Evidence update
- In the isolated fork preview, 9/9 mono thunder WAVs reported LOAD_OK, several QUEUE entries reported 9.7–13.4 second delay from 3.3–4.6 km strikes, and multiple PLAY entries confirmed playback calls with nonzero engine gains. This establishes the wiring; it does not establish audible playback.
- Owner reported stable FPS during the horizon blur; supports investigating temporal reconstruction rather than performance stalls.
- Seven targeted native weather, sheet/fork, precipitation, moon and cloud tests PASS: BUILD/g4-horizon-audio-regression-tests.log.
- The current changes have not been committed or pushed. Owner's live Studio process remains open; an isolated Runtime diagnostic process was closed after evidence was captured.

## Final local build and visual checkpoint
- Final Release Studio + Runtime build exit code 0, evidence `BUILD/g4-horizon-thunder-fix-final-build.log`.
- Embedded shader compilation succeeded; lightning-reprojection branch now only applies near illuminated cloud cells, not the full horizon.
- Post-fix isolated DX12 Runtime produced `BUILD/g4-fix-before-flash.png`, `BUILD/g4-fix-on-flash.png`, `BUILD/g4-fix-after-flash.png`. All sampled at displayed 75 FPS; static horizon did not visibly shift in these samples. Continuous-motion owner visual approval remains necessary.
- Final playback trace `BUILD/g4-audio-fix-final-thunder.log`: 9/9 WAV assets loaded via executable-relative paths, QUEUE and PLAY events confirmed with enhanced sample-level gains; software logging cannot guarantee sound is heard on the physical output device. Actual owner listening is an open gate.
- Seven targeted test suites PASS: `BUILD/g4-horizon-audio-regression-tests.log`.
- Existing running Studio session is the original pre-fix process, preserved with its unsaved changes. Old executable retained as `BUILD/g3/Studio/Release/RenegadeStudio.before-G4-audio-horizon-fix.exe`; newly built `RenegadeStudio.exe` is staged alongside it. Exit/relaunch Studio to run corrected build; do not discard unsaved changes without owner decision.
- No commit, push, PR or merge performed. Follow-up acceptance: listen in newly launched Studio and Test Game and check horizon during full-motion bolts, not merely still images.
