# Weather V2 G4 — Native snow separation (work in progress)
Date: 2026-10-30 | Branch: feature/weather-v2-all-gates
Status: IMPLEMENTATION / BUILD IN PROGRESS; OWNER VISUAL ACCEPTANCE NOT GIVEN

## Why a separate snow particle pipeline is mandatory
Owner rejected the native-snow variant because it was literally the raindrop renderer with a snowflake bitmap. DX12 review captures demonstrated dark, refractive ring-shaped giant droplets in both night and daylight. See BUILD/g4-snow-initial.png and g4-snow-daytime.png. The supplied approved snowflake image resides at Studio/assets/weather/snowflake.dds and is already in the packaged Studio/Runtime Content/weather folder.

## Scoped replacement
- Wicked (local feature submodule) WeatherComponent now recognises native rain only where rain amount >0 AND rain-streak or splash markers are enabled; the persisted snow profile (both zero) no longer creates the rain emitter, rain blocker, distortion pass or rainfall wetmaps. No cloud, lightning or visual shader effects changed.
- Renegade EngineBridge now creates its **own scene-owned snow emitter**, tagged __renegade_internal_weather_snow, only after Wicked initialization. It has its own textured material using Content/weather/snowflake.dds, separate count/cap, fall physics, motion-blur disabled, non-refractive SOFT sprite shader, and camera-relative spawn volume. Native rain's material is never assigned the snow texture.
- The familiar Rain/Snow/OFF UI, authoring commands, Wind/Speed/Turbulence values and scene weather serialization are retained.
- The smoke and regression tests now specifically assert that Snow does not enable native rain or assign snowflake to rainMaterial, and that switching Rain back restores native rain.
- Renderer modifications belong to WickedEngine local feature branch; they require an explicit submodule checkpoint and root worktree pointer commit, never push or merge without owner permission.

## Quality criteria before review
- Own real snowflakes (no ring/droplet refraction), relatively small with depth variation and moderate density.
- Slow downward drift and wind response at zero/breeze/storm speeds; no raindrop elongation or splashes.
- Snow/Rain/OFF transition; Undo/Redo, save/reopen, standalone packaging.
- Day/night legibility, 75fps representative budget, indoor blocker evaluation and weather presets future integration.
- No G4 visual signoff until owner reviews actual Studio or Runtime evidence.

## Build and evidence
- Local script BUILD/g4-snow-native-rain-guard.py and BUILD/g4-snow-emitter-implementation.py authored on DESIGN.
- Build BUILD/g4-independent-snow-build.log (not yet verified at time of this entry).
- Test BUILD/g4-independent-snow-tests.log (not yet verified).
- The user attached Falling leaves.unitypackage; it includes authored leaf particle prefabs/materials/textures, useful as a wind reference, not substituted for the snowflake asset.

## 2026-10-30 — First actual DX12 Studio visual verification (owner visual signoff pending)
- Release build finished successfully at 13:33 (BUILD/g4-independent-snow-build.log; RenegadeStudio.exe). Focused tests 5/5 PASS (BUILD/g4-independent-snow-tests.log).
- Opened the rebuilt Renegade Studio on DESIGN with --g3-weather-preview and --dx12; native viewport shows genuinely separate snowflake-shaped white particles rather than the old dark refractive rings/giant raindrops. Captures: BUILD/g4-new-snow-night.png (high density, large particle setting), BUILD/g4-new-snow-day-moderate.png (slimmer flakes, although sun/time remained at night), and BUILD/g4-new-snow-day-preview.png. 75 FPS is shown in sampled editor overlays at 1920x1080.
- Reworked system remains deliberately **NOT SIGNED OFF**: check particles at default profile and in daylight, size variation/density, moving-camera behavior, wind direction reversal, rain->snow->off transitions, scene save/reopen and packaged Runtime. Snow scene appears visually much improved, but not polished/accepted.
- Preview scene currently has **unsaved inspector changes** from the visual tests. Do NOT save over the original G3 preview scene. Its original on-disk Main.wiscene was not intentionally edited.
- Changes remain **uncommitted** on root weather feature branch and its WickedEngine submodule pending further verification; no GitHub push/merge/PR.

