## Animated skin-attached retained arrows - 2026-10-09

Owner accepted animated wounds as brilliant/effective, and identified arrows
remaining in object-relative space. Owner authorised triangle-attached arrows.
Retained now captures Character contact triangle independently of mark artwork.
Store arrow rigid pose relative to orthonormal triangle frame at impact, including
authored embed depth and incoming-angle appearance. Recompose with current native
skinned triangle frame; never inherit triangle stretch or nonuniform receiver
scale. Unresolved skin contact retains prior rigid object fallback. Deleted or
invalid captured receiver/topology retires arrow on next pre-upload Sync.

Existing completed-pose refresh now handles retained skin arrows too. It updates
arrow root and descendant world matrices while preserving their local transforms,
then refreshes only existing arrow object render slots: CPU matrices/bounds,
GPU current/raw pose and centre/radius, and native TLAS instance transform.
Previous-frame transforms, meshlet allocation, instance identity and occlusion
history are preserved. No repeated whole-object update, animation or physics
step, and no entity/component creation/removal after native scene upload.
Uses public pinned Scene mapped instance surface; review with upstream upgrades.
Arrow remains rigid; extreme joint folds can intersect body. Trail curvature
quality is still deferred. No upstream shader/source or schema change.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80 feature/p3-projectile-impact.
Changes Runtime/src/RuntimeImpactMarks.h, Runtime/src/RuntimeProjectileVisuals.h,
Tests/ImpactAudioTests.cpp, HANDOFF and docs. Broad pre-existing dirty tree remains.
No commit/push/release gate closure. Owner gameplay judgement pending.

Commands/results:
cmake --build BUILD/renegade --config Release --target RenegadeRuntime
RenegadeRuntimeProjectileSessionTests RenegadeImpactAudioTests --parallel 4
PASS BUILD/p3-skin-arrow-build.log.
cmake --build BUILD/renegade --config Release --target RenegadeImpactAudioTests --parallel 4
PASS BUILD/p3-skin-arrow-proof-build.log for native real-arrow assertions.
Helper --animated-marks BUILD/bow-projectile-playground/Bow-Playground.renegade
<owner-v2-descriptor> 7ee33e61-fad0-497c-8d63-725497cb927b
PASS EXIT0 BUILD/p3-skin-arrow-native.log: three actual governed arrows at different
incoming angles, rigid full-pose relation to animated skin, child CPU/GPU matrices,
current culling bound centres, wound/trail contact accuracy, receiver deletion.
Walk/swipe/run clips and loop reset exercised; contact travel up to1.97425m, wound
max .00100005m equals intended1mm outward offset. Initial proof selected duplicate
clip names on pre-existing hidden playground Mutant and failed motion assertion;
corrected selection restricted to new proof rig entity IDs. No false pass accepted.
Native swiping/run frame70 PNGs inspected: arrows stay at visible wounds.
ctest --test-dir BUILD/renegade -C Release -R
'(Renegade(ImpactAudio|ProjectileWorld|RuntimeProjectileSession)Tests|^LaunchSocket$)'
--timeout 20 --output-on-failure
PASS4/4 .57s BUILD/p3-skin-arrow-ctest.log.
Helper --stage Bow descriptor PASS BUILD/p3-skin-arrow-stage.log.
Same animated helper with staged GameData/Bow-Playground.renegade descriptor
PASS BUILD/p3-skin-arrow-packaged-native.log. Governed arrow product read from
staged project; extra diagnostic rig still read from owner v2 project.
Process-local RENEGADE_ARROW_POSE_PROOF=1 helper --blood-scene Bow descriptor
PASS BUILD/p3-skin-arrow-rigid-native.log: front/oblique/grazing nonuniform receiver.
Old owned PID44864 verified executable path and stopped. Studio embedded Runtime
refreshed; rebuilt staged standalone with saved animated Mutant opened PID36576,
process-local directional blood opt-in then environment removed.
python Tools/Read-RenegadeDiagnostics.py --process runtime
responsive true BUILD/p3-skin-arrow-runtime.json; both playground Characters loaded.
No serialized runtime changes; playground Mutant Save/Reload already verified.
Next: owner shoot moving Mutant and assess arrows travelling with wounds, then
ordinary-project impact dependencies and authored P3 completion work.

## Playable animated Mutant in Bow playground - 2026-10-09

Owner requested actual animated Mutant target in playground after previous
diagnostic-only proof. Added governed passive Character named Animated Mutant
impact target at world2,0,5.5 beside old dummy. Mutant001 reusable skinned payload
read from owner v2 project; source project untouched. Native flexing-muscles clip
loops for visible body motion; autonomous pursuit disabled. Saved and reloaded
playground verifies Character identity and playing loop. Original dummy retained.

