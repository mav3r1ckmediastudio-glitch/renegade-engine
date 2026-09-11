# Renegade Engine Roadmap

**Current programme:** Phase 7 — Animation, Terrain and Advanced Simulation  
**Merged implementation baseline:** PR #148 — native Wicked navigation plus terrain/rigid-body contact repair  
**Merged commit:** `50b3b3663ce42b7093e4d6c367cfe8dd863d3a9e`  
**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## Current state

**Phase 6 — Playable Core is complete.**

The final PR #148 head passed the Windows baseline and Renegade Studio workflows,
then owner testing accepted the repaired grounded Box/Cylinder terrain contact.
The owner subsequently built and ran the Phase 6 mini-game acceptance build
successfully, satisfying the existing Phase 6 exit contract: a packaged
standalone project with a controllable character, collisions, audio and a
scripted objective.

The programme therefore advances to **Phase 7 — Animation, Terrain and Advanced
Simulation**. Phase 7 begins with a pinned-source capability audit rather than an
implementation branch. Several systems originally scheduled for Phase 7 — finite
terrain, native grass/vegetation, ocean and GPU particle authoring — already exist
in Renegade, so the audit must measure the actual remaining gap.

## Merged production baseline

| Programme | Merged result |
|---|---|
| Story Flow Gates 1-10 | Project-home Journey/Graph authoring, Screen lifecycle, Runtime traversal, Build Game and standalone parity through PR #101. |
| Scene UI recovery | Shell/workspace isolation, Hierarchy and Inspector, Asset Browser placement, Environment/Terrain and consolidated whole-editor recovery through PRs #102-#106. |
| JP01 physics foundation | Wicked/Jolt physics authoring, Physics Lab, serialization, Runtime and packaged parity through PR #107, with grounded primitive/terrain-contact repair completed in PR #148. |
| Phase 5 Gates 1-9 | Scene components, cameras, decals/probes, materials/shaders, post-processing, AO/GI/reflections, ray/path-tracing exposure, lightmap/baking and render diagnostics through PRs #109-#118. |
| WD01 vegetation/frame-loop recovery | Native Wicked grass painting across Terrain chunks, restored editor interaction and accepted capped frame-loop baseline through PR #122. |
| Phase 6 Gates 1-3 | Player Start/possession, gameplay input/lifecycle and native Wicked spatial audio through PRs #123-#125. |
| Scripting S1-S7 | Inspector-provider foundation, governed Lua lifecycle, typed properties/references, gameplay API, diagnostics/IPC, Creator Library adoption and six stock Actions through PRs #127-#144. |
| Asset Browser recovery | Governed catalogue/tombstone recovery and restored creator placement path through PR #145. |
| Marker + particle authoring | Editor marker overlays and native Wicked GPU particle authoring through PR #146. |
| Objective/interaction vertical slice | Reusable Objective Counter composed with accepted stock Actions through PR #147. |
| Native navigation + final Phase 6 repair | Wicked VoxelGrid/PathQuery/CharacterComponent authoring/runtime proof plus terrain heightfield inheritance and primitive collider fitting repair through PR #148. |

## Phase 6 closure

Phase 6 now provides the complete playable-core path required by its acceptance
contract:

- governed Player Start and first-person Runtime possession;
- Wicked/Jolt character collision and movement;
- persisted gameplay action maps, Pause/Resume and deterministic Reset;
- native global/2D and positional 3D audio;
- governed Lua project/gameplay scripting and Creator Library Actions;
- reusable objective/interaction composition;
- native Wicked voxel-grid navigation and Runtime path following;
- rigid-body/terrain collision behaviour accepted after the PR #148 repair;
- Test Level supervision and live diagnostics; and
- independently packaged Windows Runtime output.

Do not reopen Phase 6 for feature expansion. A genuine regression in an accepted
Phase 6 capability is a repair; otherwise new breadth is Phase 7 or later work.

## Phase 7 scope from the master plan

Phase 7's intended outcome is that Wicked's specialised content systems are
authorable and verifiable from Renegade. The original scope is:

- skeletal animation, animation data and timelines;
- armatures, humanoids, retargeting, IK, expressions and morph animation;
- terrain generation, layers, props, virtual textures and editing tools;
- GPU emitters, hair/grass, force interaction, ocean and fluid effects;
- splines, video components, Gaussian splats and remaining scene components; and
- paint tooling and component-specific debug visualisation.

Its exit gate remains: **every registered scene-component type has a classified
exposure tier and either a completed editor path or an explicitly verified
programmer path.**

## Phase 7 starting audit

### 1. Native Wicked character-animation stack

Audit the pinned Wicked source and original Wicked Editor for:

- armature/skeleton data ownership and serialization;
- AnimationComponent clip/timeline/channel data;
- Runtime animation playback and blending semantics;
- humanoid bone mapping;
- animation retargeting;
- inverse kinematics;
- expressions and morph-target animation;
- existing Lua bindings;
- importer assumptions for FBX/glTF/VRM/VRMA; and
- the original editor windows/workflows used to author or inspect each feature.

The output must distinguish capabilities Renegade can expose directly through an
EngineBridge adapter from capabilities that need a higher-level Renegade creator
workflow. Do not design a second animation runtime if Wicked already owns the
state and evaluation path.

### 2. Reconcile already-delivered Phase 7 systems

Classify the current Renegade status of:

- finite terrain/sculpt/material controls;
- vegetation/grass;
- native GPU particles;
- ocean;
- force fields;
- hair;
- fluids;
- splines;
- video components;
- Gaussian splats;
- virtual textures; and
- remaining registered scene components.

This audit should update `docs/FEATURE_MATRIX.csv` before implementation gates are
frozen.

### 3. Freeze bounded Phase 7 gates

After the audit, define small vertical slices with explicit owner acceptance and
CI boundaries. Prioritise creator value and native Wicked reuse. Avoid splitting
one coherent animation workflow into many build-heavy PRs unless a real risk or
architectural boundary requires it.

## Deliberate deferrals

- **Shared zones:** no audio-specific or objective-specific replacement. One
  future ZoneService must serve multiple systems.
- **VSync control:** still absent from the creator-facing settings surface.
- **Player arms, weapons, combat and production enemy AI:** not silently bundled
  into the first animation gate; scope them deliberately when the animation
  foundation is stable.
- **Controller physical evidence:** automated/source coverage can remain ahead of
  owner hardware evidence where controller hardware is unavailable.
- **Commercial distribution:** Build Game remains an engineering acceptance path
  until licensing/release packaging is explicitly cleared.

## Verification policy

- Green compilation is necessary but never sufficient for creator-facing work.
- Visual or behavioural owner failure overrides nominal automated success.
- Save/reopen is required wherever authored state is persisted.
- Gameplay-facing state must be exercised in the real Runtime process.
- Packaged parity is required when a feature affects standalone gameplay.
- The original Wicked Editor remains the parity oracle for native Wicked systems.
- Exact gate evidence belongs in gate/audit documents; this roadmap records
  programme state rather than reproducing implementation logs.