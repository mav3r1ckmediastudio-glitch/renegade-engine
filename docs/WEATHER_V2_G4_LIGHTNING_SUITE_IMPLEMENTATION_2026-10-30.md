# Renegade Weather V2 G4 — Lightning & thunder integrated authoring
Date: 2026-10-30 | Branch: feature/weather-v2-all-gates
Status: Local implementation and targeted technical tests PASS. Live DX12 Inspector confirmed. Owner thunder listening, scene save/reopen, Windows packaged game and final visual acceptance remain OPEN. NO PUSH/MERGE.

## Preservation
The owner visually approved both fork and sheet cloud-lightning effects, and later visually approved snow and rain. Their shader, core sprite appearance, and existing default branching behaviour remain preserved. No new physical collisions, ignition, custom bolt artwork, or general Lua API expansion (explicitly deferred).

## Normal Environment Inspector
A new collapsible LIGHTNING & THUNDER group holds:
- Mode: OFF, IN-CLOUD (SHEET), CLOUD-TO-GROUND (FORK), BOTH; Off by default.
- Sheet flashes per minute; fork strikes per minute.
- Shared cloud illumination strength (approved default 0.14).
- Fork branching density (1 = existing randomized 3–5 branches; lower/higher authoring).
- Enable thunder, thunder volume, realistic speed-of-sound delay.
- Choose four defaults, all nine, or one of the nine specific thunder samples.
The inspector uses SetLightningCommand, scene Weather-entity metadata keys renegade.lightning.*, and Undo/Redo. No new scene formats or render data serialized.

## Runtime and Editor
- The built-in sheet and fork controllers read the authored lightning state each frame and reconfigure on selected values; legacy environment-variable preview modes remain available for test reproduction.
- Studio shares approved visuals for live authoring using the Runtime controllers, without new shader variants. Studio Save removes transient lightning mesh/flash fields before serializing any .wiscene.
- Runtime uses separate sheet/fork controllers and supports Both; fork-origin glow temporarily takes priority while a fork strike is active.
- Thunder is event-triggered on new flashes (not separate clock), uses 3D listener/emitter locations, distance/343m/s delay, per-instance volume, bounded concurrent voices, and cancels queued clips on scene change.

## SFX rights and packaging
Owner states all nine thunder WAVs are his creations and authorizes their distribution to Renegade users.
- Original stereo 48kHz WAV library: Studio/assets/weather/thunder, Runtime/Content/weather/thunder.
- Nine derived mono positional WAVs: Runtime/Content/weather/thunder/default3d.
- Manifest: Studio/assets/weather/thunder/manifest.json.
- CMake post-build installs all nine plus positional variants under Studio/Runtime Content/weather/thunder.
- Four default variation files: thunder_lightning_01/02, thunder_booming_01, thunder_distant_02.

## Verification
Build: initial BUILD/g4-lightning-fullsuite-build.log failed on a newly added Windows game thunder resource append; exact correction and serial rebuild PASS: BUILD/g4-lightning-studio-serial-build.log. Native Runtime Release PASS in first build.
Tests: BUILD/g4-lightning-tests-results.log 5/5 PASS, including authored metadata/Undo/Redo/bounds and approved sheet/fork/weather regressions. Additional exact `.wiscene` archive save/reopen test added to RenegadePrecipitationTests; rebuild and test PASS (BUILD/g4-lightning-persistence-build.log and -test.log).
Live Studio DX12: BUILD/g4-lightning-editor-panel2.png shows normal Environment Lightning & Thunder section; BUILD/g4-lightning-mode-popup.png confirms BOTH selected with Undo count incrementing, sampled 75 FPS; temporary unsaved demo, original saved G3 scene untouched.
Remaining acceptance: real audio output/volume/delay perception with owner; persist/reopen temporary scene; Test Game and fully packaged Windows game audio assets/real-time playback. These are not yet proven.
No push/merge or Gate 4 global signoff until these are verified. Keep isolated weather worktree.

## Additional integration verification
- Incremental Release build PASS: `BUILD/g4-lightning-fullsuite-rebuild.log` (exit 0).
- Seven focused CTest suites PASS: `BUILD/g4-lightning-fullsuite-tests.log` (7/7 including precipitation, sheet/fork lightning, weather effects transitions, moon and cloud formation).
- DX12 normal Studio GUI confirmed on DESIGN: `BUILD/g4-lightning-studio-ui-10.png` shows the new inspector and mode choices; `BUILD/g4-lightning-studio-ui-13.png` shows BOTH with individual sheet/fork frequency controls.
- Asset installation verified: 9 original stereo WAVs in the Studio source library, and 18 WAVs each (9 originals + 9 mono spatial) in the built Studio and Runtime content trees. `git diff --check` clean.
- The live editor holds unsaved scene changes; do not save or discard them without coordinating with the owner. Do not claim that an on-device listening test or packaged-game smoke test has passed.
- Additional on-disk scene persistence regression PASS: `BUILD/g4-lightning-persistence-build.log` and `BUILD/g4-lightning-persistence-test.log` (PrecipitationTests writes a real temporary `.wiscene`, reopens it, verifies Lightning metadata, and cleans up). This verifies format persistence headlessly, but not a human Studio save/reopen or a packaged-game smoke test.
