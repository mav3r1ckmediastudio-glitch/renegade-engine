# Roadmap

## Current milestone

**Phase 4 — Project and asset pipeline**

**Outcome:** assets become repeatable project resources rather than ad-hoc file
opens.

The phase remains open, but several backend lifecycle foundations that the
original roadmap expected later are now already complete. The current main
baseline after LP06 is `48126f859b2f9b25a60182c4311cfc6c91d98436`.

## Accepted lifecycle foundations

### LP04 — Unsaved Test Level Snapshot

Accepted. Studio can snapshot the current unsaved authoring scene, launch the
real standalone Runtime, wait for the explicit READY handshake, display the
unsaved content and STOP cleanly without overwriting the authoritative scene.

### LP05 — Representative Dependency Extraction

Accepted. Renegade owns a deterministic typed dependency graph across project,
Story Flow, Runtime Screen, WISCENE, glTF and explicit declared-reference
providers. Packaged Debug and Release proof produce the canonical 4,681-byte
graph with SHA-256
`23b67f63099293d79a239997730b287f157fb38e5421aecb5505e0ca42c84384`.

### LC01 — Asset Identity and Source Tracking

Accepted through PR #39, squash merge
`01d790bda5acea0cdb6a7735557b12224c795a64`. Exact final head
`3d3e780b38792aec866cd19ce6638a8260ffff4f` passed Studio run 166 and
Windows baseline run 185 in Debug and Release. LC01 establishes stable project
asset IDs, transactional `AssetRegistry.renegade-assets` persistence,
source-to-imported-product provenance/import settings and deterministic
moved/missing recovery. Its canonical packaged registry is 2,180 bytes with
SHA-256
`547a26c09e6a74394cc9bc67885070272928f5af14d736e8e086d022f0aeea0e`.

LC01 deliberately does **not** execute reimport or provide creator-facing asset
management UI.

### LP06 — Named Standalone Windows Build and Safe Rebuild

Accepted through PR #44, squash merge
`48126f859b2f9b25a60182c4311cfc6c91d98436`. Exact accepted PR head
`99cfe1f74016bb6a53a4c35e020f6884099a52fd` passed Renegade Studio run
239 and Windows baseline run 364 in Debug and Release. Release passed 42/42
CTest entries including the real standalone package smoke and the owner-build
scene-identity regression; Debug retained only the accepted Gate 4 hosted audio
capability skip.

LP06 now provides:

- deterministic Windows build planning;
- same-volume governed staging;
- a named game executable with package-relative startup;
- exact package integrity and runtime-support manifests;
- detached Release DX12 smoke/Test All parity from an unrelated working folder;
- safe last-good-build preservation, rollback and promotion;
- **BUILD > BUILD WINDOWS GAME...** in Release Studio; and
- direct owner launch of the promoted named executable without Studio or an
  explicit `--project` argument.

The first owner build exposed a real missing scene-identity-sidecar integration
bug. The corrected LP06-only scene companion provider packages reachable
`<scene>.wiscene.rmeta` files without changing the accepted LP05 canonical
graph. The corrected build completed successfully, the promoted executable
launched directly from Explorer, and Runtime Screen -> PLAY -> Level One was
accepted on owner hardware. Independent exact-head audit passed.

LP06 output remains deliberately `distribution_ready=false`; commercial
redistribution/signing/installer policy is a later release boundary.

## Phase 4 position after LP06

The original Phase 4 deliverables were:

- project-relative asset database and stable asset IDs;
- Content Browser with folders, filters, previews and drag/drop;
- import/reimport for supported model formats;
- texture/audio/video/script/font handling;
- import settings and source-file tracking;
- thumbnails and dependency/reference reporting;
- moved/missing asset recovery;
- background import jobs and visible error reporting.

Stable asset IDs, durable registry persistence, source tracking, provenance,
dependency extraction and backend moved/missing recovery are now established.
What remains is primarily the **creator-facing reusable asset workflow and real
reimport execution**, followed by broader asset classes and polish.

## Immediate next programme

**LP07 — Reusable Project Asset Workflow**

LP07 is the next bounded lifecycle programme. It is now explicitly **FBX-first
and multi-format**, because the primary creator asset workflow depends on FBX,
including animated/skinned FBX.

The exact pinned Wicked revision already contains a dedicated FBX converter based
on its bundled `ufbx` loader. That converter maps meshes, materials/textures,
armatures/skinning, morph data and FBX animation stacks/takes into native Wicked
scene/animation components. Renegade should prove and expose that existing path
through its own service boundary before considering another FBX stack.

### LP07 format priority

- **P0:** FBX — static, skinned and animated; owner/package acceptance format.
- **P1:** GLTF/GLB — already proven foundation, retained as required regression
  and supported alternative.
- **P2:** OBJ and PLY through the common import service when the pinned Wicked
  converter seams are proven clean; VRM/VRMA after an exact pinned-source seam
  audit.
- **Fallback only:** Assimp or another external importer when a concrete required
  format/FBX feature is proven unsupported or unreliable through the Wicked
  path. A second importer stack is not added merely to increase extension count.

Target outcome:

> A creator imports an FBX once as a governed project asset, including preserving
> skinned/animated content where present, sees it in the Renegade Asset Browser
> with stable identity/provenance status, places it repeatedly without
> reconverting the external source, changes the source, performs an explicit safe
> reimport that preserves asset identity, then reopens and builds the project
> without broken references. GLB/GLTF remains functional throughout.

