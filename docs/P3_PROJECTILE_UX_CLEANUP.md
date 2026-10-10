# Projectile UX cleanup — 7 October 2026

Owner approved continuing the previous conversation's three-stage sequence after extensively testing the repaired animated bow: (1) UX cleanup, (2) preset refinement, (3) visual animation event authoring. Human arms replacement/retargeting is deferred until projectile work finishes. Bow gameplay acceptance does not close the wider P3 framework gate.

## First stage

New/Edit Copy opens in Basic mode: name, starting preset, visible model, scale, speed, gravity, impact behaviour and flight/impact effect choices. SHOW ADVANCED reveals lifetime, model rotation, damage payload, particle size/rate/lifetime/offset, and stick duration/embed depth when impact is Stick. Hiding controls retains their draft values. Advanced is transient editor state; schemas and Runtime remain unchanged.

Weapon setup uses Spawn point, Firing and Launch projectile labels. A new binding with one authored spawn point selects it automatically. Existing camera-origin and named bindings retain their saved choices. Multi-point policy appears only for multiple authored points or a saved multi-point policy needing repair. The second point is shown only for Alternate/Both. Unavailable saved names remain visible rather than silently becoming None. Launch at animation start uses zero seconds; After a delay exposes the existing numeric delay. A visual timeline marker remains stage three.

Basic/Advanced presentation is reapplied after native Window visibility propagation. Source changes stay in Studio and use existing bridge persistence. No shotgun or bow assembly payload is changed by this UI implementation. Existing presets already cover Bullet/Arrow/Bolt/Thrown/Spell; stage two must refine that implementation rather than create a parallel template system. Rocket explosions and guided flight must not be implied by labels before implemented.

## Validation

Windows x64 Release Studio build succeeds. Final incremental MSBuild run: 16.43 seconds. Command: `MSBuild BUILD/renegade/Studio/RenegadeStudio.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:2 /v:minimal /nologo`, with `CL=/MP4`. Build logs: `BUILD/projectile-ux-build.log`, `BUILD/projectile-ux-build-final.log`.

Focused Release CTest passes: RenegadeProjectileAssetTests, RenegadeRuntimeProjectileSessionTests (2/2, .90 seconds); EquipmentAsset, LaunchSocket (2/2, .54 seconds). Native visual/save-reopen evidence is recorded below once completed. No independent exact-commit verification or release-gate closure claimed.

Native checks: hiding/showing Advanced retained X rotation 30. Saving from Basic produced proof asset 66c57800-f4b0-40d1-bdd7-d7ae632e32fc; EDIT COPY reloaded rotation [30,0,0] from saved .rprojectile. Proof remains unassigned. Bow binding reopened Bow_Nock and .133333 seconds, with policy/second-point controls hidden. Search and immutable-copy opening also exercised.

Visual inspection caught footer clipping and per-frame SetPos invalidating cached native dropdown geometry; both corrected. A duplicate local declaration in the first layout-cache build failed compilation and was removed. Final build after those corrections PASS (28.15 seconds), log BUILD/projectile-ux-build-verified-final.log. Final rebuilt visual evidence follows below.

Final native verification: rebuilt Basic and Advanced windows have visible Save/Cancel controls; impact and launch timing dropdown geometry is correct. Single Bow_Nock UI retains saved delay 0.133333 and hides multi-spawn controls. Selecting At animation start hides the delay and moves subsequent controls correctly. Timing selection was discarded without Apply. Evidence: BUILD/projectile-ux-basic.png, projectile-ux-advanced.png, projectile-ux-single-spawn.png, projectile-ux-animation-start.png. A brief native not-responding state recovered; no restart or asset edits were needed. Two-spawn native visual check remains pending. Four focused tests and final Release build pass.

