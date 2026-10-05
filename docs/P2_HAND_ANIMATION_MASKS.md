# P2 native hand animation mask checkpoint

This is a local C++ foundation and manual owner-asset proof. It does not enable
live off-hand gameplay, introduce an assembly schema, or close P2.

## Native ownership

One native arms armature is partitioned into primary, off-hand and base bones.
Callers provide explicit skeletal subtree-root entities; product code must not
guess roots from mannequin filenames. The owner-asset harness uses clavicle_r
and clavicle_l to inspect this particular supplied pack.

CollectPlayerViewBonePartition validates both roots against one armature,
rejects overlapping subtrees and malformed cyclic hierarchy, and keeps the
output unchanged on failure. Base bones belong to neither hand.

CreatePlayerViewMaskedAnimationClip creates a paused, zero-weight native
AnimationComponent containing only the requested transform channels. It shares
the retained, same-scene AnimationDataComponent keyframes and leaves the source
clip untouched. Selected events, non-transform paths, retarget channels,
external sampler scenes, missing targets/data and empty masks are rejected.
No custom keyframe evaluator, IK solver or Wicked patch is introduced.

The caller owns generated clip lifetime and must remove these transient clip
entities before view-model replacement/despawn. Shared source data must remain
alive. Generated clips belong only to Runtime/private preview scenes and must
not enter saved authored products. Caller-selected native amount/timer/play
states remain Wicked animation authority. Missing-channel completion and
layer crossfades are not supplied by this helper.

## Verification

Release RenegadePlayerViewRigTests includes mask validation and native channel
evaluation: independent left/right sample times, finger inclusion, source
preservation, shared-data lifetime, atomic rejection, foreign roots, overlap,
cycles, retargets and external sampler rejection. Existing socket/grip and
paired-shotgun regressions remain in the same executable.

Manual graphics command, from the checkout with prepared owner assets:

    BUILD/renegade/Tests/Release/RenegadeFirstPersonAssemblyWorkflowProof.exe BUILD/sword-normalized-source BUILD/sword-layer-proof --sword-layers

Preparation/provenance is recorded in HANDOFF.md at the sword/shield source
checkpoint. The mode imports the supplied Idle arms and seven directly matching
native clips. Its 68 bones partition into 21 primary, 21 off-hand and 26 base.
Four directional attacks at three times preserve all off-hand world matrices;
shield windup and release at three times preserve the attacking right arm.
Measured maximum interference is 4.17233e-07 left and 8.9407e-07 right, within
the 0.0001 acceptance tolerance. Right-arm motion reaches 1.43963.
Native held-loop time continues across right-clip restart; pause freezes both
timers, and loop progression is checked frame by frame.

The harness removes the imported humanoid modifier on its private scene to
isolate native clips. An initial base-bone check failed on head movement
(delta 0.0798943) with that modifier present. The hand masks themselves remained
isolated. Live presentation sanitation must explicitly address procedural
modifiers before this test can become a gameplay acceptance claim.

Twenty diagnostic renders cover held block, four attacks, windup/release and
released idle. Matte neutral material is private diagnostic styling; supplied
textures, first-person camera framing and weapon grip alignment are not proven.

## Remaining integration

Persist governed primary/off-hand presentation bindings with explicit bone
identity and reusable mesh attachments. Add an authored left-arm start/held/end
action set and Runtime lifecycle cleanup, blending, equip/cancel/recovery and
movement policy. Verify actual sword and shield alignment, then Test Level and
independently packaged Runtime with authored input. Keep the live off-hand
capability disabled until that route passes. Existing two-hand shotgun actions
must retain full-arm ownership.


