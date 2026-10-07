# P3 Authored Projectiles gate

One bounded gate covers four checks requested by the owner. Health is not a
prerequisite; this work makes no claim that Player/NPC gameplay health exists.

| Check | Authored behavior | Current evidence |
|---|---|---|
| Animation release | Equipment binding saves seconds from firing/release animation start; accepted shots queue once, pause freezes release, changed clips cancel pending release. | CPU queue tests pass. Native saved 350 ms marker observed pending at 12 ms and launch at 359 ms. |
| Multiple PSPs | First only, alternate first/second on accepted actions, or both together. One accepted shotgun action consumes one shell. | Native Left then Right pass; Both launches two per shell; dry fire launches nothing. |
| Layered effects | Projectile v3 saves two bounded Flame/Smoke/Sparks/Tracer flight layers and an impact effect; private preview uses the same native emitter helper. | Save/reload tests and native emitter counts/cleanup pass. Square fallback particles failed visual inspection; replaced by shared procedural radial-alpha mask. Final visual/editor/package acceptance remains open. |
| Impact behavior | Disappear removes flight appearance; Stick aligns the forward tip at contact, embeds to authored depth, attaches to hit-object transform, and retires after authored lifetime. | Native disappear/stick pass; CPU native transform test proves moving-object following, paused lifetime and expiry. |

## Creator workflow

Add > Projectile creates a reusable Content/Projectiles asset. Choose a model,
set flight/model values, add up to two flight effects, and select disappear or
stick. SAVE AS NEW persists the definition. Weapon Projectiles binds the saved
asset to the first/second PSP and release time, then APPLY TO THIS PLAYER saves
and assigns an immutable equipment copy through existing commands. Searching or
changing the projectile preserves the current unsaved PSP/timing draft.

Effects currently share size/rate/lifetime/forward-offset controls in this bounded
UI. They are built-in native presets, not a general effects graph or custom
texture/flipbook importer. Projectile meshes retain governed dependency closure.
Imported mesh/texture files are not committed as engine source.

## Runtime boundary

Wicked owns native animation, particle simulation/rendering and scene contacts.
Renegade owns validated definitions, accepted-action scheduling, transient visual
instances and cleanup. Impact bursts use burst_on_create: calling Burst after the
scene update made a newly created emitter drawable before its indirect GPU buffer
existed. A DX12 debugger stack identified DrawInstancedIndirect via
EmittedParticleSystem::Draw; deferred activation fixed the reproduced crash.
A regression asserts the new burst remains inactive until native update.

Diagnostics keep normal 250 ms sampling, but sample queued/released transitions
promptly so short markers remain observable. Screens clear queued releases,
retained appearances and effects. Restart/loadout reset clears sequence state.
Stick follows the object transform, not a specific animated skeletal limb.
This does not implement shotgun pellet spread, hitscan/beam authoring, grenade
explosions, surface-specific decals/audio or Jolt-only collision acceptance.

## Windows validation, 2026-10-07

Release build, CL=/MP2:

```
cmake --build BUILD/renegade --config Release --target RenegadeRuntime RenegadeStudio RenegadeRuntimeProjectileSessionTests -- /m:1 /p:BuildProjectReferences=false /verbosity:minimal
ctest --test-dir BUILD/renegade -C Release -R "ProjectileAsset|RuntimeProjectileSession|EquipmentAsset|LaunchSocket" --output-on-failure --timeout 30
```

Final soft-effects build PASS 107.63 s; targeted CTest 4/4 PASS 5.26 s.
Native disposable fixture under BUILD/p3-authored-projectiles uses the supplied
shotgun and Arrow model with saved named PSPs. Evidence: p3-timing-native-proof.json,
p3-both-native-proof.json, p3-disappear-native-proof.json and impact captures.
Native timing/Both/disappear scripts pass; original slow flame capture proves
flight/pause/stick but exposed square particles. After the radial-mask repair,
firing/emitter state was observed without the old crash; desktop focus changes
prevented a reliable final flight capture in that run.

Tools/ProjectilePlayground.cpp is an explicit disposable-fixture preparation tool,
never a CTest or startup path. It refuses projects without the Projectile Playground
name. Initial preparation expects a fresh copied project with saved Muzzle and
Arrow. Explicit timing/both/disappear/alternate/visual modes select saved variants;
visual creates a slow inspection copy, preserving normal projectile settings.

A full editor save/reopen, final soft-effect visual inspection, packaged gameplay
and independent exact-commit verification remain required before gate closure.
P2 PR #180 remains unmerged. Wicked source/pin is unchanged.


### Editor/package follow-up

Native cold Studio reopen shows saved PSP_Left, PSP_Right, alternate policy and
0.1-second release. Edit Copy shows Flame + Smoke, Sparks impact, Stick, 30-second
retention and 0.05-metre embedding. The radial-alpha preview now shows soft round
particles (p3-layered-preview-soft-mask.png). SAVE AS NEW produced a persisted
projectile copy; subsequent assignment/reopen and packaged gameplay still require
acceptance.

Build Game found two catalog defects: generic lp07.rasset discovery provider
caused authored equipment/projectile loaders to reject valid IDs; package-closure
refresh also tombstoned unrelated still-present creator assets, losing dependency
edges when later recovered. Loaders now accept the governed discovery provider
while retaining strict format/project/path/dependency validation. Source catalog
persistence preserves untouched present assets outside the package closure;
returned package state remains restricted to reachable content. Regression tests
prove both behavior and package exclusion. Build PASS 28.08 s; Build project CTest
2/2 PASS 0.59 s. Provider compatibility build PASS 45.23 s; targeted 4/4 PASS 1.79 s.

The disposable fixture's legacy imported arrow referenced three generated texture
files that existed only as GLB-embedded image data. They were extracted using
Wicked's exact byte hashes into the expected fixture paths. This repairs this
fixture; it does not claim a general embedded-resource dependency/importer repair.

### Runtime registry package repair

Native Build Game completed in 33 s, including DX12/isolation smoke. Manual
packaged firing then correctly failed acceptance: Runtime reported the missing
GameData/AssetRegistry.renegade-assets and withheld authored equipment. Build
workflow now passes canonical reachable registry data to staging. The generated
registry is a governed package-manifest file with hashes; staging validates its
project identity/canonical bytes and tamper detection. Release bridge/Runtime/
Studio/BuildStage build PASS 36.63 s; BuildStage CTest 1/1 PASS 0.50 s.

The rebuilt package then passed authored gameplay acceptance. Runtime loaded the
saved primary equipment, queued and released the authored shot, fired PSP_Left
then PSP_Right across two accepted actions, produced 2 impacts, observed up to 3
projectile effect emitters, and retained 2 stuck arrows. Loaded shells reached 0
after the two shots. Evidence is BUILD/p3-package-native-proof.json,
BUILD/p3-package-acceptance.json and BUILD/p3-package-impact.png. This closes the
package-gameplay evidence gap for this bounded gate; owner visual acceptance and
independent exact-commit/CI review remain before final gate closure.