Source product contains no live/governed textures and stale paths to missing
Mutant_diffuse.png from original FBX exporter. Test installation clears those
missing native texture names from new target only; neutral grey appearance.
Initial texture-slot iteration crashed due null Resource::GetTexture without
Resource::IsValid; corrected guard. Initial Stage rejected missing texture path;
existing-target repair clears missing names and saves/reloads. Final stage passes.
Source reusable asset and owner v2 scene not modified.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80 feature/p3-projectile-impact.
Changed Tests/ImpactAudioTests.cpp --install-mutant helper and disposable BUILD
Bow scene; docs. No production code/upstream/schema changes, commit or gate closure.

cmake --build BUILD/renegade --config Release --target RenegadeImpactAudioTests --parallel 4
PASS BUILD/p3-mutant-playground-build.log.
Helper --install-mutant BUILD/bow-projectile-playground/Bow-Playground.renegade
<owner-v2-descriptor> 7ee33e61-fad0-497c-8d63-725497cb927b
PASS Save/Reload BUILD/p3-mutant-playground-install.log; existing-target texture
repair PASS BUILD/p3-mutant-playground-repair.log.
Helper --stage same Bow descriptor PASS BUILD/p3-mutant-playground-stage.log.
Owned old PID40304 verified executable path and stopped. New staged standalone
PID44864 opened with process-local RENEGADE_DIRECTIONAL_BLOOD=1; env removed.
python Tools/Read-RenegadeDiagnostics.py --process runtime
responsive true BUILD/p3-mutant-playground-runtime.json, Character count2.
Native desktop capture BUILD/p3-mutant-playground-live.png inspected: animated
Mutant visible right of dummy with bow, retained arrows and wounds/blood present.
Owner was already testing during screenshot; appearance acceptance not inferred.
Next owner assess animated wounds/trails directly. Known retained arrows still
follow contacted object rather than exact skinned triangle; same-pose mark fix
does not establish exact animated arrow embedding. Curved runoff remains deferred.

## Same-pose animated wound attachment correction - 2026-10-09

Owner authorised fixing measured previous-pose wound delay. Runtime now performs
transform-only RefreshImpactMarkPose after wi::Application::Update completes
native skinning and before PreRender visibility/decal renderer upload.
Runs native public Scene::RunDecalUpdateSystem to refresh render-facing world,
position and AABB data. No second Scene::Update, no extra physics/animation step.
Transient creation, removal, expiry and growth remain exclusively in earlier
Sync. Invalid late anchors are left for next pre-upload cleanup. All governed
triangle marks benefit; mark art, growth and accepted blood/water/glass unchanged.
No upstream changes or serialized state changes.

Actual Mutant walking/swiping/running proof now asserts wound centre error below
3mm including raw locomotion loop resets. Maximum .00100005m across all clips,
equal intended 1mm outward normal offset (previous maximum1.89457m at root reset).
Verifies render-facing decal world translations and culling-bound centres, and
fully-grown trail top remains within3mm of current wound contact. Six tracked
entities retained; paused refresh and receiver deletion cleanup pass. Native
walking/swiping/running frame70 images inspected: wounds follow deformed body.
Flat trails still clip on curved skin; this fix is timing, not curved runoff.
Diagnostic character is grey because texture restoration not performed on merged
Character product; no final character-material quality claim.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80 feature/p3-projectile-impact.
Changed Runtime/src/RuntimeProjectileVisuals.h, Runtime/src/RuntimeApplication.cpp,
Tests/ImpactAudioTests.cpp, HANDOFF and docs. Pre-existing dirty tree preserved.
No commit/push/gate closure or final owner animated gameplay acceptance.
Historical diagnostic section below describes pre-fix failure.

Commands/results:
cmake --build BUILD/renegade --config Release --target RenegadeRuntime
RenegadeRuntimeProjectileSessionTests RenegadeImpactAudioTests --parallel 4
PASS BUILD/p3-same-pose-marks-build.log.
cmake --build BUILD/renegade --config Release --target RenegadeImpactAudioTests --parallel 4
PASS BUILD/p3-same-pose-trail-build.log after stronger trail assertions.
BUILD/renegade/Release/RenegadeImpactAudioTests.exe --animated-marks
BUILD/bow-projectile-playground/Bow-Playground.renegade <owner-v2-descriptor>
7ee33e61-fad0-497c-8d63-725497cb927b
PASS EXIT0 BUILD/p3-same-pose-marks-native.log. Native rest/walking/swiping/run PNGs
BUILD/p3-animated-wounds-*.png; inspected three frame70 captures.
ctest --test-dir BUILD/renegade -C Release -R
'(Renegade(ImpactAudio|ProjectileWorld|RuntimeProjectileSession)Tests|^LaunchSocket$)'
--timeout 20 --output-on-failure
PASS4/4 .63s BUILD/p3-same-pose-marks-ctest.log.
Helper --stage BUILD/bow-projectile-playground/Bow-Playground.renegade
PASS BUILD/p3-same-pose-marks-stage.log.
Same --animated-marks with staged GameData/Bow-Playground.renegade descriptor
PASS BUILD/p3-same-pose-marks-packaged-native.log. Character still read from owner
project, so this proves packaged mark resources, not packaged Character closure.
Existing owned PID65852 verified path and stopped; Studio embedded Runtime copied
from final Release. Staged standalone PID40304 launched with process-local
RENEGADE_DIRECTIONAL_BLOOD=1, environment removed after launch.
python Tools/Read-RenegadeDiagnostics.py --process runtime
PASS responsive true BUILD/p3-same-pose-marks-runtime.json.
No serialization test needed for transient timing-only change.
Next: owner animated gameplay judgement; ordinary-project impact dependency
availability and remaining authored P3 completion checks. Curved runoff quality
stays in later blood pass.

