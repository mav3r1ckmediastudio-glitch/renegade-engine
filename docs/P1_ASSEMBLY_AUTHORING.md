# P1 first-person assembly authoring candidate

This slice turns retained arms and weapon products into one reusable first-person
assembly. The owner-accepted shotgun attachment remains the reference. P1 is still
in progress; this is not acceptance of firing, reload gameplay, inventory or P2.

## Creator workflow

1. Import and retain the rigged parts and their matching animation clips.
   Explicit texture relinks currently have a bridge API and real-pack proof;
   their general import UI remains outstanding.
2. Select the existing Player Start in Studio and press ASSEMBLY.
3. Select the arms and weapon products, then LOAD PARTS.
4. Choose the weapon's authored parent bone. Enter its relative position in
   metres and rotation in degrees. Author view placement relative to the existing
   player camera, whose forward direction is +Z and up is +Y.
5. Choose an arms clip and weapon clip for each action. Both must be selected,
   or both NONE. The preview evaluates both native tracks on one clock; a shorter
   track holds its last pose. Valid draft edits refresh automatically after a short
   delay; UPDATE PREVIEW remains available for an explicit retry.
6. Choose a preview action, play/pause or scrub. Save is enabled after rendering.
7. SAVE CHANGES rebuilds an assigned assembly while retaining its product and recipe
   IDs, paths and Player Start assignment. Existing users of the shared asset see
   the rebuilt product on their next load.
8. To create a variant, enter a unique Copy name and SAVE AS NEW. This creates a
   separate product and assigns it through the existing Player settings command.
   Save the level to persist that assignment. Reopen the panel to recover its recipe.
9. UNDO and REDO restore draft part selection, parent, attachment/view transforms
   and clip pairs. Restoring a valid draft also refreshes the preview automatically.
   Closing discards the draft.

Selecting different parts requires LOAD PARTS. Changing parent, transforms or
pairs marks the preview stale, disables saving and schedules a refresh after
150 ms. The image retains its last valid render until the replacement is ready;
incomplete/invalid edits show an explanation and keep saving disabled. A part
registry hash change after preview
requires a fresh load/preview before saving. Player assignment remains undoable;
the assembly draft uses its own CommandService history and saved-state boundary.
Save changes rejects a stale product hash. Failed document replacement rolls back
product, recipe, projection, registry, catalogue and optional thumbnail together.
Opening the editor refreshes retained authoring assets from disk after build scans.

## Durable contract and ownership

FirstPersonAssemblyService composes private native scenes through existing
ReusableAssetService stable-ID validation. The weapon must have one transform
root; its explicit bone hierarchy path must resolve in the arms. Selected clips
retain native channel targets/keyframes and separate skeletons, with root motion
off. Action labels and an arms/weapon track marker are native animation metadata.
No guessed socket, skeleton retarget, model converter or second Player controller
is introduced.

SourceAssets/Assemblies/<name>.json records canonical version-1 settings: both
part product IDs, the exact parent path, weapon position/quaternion, camera
position/quaternion and paired native clip indices. Content/Assemblies/<name>.rasset
contains the composed, compressed WISCENE with embedded resources. Its source
format is assembly and importer is renegade.first_person.assembly. The existing
RAsset schema and model settings envelope remain version 1. Product, source
recipe, managed projection, optional rendered thumbnail, registry/provenance and
catalogue metadata cross one ProjectDocumentTransaction.

The assembly is a baked product: changing a source part does not silently alter
it. Its recipe allows explicit reconstruction while the part products remain
available. Generic FBX/glTF reimport does not rebuild assembly recipes yet.
The embedded product reopens without the original packs or retained model-source
folders. Part identities belong to authoring provenance, not an extra Runtime
skeleton dependency. Character placement template markers are removed from the
composed parts; the assembly is not classified as an NPC.

Reusable placement now retains embedded resource bytes before deleting its
temporary native archive. Native Archive data is already decoded in memory;
its compression/retention policy prevents deleted temporary files becoming
texture streaming containers. In-memory scene clones preserve resolved texture
handles on private copies. Visual testing found and rejected the earlier grey
material result before this repair.

## Verification and limits

Windows x64/DX12 Release bridge, Studio, assembly recipe tests and manual workflow
proof are the intended configuration. The real pack proof checks two armatures,
four Idle/Reload clips, ten decoded retained textures, exact recipe reopen,
invalid parent/clip/rotation rejection, camera-relative rendered frames and
Player Start assignment surviving WISCENE save/reopen. Separate-process assembly
load and a source-unavailable load exercise the embedded payload.

