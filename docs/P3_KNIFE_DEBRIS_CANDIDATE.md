## KNIFE debris candidate - 2026-10-09

Owner requested KNIFE glass, rock and wood shard artwork for surface impacts.
Converted the three original 2x2 TGA alpha atlases to lossless PNG in disposable
BUILD/knife-debris-candidate. Imported governed ResourceAsset products into Bow
playground; no pack artwork embedded in source or committed. Scene material
metadata renegade.impact.{glass,rock,wood}_shards owns dependencies. Runtime
renders individual static atlas quarters on native double-sided lit cards with
ballistic motion/spin and existing128-piece cap/reset/expiry. Glass retains12
pieces at strength1; wood18; rock14. Stone and Concrete use rock donor. Missing
donors preserve procedural fallback. Dust remains separate. Glass uses native
transmission/refraction; source is white mask, and native proof still looks fairly
flat. Wood/rock cards are thin and can disappear edge-on. No physical shard
collision/rigid bodies; no upstream renderer changes. Blood/water unchanged.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80 on feature/p3-projectile-impact;
changes Runtime/src/RuntimeImpactGeometry.h, RuntimeProjectileVisuals.h,
Tests/ImpactAudioTests.cpp, documentation. Broad pre-existing dirty tree remains.
No commits/push/gate closure. Owner appearance review next.

Commands/results:
cmake --build BUILD/renegade --config Release --target RenegadeRuntime
RenegadeRuntimeProjectileSessionTests RenegadeImpactAudioTests --parallel4 PASS
BUILD/p3-knife-debris-build.log (initial helper build failed; corrected native
application/project inspection setup; final build passed).
Helper --install-knife-debris BUILD/bow-projectile-playground/Bow-Playground.renegade
BUILD/knife-debris-candidate PASS BUILD/p3-knife-debris-native.log: governed identity
Save/Reload, UV quartet geometry and bounded expiry. Native Glass/Rock/Wood frame5
images visually inspected. Focused ctest regex
'(Renegade(ImpactAudio|ProjectileWorld|RuntimeProjectileSession)Tests|^LaunchSocket$)'
-C Release --test-dir BUILD/renegade --timeout20 --output-on-failure PASS4/4
BUILD/p3-knife-debris-ctest.log. Helper --stage same descriptor PASS
BUILD/p3-knife-debris-stage.log. Helper --debris-scene packaged GameData descriptor
PASS BUILD/p3-knife-debris-packaged-native.log; no source-folder dependency.
Studio embedded Runtime refreshed; staged standalone PID42984 launched with
process-local directional blood opt-in. Diagnostics BUILD/p3-knife-debris-runtime.json.
