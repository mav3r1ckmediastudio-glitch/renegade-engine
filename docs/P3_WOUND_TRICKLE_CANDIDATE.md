## Short arrow-wound blood trickle candidate - 2026-10-09

Owner asked for slow blood line beneath character arrow wound and authorised
short local prototype. For embedded/stick-on-impact projectile Character hits
with resolved skin donor and mesh triangle, add one thin native projected blood
trail beside wound decal. Growth starts after.2s, reaches12cm over4s, then stops
and persists within120s mark lifetime. Four procedural64x128 uneven masks; dark
red material and existing quantised blood stain surface wet-to-dry over90s.
Gravity projected onto contact tangent plane determines downward growth. Once
stopped, roll is retained relative to triangle so old stain moves with receiver.
Trail uses existing64 mark-entity cap/reset/removal; one extra slot per wound.
No donor/triangle means no trickle; horizontal surfaces with no tangent gravity
omit trail. No falling drops or floor drips added in this scoped prototype.
This is a shallow local projection, not fluid simulation or traced skin runoff;
curved limbs can clip it and full live animation acceptance remains open.

Native mid-growth and fully-grown frames inspected: narrow red line extends
beneath wound. Existing blood splash, glass and other surface behaviour retained.
Owner previously said glass contrast version is better; retain provisionally
and revisit alongside later blood/water/glass quality pass.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80; feature/p3-projectile-impact.
Changes Runtime/src/RuntimeImpactMarks.h, RuntimeProjectileVisuals.h,
Tests/ImpactAudioTests.cpp and docs. Transient state only; no scene schema or
serialized preset changes. No upstream changes, commits, push or gate closure.

cmake --build BUILD/renegade --config Release --target RenegadeRuntime
RenegadeRuntimeProjectileSessionTests RenegadeImpactAudioTests --parallel4 PASS
BUILD/p3-wound-trickle-build.log. Helper-only proof rebuild PASS
BUILD/p3-wound-trickle-proof-build.log. Helper --marks-scene Bow descriptor PASS
BUILD/p3-wound-trickle-native.log: native growth/pause/stop, wound movement and
triangle deformation, mark expiry; native mid/full-grown screenshots inspected.
ctest --test-dir BUILD/renegade -C Release -R
'(Renegade(ImpactAudio|ProjectileWorld|RuntimeProjectileSession)Tests|^LaunchSocket$)'
--timeout20 --output-on-failure PASS4/4 BUILD/p3-wound-trickle-ctest.log.
Helper --stage Bow descriptor PASS BUILD/p3-wound-trickle-stage.log.
--marks-scene packaged GameData descriptor PASS
BUILD/p3-wound-trickle-packaged-native.log. Studio embedded Runtime refreshed;
standalone PID65852 launched with process-local directional blood opt-in;
BUILD/p3-wound-trickle-runtime.json diagnostics. Next owner test: hit dummy with
arrow and watch wound for4-5s; assess trail length/width and curved-body attachment.


Owner provisionally accepted current run on 2026-10-09, while noting it is not natural. Later blood realism refinement remains open. Actual skeletal attachment diagnostic found previous-pose latency; see P3_ANIMATED_WOUND_VERIFICATION.md.
