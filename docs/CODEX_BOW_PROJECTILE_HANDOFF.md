# Renegade Engine — Bow / Projectile Integration Handoff for Codex

**Date:** 2026-10-07
**Repository:** `<USER_HOME>\source\repos\renegade-engine`
**GitHub:** `mav3r1ckmediastudio-glitch/renegade-engine`
**Current branch:** `feature/p3-projectile-impact`
**Branch HEAD at start of this work:** `cf04bd3`

## Goal

Replace the current “arrow fired from a sawn-off shotgun” proof with a real animated first-person bow using Renegade’s existing systems.

Do not build a bow-specific hack.

The proof should use:
- first-person arms
- animated LongBow
- First Person Assembly
- existing authored arrow projectile
- Projectile Spawn Point (PSP)
- release timing
- existing projectile flight / impact / stick behaviour

## Disposable working project

Use only:

`<USER_HOME>\source\repos\renegade-engine\BUILD\bow-projectile-playground`

Project:

`<USER_HOME>\source\repos\renegade-engine\BUILD\bow-projectile-playground\Bow-Playground.renegade`

Do not use the original P3 playground while iterating.

## Current P3 state

P3 authored projectiles are already substantially complete:

- physical projectile authoring
- PSPs
- release timing
- multiple PSP policies
- flight / impact FX
- Stick / Disappear impact modes
- package registry staging
- packaged acceptance proof

The existing arrow projectile already works.

Do not redesign P3 for this bow task.

## Clean source assets

Prepared clean source lives under:

`<USER_HOME>\source\repos\renegade-engine\BUILD\bow-projectile-playground\BowSourceClean`

### Arms

`BowSourceClean\Arms`

Contains:
- `SK_FPSArms.fbx`
- `Idle.fbx`
- `ADSIdle.fbx`
- `Draw.fbx`
- `Release.fbx`
- `FPArms_Male1_DIF.png`
- `FPArms_Male1_NRM.png`

Verified:
- arms rig = 69 bones
- mesh and animations match

### Bow

`BowSourceClean\Bow`

Contains:
- `SKM_LongBow.fbx`
- `Idle.fbx`
- `ADSIdle.fbx`
- `Draw.fbx`
- `Release.fbx`
- `YVR_WP3D_LongBow_NRM.png`

Verified:
- bow rig = 10 bones
- mesh and animations match
- bow rig contains `arrow_joint`
- `arrow_joint` is the preferred future PSP anchor

## Texture cleanup already done

Original exported FBXs contained hard-coded creator paths such as:

`C:\Users\Jonathan\Desktop\WEP_CH3D_FPArms_Male_LPR_EXP\Texture Maps\...`

Renegade correctly rejected those because dependencies lived outside the selected source tree.

The FBXs were round-tripped through Blender and rewritten to use local/self-contained texture references.

Do not redo this unless necessary.

## Arms asset already imported successfully

Governed asset:

`<USER_HOME>\source\repos\renegade-engine\BUILD\bow-projectile-playground\Content\SK_FPSArms.rasset`

Role:

**Player arms**

Arms clip order:
- clip 0 = Idle
- clip 1 = ADSIdle
- clip 2 = Draw
- clip 3 = Release

Importer gameplay-action mapping:
- Idle -> Idle
- Release -> Attack
- ADSIdle -> Unassigned
- Draw -> Unassigned

This asset appears correctly in the First Person Assembly editor’s **Arms product** dropdown.

Do not reimport it unless required.

## Existing governed bow asset

The disposable project already contains:

`Content\LongBow_Combined.rasset`

This is a valid governed bow asset (~15 MB), not a stub.

It contains the correct LongBow rig and four clips:
- bow clip 0 = ADSIdle
- bow clip 1 = Draw
- bow clip 2 = Idle
- bow clip 3 = Release

It appears in the First Person Assembly editor’s **Weapon product** dropdown.

Reuse it for this proof.

## Important importer limitation

Trying to import the LongBow mesh plus external bow animation FBXs through the generic `Import Model -> Add Animation FBX` workflow fails because the external animation path expects a complete humanoid armature.

The bow is a 10-bone animated weapon rig.

