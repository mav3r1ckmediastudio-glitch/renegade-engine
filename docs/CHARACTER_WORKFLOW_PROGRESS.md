# Character workflow implementation progress

## Recovery authority

- **Authoritative specification:** `docs/RENEGADE_CHARACTER_WORKFLOW_IMPLEMENTATION_SPEC.md`
  (SHA-256: `dff426869be84ddeecbb98511d96126bbf9a4fa6d83bb6a588767a5c536d43d5`).
- **Programme branch:** `feature/character-workflow-programme`.
- **Accepted starting baseline:** `cc31ef318ec55db5d115b48a0242665ce571c561`
  from AI-05 PR #158, based on Phase 7 baseline
  `d36918878776d0d91e0c39f88f6764a1926a6534`.
- **Latest implementation/source-contract checkpoint before this progress update:**
  `d824045aaca6d9fb0d4dd51b9d9d70906a40e113`.
- **CW-03 primary implementation commit:**
  `ca5427f0f23529d1497f14953897700075bdf89d`.

## Gate state

| Gate | Status | Notes |
| --- | --- | --- |
| CW-00 | Complete | Live branch, PR #158 checks, workflow triggers and the import/asset/placement architecture were audited. |
| CW-01 | Complete / source-validated | The normal importer persists the Model/Character designation, commits Character assets to `Content/Characters`, and registers a source contract for that path. |
| CW-02 | Complete / source-validated | Character import accepts queued external WISCENE/FBX/GLTF/GLB/VRM/VRMA animation sources, retains immutable governed snapshots (including local glTF dependencies), persists source/action provenance in the import recipe, and has a registered headless regression target. |
| CW-03 | Complete / source-validated | Character import validates/prepares a native Wicked humanoid mapping where appropriate and retargets included external animation sources through the existing `RetargetHumanoidAnimationsCommand` before the reusable Character asset payload is saved. The same recipe application path is used by governed reimport. |
| CW-04 to CW-08 | Not started | No implementation has begun for these gates. |

## CW-03 implementation details

- `ApplyCreatorModelImportRecipe()` is the preparation boundary for both first
  import and governed reimport. CW-03 is implemented there rather than as a
  scene-level post-import ceremony.
- Character imports reuse the existing native Wicked humanoid map and accept it
  immediately when it is valid. A single compatible armature is auto-mapped
  through the existing `BuildAutoHumanoidMapping()` and
  `SetHumanoidMappingCommand` path when required/appropriate.
- External animation provenance is Character-only. Generic MODEL recipes fail
  closed if external humanoid animation rows are present.
- Retained sources are resolved only from the governed
  `SourceAssets/Animations/Snapshots/...` tree and are canonicalised before
  use. Missing/escaped provenance fails clearly instead of silently dropping a
  requested animation.
- External rows are grouped by retained source and retargeted through the
  existing `RetargetHumanoidAnimationsCommand`, which in turn uses Wicked's
  native baked `Scene::RetargetAnimation()` path. Creator clip names,
  include/exclude state and requested source ranges are applied to the prepared
  destination clips before final asset serialization.
- Multiple ambiguous destination humanoids fail deterministically with a repair
  route rather than selecting a rig by unstable ECS ordering.
- The pinned Wicked FBX importer at submodule
  `3a800b7134aafe58461093c8abb2e274d4e64033` already creates/validates native
  humanoid mapping for Mixamo bone naming during import, so Mixamo-style
  external FBX animation sources enter the existing retarget backend with the
  source humanoid mapping it requires. Renegade does not introduce a second
  source retargeter.
- `RenegadePhase7Gate7BTests` now also covers the CW-03 importer boundary:
  Character recipe auto-preparation for a synthetic Mixamo-style rig, rejection
  of external humanoid provenance on a generic Model recipe, and a clear
  retained-source failure path.
- The existing Phase 7B CMake source contract now locks the CW-03 integration
  points too: importer auto-map/validation, native retarget backend reuse,
  governed snapshot provenance, Character-only ownership and the shared
  governed reimport boundary.

## Validation and known state

- AI-05 PR #158 head `cc31ef3` was accepted by the owner. The four required PR
  checks were green on 2026-09-14 before the Character Workflow programme
  began: Windows Debug, Windows Release, Renegade Studio Debug and Renegade
  Studio Release.
- Programme-branch pushes do not trigger the expensive Windows matrix; PRs
  targeting `main` do. No new full Windows CI run is claimed for CW-02/CW-03.
- CW-02's `RenegadeCreatorExternalAnimationImportTests` target remains
  registered in the LP07 Gate 4 test chain.
- CW-03 extends the already-registered `RenegadePhase7Gate7BTests` executable
  and the Phase 7B source-contract test, so both CW-03 regression layers will
  execute when the Windows test suite next runs.
- The initial reusable import path and `ReusableAssetReimportService` were both
  re-audited: each parses the persisted creator options and calls the same
  `ApplyCreatorModelImportRecipe()` before saving its prepared WISCENE payload.
  This is the deterministic import/reimport authority for CW-03.
- No local native compilation is claimed from this connector-only execution
  environment. A full CI should remain deferred until an intentional programme
  confidence point or owner request rather than spending a Windows matrix on
  every CW gate.
- Owner-facing CW-03 acceptance still requires a real representative character:
  import a Mixamo-style Character plus separate animation files in one import,
  finalize it, and verify that the resulting Character asset already contains
  the prepared destination clips without using the scene-level HUMANOID /
  RETARGET operation.

## Exact next task

Begin CW-04 — Prepared Character Placement and Inspector Routing. Dragging a
prepared Character Asset must create a fresh governed Character instance with
new persistent identity, default gameplay settings and immediate Character
Inspector routing, without a mandatory MAKE CHARACTER step.