## Preset refinement
Bullet now starts with a thin tracer and impact sparks; Spell starts with flame and impact sparks. Arrow, Bolt and Thrown retain their established trajectories and model-free validity. Preset selection explains flight and impact choices, including the model requirement for Stick, and preserves model/scale/rotation. No saved assets are migrated. Runtime implementation/schema are unchanged. No homing, explosion or bounce support implied.
Validation: Release Studio build with references PASS (BUILD/projectile-presets-build.log). ProjectileAssetTests rebuild PASS; CTest --test-dir BUILD/renegade -C Release -R '^RenegadeProjectileAssetTests$' --output-on-failure PASS 1/1 (0.69 seconds), including all-five-preset serialization and model-free feedback checks. Native verification follows.

Preset refinement based on cf04bd38434dbc3e84c90ce22ee628e3c3953a80: Bullet tracer/sparks and Spell flame/sparks defaults; explanatory Studio preset help, model transforms retained. Changes: EngineBridge/src/ProjectileAssetService.cpp, Studio/src/PlayerProjectileEditor.cpp, Tests/ProjectileAssetTests.cpp. Release Studio build PASS, preset serialization/model-free feedback CTest PASS 1/1. Logs BUILD/projectile-presets-build.log and projectile-presets-tests-build.log. Native Spell selection verified (flame, sparks, gravity 0, readable help), BUILD/projectile-presets-spell.png. Saved bow/shotgun assets untouched; no migration or gate closure. Next: animation markers; two-spawn visual proof pending.

## Visual release timing candidate
Adds SET RELEASE IN ANIMATION to Weapon projectiles. Uses saved equipment action animation and production FirstPersonAssemblyService in a private ModelImportPreview. Preview cursor scrubs the synchronized assembly; release marker scrubs to its pose; Mark current pose copies playhead time. Use release time transfers marker seconds to the existing draft releaseSeconds control. Cancel discards marker changes; Apply to this player and Save level remain the persistence path. No assembly edits or new schema. Missing assembly/action reports a message and keeps numeric timing available. Marker must be finite, nonnegative, before clip end and at most five seconds.
Changed: Studio/src/PlayerProjectileEditor.cpp, Studio/src/StudioApplication.h, Studio/src/StudioApplication.cpp, Studio/src/ModelImportPreview.h. Based on cf04bd38434dbc3e84c90ce22ee628e3c3953a80. Build/visual verification pending; no release-gate closure. This is projectile release authoring; a general multi-event persisted timeline remains future work.

Marker candidate Release builds PASS: BUILD/projectile-marker-build.log, projectile-marker-build-final.log. Final build removed a source encoding warning. Three focused CTest checks PASS (1.09 seconds): RenegadeProjectileAssetTests, EquipmentAsset, LaunchSocket. Git diff --check PASS. Native marker verification remains required.

Marker native verification PASS on final rebuilt Studio: correct saved mannequin/bow assembly shown at QuickShot 0.133333 s, duration 0.833333 s. Numeric marker 0.3 scrubs both tracks and transfers 0.3 into draft delay. Cancel retains original 0.133333. Closing without Apply/reopening restores saved timing. Play reaches clip end; Mark current pose copies 0.833333; end marker is rejected by Use time validation. Restored marker to 0.133333 and left window open. No Apply, asset save or level save during marker verification. Evidence: BUILD/projectile-marker-bow.png, projectile-marker-scrub.png, projectile-marker-transfer.png, projectile-marker-end-rejected.png. Release build logs projectile-marker-build.log and projectile-marker-build-final.log PASS. CTest --test-dir BUILD/renegade -C Release -R '^(RenegadeProjectileAssetTests|EquipmentAsset|LaunchSocket)$' --output-on-failure PASS 3/3 in 1.09 s; git diff --check PASS. Runtime launch at a newly authored marker, two-spawn visual check, independent exact-commit review and generic multi-event timeline remain pending. No gate closure or push.