Observed status message:

> External animation has no complete humanoid armature; verify its bone names and required mappings.

This does not mean the bow animations are bad.

Do not waste time forcing them through the generic external-animation importer.

Use `LongBow_Combined.rasset`.

Longer-term importer task:
- support animated non-humanoid weapon/model rigs
- support external animation clips for weapon rigs

That is not required to finish this proof.

## Correct UI path

Use the production **First Person Assembly** editor:

1. Open Bow Playground.
2. Select `Player Start` in Scene Hierarchy.
3. In Player Start inspector click **ASSEMBLY**.

This editor uses the production `FirstPersonAssemblyService`.

## Current assembly state reached

Inside First Person Assembly:

- Arms product = `Content/SK_FPSArms.rasset`
- Weapon product = `Content/LongBow_Combined.rasset`
- `LOAD PARTS` succeeded
- old shotgun pairings were correctly cleared when the arms skeleton changed
- the draft pair slots are currently blank / NONE

The existing shotgun assembly is still the active Player Start assembly outside this draft.

Do **not** overwrite it.

Use **SAVE AS NEW** once the bow assembly works.

## Parent bone

The imported arms skeleton was independently enumerated.

Sorted relevant entries:

1. `root > ik_foot_root > ik_foot_l`
2. `root > ik_foot_root > ik_foot_r`
3. `root > ik_foot_root`
4. `root > ik_hand_root > ik_hand_gun > ik_hand_l`
5. `root > ik_hand_root > ik_hand_gun > ik_hand_r`
6. `root > ik_hand_root > ik_hand_gun`
7. `root > ik_hand_root`

Preferred first attachment target:

`root > ik_hand_root > ik_hand_gun`

This is choice #6 in the sorted bone list after the blank selector.

The UI hierarchy labels are clipped, so select carefully.

## Minimal animation pairing target

Do not attempt the full bow state machine yet.

First prove synchronized Idle and Release.

### Idle

- arms clip 0
- weapon clip 2

### Attack / Release

- arms clip 3
- weapon clip 3

So the minimum pair set is:

- `Idle` -> arms 0 / weapon 2
- `Attack` -> arms 3 / weapon 3

Leave these blank initially:

- Reload
- Walk
- Run
- Equip

ADS / Draw / Hold are intentionally deferred until the minimal pair works.

## Transform warning

During window/focus trouble, Weapon yaw was accidentally nudged.

It reached roughly 27.9 degrees, and a later correction left it near -0.7 degrees.

Before saving anything, explicitly reset the draft weapon transforms:

- Weapon X = 0
- Weapon Y = 0
- Weapon Z = 0
- Weapon pitch = 0
- Weapon yaw = 0
- Weapon roll = 0

Also begin with view/camera transforms at 0 for this new proof.

Tune deliberately only after preview works.

## First preview target

After:

- Arms product selected
- Weapon product selected
- LOAD PARTS
- parent = `ik_hand_gun`
- Idle paired
- Attack paired
- transforms reset

Click:

`UPDATE PREVIEW`

Expected:

- FPS arms visible
- LongBow visible
- Idle pair works
- Attack / Release pair works

The bow may be offset or rotated badly initially.

That is acceptable.

Fix alignment with the assembly editor’s position / pitch / yaw / roll controls.

Do not rewrite systems just to fix asset alignment.

## Save strategy

Once the preview is usable:

Use:

`SAVE AS NEW`

Suggested name:

`LongBow FPS Proof`

Do not overwrite:

`Shotgun Full Library`

The shotgun remains the known-good reference.

## Next step after assembly works

Once a governed bow assembly exists:

1. Assign it to the disposable player/equipment flow.
2. Bind the existing authored arrow projectile.
3. Add one PSP.
4. Prefer `arrow_joint` if launch-socket authoring can target it.
5. If not, author a PSP at the visual arrow rest / nocking point and parent it to the bow rig.
6. Bind projectile release timing to Attack / Release.
7. Test Game.

Desired loop:

- LMB
- arms Release animation
- bow Release animation
- one physical arrow launches from the bow
- existing flight / impact / stick behaviour still works

