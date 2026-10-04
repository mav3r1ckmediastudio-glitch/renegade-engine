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
   track holds its last pose. UPDATE PREVIEW applies the draft settings.
6. Choose a preview action, play/pause or scrub. Save is enabled after rendering.
7. Give the assembly a new name and SAVE + ASSIGN. This creates a governed product
   and assigns its stable ID through the existing Player settings command.
   Save the level to persist that assignment.
8. Reopen the panel on an assigned assembly to recover its recipe. Saving makes
   a new product rather than overwriting a shared asset. Closing discards the draft.

Selecting different parts requires LOAD PARTS. Changing parent, transforms or
pairs invalidates the previous preview. A part registry hash change after preview
requires a fresh load/preview before saving. Player assignment remains undoable;
draft assembly controls do not yet have command history.

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
classification, draft Undo/Redo, assembly rebuild/update lifecycle and actual
Build Game parity for this real assembly. Reload preview is not a reload gameplay
implementation. Independent exact-commit verification remains pending; no release
gate is closed by this slice.
