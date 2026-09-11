# Phase 7 A-E — One-Branch / One-CI Build Contract

**Date:** 11 September 2026

The project owner explicitly requires Gates 7A through 7E to be implemented on
one staging branch before the next intentional Windows CI run. Intermediate
commits are allowed for safety/reviewability, but no implementation PR is to be
opened until the complete A-E candidate has passed source audit.

## 7A — Native Animation Playback Inspector

Expose native Wicked AnimationComponent playback/authoring state for the
selected animation/character hierarchy: clip selection, play/pause/stop/from
start, loop/ping-pong/play-once, timer scrub, speed, blend amount, start/end and
root-motion status/bone.

## 7B — Humanoid Mapping + Native Retargeting

Expose armature/humanoid inspection, native pose reset, automatic humanoid
mapping, manual mapping correction and native RetargetAnimation-based animation
import/bake for WISCENE/GLTF/GLB/FBX/VRM/VRMA.

## 7C — IK + Look-at + Expressions

Expose native InverseKinematicsComponent, Humanoid look-at state and
ExpressionComponent controls while reusing the accepted JP01 humanoid/ragdoll
ownership.

## 7D — Native Timeline / Keyframe Authoring

Expose a bounded Renegade timeline over Wicked AnimationComponent channels and
AnimationDataComponent. Cover native transform and morph targets first, with
existing Wicked-supported light/audio/emitter/camera/script/material channel
families available through the same native data model. Do not create a second
sequencer format.

## 7E — Specialist Native Component Exposure

Expose the remaining bounded Wicked systems in Renegade creator UX: generic
HairParticle, ForceField, Video, Spline and Gaussian Splat, plus honest handling
of remaining terrain/virtual-texture gaps. Use native Scene components and
serialization.

## Shared rules

- Wicked remains the sole low-level runtime/evaluator/serializer authority.
- Persisted edits go through EngineBridge command history and Undo/Redo.
- Preview-only actions do not dirty the document unless they change authored
  state.
- No Wicked submodule edits.
- Save/Reopen, Test Level and packaged Runtime parity are covered in the single
  final candidate where applicable.
- No implementation PR / intentional CI before A-E source completion.