## Do not expand scope yet

Do not add yet:

- full draw / hold state machine
- hold-to-charge
- visible pre-nocked arrow
- different draw strengths
- ADS system
- stamina
- quiver / inventory
- bow-specific damage
- hitscan
- advanced animation-event timeline

First prove:

**bow arms + bow weapon + release + physical arrow projectile**

## Future architecture note

Current generic importer action vocabulary is still primarily:

- Idle
- Walk
- Run
- Attack
- Reload
- Hit
- Death

A polished bow will eventually need more generic first-person equipment actions/events such as:

- DrawStart
- DrawHold
- Release
- AimIn
- AimLoop
- AimOut
- Equip
- Unequip

Do not hard-code bow-only actions.

Prefer the planned generic animation-event/action-slot mechanism.

That same system should later support:
- projectile release
- muzzle flash
- shell ejection
- sounds
- melee windows
- footsteps
- spell release
- combo windows

## Repo cleanliness

Before this work, branch status was clean except existing untracked:

- `Tools/__pycache__/`
- `log.txt`

Temporary edits to:

`Tests\FirstPersonAssemblyGraphicsProof.cpp`

were fully reverted using `git restore`.

Do not assume any temporary helper patch remains.

## Failed / unproductive paths to avoid

### Unreal headless skeletal export
Caused shader/rendering friction. Not needed now.

### Mixing second-pack bow animations with first-pack mesh
Do not do this.

Those bow rigs differ:
- one bow rig = 10 bones
- the other = 5 bones

### Generic external animation import for bow
Current route expects a humanoid armature.

Use `LongBow_Combined.rasset`.

### Piggybacking on FirstPersonAssemblyGraphicsProof
That test file contains older stale proof code with helper-visibility compile problems.

Do not waste time fixing that merely to create the bow assembly.

Use the production assembly editor/service.

## Useful source files

Assembly editor:

`Studio\src\FirstPersonAssemblyEditor.cpp`

Assembly service:

`EngineBridge\include\renegade\bridge\FirstPersonAssemblyService.h`

`EngineBridge\src\FirstPersonAssemblyService.cpp`

Bone enumeration / grips:

`EngineBridge\include\renegade\bridge\PlayerViewGripService.h`

`EngineBridge\src\PlayerViewGripService.cpp`

Projectile editor:

`Studio\src\PlayerProjectileEditor.cpp`

P3 projectile gate:

`docs\P3_AUTHORED_PROJECTILES_GATE.md`

General handoff:

`HANDOFF.md`

## Recommended Codex sequence

### BOW-1 — Minimal assembly

- Open disposable Bow Playground
- select Player Start
- open ASSEMBLY
- Arms = `Content/SK_FPSArms.rasset`
- Weapon = `Content/LongBow_Combined.rasset`
- LOAD PARTS
- Parent = `ik_hand_gun`
- Idle = arms 0 / weapon 2
- Attack = arms 3 / weapon 3
- zero weapon/view transforms
- UPDATE PREVIEW
- tune weapon transform only if necessary
- verify Idle
- verify Attack / Release
- SAVE AS NEW

### BOW-2 — Projectile binding

- assign new bow assembly to disposable player/equipment
- bind existing arrow projectile
- add one PSP
- use `arrow_joint` if possible
- bind release timing

### BOW-3 — Runtime proof

- Test Game
- fire one arrow
- verify synchronized release
- verify one projectile
- verify correct launch origin
- verify correct forward direction
- verify impact
- verify stick behaviour

### BOW-4 — UX follow-up

Only after BOW-1 to BOW-3 pass:

- Draw
- DrawHold
- release event marker
- richer generic first-person equipment action/event authoring

## Success criteria

This task is complete when:

1. A new governed bow first-person assembly exists.
2. Existing shotgun assembly remains untouched.
3. Arms + bow Idle pair works.
4. Arms + bow Release pair works.
5. Player fires the existing physical arrow projectile from the bow.
6. Projectile origin visually matches the bow.
7. Arrow impact/stick still works.
8. No permanent bow-specific engine hack was added.
9. Branch remains clean apart from intentional approved source changes.