## Saved release marker runtime proof
Native marker set to 0.3 s, Use release time then Apply produced NEW equipment 8be37877-0bec-4c93-99fb-23bf2032e28c. Persisted .requipment has release_seconds 0.30000001192092896, Bow_Nock, existing assembly and Flaming Arrow - Stick identities. Original equipment 14a147e6-be41-45e2-adda-67b9d00abb77 remains unchanged. Test Game snapshot launched from current unsaved scene through production Studio Play. Runtime startup took longer than expected; initial lack of visible window was startup, not a failed Play action.
Controlled shot diagnostic samples show pending=1 at animation 266 ms (launch count 1 baseline), pending=0 and launch count 2 at 306 ms. Impact count becomes 2 at 813 ms. Exactly one new launch and one impact for the controlled click; launch socket Bow_Nock; no projectile_error or projectile_visual_error. This brackets launch to (266,306] ms using sampled diagnostics; does not claim frame-exact timing. Existing effect/stick presentation visibly intact. Evidence BUILD/projectile-marker-runtime-before.json and projectile-marker-runtime-timing.json.
Runtime closed; Ctrl+Z restored original equipment assignment (Undo 0 / Redo 1), native Weapon projectiles reopened Bow_Nock and 0.133333 seconds. No level save during this validation; original scene on disk retained. Disposable 0.3 s proof copy remains unassigned for review. All work confined to Bow Playground; shotgun untouched. No new source/build change this validation turn. Independent exact-commit review and two-spawn visual check remain; no release gate closure.

## Two-spawn native UX verification complete
Temporarily assigned existing Shotgun - Both PSPs equipment 0733db52-8b6f-472c-a9bd-948e9567ab5c in disposable Bow Playground through Apply loadout. Weapon projectiles correctly reopened PSP_Left, Both together, PSP_Right and 0.1 second delay. All controls/footer remain visible without overlap. Firing dropdown correctly positioned. Selected spawn point hides second selector and moves timing/PSP/marker controls up; Alternate between two restores PSP_Right selection. Draft policy changes discarded without Apply to this player. Closed panel and Ctrl+Z restored original ZIP Bow QuickShot Arrow Proof 14a147e6-be41-45e2-adda-67b9d00abb77, confirmed in Starting Equipment and bow camera preview. No equipment/assembly or level save during check. Evidence BUILD/projectile-ux-two-spawn-both.png, projectile-ux-two-spawn-single.png, projectile-ux-two-spawn-alternate.png, projectile-ux-two-spawn-restored.png.
This resolves the previously pending two-spawn visual check. Three owner-directed local implementation stages now have local build, persistence/native UI evidence, and saved-release runtime timing evidence. General multi-event animation timeline and independent exact-commit release review remain outside this completed local pass. No commit, push, merge or release-gate closure.

## First-class Hitscan continuation — 8 October 2026

The owner-requested default raycast path is now a first-class weapon fire mode,
not a projectile preset. Weapon firing exposes Physical projectile or Hitscan /
instant ray. Hitscan shows Range and Damage instead of projectile search/create
controls, while reusing the same PSP choices, multi-PSP policy and visual
animation fire marker. Equipment schema v3 persists the mode/range/damage and
retains v1/v2 physical bindings. Runtime resolves Hitscan without a projectile
asset, reuses muzzle-cover/camera convergence and the shared nearest-contact /
attributed-damage seam, and shows a short centre hit confirmation on Character
contact. Static world contacts work without health. Beam and pellet spread
remain later bounded increments. Shared surface-aware impact presentation is now
implemented in the 2026-10-08 impact-surface checkpoint; governed audio cue
binding remains follow-on work.

Release EngineBridge/Runtime compiled; alternate-output Studio linked at
BUILD/hitscan-studio/RenegadeStudio.exe because the owner's normal Studio process
held the default executable open. EquipmentAsset, LaunchSocket and
RuntimeProjectileSession focused executables PASS; git diff --check PASS.
Native owner visual/gameplay acceptance and package/Jolt-only proof remain open.
