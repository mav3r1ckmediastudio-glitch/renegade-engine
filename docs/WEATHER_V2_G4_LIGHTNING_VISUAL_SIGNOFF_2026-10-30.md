# Weather V2 G4 - Lightning visual owner signoff
Date: 2026-10-30
Branch: feature/weather-v2-all-gates
Status: VISUAL SIGNOFF ONLY. G4 and the five-gate weather delivery remain OPEN.

## Owner decision
The owner explicitly said: "I think.we can visually sign off on the lightning and move onto the other sections now wind rain and snow".

This approves the current visual appearance of the sheet lightning and the forked lightning, including the organic bolt, trunk/branch brightness hierarchy, embedded cloud illumination and synchronization of cloud glow with visible return strokes. Do not rework the approved lightning look unless requested.

## Evidence
- Fork cloud glow reuses the approved sheet-lightning scattering. The independently timed post-fork extra cloud pulse was removed.
- Release build PASS: BUILD/g4-fork-synchronized-light-build.log.
- Focused CTest 5/5 PASS: BUILD/g4-fork-synchronized-light-tests.log, including the no-post-fork-flash regression.
- Captures: BUILD/g4-fork-flash-1.png, -2.png, -3.png; DX12 at 1920x1009; 75 FPS observed in sampled captures on RTX 4070 Ti.
- Prior G4 sheet owner approval recorded in WEATHER_V2_G4_SHEET_LIGHTNING_OWNER_SIGNOFF_2026-10-30.md.

## Still in current weather iteration
- Wind, rain and snow review/development (owner's latest requested sequence).
- Basic Lightning Inspector authoring controls, frequency, branching and thunder, where required by existing G4/G5 scope. Visual acceptance is NOT editor, packaging or overall Gate 4 approval.
- Existing native snow was previously retained while Snow V2 was deferred; owner has now explicitly requested snow next after wind/rain. Assess bounded snow quality and workflow without silently adding unrelated gameplay systems.

## Explicitly deferred to future iterations
- Custom lightning artwork, custom VFX/material override.
- Physical lightning impacts, scripted tree ignition and gameplay damage.
- Full Lua/C++ weather API parity and comprehensive weather event scripting.
These future requirements are documented, but NOT blockers for current weather delivery.

Development remains on the existing local branch. Do not push, open PR or merge without owner authorization.
