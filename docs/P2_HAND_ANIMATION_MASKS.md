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
