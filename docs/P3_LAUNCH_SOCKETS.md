# P3 launch sockets - 2026-10-06

Status: implementation candidate on feature/p3-projectile-impact. No P3 gate closure;
independent exact-commit review remains required. P2 PR #180 remains unmerged.

## Creator workflow

Player Start > Starting Equipment > Weapon Projectiles > Edit Launch Sockets opens
the assigned weapon's existing first-person assembly. Choose Primary weapon,
Off-hand weapon or Arms / palm, then a named bone/marker or the single model root.
Unique bone names are readable in the list; the selected full path is a tooltip.
USE FIRESPOT accepts only one unambiguous imported marker. Otherwise PLACE ON MODEL
arms one surface click. Offsets are parent-local metres. Pitch/yaw/roll are
degrees; the orange arrow shows local +Z launch direction. Left-drag orbits,
right-drag pans, wheel zooms, Fit resets inspection only.

APPLY TO ASSEMBLY changes the existing undoable assembly draft. SAVE CHANGES in
the assembly editor persists its native scene, recipe and managed registry
through the existing journal. Cancel discards the socket draft. After closing
the assembly, reopen Weapon Projectiles, select a saved projectile and Fire from
socket, APPLY TO THIS PLAYER, then SAVE LEVEL. The empty binding is explicitly
labelled Camera aim legacy and preserves existing equipment behaviour.

The inspection arrow and stripped simulation components belong only to the
private preview scene. They never become weapon geometry or source-asset edits.
Sockets are non-rendering native transforms with owned metadata attached before
assembly merging, preserving authored model/bone paths through entity remapping.
Recipes accept an optional nested launch_sockets schema 1; equipment v2 bindings
accept an optional launch_socket name. Existing recipes and bindings remain valid.
Names are unique within an assembly and missing assignments fail validation.

## Runtime boundary

After native scene animation/hierarchy evaluation, accepted primary equipment
actions read the named socket under the player's view rig. Camera aim chooses
the intended nearest target; the projectile originates at the muzzle and
converges toward that target. An eye-to-muzzle obstruction clips origin to just
before cover so the normal shared simulation records contact. Blocked queries,
missing/ambiguous poses and backwards sockets report an error and refuse launch.
Diagnostics expose projectile_launch_socket and projectile_launch_x/y/z.

This remains point-projectile Scene query coverage. It does not establish
Jolt-only coverage. Invalid runtime socket rejection follows an already accepted
weapon action, so ammunition is already consumed. Authoring validation catches
missing saved parents/names; orientation should be checked before use.

## Validation

Release bridge build passed (188.96s initial build); Runtime/Studio and selected
tests build passed (72.51s). Final preview rebuild passed 13.40s; modal fixes
passed 15.64s and 15.57s; binding layout rebuild passed 15.56s.
Commands, with CL=/MP4:

```
cmake --build BUILD/renegade --config Release --target RenegadeEngineBridge -- /m:2 /p:BuildProjectReferences=false /verbosity:minimal
cmake --build BUILD/renegade --config Release --target RenegadeRuntime RenegadeStudio RenegadeLaunchSocketTests RenegadeFirstPersonAssemblySettingsTests RenegadeEquipmentAssetTests RenegadeRuntimeProjectileSessionTests -- /m:2 /p:BuildProjectReferences=false /verbosity:minimal
ctest --test-dir BUILD/renegade -C Release -R 'LaunchSocket|FirstPersonAssemblySettings|EquipmentAsset|RuntimeProjectileSession' --output-on-failure --timeout 30
```

Final targeted tests with Studio closed: 4/4 PASS, 0.61s. LaunchSocket checks
recipe roundtrip, duplicate/nonfinite rejection, native parent archive remapping,
hierarchy motion, scene save/reload, camera convergence, cover clipping,
backwards/blocked rejection, aliased call inputs and equipment binding roundtrip.
Its first full Scene.Update attempt crashed without a graphics device; the
CPU-only fixture now invokes native transform/hierarchy systems directly.
A Studio-open rerun timed out in RuntimeProjectileSession and LaunchSocket;
cold retry passed. The intermittent stall remains a gate-verification concern.

Native socket placement, direction edits, assembly Apply and Save passed.
Evidence: BUILD/p3-socket-placement.png. Disposable shotgun fixture assembly
07858b1d-4b5a-4be9-90ab-c9dd5629761c saved Muzzle at
[0.013476461,-0.246668816,-0.062851310], pitch 90 degrees, primary model root.
Cold Studio reopen retained the same values and direction, with no world overlays
on the private preview. Evidence: BUILD/p3-socket-cold-reopen.png. Saved Muzzle is
listed in Fire from and immutable equipment assignment passed:
BUILD/p3-socket-binding.png. Final modal restore rebuild passed 78.50s.
Final build native modal test: socket Cancel restores the assembly once; closing
the assembly remains closed. Final saved player equipment:
b8e8ca55-55f3-49f9-bde7-2ad27a758562. File > Save persisted ArmsPlayground;
cold standalone --project BUILD/p3-shotgun-ui-project/ProjectileTest.renegade dx12
loaded that assignment. Owned Runtime diagnostic PID was verified by the helper.
BUILD/p3-socket-native.py PASS: accepted named-socket launches, actual ground
impact, pause freeze, dry fire, reload and reset. Full launch/check process
20.25s; diagnostic first muzzle origin [0.067675,3.238707,0.635946].
Evidence: BUILD/p3-socket-native.log, p3-socket-native-events.json,
p3-socket-shot-impact.png and p3-socket-paused-impact.png. Native captures show
the shotgun, ground contact X and pause feedback; PrintWindow colour quality is
not a renderer-parity claim. Final cold socket image: p3-socket-final-reopen.png.
This is standalone source-project proof, not Build Game/package closure.

## Deliberate remaining scope

One selected socket launches one record per accepted primary shot; barrel
alternation, pellet patterns and simultaneous sockets remain open. Generic
off-hand Cast launch dispatch remains open. Precise animation release markers,
animated socket-preview action selection, spatial gizmos, direct model-importer
socket authoring, compact responsive layout, effects, beams, thrown physics,
hitscan and material/decal impact profiles remain separate increments.
No usable Player/NPC health, damage-test success or package proof is claimed.
No Wicked source or pinned commit change.
