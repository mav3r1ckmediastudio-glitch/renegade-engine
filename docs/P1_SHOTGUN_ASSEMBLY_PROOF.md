# P1 shotgun assembly inspection checkpoint

Date: 4 October 2026. Branch: feature/p1-first-person-arms-rig.
Scope: isolated native DX12 asset inspection, not production Player integration.
P1 remains IN PROGRESS; owner acceptance of this assembly is outstanding.

## Result

The owner-supplied SawedOffShotgun pack contains separate skinned first-person
arms and weapon rigs. Four source FBXs convert through the pinned native Wicked
converter, render, and evaluate their existing clips without animation recreation:
arms Idle (161 bones, 7 seconds), weapon Idle (7 bones, 0.666667 seconds), and
matching arms/weapon Reload clips (3 seconds each). Source channel counts are
483 for the arms and 21 for the weapon.

Tests/FirstPersonAssemblyGraphicsProof.cpp and its manual Windows target inspect
these files without adding third-party assets to Git or changing Studio, Runtime,
Player ownership, production import services, or the upstream pin.

## Texture evidence and explicit diagnostic relink

The supplied weapon and shell textures ARE present. Separate owner-provided
Manny textures were also found locally and copied into the isolated input tree.
The FBXs refer to stale export-machine paths and different texture filenames.
Normal governed PrepareModel correctly refuses those unresolved dependencies.
The manual fixture reports that refusal, then uses the native converter with
an explicit, fixture-only texture mapping. It does not prove governed import,
source retention/reimport, a texture-relink UI, or standalone game packaging.

| FBX material | Supplied files used |
| --- | --- |
| MI_Manny_01 | T_Manny_01_D.PNG and T_Manny_01_N.PNG |
| MI_Manny_02 | T_Manny_02_D.PNG and T_Manny_02_N.PNG |
| M_Weapon | T_Sawed_Off_Shotgun_BaseColor.PNG, Normal.PNG, OcclusionRoughnessMetallic.PNG |
| M_Shell | T_Shell_Red_BaseColor.PNG, Normal.PNG, OcclusionRoughnessMetallic.PNG |

Weapon/shell ORM channels match the pinned metallic-roughness surface map:
R occlusion, G roughness, B metallic. The proof enables primary occlusion.
Manny's other Unreal material masks are not interpreted; arms use albedo/normal
with a fixed roughness. This is not complete Unreal material reproduction.

## Assembly and camera limits

Arms and weapon remain separate skeletons. The earlier weapon_r/Handle-to-hand
placement and 25 mm lateral nudge were rejected by the owner: stock on the back
of the hand and reload shells below the palm. Those calibrations are superseded.

Read-only inspection of the original Unreal BP_DemoCharacter package identified
SKM_Weapon_GEN_VARIABLE (export serial range 369438..370399), whose relative
location is (-3.466970,-27.336276,4.505738) cm and rotation is
(Pitch 6.552304,Yaw -182.929938,Roll -10.254553) degrees. Its construction node
sets AttachToName=ik_hand_gun. The diagnostic now uses this parent and exact
converted local transform: translation (-0.03466970,0.27336276,-0.04505738) m,
quaternion (-0.059182247,0.087738050,0.994176416,-0.020316230).
The exported FBX retains Z-up; UE to this imported basis is (x,-y,-z).
No individual mesh, shell track or hand animation was nudged.
The skeleton's GripPoint socket is on middle_01_r; it is not the demo weapon
attachment. Finding a socket name alone does not justify selecting it.

First-person inspection uses a fixed camera in the imported coordinate system:
eye (0, 0.08, -1.65), target (0, -1, -1.65), up (0, 0, -1), FOV 80 degrees.
Production axis normalization, camera-relative placement and Player View Rig
integration are still required. No second Player/controller exists.

Each paired action samples times 0, 0.6, 1.2, 2.0 and 2.9 seconds and captures
both overview and first-person views. Idle and Reload assemblies serialize to
WISCENE, reopen, evaluate at 1.2 seconds and render again. This demonstrates
native assembly serialization, not the governed reusable weapon workflow.

