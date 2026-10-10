## Owner water review - 2026-10-09

Owner: smaller droplets are better; water is acceptable for now. Crown explicitly
accepted as superb. Schedule further water spray/droplet realism refinement
alongside the future blood effects quality pass. This is provisional visual
acceptance, not final reference-quality completion or overall P3 gate closure.

## Smaller water droplets - 2026-10-09

Owner accepted crown as superb; crown geometry/material/motion unchanged.
Water mesh drops increased32 to64 and linear dimensions reduced to about39-40%
(.0035-.007 X/Z, .0045-.010 Y). Fine plume/skirt sprites unchanged.
Existing shared128-piece cap retained. Native frame20 inspected: small scattered
highlights replace large round drops. Owner droplet acceptance pending.
Release build/native proof/focused ctest4of4/staging PASS; logs respectively
BUILD/p3-water-small-drops-build.log, -native.log, -ctest.log, -stage.log.
Studio runtime refreshed; standalone PID59412 launched with directional blood
process-local opt-in. Diagnostics BUILD/p3-water-small-drops-runtime.json.
No upstream changes, commits, push or gate closure.

## Fine noisy water spray candidate - 2026-10-09

Owner reference image(20261009-145221).png requests dense fine irregular spray.
Crown now uses white vertex RGB with alpha .07 at body rising to .85 at lip;
native PBR transmission .98, roughness .025, reflectance .08, refraction .003,
zero emission. Thirty-two ballistic transmissive mesh drops provide larger accents.
Native soft-lighting sprite bursts add 160 fine upward particles and 96 wider base
particles at strength1 (projectile strength scales counts). Per water emitter cap192;
random size/life/color, gravity and motion stretch break up uniformity. Sprite mask
has clearer centres and stronger rims/glints; sprites are presentation accents,
not physically refractive droplets. Crown and larger drops use native PBR.

Release Runtime/session/helper build PASS BUILD/p3-water-noisy-build.log.
Native --water-scene proof PASS BUILD/p3-water-noisy-native.log; frames12/20
inspected. Fine plume is denser, but still compact; reference parity not claimed.
Focused ctest PASS4/4 BUILD/p3-water-noisy-ctest.log. Stage PASS
BUILD/p3-water-noisy-stage.log. Studio embedded Runtime refreshed; staged standalone
PID45752 launched with process-local directional blood enabled. Diagnostics in
BUILD/p3-water-noisy-runtime.json. Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80,
branch feature/p3-projectile-impact. No upstream edits, commits, push or gate closure.
Owner appearance acceptance remains pending. Earlier sections are historical.

## Water refined candidate completion evidence - 2026-10-09

Final native inspection rejected the pointed film as too glass-like; refined to
low-frequency rounded rolling lip with small top breaks, transmission .98,
roughness .035, reflectance .02, refraction .003, zero emission. Frame12 of the
refined run inspected; prior frames5/12/35 inspected during iteration. Crown
still has a procedural silhouette; reference-quality irregular jet/sheets remain
open. Native surface waves are visually subtle in this shallow basin, so ripple
quality is not accepted solely from successful API/texture/lifecycle checks.

cmake --build BUILD/renegade --config Release --target RenegadeRuntime
RenegadeRuntimeProjectileSessionTests RenegadeImpactAudioTests --parallel 4:
PASS BUILD/p3-water-clear-refined-build.log.
Helper --water-scene BUILD/bow-projectile-playground/Bow-Playground.renegade:
PASS BUILD/p3-water-clear-refined-native.log (native render/pause/reset isolation).
ctest --test-dir BUILD/renegade -C Release -R
'(Renegade(ImpactAudio|ProjectileWorld|RuntimeProjectileSession)Tests|^LaunchSocket$)'
--timeout 20 --output-on-failure: PASS4/4 BUILD/p3-water-clear-refined-ctest.log.
Helper --stage same descriptor: PASS BUILD/p3-water-clear-refined-stage.log.
Studio embedded Runtime refreshed; candidate standalone PID48248 launched with
accepted directional blood opt-in. Native pool sits right of existing targets.
No changes to upstream submodule/source; no commits or gate closure. Owner
appearance acceptance is next; simple material checks do not establish realism.

## Clear water impact candidate - 2026-10-09

Owner accepted the prior directional blood/arrow revision as SIGNIFICANTLY better.
Working standalone continues process-local RENEGADE_DIRECTIONAL_BLOOD=1.
Water movement accepted, prior pale crown/mesh hoops rejected as cartoonish.
Reference: transparent splash sheets, broken strands, lit curved rims and drops
in uploaded image(20261009-140149).png. No reference parity claim.

RuntimeImpactGeometry replaces water mesh hoops with native Scene::PutWaterRipple
normal distortion for horizontal contacts. This is visible on native WATER shader
surfaces, not an ordinary opaque object merely tagged Water. Semantic impact
classification remains independent of visual material. Native ripples use the
built-in texture, gameplay ageing, 32 owned-ripple cap and reset removes only
impact-owned ripples; unrelated Character/native ripples survive. Vertical Water
fixtures still receive splash geometry but no horizontal ripple.

Crown geometry is a curved smooth-normal film with uneven lip and upper breaks;
duration .38s preserved, readable size/fade increased. Eighteen small transmissive
ellipsoid drops replace pale sprite drops, under gravity with bounded expiry.
Native PBR material: transmission .98, roughness .035, reflectance .02,
refraction .003, zero emission. No rigid bodies, SPH or renderer/upstream edits.
Shared 128 geometry-piece cap and scene reset retained. Geometry remains a
procedural candidate; the reference's complex rebound jet is not implemented.

A native water-shader test pool beside the target row was added only to the
disposable Bow playground, with patterned basin/light for reflection inspection.
Normal scene document Save/Reload and semantic Water classification verified.
Helper --water-scene is read-only; --install-water-proof saves the test fixture.
Native proof checks valid ripple texture, zero-dt pause and reset isolation.

Changed Runtime/src/RuntimeImpactGeometry.h, RuntimeProjectileVisuals.h,
Tests/RuntimeProjectileSessionTests.cpp, Tests/ImpactAudioTests.cpp.
Branch feature/p3-projectile-impact; base cf04bd38434dbc3e84c90ce22ee628e3c3953a80.
No serialization/UI exposure change, commits, push or gate closure.

Verification:
- Release Runtime/session/helper initial build PASS BUILD/p3-water-clear-build.log.
- Focused tests PASS4/4 BUILD/p3-water-clear-ctest.log.
- Native pool save/reload/proof PASS BUILD/p3-water-clear-native.log.
- Lighting proof rebuild/native PASS p3-water-proof-lit-build.log,
  p3-water-lit-native.log. Initial grey proof lacked useful specular contrast;
  lit proof was too washed out and drove a lower test-light intensity.
- Final candidate build/native logs: BUILD/p3-water-clear-final-build.log,
  BUILD/p3-water-clear-final-native.log. Final completion recorded separately.
Refined frame12 inspected; crown remains a procedural candidate and native ripples visually subtle.
Owner acceptance is pending; do not mark realistic water artwork completed.
