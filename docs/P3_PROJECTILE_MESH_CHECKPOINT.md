# P3 projectile model checkpoint — 2026-10-06

## Implemented increment

Add → Projectile opens project-level authoring without selecting a Player Start.
Save As New writes registered Content/Projectiles/*.rprojectile assets. Appearance
uses an imported .rasset model, uniform scale and XYZ rotation. Flight presets,
speed, gravity and lifetime remain available. Weapon Projectiles assigns the
saved projectile to the shotgun PrimaryUse action through an equipment copy.
Import Mesh reuses the existing model preview/importer, defaults its destination
to Content/Projectiles/Models, then returns to the draft with the imported model
selected; cancelling the importer restores the draft. Cancelling the native file
chooser leaves the draft visible.

Projectile schema v2 records optional mesh identity and appearance settings.
Schema v1 remains readable with no mesh and default appearance. Required registry
edges and reusable dependency discovery include the model and its texture closure;
Test Level snapshots copy the same inputs. Runtime prepares models once per
session, creates transient render instances per accepted shot, follows simulation
position/velocity, excludes visual instances from projectile queries, and removes
the complete instance hierarchy at impact, expiry or reset. Missing prepared mesh
appearance produces an explicit diagnostic error rather than success.

## Evidence

Disposable BUILD/p3-shotgun-ui-project uses the owner's supplied low-poly-arrow-v20.zip.
The FBX referenced an unavailable external texture path. Blender 4.3 converted it
to GLB with supplied textures, including DirectX normal green-channel inversion.
Original input remains untouched; the asset is not redistributed in source control.
Native model importer preview, Add → Projectile creation, Weapon Projectiles
assignment, File → Save and standalone cold load were exercised.

Model ID: 58cf6cb9-da89-4370-8a04-71d861aef990.
Arrow projectile ID: b8d9e723-9e6b-40d3-970e-c3bb631a0e80.
Saved equipment copy: da7bdfaf-d2aa-4700-b71d-4a966ca2b0b8.

Release bridge, Runtime, Studio and targeted tests built successfully. Targeted
ctest regex Projectile|EquipmentAsset|EquipmentActionState|PlayerViewRig|TestLevelSnapshot
passed 10/10 in 2.50 seconds after the final importer-return rebuild (102.56s). Native
standalone arrow proof passed pause/freeze, live instance, expiry retirement and
reset (10.11s). Ground-contact proof passed accepted firing, pause, dry fire,
reload and reset (11.22s). A paused flight screenshot shows a small arrow in the
fixture; it is distant and not a substitute for the planned inspection preview.
Local evidence: BUILD/p3-arrow-visual-events.json, p3-arrow-flight-paused.png,
p3-live-native-events.json, p3-mesh-final-build.log. Earlier native Test Level
launch exercised the model snapshot closure. Build Game firing parity remains open.

Build uses CL=/MP4, cmake --build BUILD/renegade --config Release with targets
RenegadeRuntime RenegadeStudio RenegadeRuntimeProjectileSessionTests and MSBuild
/m:2 /p:BuildProjectReferences=false /verbosity:minimal. Test command:
ctest --test-dir BUILD/renegade -C Release -R
'Projectile|EquipmentAsset|EquipmentActionState|PlayerViewRig|TestLevelSnapshot'
--output-on-failure. Source standalone uses --project <descriptor> dx12.

## Explicit limits and next steps

This increment supplies model appearance and a basic editor, not the complete
[P3 projectile editor design](P3_PROJECTILE_EDITOR_DESIGN.md). Launch still uses
the camera-eye policy. Dedicated rotatable projectile preview, weapon/palm sockets,
launch animation timing, shotgun pellet spread, first-class Hitscan/Beam controls,
impact sticking, authorable damage UI, particles/trails and material impact profiles
remain open. CPU Scene query coverage does not include Jolt-only bodies. Player/NPC
health is not a prerequisite. P3 is not closed; independent exact-commit verification
is still required. P2 PR #180 must not be merged automatically.

Final rebuilt native editor check: Cancel Model Import restored the projectile
window; importing ArrowReturn into the default Content/Projectiles/Models returned
to the original draft with ArrowReturn selected and flight values retained.
Screenshot: BUILD/p3-projectile-import-return.png. The added model is disposable;
the saved shotgun remains assigned to the original Arrow projectile above.