# Sword/shield playable checkpoint - 2026-10-05
Implemented schema-v2 shared arms with static primary/off-hand attachments,
explicit clavicle partitions, transient native per-hand clips and independent clocks.
Runtime routes LMB Attack and RMB held Block only for a matching shared presentation.
Fixed missing MOUSE_RIGHT press support in GameplayInputService.
Owner requested camera-local 90-degree right yaw and sword 2cm lower in grip.
Saved project: C:/Users/paulw/OneDrive/Desktop/renegade tests/SwordShieldTest.
Launcher: Play Sword Shield Test.cmd. All 35 source clips retained; only Idle,
Walk, Sprint, AttackLeft and BlockStart/Loop/End wired for tonight.
Release bridge and Runtime builds pass. Alternate Studio build passes at
BUILD/sword-studio (owner Studio untouched). Five focused CTests pass:
PlayerViewRig, source contract, FirstPersonAssemblySettings (v1/v2/undo),
EquipmentActionState, EquipmentAsset.
Native input evidence: BUILD/sword-native-events.json: equipment ready,
independent hands enabled; held block phase2 concurrent primary attack;
release phase3 then phase0. Fixture save/reopen and TestLevel closure checked
by --sword-playable in Tests/SwordShieldPlayableProof.h (final output proof6).
No gate closure. Remaining: sword/shield clipping is visible and unsolved;
no collision, hit damage, directional selection, parry, charge or equip wiring.
Hard animation transitions currently; blends and compatible combined poses need work.
Generic v2 assembly editor controls not yet exposed; fixture authored through service.
Native TestLevel snapshot smoke is required before claiming editor-button parity.
Files: FirstPersonAssemblyService.h/.cpp, FirstPersonHandAssemblyPreparation.h,
PlayerViewHandAnimation.h, PlayerViewAnimation.h, GameplayInputService.cpp,
RuntimeApplication.cpp, RuntimeLiveDiagnostics.cpp, RuntimeEquipmentLoadout.h,
FirstPersonAssemblyGraphicsProof.cpp, FirstPersonAssemblySettingsTests.cpp,
SwordShieldPlayableProof.h.
Reproduce: BUILD/sword_bridge_runtime_build.ps1, sword_playable_build.ps1,
sword_tests.ps1; manual native input BUILD/sword_native.py.
Next: correct clipping with compatible block/attack poses, meaningful per-hand
controller edge-case tests and v2 authoring controls; do not declare P2 complete.


Final TestLevel native smoke: snapshot load ready and simultaneous attack/block plus lowering observed in events; strict automated script assertion interrupted by foreground input changes, so no clean editor-button parity claim. Sword grip lowered 2cm; owner visual clipping acknowledged.

## Four sword attacks - 2026-10-06
Independent schema-v2 assemblies now admit multiple explicit Attack bindings
to distinct source indices. Legacy paired assemblies still reject duplicate
actions. Shield/movement/attack bindings cannot alias a source clip.
Recipe order persists as native attack_order metadata and sorts generated
primary clips, so LMB cycles Left, Right, Down, Stab in the owner fixture.
Each swing completes before another is accepted; no click buffering added.
Shared arms orientation and lowered grip retained. Independent held block retained.
Five focused CTests pass; Release bridge/Runtime builds pass; saved/reopened
fixture TestLevel closure passes; four unique generated attack clips exercised
with shield phase2 maintained and rendered captures inspected.
Evidence: BUILD/sword-playable-variants/sword-variant-0..3.png.
Updated owner project stays Desktop/renegade tests/SwordShieldTest;
prior single attack project preserved in SwordShieldTest-single-attack.
Files: FirstPersonAssemblyService.cpp, FirstPersonHandAssemblyPreparation.h,
PlayerViewHandAnimation.h, FirstPersonAssemblySettingsTests.cpp,
SwordShieldPlayableProof.h. Reproduce BUILD/sword_variants_build.ps1.
Still no directional input, charge mapping, damage, collision or parry.
Sword/shield clipping remains visible; no P2 gate closure.
Next: compatible attack/block poses and blending, then directional selection.

## Directional charge playback - 2026-10-06
Replaces automatic basic-attack cycling in the owner fixture with explicit
four-direction Charge/Hold/Release bindings. Hold LMB, move left/right/down/up
to select Left/Right/Down/Stab; release to strike. Quick tap uses last direction.
Gesture threshold 0.025 radians ignores small motion; selection consumes camera
look while LMB held. Charge strength saturates at 1 second and is recorded on
release; no damage calculation or hit detection is claimed. RMB block independent.
C cancels pending charge; zero dt freezes clocks and selection.
Runtime shows selected direction and charge percent near screen centre.
Legacy paired shotgun and existing basic-attack independent assemblies keep
their existing path unless all 12 directional clips are explicitly bound.
Native fixture proof checks all four charge-to-hold/release groups, four unique
release clips, held shield, zero-dt freeze, low-strength quick release and cancel.
Release bridge/Runtime builds and five focused CTests pass; fixture cold load
and TestLevel equipment/presentation closure pass.
Evidence BUILD/sword-playable-directional/sword-charge-0..3.png and
sword-release-0..3.png. Owner project Desktop/renegade tests/SwordShieldTest;
previous cycle fixture retained as SwordShieldTest-attack-cycle.
Files: PlayerViewHandAnimation.h, PlayerViewAnimation.h,
FirstPersonAssemblyService.cpp, RuntimeApplication.cpp/.h,
RuntimeLiveDiagnostics.cpp, SwordShieldPlayableProof.h.
Reproduce BUILD/sword_variants_build.ps1 (directional output).
Remaining: animation fades, sword/shield clipping, authored charge settings,
stamina, directional block/damage, collision and generic v2 UI. No P2 gate closure.

Final native mouse PASS: four selected full-charge releases, independent held block and low-charge quick tap. Evidence BUILD/directional-native-events.json. Alternate Studio Release build also passes. Runtime left open on the updated owner project.