See `docs/LP07_REUSABLE_PROJECT_ASSET_WORKFLOW.md` for the gate contract.

### LP07 gate map

1. **Gate 1 — Common model-import seam and FBX proof**
   - generalise the current GLB/GLTF-shaped `ImportService` contract;
   - compile/call the exact pinned Wicked FBX converter without exposing the
     stock Wicked Editor;
   - prove static FBX plus skinned/animated FBX through WISCENE round trip;
   - retain GLB/GLTF regression proof;
   - audit exact OBJ/PLY/VRM/VRMA seams and record what Renegade can safely expose.
2. **Gate 2 — Registry-backed asset catalogue**
   - join the existing Asset Browser filesystem view with LC01 stable registry
     identity/provenance/status;
   - expose a UI-free catalogue model first;
   - no import or file mutation.
3. **Gate 3 — Governed reusable model-asset import transaction**
   - FBX is the primary acceptance format; GLB/GLTF shares the same transaction;
   - commit a reusable WISCENE project product and LC01 provenance only after
     successful conversion/round-trip validation;
   - failed import preserves the previous project/registry state.
4. **Gate 4 — Stable explicit reimport**
   - explicitly re-run the stored importer/backend/settings recipe against a
     stale source;
   - preserve source/product asset IDs and last-good product on failure;
   - animated FBX reimport must preserve/prove the expected animation structure;
   - no silent/background overwrite.
5. **Gate 5 — Creator Asset Browser workflow**
   - display stable asset type, identity and source health;
   - import/browse/filter/select reusable assets;
   - place a registered model repeatedly through Renegade-owned commands;
   - surface missing/moved/stale states and explicit reimport;
   - owner acceptance uses a representative animated/skinned FBX.
6. **Gate 6 — Packaged lifecycle acceptance**
   - FBX import -> place -> Save/Open -> source update -> explicit reimport ->
     reopen;
   - dependency closure and LC01 identity remain coherent;
   - Build Windows Game succeeds and the standalone Runtime uses the updated
     product;
   - GLB/GLTF remains a required regression path;
   - owner acceptance plus independent exact-head review.

### LP07 deliberate exclusions

LP07 does not add Assimp without a proven capability gap, promise dozens of DCC
formats merely to increase extension count, or build texture/audio/video/font
asset editors, universal thumbnails, background import queues, animation state
machines/timelines, physics authoring, particles, Lua gameplay or multiplayer.

Imported FBX animation data must survive and be usable; **animation authoring** is
still a later bounded programme.

## After LP07

Subject to LP07 acceptance, the likely order is:

- broaden the asset pipeline beyond model assets: textures, audio, video,
  scripts and fonts;
- add any further high-value model formats behind the accepted common importer
  contract, using external libraries only where justified by evidence;
- previews/thumbnails, dependency/reference reporting and background jobs;
- complete creator-facing materials/render/world exposure where gaps remain;
- physics, character controller, input and audio gameplay foundations;
- animation/retargeting/IK workflows;
- particles/effects and the generic projectile/magic framework;
- scripting/gameplay framework and richer Runtime game-state/save systems;
- later networking/multiplayer, specialist systems and distribution hardening.

The ordering follows the master-plan rule that higher-level gameplay systems
should consume reliable project assets and Runtime/build semantics rather than
inventing parallel file-loading paths.

## Master phase plan

| Phase | Outcome | Current position |
|---|---|---|
| 0. Charter and baseline | Traceable target and pinned Wicked source | Complete |
| 1. Reproducible build | Reliable Wicked/Renegade Windows build | Complete |
| 2. Architecture/UI proof | Branded Studio/Runtime architecture proof | Complete |
| 3. Studio foundation | Dependable project/scene/editor workflows | Substantially complete |
| 4. Project and asset pipeline | Repeatable assets/import/reimport/content browser | **Active** |
| 5. Scene/render exposure | Core visual/world systems authorable | Partially advanced early |
| 6. Physics/audio/gameplay | Small interactive packaged game | Future |
| 7. Advanced systems | Animation, particles and specialist systems | Future |
| 8. Scripting/runtime/export | Lua/runtime/export maturity | Partially advanced early by LP04/LP06 |
| 9. Exposure audit/platforms | Pinned-baseline coverage/performance/platform audit | Future |
| 10. Beta and V1 | Hardened supportable release | Future |

The original time ranges in `docs/MASTER_PLAN.md` remain planning guidance, not
a promise. Work has intentionally crossed some original phase boundaries where
a later lifecycle foundation was required to prove an earlier system safely.

## Status rules

- A lifecycle gate completes only with a runnable increment and updated evidence.
- Compilation alone is not behavioural proof.
- Visual/behavioural failure overrides green CI.
- Persistent creator mutations require Undo/Redo and Save/Open evidence where
  applicable.
- Hosted CI limitations are recorded, not disguised by weakening Wicked or the
  product contract.
- Wicked remains pinned; modify it only through an explicit justified core-patch
  decision.
- New Wicked features enter through controlled roadmap/matrix review rather than
  silently expanding an active gate.
- Owner-visible standalone build success is not equivalent to commercial
  redistribution approval.