Manual commands (from the repository; PROJECT is the ignored fixture project):

```powershell
$env:CL='/MP4'
# MSBuild target options: /m:2 /nologo /verbosity:quiet
# /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false
& BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe PROJECT BUILD/p1assembly-final --assembly
& BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe PROJECT BUILD/p1assembly-final/cold --assembly-reopen
```

The manual proof requires the owner's licensed retained products; no third-party
assets enter source or CTest. Final local evidence and exact source checkpoint
are recorded in HANDOFF.md.

Owner accepts the native assembly preview controls, paired reload playback,
replay and scrubbing on 4 October. Named product save was confirmed on disk; the
name-field reopen regression is repaired separately.

Runtime now recognizes the explicit assembly marker and arms/weapon track roles.
Idle/Walk/Run pairs are evaluated together on one clock before the normal Wicked
scene update, with root motion disabled and shorter tracks holding their last
pose until the whole pair loops. Missing Run falls back to Walk then Idle;
missing Walk falls back to Idle without restarting the same pair. Native track
timers are paused so Wicked evaluates channels without advancing the clock twice.
Legacy single-rig variant playback and compatible crossfades remain unchanged.
Assembly movement pair changes currently switch immediately; paired crossfades
and animation layers are not implemented. Reload/Attack/Equip remain preview-only
and are suppressed in movement playback. Runtime requires an explicit Idle pair.

Real-product project loading, Test Level snapshot identity and fresh-process
isolated packaged asset loading pass through production services, including two
armatures, textured rendering, paired pose, pause and cleanup. The isolated
package fixture tests the packaged loader; it is not a completed Build Game export.

Owner reports "everything looks great" after the real Assembly Proof Test Level
camera/look check on 4 October. The supplied DX12 screenshot shows textured arms
and shotgun together in the foreground at 75 FPS. This is bounded visual acceptance.

Outstanding: general texture relink UI, a dedicated rigged-part import
classification and owner verification of the full-library and auto-refresh UI.
The owner confirms Save changes, Save as new and draft Undo/Redo work as expected
on 4 October. This bounded acceptance does not close the overall P1 gate.
Windows long-path staging and package integrity repair is implemented locally;
a 312-character stage/integrity regression and actual 35-file owner candidate
validation plus DX12 automatic Runtime smoke pass. Original deep-root Studio
export now reports BUILD COMPLETE in 23 seconds; promoted final package
passes all 35 file checks and automatic DX12 smoke/promotion. Owner reports "the build works" for actual
Studio Build Windows Game export from a short-root project copy on 4 October;
screenshot shows textured arms/shotgun in exported Runtime at 74 FPS. Original
265-character staging texture destination failed; short-root export succeeds.
Actual export launch/visual acceptance is recorded. Reload preview is not a reload
gameplay implementation. Independent exact-commit verification remains pending;
no release gate is closed by this slice.


## Complete supplied shotgun animation library

The retained creator fixture now contains 14 arms clips and four weapon clips.
All are ingested through the matching-rig path and retained import recipes.
The assembled product contains 14 semantic pairs (28 native tracks):
Idle, Reload, Walk, Run, Attack, Equip, Unequip, AimIn, AimOut, AimAttack,
JumpStart, JumpLoop, JumpLand and ReloadPartial. Native FBX sources with a generic
"Unreal Take" label receive descriptive authored names before governed commit;
those names survive retained recipe reconstruction.

Studio exposes the paired selectors on three pages: Movement / use, Aim / jump,
and Land / partial. The preview action selector contains every assigned action.
The supplied pack has dedicated weapon clips for Idle, Fire, full Reload and
Partial. Walk, Run, Equip, Unequip, aim transitions and jump stages explicitly
pair their arms clip with weapon Idle; AimAttack uses weapon Fire. No weapon
track or socket is inferred from a guessed name at Runtime.

Fresh-process preview checks render each action at start, midpoint and near end.
Equip/Unequip may intentionally leave the camera view. Movement remains the
existing Idle/Walk/Run Runtime controller. Aim, jump, equip, firing and reload
are retained authoring/preview actions; their gameplay input and staged action
ownership are not implemented by this library slice. No damage, ammo or recoil
gameplay claim is made.
