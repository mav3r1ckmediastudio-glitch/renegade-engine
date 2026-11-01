# Renegade Weather V2 — G4 Wind, Rain and Snow continuation

Date: 2026-10-30 | Branch: feature/weather-v2-all-gates
Status: ACTIVE WORK PLAN; effects are not visually approved.

## Owner decision and safeguards
Owner visually signed off both fork and sheet lightning and requested development to continue with WIND, RAIN, and SNOW, in that order. See WEATHER_V2_G4_LIGHTNING_VISUAL_SIGNOFF_2026-10-30.md. The visual checkpoint is committed locally as a89e3e6. No push/PR/merge without explicit owner approval.

Custom lightning art, physical lightning impacts, ignition/gameplay and full Lua/C++ weather API parity are deferred future work. No further cosmetic work on accepted lightning without owner request.

## Verified current baseline
- Bridge PrecipitationService owns None/Rain/Snow, intensity, fall speed, particle scale, global wind azimuth/speed, turbulence, streak length, splash scale and colour, using native Wicked WeatherComponent.
- StudioApplication already shows Rain/Snow mode, Intensity, Fall Speed, Particle Scale, Wind Azimuth, Wind Speed and Turbulence controls, with preview and command-backed Undo/Redo.
- Native Wicked wind values feed the GPU wind field (windCS.hlsl), rain emitter and spring objects. The volumetric clouds have independent cloud-motion and coverage wind parameters. Verify visual coherence before wiring them blindly.
- Snow is currently only a slower, larger, zero-streak, zero-splash particle profile using snowflake.dds. That baseline is NOT evidence of adequate snow realism or any accumulation system.
- Existing WeatherEffectsTransitionService (earlier untracked prototype) blends rain/wind parameters and handles rain-to-snow with the shared single native emitter. This is newly registered in EngineBridge/Tests CMake for validation; integration into live Inspector and gameplay remains to be implemented and visually verified.

## Development sequence and owner acceptance
### W1: Wind
- Start with a reproducible scene containing trees/grass, cloud layers, water and rainfall. Verify the current Wind Direction, Speed and Turbulence controls change *visible* foliage/precipitation motion with coherent orientation.
- Evaluate calm, breezy and storm conditions, sudden direction change and gust behavior; test performance and persistence. Existing cloud motion may need a clearly named optional link to global wind rather than forcing an unwanted override.
- Separate simple beginner control from advanced influence/detail where supported. Avoid unrelated vegetation/river shader changes or regressions.

### W2: Rain
- Test rain visibility at near/mid/far camera distance by day and night, including silhouette against dark clouds; check direction, fall speed, density, streak thickness, wind/turbulence response, landing splash, surfaces and audio if available.
- Fix obvious visual artifacts and demonstrate controlled transitions from clear->rain->storm->clear. Check camera movement, pausing, level save/reopen, Test Game and 75fps baseline where reproducible.
- Do not add full wetness/PBR puddles unless separately accepted.

### W3: Snow
- Owner requests snow work in this iteration, superseding earlier purely deferred status. Audit existing native snow profile and render first; the previous assessment was that it was poor and incomplete, so simple reuse is not sufficient for approval.
- Improve flake appearance/size/variation, speed, wind-driven drift, density, distance and transition from rain. Avoid white-disc sprites, indoor particle leakage and visually uniform snowfall.
- Surface accumulation/settling, footprint/deformation and dynamic ice effects are separate scope decisions, not automatic assumptions.

## Tests and checkpoints
- Foundation: WeatherEffectsTransitionTests + PrecipitationTests + signed-off lightning and G3 transition regressions. The first registered transition-test build log is BUILD/g4-weather-effects-foundation-build.log and tests log BUILD/g4-weather-effects-foundation-tests.log (pending until verified).
- Keep only one weather feature branch and document actual DX12 captures, FPS, visual owner decisions. Do not claim test or visual approval without evidence.
- G4 visual acceptance is not G5 editor/packaging/integration signoff.

## G4 integration gap confirmed from code inspection
G3 MakeWeatherPreset() in CommandService.cpp only changes WeatherState (cloud coverage/height, fog, exposure, ambient and cloud shadows). It does NOT apply rain/snow/wind because those live in separate PrecipitationState and SetPrecipitationCommand; similarly the governed Lua set_preset currently drives WeatherTransitionController but not PrecipitationState. Consequently "Storm" means storm-cloud sky rather than automatic rainy/windy weather at present.

Next integration task after transition tests: define bounded precipitation/wind targets for each preset, with explicit creator choice (presets must not silently clobber separately authored custom effects unless requested), and connect transitions using one scene update/Undo transaction. Test Clear->Storm->Clear and Storm->Snow separately. Existing G3 preset visual approval must remain unchanged.