## 2026-10-07 correction: supplied ZIP is authoritative

User explicitly requires First Person Bow & Arrow Animation Set.zip from Downloads. Earlier SK_FPSArms / SKM_LongBow instructions above describe a DIFFERENT pack and must not guide further assembly work. Human-skin arms / LongBow repair work is abandoned.

Correct ZIP assets: SK_Mannequin_Arms (67 bones), SkeletalMesh_SampleBow (4 bones), Bow_Idle + Bow_QuickShot arms clips, EquipedBow_Idle + EquipedBowBow_QuickShot weapon clips. SHA-256 comparison verified the extracted six .uasset files match the supplied ZIP byte-for-byte.

Regular Unreal 5.5 editor export succeeded (not headless commandlet). FBXs and paired derivative FBXs are inside BUILD/bow-projectile-playground/ZipPackSource. Bone curves preserved; non-bone armature-object curves removed when combining. No humanoid retarget used for these embedded clips.

Governed imported products in disposable playground:
- ZIP Pack Arms: 9cd512b3-8e2b-4fcf-9f3a-a68f9ab5a805
- ZIP Pack Bow: c32963b5-21d8-41f5-8182-7248494a0d1a
Both have clip 0 Idle and clip 1 QuickShot/Attack. Native candidate and reopened arm poses have identical measured skinned bounds. Bow import and reopen succeeded. Character/animated import route is required even for the non-humanoid bow; static model route rejects animated content.

No correct-pack assembly has yet been saved or assigned. Attachment/camera transforms, visual verification and runtime arrow proof remain pending. Do not claim BOW-1 or BOW-2 passed. Shotgun assembly has not been overwritten. All new bow assets remain in disposable Bow Playground. Temporary diagnostic code lives in Windows Temp and generated BUILD project only; old FirstPersonAssemblyGraphicsProof source was not edited by this session.

Correct-pack authored layout extracted from Unreal Blueprint component templates and sockets:
- BowEquipSocket belongs to hand_l, not ik_hand_gun. Unreal socket location in cm: (9.331025, -4.030410, -5.148481); Rotator pitch/yaw/roll: (1.546869, -93.202592, -19.119948). Coordinate/bone-basis conversion is still required before assigning these directly in Renegade.
- BP_Bow mesh authored scale is 0.73. Both relative translation and rotation are zero.
- FirstPersonMesh Blueprint relative location in cm (-5,-1,-168), yaw -90. Camera template relative location (-10,0,60); attachment hierarchy still needs verification before deriving camera-space transform.
- ArrowSocket belongs to hand_r. Bow ArrowHand_Socket belongs to bow_string_mid; socket lists include arrow_nock but the imported bow skin bone collection has four bones.
Raw layout.json and sockets.json reside under the disposable project ZipPackSource. Do not blindly reuse old pack parenting, offsets or clip indices.

## Correct-pack assembly saved, 2026-10-07 evening

New ready arms product: ZIP Pack Arms Render Ready, eebe03fc-2c44-41dd-8d2a-463489f8701f. The Blender-derived FBX conversion produced a skinned mesh object rotated 90 degrees relative to its armature. The private import candidate was corrected by resetting that mesh object's local transform and attaching it directly to its armature before governed commit. Skeleton/weights and embedded clips were preserved. Do not apply this mesh correction to ZIP Pack Bow: its authored mesh transform is required for the upright bow. This asset-level correction is stored in the native payload; reimporting the derivative FBX without the correction would reintroduce the arms mesh-space defect. No engine/importer source change has been made.

New assembly: ZIP Bow FPS Proof, df105ce1-8c55-4836-b083-cf0508650c5a, using ready arms above and ZIP Pack Bow c32963b5-21d8-41f5-8182-7248494a0d1a. Idle 0/0; Attack 1/1. hand_l authored socket transform converted using matching source/native posed bone matrices. Weapon scale .73. Pack's camera-space arms placement converted into view-root position/rotation. resolved-transforms.json in ZipPackSource documents calculation.

