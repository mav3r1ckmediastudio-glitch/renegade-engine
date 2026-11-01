# Weather V2 G4 - Wind is independent of precipitation

2026-10-30 | feature/weather-v2-all-gates | OWNER VISUAL REVIEW NOT YET REQUESTED

## Actual bug found
Studio's Environment Inspector disabled Wind Direction, Wind Speed and Turbulence whenever Precipitation Mode = OFF, even though these fields write Wicked's global WeatherComponent.windDirection, windSpeed and windRandomness, used by GPU wind, responsive vegetation and the rain emitter. A developer could not author a dry windy scene. Labeling wind sliders as "Precipitation Wind ..." reinforced this confusion.

The native Rain/Snow profile helper also overwrote GLOBAL windRandomness with fixed 3/8 values whenever switching precipitation type, potentially changing foliage motion merely because a user enabled snow.

## Scoped solution
- Studio/src/StudioApplication.cpp: rename controls as Environment Wind Direction, Environment Wind Speed and Environment Wind Turbulence with explicit tooltips. Keep all three enabled even when Precipitation Mode is OFF. The intensity, fall speed and particle scale controls remain dependent on precipitation mode.
- EngineBridge/src/PrecipitationService.cpp: MakePrecipitationProfile changes particle properties but preserves inherited windDirection, windSpeed and windRandomness when toggling Rain/Snow. Global wind may still be intentionally set by the user.
- Tests/PrecipitationTests.cpp: new dry wind regression verifies None with wind azimuth -45, speed 11 and turbulence 4, native Scene update, Undo/Redo, and preservation of all wind values across Rain/Snow mode switches.
- No approved cloud or lightning source changed. G1-G3 signed-off visual checkpoints are retained.

## Important verified limitation
Wicked's shaders use sample_wind(...) only when a mesh material is configured to use wind. Mesh vertex weights also influence motion; arbitrary imported trees are not automatically promised to sway. Cloud movement currently has separate animation/coverage winds, which should not silently be overwritten.

## Acceptance outstanding
1. Release Studio + unit tests pass for exact changes (see BUILD/g4-wind-dry-presets-final-build.log and -tests.log after verified).
2. Studio visual interaction with precipitation OFF: all wind controls enabled; speed/direction/turbulence update native WeatherComponent; Undo/Redo and save/reopen.
3. Separate visual scene with authored wind-responsive foliage, precipitation and cloud movement; compare calm/breezy/storm and confirm performance.
4. Do not close Wind visual milestone or G4 without owner review. No push, merge or PR.

## Verified DX12 Studio interaction — PASS, 2026-10-30
- Built `RenegadeStudio.exe` Release with the wind Inspector fix; exact log `BUILD/g4-wind-dry-presets-final-build.log` (PASS).
- CTest targeted 5/5 PASS in `BUILD/g4-wind-dry-presets-final-tests.log`, including new dry-wind mode and precipitation mode-switch tests; sheet and fork lightning regressions protected.
- Opened the real Studio on DESIGN (DX12) with `--g3-weather-preview`, passed the startup identity screen, Project Hub -> Story Flow -> Main Level -> Open Editor -> Environment.
- With the Precipitation dropdown still **OFF**, observed all wind sliders enabled and successfully changed **Direction 0 -> 88 degrees**, **Speed 1 -> 12.75**, **Turbulence 5 -> 13.56**. Main.wiscene acquired unsaved marker and Undo count 3. Screenshot `BUILD/g4-wind-dry-all-controls.png`.
- Sent 3 Ctrl+Z undo actions: values returned to **0, 1, 5**; unsaved marker disappeared, Redo count 3. Screenshot `BUILD/g4-wind-after-undo.png`. Original generated preview `BUILD/g3-weather-preview/Content/Scenes/Main.wiscene` remained unchanged (1680 bytes, original 2026-10-29 20:54:55 mtime).
- Studio FPS overlay was **75 FPS** in the sampled dry-wind screenshots. That is not a controlled vegetation/visual benchmark; the current generated preview contains clouds and sun only, not foliage.

### STILL OPEN
- A dedicated scene with grass and trees **configured for wind** must show calm/breezy/storm response, live direction reversal, and acceptable performance. Some imported mesh materials lack wind enablement / vertex weights; test setup must not mislabel those assets as a weather-engine failure.
- Rain and snow visual appearance / environment interaction; Storm preset currently lacks automatic precipitation pairing. Gate 4 NOT visually signed off for wind/rain/snow.
- Do not modify accepted G1-G3 cloud/lightning visuals. No push/PR/merge without authorization.


## 2026-10-30 continuation: grass-only scope and new evidence

Owner confirms only grass currently exists; trees belong to a future separate system and are not required for this weather gate. See WEATHER_V2_G4_GRASS_WIND_VALIDATION_2026-10-30.md for actual grass calm/wind/direction sequences, Undo/Redo, Save/Reopen, standalone Runtime and 3/3 checks. This supersedes the earlier mandatory grass-and-trees wording. Owner grass-motion signoff remains outstanding. Actual Test Game on the new Save As derivative exposed missing level identity metadata and is not marked passed; a separate registered-level fixture provides standalone Runtime evidence only. G4 overall remains open.