The left hand performs existing reload choreography and the weapon opens and
moves shells. Loose-shell attachment/visibility timing, final grip fit and
authored gameplay/ammo events remain unverified. The exported T3D montages do
not provide a complete demo Blueprint attachment/event setup.

## Reproduction and evidence

Prepare an isolated pack folder with Animations/, Animations/Weapon/,
Meshes/Textures/ and Textures/Manny/ containing the files above. Do not modify
the originals or place licensed source assets in the repository.

~~~text
cmake -S . -B BUILD/renegade
MSBuild BUILD/renegade/Tests/RenegadeFirstPersonAssemblyGraphicsProof.vcxproj /m:2 /nologo /verbosity:quiet /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
~~~

Use CL=/MP4. Run from BUILD/renegade/Tests/Release:

~~~text
RenegadeFirstPersonAssemblyGraphicsProof.exe ../../p1-shotgun-proof/input ../../p1-shotgun-proof/captures
RenegadeFirstPersonAssemblyGraphicsProof.exe ../../p1-shotgun-proof/input ../../p1-shotgun-proof/captures --view
~~~

Configure and Release build exit 0; textured capture/save/reopen proof exits 0.
Existing MSB8029 warnings remain. Captures idle-fp-0.000000.png and
reload-fp-1.200000.png were visually inspected: arms extend from below, shotgun
points forward, textures are visible, and reload opens the weapon. Earlier
neutral-material overview/reopen captures were also inspected.

Ignored evidence is under BUILD/renegade/p1-shotgun-proof: input copies,
captures, textured-proof.log and viewer.log. Build log:
BUILD/renegade/p1-shotgun-build.log. The interactive view starts paused;
R restarts both reload clips, Space pauses/resumes, Escape closes it.
Interactive controls require owner acceptance; automated capture exit 0 alone
does not establish them.

Next bounded task: verify the preview with the owner, resolve the exact authored
weapon/loose-shell attachment and timing, then design the minimal governed
assembly import/relink/save workflow. Preserve the accepted proxy Player project
and exported Runtime; original withdrawn BareArms_SK/GGMAX arms remain unused.

Native interactive window also visually inspected after correcting its initial
64-pixel capture-window canvas: the live view now renders sharply at the larger
window size with the textured arms and shotgun. No mouse/keyboard automation
was performed; R/Space/Escape controls remain for owner verification.

Final Release executable SHA-256:
8C92B649D9279B5853DBD76B2D3A01C6B1B0AC1AB6A639FC3BC488827DCF8282.
Built before committing these source changes; no exact-commit owner acceptance
or completed P1 gate is inferred.

## Owner animation feedback and lateral grip adjustment

Owner reports animations work very well, but weapon/shells sit to the right.
The screenshot confirms the provisional attachment does not fit the palms.
Adjusted the entire weapon root by +0.025 metres in imported X before deriving
its local attachment; supplied camera right is -X, so this moves it left.
Weapon mesh and existing animated shell bones move together; no animation,
hand, camera, or individual shell-track edit. This is still provisional fixture
calibration; matching action durations alone does not establish authored fit.

Release rebuild and full textured capture/save/reopen proof exit0.
Idle and Reload at1.2s/2.0s visually inspected: lateral offset is reduced.
Reopened preview retains R/Space/Escape. Owner confirmation of corrected
alignment remains pending; animation playback feedback does not close P1.

## Authored demo attachment correction

Release build and authored-proof.log capture/save/reopen exit 0. Idle, reload
1.2s/2.0s and reopened native live window visually inspected. Contact appears
improved; exact owner grip/reload acceptance remains pending. Original Unreal
projects were read only. No full original-editor parity or production acceptance
is claimed. Earlier executable hashes describe superseded previews.
Current Release proof SHA-256:
A81A3421BEBD20CD4F52E74B8F46736153B7ECA85A84098E252868A431641E20.
