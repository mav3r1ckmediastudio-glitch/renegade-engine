## Directional blood prototype â€” 2026-10-09

Owner requested an experiment beyond the repeating large animated sprite.
Engineering opt-in: RENEGADE_DIRECTIONAL_BLOOD=1. Default retains the accepted
frame-order / density baseline. No serialized setting or creator UI added.

Candidate reflects incoming direction against the struck surface and blends the
outward normal. Tracked droplets vary cone trajectory and speed, with gravity,
collision traces and existing stain caps. A per-hit seed changes the pattern.
The two authored sheets become smaller moving accents with varied rotation,
size and lifetime. A native GPU BloodDrop emitter supplies additional fine spray;
those presentation particles do not create collision stains. This is not SPH.

Changed Runtime/src/RuntimeBloodEffects.h, RuntimeBloodSheets.h,
RuntimeImpactTextures.h, RuntimeProjectileVisuals.h, Tests/ImpactAudioTests.cpp
and Tests/RuntimeProjectileSessionTests.cpp. WickedEngine source unchanged.
Branch feature/p3-projectile-impact; base cf04bd38434dbc3e84c90ce22ee628e3c3953a80.
Broad pre-existing dirty tree retained; no commit, push or gate closure.

Verification: Release Runtime/helper/session build PASS (BUILD/p3-directional-
blood-build.log and p3-directional-blood-tests-build.log). Direction, gravity,
pause and seed variation assertions pass. Focused ImpactAudio, ProjectileWorld,
RuntimeProjectileSession and LaunchSocket tests PASS 4/4 in
BUILD/p3-directional-blood-focused-ctest.log. Additional ProjectileSimulation
suite passes in p3-directional-blood-ctest.log.
Native --blood-scene front/side/angled runs PASS. Frames12/20/40 saved as
BUILD/p3-directional-{front,side,angled}-{frame}.png; frame12 inspected for all
angles and front frame20 inspected. Side and oblique jets visibly differ.
Package --stage PASS in BUILD/p3-directional-blood-stage.log; embedded Studio
Runtime updated. Candidate standalone launched with process-local opt-in.

Limitations: disconnected round droplets remain visually apparent; floor masks
are procedural fallback art; repeated sheet source remains but is less dominant.
Owner gameplay judgement is pending. Do not mark realism/visual acceptance done.