# Animated wound verification

## Animated wound verification - 2026-10-09

Owner provisionally accepts short trickle ("not a natural run ... it'll do");
retain current artwork for later blood realism pass. Owner authorised checking
wounds/trails through animated characters. Added read-only helper mode in
Tests/ImpactAudioTests.cpp: --animated-marks <Bow descriptor> <Character project
descriptor> <Character asset ID>. Prepares governed reusable skinned product,
merges only in memory with Bow lighting/mark donors, hides original objects,
uses native ray contacts for three skin hits (upper body and leg), plays walking,
swiping and running clips, captures native frames and measures pose mismatch.
No owner project Save, runtime code or upstream edits. Character textures were
not restored by this diagnostic merge; grey presentation frames assess attachment,
not final character material quality. Native images rest/walking/swiping/run
inspected; dark wounds move with body but trails clip and can be hard to see.

RESULT: functional anchoring/lifecycle passes; animated appearance NOT ACCEPTED.
Same completed pose wound centres are accurate to intended 1mm normal offset
(max .00100005m). At representative frame70, pre-update wound versus current
skin errors are .014-.020m walking, .017-.045m swiping, .034-.040m running.
Maximum includes root-translation loop resets/clip transitions: walking1.72946m,
swiping1.75983m, running1.89457m; swiping after first3frames max .0487016m.
These are discontinuities in this raw clip fixture, not a claim of metre-scale
continuous gameplay drift. Runtime also Syncs projectile marks before native
Scene animation/skinning Update; helper reproduces that order. Consequently
the known one-pose latency is now measured on an actual governed character.
Six wound/trail entities retain valid anchors through clips; paused refresh and
deleted-receiver cleanup pass. Trail gravity freezes after growth as authored,
but local tangent projection does not wrap over skin or joints.

Base cf04bd38434dbc3e84c90ce22ee628e3c3953a80 on feature/p3-projectile-impact;
broad pre-existing dirty tree remains. Changed Tests/ImpactAudioTests.cpp and docs.
No runtime rebuild/deployment required for test-only addition; existing owner
standalone retained. No serialized changes, commit, push or release gate closure.

Commands:
cmake --build BUILD/renegade --config Release --target RenegadeImpactAudioTests --parallel 4
PASS BUILD/p3-animated-marks-build.log. Initial proof compile assignment mismatch
corrected to explicit ProjectileVector fields. Initial bounds-based contacts
missed; final proof uses native ray contacts rather than assuming broad bounds.
BUILD/renegade/Release/RenegadeImpactAudioTests.exe --animated-marks
BUILD/bow-projectile-playground/Bow-Playground.renegade <owner-v2-descriptor>
7ee33e61-fad0-497c-8d63-725497cb927b
EXIT0 BUILD/p3-animated-marks-native.log. This exit covers functional assertions,
and explicitly logs ANIMATED APPEARANCE NOT ACCEPTED; do not treat exit0 as visual
acceptance. Images BUILD/p3-animated-wounds-{rest,walking-70,swiping-70,run-70}.png.
ctest --test-dir BUILD/renegade -C Release -R
'(Renegade(ImpactAudio|ProjectileWorld|RuntimeProjectileSession)Tests|^LaunchSocket$)'
--timeout 20 --output-on-failure
PASS4/4 .54s BUILD/p3-animated-marks-ctest.log.

Next bounded task: design same-pose skin attachment that reaches the native GPU
decal upload in the correct frame. Do not move transient creation/removal after
Scene upload: this previously caused giant close-camera blood artifacts.
Do not casually add a second full Scene::Update(0), which can affect temporal
skinning/physics and doubles scene work. Keep accepted static effects unchanged.
Then rerun actual walking/running/swiping appearance and retained-arrow following.
Full playable character gameplay acceptance remains open.
