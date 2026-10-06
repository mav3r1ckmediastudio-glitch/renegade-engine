# P3 live shotgun projectiles - 2026-10-06

P3 remains open. Native Test Level and standalone source Runtime now fire the
assigned Bullet from the shotgun. Player/NPC health is not a prerequisite.

## Behaviour and authoring
Select Player Start -> Starting Equipment -> Weapon Projectiles, choose Bullet
for PrimaryUse, Apply to this Player, Save Level, then Test Level. The editor
creates an immutable equipment copy; shared equipment remains unchanged.
Definitions resolve once on equipment load. Accepted firearm animation starts
gate launch after ammunition, equipped-state and cooldown checks. Dry fire,
pause, holster and rejected actions do not launch. R reloads; F8 resets the
legacy shotgun fixture whose former R reset binding is migrated in memory.

Each accepted shot creates one transient point projectile. Initial origin is
the authoritative camera eye and direction is camera forward after player
camera update. This prevents muzzle offsets bypassing nearby cover. Authored
muzzle sockets and convergence are pending; this is not a pellet spread.
Owner/source/faction attribution survives impacts. Native Scene ray/overlap
queries exclude the shooter hierarchy and select nearest cover. Existing
Character damage routing remains an integration seam without adding health.

Flight lines/points and bounded two-second orange world-contact markers provide
basic feedback. Markers project actual contacts and reject nearer occluding
cover; they do not indicate damage. Pause freezes flight and feedback. Reset,
scene replacement and shutdown clear transient state. Screen transitions clear
the aim/contact overlay. Sky shots continue until the authored lifetime expires.

Test Level snapshots now copy and validate projectile dependencies before
validating equipment. Runtime diagnostics expose a separate projectiles group
to avoid the existing per-group field limit.

## Validation
Windows x64/DX12 Release, pinned Wicked unchanged.
Build commands (CL=/MP4):
`cmake --build BUILD/renegade --config Release --target RenegadeEngineBridge -- /m:2 /p:BuildProjectReferences=false /verbosity:minimal`
then Runtime, Studio, RenegadePlayerViewRigTests, RenegadeProjectileWorldTests,
RenegadeRuntimeProjectileSessionTests and RenegadeProjectileAssetTests with the
same options. Builds PASS. Fresh Runtime copied into Studio/Release/Runtime so
native Test Level uses the current executable.
`ctest --test-dir BUILD/renegade -C Release -R "Projectile|EquipmentAsset|EquipmentActionState|PlayerViewRig|TestLevelSnapshot" --output-on-failure`:
10/10 PASS (2.13s before final overlay cleanup).
Tests cover accepted-shot pulse, cached dispatch, pause/holster/deduplication,
real native collider contact, bounded elapsed subdivision, marker expiry/reset
and snapshot equipment/projectile dependency closure.

Native disposable BUILD/p3-shotgun-ui-project: saved assignment/reopen previously
verified; actual Test Level PASS for two accepted shots, ground impacts, pause
and dry fire (4.74s). Standalone source ProjectileTest.renegade PASS (11.15s):
two ground impacts, paused feedback, dry fire, reload, third shot and F8 reset.
Orange contact x visually inspected in BUILD/p3-live-shot-impact.png;
paused capture BUILD/p3-live-paused-impact.png. Counter evidence:
BUILD/p3-live-native-events.json. Native input proof requires focus and a cursor
inside the Runtime window; background windows pause as designed.

## Remaining scope
CPU scene meshes/colliders covered; Jolt-only bodies are not yet covered.
Authorable muzzle sockets, pellet spread, projectile visuals, sound/effects,
decals/lights and packaged firing proof remain open. Standalone source proof is
not packaged-game parity. Generic Cast/AlternateUse/offhand launch acceptance
is not claimed. Independent exact-commit verification is required before gate
closure. Do not merge dependent P2 PR #180 automatically.