## 2026-10-30 - Owner snowstorm and wind physics acceptance questions
- Owner asked if snow is affected by "golf" (interpreted as Jolt) physics or faked; specifically said maximum snow is NOT dense enough and questioned if wind genuinely affects flakes.
- Accurate implementation distinction: dedicated Wicked GPU particle simulation (per-flake velocity, lifetime, gravity, drag), **not** Jolt rigid bodies. No per-flake collision with meshes, ground accumulation or wetness yet. Before this change wind only set initial velocity; the GPU update did not steer airborne flakes.
- **Under construction**: weather-snow-only emitter flag in Wicked `wiEmittedParticle.h/cpp` and shader `ShaderInterop_EmittedParticle.h`; snow-only branch in `emittedparticle_simulateCS.hlsl` now continuously approaches global wind direction/speed and spatial/time varying curl gusts based on wind randomness. Does not affect other particle systems.
- Fixed snow world-space velocity for a scaled volume emitter. Previously inherited Wicked's transform-normal multiplication, so scaling the spawn volume amplified fall and wind velocity. Only weather snow bypasses the transform scaling; other mesh emitters unchanged.
- Max snow emission now `1150*i + 8300*i^3` particles per second, i in [0,1]. At max 9,450/s and life 4.8 seconds = approx 45,360 concurrent flakes; independently capped to 56,000. At default i=0.50, emission 1,612.5/s. This makes the high end much denser without making all light snow a blizzard. GPU performance must be measured before acceptance.
- Separate GPU simulation continues to be art-driven, not Jolt. If actual 3D solid obstruction, roof masking or accumulation is desired, this needs its own explicit feature/gate and must not be claimed already working.
- Regression updates check maximum emission density/budget, direction reversal, separate snow emitter GPU wind flag and the existing G4 rain/snow and lightning tests.
- **Build in progress, not yet visually signed off:** BUILD/g4-snowstorm-dynamic-wind-build.log; BUILD/g4-snowstorm-dynamic-wind-tests.log. Need exact-tree incremental rebuild after applying world-velocity fix. No push/merge or owner signoff.

## 2026-10-30 — Snowstorm high-density / live-wind DX12 visual review (checkpoint candidate, not owner signoff)
- Scoped changes: weather-snow-only GPU emitter flag and per-frame evolving wind/turbulence in Wicked's emitted-particle compute shader; world-metre-per-second snow velocity now independent of spawn-volume scale. Other particle systems unchanged.
- Snow emission rises nonlinearly with intensity (1150*i + 8300*i^3). Max ~9450/s, 4.8s nominal life, fixed cap 56,000. Snowflake size clamped to 0.022 world units. Individual flakes are sprite particles; they do not use Jolt rigid-body physics, accumulate on terrain, or automatically collide with every building.
- Regression check was initially 4/5 due to a stale 1500/s test expectation; corrected to assert the new intensity curve. BUILD/g4-snowstorm-corrected-tests.log: 5/5 PASS.
- Live DX12 on DESIGN: BUILD/g4-snowstorm-max-first.png showed high-density snow with natural near/mid/far flake scale, sampled 76FPS at almost maximum intensity and moderate wind. BUILD/g4-snowstorm-highwind-1.png showed wind direction ~114 degrees, speed ~14.4, turbulence ~13.9, high density at 75FPS. BUILD/g4-snowstorm-reverse-wind.png shows direction reversed to ~-156 degrees, continuing ~75FPS.
- Transition tested: Snow -> Rain -> Off. Rain reactivated the original Wicked rain path without snowflake sprites; however BUILD/g4-snowstorm-switch-to-rain.png exposed high-intensity rain looking like nearly opaque noisy diagonal streaks and 62FPS. This is NOT approved; a dedicated rain-visual refinement is required. Off cleared all snow and rain: BUILD/g4-snowstorm-off-clear.png, 74FPS.
- Studio test modifications: ten Ctrl+Z Undo actions returned Wind Direction=0, Wind Speed=1, Turbulence=5, mode Off/intensity 0; saved scene remained 1680 bytes with original 2026-10-29 20:54:55 mtime; no unsaved marker (BUILD/g4-snowstorm-undo-clean.png).
- Key uncertainty: screenshots prove visually dense snow and sampled FPS, tests enforce data and GPU flag; a longer moving-camera video, packaging and collision/roof occlusion should still be checked. Owner has not signed off snow.
- Mandatory final exact-tree build launched because Wicked wiEmittedParticle.cpp world-space velocity correction landed during the previous compilation: BUILD/g4-snowstorm-exacttree-final-build.log and -tests.log. Verify success before any local checkpoint.

## 2026-10-30 — Exact-tree final snowstorm verification
- After all Wicked and bridge changes (including scaled-volume velocity correction), `BUILD/g4-snowstorm-exacttree-final-build.log` passed Release Studio plus precipitation/transition test builds; `BUILD/g4-snowstorm-exacttree-final-tests.log` passed 5/5.
- Actual DX12 UI at 1920x1080 with snow intensity 0.965, fall speed ~0.413, particle scale ~0.034, wind direction 114°, speed 14.4, turbulence 13.86 showed full snowstorm at 75FPS (BUILD/g4-snowstorm-highwind-1.png); reverse to -156° continued at 75FPS (BUILD/g4-snowstorm-reverse-wind.png).
- Switched Snow -> Rain -> Off without crashing; Rain rendered but was extremely opaque/noisy and sampled 62FPS (BUILD/g4-snowstorm-switch-to-rain.png). This is the next HIGH PRIORITY rain-quality defect, not a snow-regression. Off cleared precipitation, restoring 74FPS in screenshot (BUILD/g4-snowstorm-off-clear.png).
- Ten Undo actions returned the temporary preview to precipitation Off, original wind direction/speed/turbulence and cleared unsaved state; original preview Main.wiscene mtime unchanged at 2026-10-29 20:54:55.
- This is an implementation checkpoint, **NOT** full visual snow signoff: user review, ground/roof occlusion, weather save/reopen, full Runtime and packaging still need acceptance. Jolt per-flake collision, accumulated snow and dynamic ice remain out of current scope.
