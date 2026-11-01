# Weather V2 G4: grass wind validation and integration findings

2026-10-30. Engineering validation of the grass slice; G4 overall remains OPEN.

## Owner scope

Renegade currently has grass, not trees. The owner plans a future tree authoring system. Trees are not a weather gate dependency. This clarification supersedes earlier grass-and-trees fixture requirements. Preserve the owner-approved cloud, lightning, precipitation, audio and performance candidate.

## Exact candidate and preservation

Root HEAD f073233a61e8d8814cdc29a610e242689832c37f, Wicked HEAD 7e2326c0338e625f4405d8fe5150fde9547eaded, with existing dirty work retained. Approved manifest source digest: 6a632022e6ec0f28553a7f1fe3f9ae02e6e06b56596d7b51afd774d9e34fb7d1.

No production source edits, builds, commits, pushes or merges in this continuation. BUILD/g4-grass-closure-audit.json confirms all recorded approved source and binary hashes match, original saved Main is unchanged, and all nine original thunder WAVs match both asset copies. The performance review project's TestLevel.wiscene also matches the preserved owner-unsaved snapshot byte-for-byte. Only a separate GrassWindValidation scene and an isolated derived Runtime fixture were authored.

## Direct Studio validation

Used the existing Renegade terrain grass brush and native Wicked HairParticleSystem. Terrain initialization initially reported CREATE TERRAIN FIRST, then became READY; this was not a confirmed missing-terrain defect. Painted a local patch, moved the camera back to see the strands, and selected Environment to disarm the brush. The saved scene is GrassWindValidation.wiscene in the isolated performance review project's Content/Scenes folder.

- Precipitation OFF: Environment Wind controls remain enabled.
- Calm: wind direction 0, speed 0, turbulence 0. Captured 24 frames.
- Moving wind: direction 0, speed 3, turbulence 5. Captured 24 frames showing strand displacement compared with the calm sequence.
- Direction reversal: entered 180 degrees, displayed as equivalent -180 after native capture; speed 3 and turbulence 5 preserved. Captured another 24 frames.
- Ctrl+Z restores direction 0 and changes Undo/Redo counts from 10/0 to 9/1. Ctrl+Y restores -180 and counts 10/0.
- File Save followed by Reopen preserves the painted patch, precipitation OFF, direction -180, speed 3 and turbulence 5. Reopen clears the command history and unsaved marker.

Evidence: BUILD/g4-grass-{calm,wind3,reverse180,undo,redo}.{png,gif}, g4-grass-motion-contact.png, g4-grass-undo-redo-controls.png and g4-grass-reopened.png. Images were reviewed as frame sequences/contact sheets, not inferred from command success. This is an engineering smoke check; owner approval of grass motion is still required.

## Native wind semantics

PrecipitationService writes a unit horizontal windDirection, native windSpeed and windRandomness. windCS.hlsl computes sin(time * wind.speed + spatial noise * wind.randomness); globals.hlsli multiplies the sampled field by wind.direction and strand weight; hairparticle_simulateCS.hlsl integrates that force. Therefore the existing Wind Speed is a sway-frequency parameter, not physical metres per second or an independently adjustable force magnitude. Speed zero alone does not guarantee stillness if turbulence remains nonzero. Calm was tested with both at zero. No shader or legacy wind semantics were changed. Cloud animation winds remain separate.

## Runtime and actual Test Game distinction

The actual Studio Play action on the new Save As scene FAILED before Runtime launch. The exact log reports:

    Could not snapshot creator scripting state: Could not bind live creator scripting for Test Level: Could not read the Level identity for scripting: Could not read document envelope: .../GrassWindValidation.wiscene.rmeta

The new Save As scene has no identity sidecar. This is an editor/level-registration integration defect, not evidence of failed grass simulation or thunder. Do not mark Test Game on this saved derivative PASS. Source log preserved as studio-saveas-snapshot-failure.log in the external grass evidence folder. The Studio log also reports unresolved scene-relative default_grass texture names and grassparticle.dds after Save As; dependency/path preservation needs auditing alongside level identity.

For a separate standalone Runtime smoke check, created an isolated project copy and replaced only the contents of its already registered TestLevel.wiscene with the grass validation scene. Kept that copy's existing level identity and scripting bindings. This fixture does not repair or excuse Save As. Launched the existing Release RenegadeRuntime.exe with dx12 --project <isolated descriptor>. Grass loads and moves in the native player; captured 24 frames. Maximized primary-display capture reports 1920x1009 and 75 FPS (BUILD/g4-grass-runtime.png/.gif). This is one sampled dry-grass view, not combined-Storm timing or packaged-game approval. Studio is minimized; the grass Runtime review window remains open.

## Focused regression checks

Ran existing Release tests, without rebuilding:

    ctest --test-dir BUILD/g3 -C Release -R "Renegade(Precipitation|WeatherEffectsTransition|WD01Vegetation)" --output-on-failure

3/3 PASS, BUILD/g4-grass-closure-tests.log: precipitation/dry wind and mode preservation, effects transition controller, vegetation source contract. The vegetation source contract is not a visual test. Prior 11/11 exact-candidate results remain in the optimization review.

## Remaining closure work

1. Owner reviews grass motion; no tree requirement.
2. Repair/audit Save As identity registration and scene-relative dependency handling, then repeat real Studio Test Game using the saved derivative. Keep level identity unique; do not paper over the defect by copying sidecars into production scenes.
3. Integrate the already tested WeatherEffectsTransitionController into live authoring/gameplay with an explicit opt-in to affect authored rain/wind and one coherent Undo transaction. Current G3 presets affect sky/fog/cloud state and intentionally do not automatically start rain; a passing pure-controller test does not close this live integration gap.
4. Demonstrate combined effects transitions, full packaged Windows Runtime and the required configuration matrix. G1-G3 and accepted G4 appearance/performance retain their existing scoped owner signoffs. G4/G5 overall are not declared complete.

No new performance optimization or visual modification was made. Runtime fixture and capture scripts are diagnostic-only BUILD artifacts. Grass evidence is also preserved with the existing external critical-repair audit evidence.


## Owner decisions and repair — 2026-10-31

Owner explicitly approves grass motion and confirms personally tested preset transitions work as intended. Those are accepted, not outstanding work. Earlier suggestions to add automatic rain/wind preset coupling are superseded; preserve the current intended behaviour. The Save As identity blocker is fixed and the actual Studio Save As -> Reopen -> Test Game -> return workflow passes; see WEATHER_V2_G4_SAVE_AS_FIX_2026-10-31.md for exact candidate,13/13 tests and live evidence. Final independent review of that repair remains before formal G4 closure. Trees remain future scope.