Native assembly rendered in a new disposable diagnostic executable using production FirstPersonAssemblyService and Wicked RenderPath3D (not the old GraphicsProof). Idle and Attack PNGs saved in ZipPackSource. Visual correction shows mannequin left hand holding upright bow and right hand at string in Idle. New asset saved/reopened. Across five timestamps each for Idle and Attack, maximum reopened skinned world-vertex difference was 3.57628e-7 metres (most samples exactly zero).

PlayerStart in Content/Scenes/ArmsPlayground.wiscene now assigned to df105ce1-8c55-4836-b083-cf0508650c5a using SetPlayerControllerSettingsCommand; scene save/reopen verified assignment. Original level backed up alongside as ArmsPlayground.wiscene.before-zip-bow. Shotgun assembly itself untouched. Studio relaunched for final editor/TestGame visual check; that check and independent user approval are not yet recorded. Projectile binding/PSP/runtime arrow proof still pending. No gate completion claimed.


## Runtime bow proof completed locally — latest authoritative state

Supersedes the earlier instruction to preserve the bow mesh object transform. Quantitative playback comparison showed BOTH arms and bow required the mesh-object reset/armature attachment. Arms source vs native nearest-vertex error: max 6.70e-6 m, mean 2.67e-6 m at Idle/Attack 0.25 s. Bow original object orientation error: max 0.562 m; corrected max 0.0162 m, mean 0.00254 m. The earlier upright-looking original bow was a misleading perspective result and turned sideways in QuickShot. Do not restore it.

Final weapon product: ZIP Pack Bow Mesh Space Correct, 227986f8-ea2c-4d5f-bc00-61f0cdcdfd65. Final arms remain ZIP Pack Arms Render Ready, eebe03fc-2c44-41dd-8d2a-463489f8701f. Assembly remains ZIP Bow FPS Proof, df105ce1-8c55-4836-b083-cf0508650c5a. Idle 0/0, Attack 1/1. Weapon scale .73, weapon rotation quaternion (0,0,1,0), existing socket-position offset retained. The old UE socket rotation conversion was visually wrong; this is a deliberate authored grip alignment. View presentation is original view matrix post-multiplied by camera-space X rotation -20 degrees, keeping shoulders clipped while lifting hands. Final serialized settings are in ZipPackSource/final-assembly-settings.json.

After saving/reopening, all vertices at Idle and Attack times 0/.15/.30/.45/.60 were identical (max error 0). Corrected mesh-space fix is native asset payload only; reimporting paired FBX without correction reintroduces error. No engine source was changed.

One PSP: Bow_Nock, primary weapon, parent path [ZipPack_Bow_Paired.fbx,SkeletalMesh_SampleBow,bow_base,arrow_nock], local position zero, rotation degrees (27.5989,-61.4192,0), facing camera aim at release. Existing Flaming Arrow - Stick projectile 278e832d-98f7-4407-858b-c21ece530d11 reused unchanged. Release .133333 s from QuickShot start (draw peaks around .10 s then snaps). New TwoHanded equipment ZIP Bow QuickShot Arrow Proof, 14a147e6-be41-45e2-adda-67b9d00abb77, presentation = this bow assembly, PrimaryUse = Attack. Assigned as disposable PlayerStart primary equipment; off-hand empty. Save/reopen verified. Never restore original primary shotgun equipment on this disposable PlayerStart, as it overrides the bow presentation.

Proof-only capacity 1000 uses existing generic capacity setting; avoids inherited default two-shot dry fire. This is not finished bow ammunition/inventory design. Draw/hold/ADS remain deferred. Disposable level lighting reduced from directional intensity 10 / ambient .4 to 1 / .08 for readable geometry. Only disposable scene was changed; original backup remains ArmsPlayground.wiscene.before-zip-bow.

Standalone DX12 runtime: three accepted LMB clicks produced exactly three physical launches and three target impacts; Bow_Nock reported each launch; one stuck arrow record; projectile_error and projectile_visual_error empty. Third shot consumed capacity to 997, proving no two-shot cutoff. Target impact z=7.8 m. Editor level reopened from disk: PlayerStart selector and Player Camera Preview show correct bow/mannequin arms, not shotgun. Evidence: ZipPackSource/Runtime-Idle.png, Runtime-QuickShot.png, runtime-third-shot.json, runtime-impact.json. Temporary native harness in Windows Temp, generated BUILD vcxproj only. Existing tracked GraphicsProof modification was not changed by this session.

