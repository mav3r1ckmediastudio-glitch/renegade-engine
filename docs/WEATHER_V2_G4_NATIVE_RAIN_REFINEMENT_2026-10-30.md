# Renegade Weather V2 G4 — Native Rain Heavy-Storm Rendering Correction
Date: 2026-10-30 | Branch: feature/weather-v2-all-gates
Status: IMPLEMENTING, TESTS AND LIVE DX12 OWNER SIGNOFF OPEN. No push/merge.

## Defect reproducible in DX12
Immediately after Snow -> Rain, the native rain emitted near-opaque diagonal noise across nearly all of a cloud/night sky preview at high intensity and global wind ~14.4 / turbulence ~13.9. Studio sampled ~62 FPS, compared with ~75 FPS with Snow or Off. Evidence: BUILD/g4-snowstorm-switch-to-rain.png.

## Architectural cause
Wicked Scene update originally allocated 1,000,000 native rain particles and used count = 1,000,000 * rain_amount per second, with 1 second particle lifetime. Also wiEmittedParticle::UpdateGPU transformed initial velocity by the rain emitter's 30-metre scale, multiplying both global wind and fall speed by 30. This produced enormous overdraw and unnatural sideways streaks, especially at high wind. It is not a problem with the separate snow renderer.

## Scoped correction
- Add dedicated CPU-only `FLAG_WEATHER_RAIN` marker to the native rain emitter to use true world-metre-per-second initial velocity, without changing mesh/other scene emitters. Do not enable snow's unique shader-driven wind-gust option for rain; native rain gets wind at spawn and retains its own render/lighting/occlusion path.
- Decouple the rain emitter spawn volume from velocity. Set physical fall speed to 13 * authored rain fall-speed (default 1.25 -> ~16.25m/s); global wind speed applies directly rather than being scaled 30x.
- Limit native rain to 120,000 particle capacity, emission `11,000*i + 85,000*i*i` (at maximum, ~96,000/s; at 0.45, ~22,200/s). The intensity slider remains 0..1, with a more controllable density curve that maintains room for heavy rain without an opaque texture wall.
- Keep the approved Snow GPU particle path, sheet/fork lightning, cloud shaders, existing material blending and precipitation authoring commands unchanged. Scene wetmap/water and rain-blocker effects continue only for Rain, not Snow.

## Verification plan (not yet complete)
- Release build and ctest targeted precipitation, effects/sky transitions, and signed-off lightning regressions: BUILD/g4-rain-physically-bounded-build.log and -tests.log.
- DX12 preview light 0.2, moderate ~0.45 and heavy ~0.95; calm and strong-wind screenshots, rain -> snow -> off, weather persistence and FPS.
- Compare before screenshot, ensure clouds remain legible, streak geometry stays believable, upper particle budget is bounded, no artificial giant horizontal curtains, no loss of required rain intensity.
- Use project weather preview only; avoid saving over original G3 Main.wiscene. Keep all changes local pending owner visual review.