Shotgun assembly and shotgun equipment assets untouched. All bow work remains isolated in Bow Playground. This is local runtime evidence, not an independently reviewed release-gate sign-off. No gate marked complete. Remaining: independent visual review, optional proper texture/material export and production generic ammo/action authoring; no full draw/hold state machine added.

Final editor state: First Person Assembly panel OPEN, corrected arms/bow products, Idle preview visible, both Idle and Attack paired. Editor-Assembly-Idle.png saved in ZipPackSource. Runtime closed after proof to free mouse capture and leave editable Studio visible. Build command used throughout: MSBuild BUILD/renegade/Tests/CodexBowNativeInspect.vcxproj /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo; success with Temp intermediate-directory warning only on final builds. Run CodexBowNativeInspect.exe from generated Tests/Release. Runtime launch used dx12 --project disposable Bow-Playground.renegade. No deployment/commit, no alteration to existing shotgun assets.

## 2026-10-07 reference pose and grip correction — authoritative

Supersedes the upright quaternion and -20 degree presentation above. User reference requires the source-authored cant. Weapon position (0.0932869017,0.0402441397,0.0515471026), quaternion (-0.4062247276,-0.4137873054,0.5973896384,0.5536039472), scale .73. View position (-0.0100082997,-1.6706054211,0.1843224615), quaternion (0.00019648568,0.99756205,-0.0697838664,0.0001379221), equivalent original view post-multiplied by camera X -8 degrees. User confirmed this pose was better.

The thin/pinched grip was zero-weight skin collapse: actual bow has 110 zero-weight vertices. Initial repair mistakenly selected Blender default Cube (8 vertices); asset da53a195-4a68-4787-b2c1-6157adb88686 is invalid and must not be reused. A residual-weight repair also changed weighted limb animation; a4973ba0-b891-41b6-8344-0fc24a9f766e is superseded. Final derivative removes Blender default objects, adds static deform bone bow_bind_root, parents prior root bones preserving rest matrices, assigns weight 1 ONLY to the 110 zero-weight vertices. All existing nonzero weights preserved (importer normalizes them). No engine changes.

Final governed weapon: ZIP Pack Bow Grip Verified, 63994e63-eb90-4e9e-9e14-bc99516948ac. Assembly df105ce1-8c55-4836-b083-cf0508650c5a updated to this weapon; mannequin arms unchanged. Native object-transform reset/armature attachment still required. New arrow_nock parent path resolved and launch direction recalculated at Attack .133333 seconds. Serialized settings: ZipPackSource/final-assembly-settings.json.

Source-v-native nearest vertex comparison at .25 seconds now max 8.38e-7 m Idle, 7.40e-7 m Attack; mean about 2.9e-7 m. Standalone runtime restarted from saved assembly: full black grip visibly intact in Idle and QuickShot; accepted runtime click visibly launched flaming arrow. Evidence Runtime-Grip-Repaired.png and Runtime-Grip-QuickShot.png in ZipPackSource. Prior three-impact proof predates this grip correction; no new three-impact telemetry recorded this turn. Runtime left running with latest weapon. Studio previously open panel may contain stale draft; close/reopen before saving. Shotgun untouched, all assets remain disposable Bow Playground; no release gate signoff.

## Owner acceptance and next scope — 2026-10-07

Owner approved the corrected bow appearance ('wow, that looks so much better') and later reported extensive testing: 'ive tested it all alot, its all fine'. Record this as owner acceptance of the repaired bow's tested gameplay. Do not re-run its completed setup or replace it with a different pack. Wider P3 and full P6 draw/hold scope are not thereby complete.

The supplied previous-chat transcript sets the next sequence: Projectile UX Cleanup, existing preset refinement, then visual animation release-event authoring. Human arms replacement/retargeting is explicitly deferred until projectile work finishes. Current UI work and evidence are in docs/P3_PROJECTILE_UX_CLEANUP.md. Bow and shotgun assembly payloads remain unchanged by that UI work.
