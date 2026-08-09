# Roadmap

## Current milestone

**Phase 4 — Project and asset pipeline**

**Outcome:** Assets become repeatable project resources rather than ad-hoc file opens.

Phase 3 established the Renegade Studio and Runtime foundations needed to move into the asset pipeline: project/scene persistence, hierarchy and inspector workflows, undo/redo, environment and terrain authoring, native light authoring, model import and placement, stable project/document/entity identity, transactional Renegade-owned document writes, Runtime screens/flow, and the accepted LP04 Test Level workflow.

LP04 is complete and accepted. Studio can launch the real standalone Runtime from the current live, unsaved scene through a disposable snapshot, wait for an explicit Runtime READY handshake, and STOP the Runtime cleanly without forcing an authoritative scene save.

LP05 is complete and accepted. Its deterministic Renegade-owned dependency graph passed separate-process Debug/Release packaged proof at exact head `8abb9fad959268ee00c32e046ede13d852883fa4` and was squash-merged through PR #33 at `fec9b521884d1a8e9017b8bac574b0ef615ca6cd`.

LC01 Gate 1 is complete and accepted. Exact final head `0cd63d844c655eecebaf3f7bf04fdacbad2d50ed` passed Studio run 147 and baseline run 154 before squash merge through PR #34 at `580e5a5289e35b9bc60929a5b0c3cec6aaec0b2f`.

PR #35 landed LC01 Gate 2 at `bdb1fb98`, but its acceptance was reopened after
raw Debug and Release logs revealed a real 24/25 result hidden behind green
jobs. Corrective PR #36 fixes Windows short/long path identity during recovery
and makes native test-process failures propagate reliably into CI. Candidate
head `09601bc0` passed a genuine 25/25 in Studio run 156 and both jobs in
baseline run 172. Independent review and squash merge remain pending.

The active bounded milestone remains **LC01 Gate 2 — Transactional Project
Persistence corrective close-out**.

### LC01 Gate 2 target

LC01 Gate 2 should prove that Renegade can:

- commit the canonical registry to `AssetRegistry.renegade-assets` at the project root;
- reload it only for the owning project UUID;
- treat unchanged writes as byte-preserving no-ops;
- preserve exact previous bytes across validation and replacement failures;
- retain a durable journal across simulated interruption;
- recover that interruption automatically before Project Open;
- reject substituted, cross-project and non-canonical registry documents;
- leave the pinned Wicked source unchanged.

Gate 2 persists and recovers the accepted Gate 1 contract only. It does not yet define source/product import records, infer moved assets, run reimport, copy/cook content, build a game or alter the Content Browser UI.

## Phase 4 deliverables

The authoritative Phase 4 plan in `docs/MASTER_PLAN.md` remains:

- Project-relative asset database and stable asset IDs.
- Content browser with folders, filters, previews and drag/drop.
- Import and reimport for WISCENE, OBJ, FBX, glTF/GLB, VRM/VRMA and PLY.
- Texture, audio, video, script and font handling.
- Import settings and source-file tracking.
- Thumbnails and dependency/reference reporting.
- Missing-asset and moved-asset recovery.
- Background import jobs and visible error reports.

**Phase 4 exit gate:** a reference asset pack imports, survives source updates and reopens without broken references.

## Completed foundations relevant to Phase 4

- Renegade Studio and standalone Runtime are separate applications.
- Project Hub and project-aware scene workflows exist.
- Scene Save/Open and protected scene-document behaviour are established.
- Undo/Redo-backed authoring exists for transforms and multiple native systems.
- Terrain Authoring V1 is accepted on packaged DX12 and Vulkan.
- Native light creation/edit/delete workflow is accepted.
- Model Import V1 conversion/round-trip proof is accepted.
- Imported GLB/GLTF scene placement, Undo/Redo, Save/Open and automatic scale correction have been exercised on packaged DX12 and Vulkan.
- Stable UUID-v4 identity exists for projects, Renegade documents and authored scene entities.
- Renegade-owned project/flow/screen writes use transactional persistence and recovery.
- Runtime screen and stable action-dispatch foundations exist.
- LP04 Test Level unsaved-snapshot launch is accepted end-to-end on real Windows hardware.

These foundations do **not** by themselves constitute the Phase 4 asset database, stable asset IDs, dependency graph, reimport system or Content Browser.

## Phase plan

| Phase | Duration | Outcome |
|---|---:|---|
| 0. Charter and baseline | 1 week | Traceable product target and pinned Wicked source |
| 1. Reproducible build | 2–3 weeks | Wicked core, editor, tests and samples build reliably |
| 2. Architecture/UI proof | 3–4 weeks | Branded editor shell proves viewport, save/reload, HDR, DPI and input |
| 3. Studio foundation | 6–8 weeks | Dependable project, scene, hierarchy, inspector and undo workflows |
| 4. Project and asset pipeline | 6–8 weeks | Repeatable import, reimport, IDs, dependencies and content browser |
| 5. Scene/render exposure | 8–10 weeks | Core world and visual systems authorable in Renegade |
| 6. Physics/audio/gameplay | 8–10 weeks | Small interactive packaged game |
| 7. Advanced systems | 10–14 weeks | Animation, terrain, particles, fluids and specialist components |
| 8. Scripting/runtime/export | 10–12 weeks | Lua workflow and repeatable standalone builds |
| 9. Exposure audit/platforms | 8–10 weeks | Demonstrated coverage of the pinned Wicked baseline |
| 10. Beta and V1 | 8–12 weeks | Hardened, documented, independently verified release |

Planning range: 16–21 months at approximately 12–18 focused hours per week.

The full reasoning and milestone calendar are in `docs/MASTER_PLAN.md`.

## Immediate next gate

**LC01 Gate 2 — Corrective review and merge**

Acceptance is bounded to independently reviewing corrective PR #36 and
squash-merging it only if the exact reviewed head retains genuine 25/25 Debug
and Release Studio evidence plus both pinned-Wicked baseline passes. Gate 3
does not begin before that correction is accepted on `main`.

The architectural rule is:

> Durable project metadata uses the same staged validation, containment, journal and rollback boundary as other Renegade-owned documents. Registry persistence must not invent a weaker write path.

See `docs/LC01_ASSET_IDENTITY_SOURCE_TRACKING.md` for the contract, exclusions and evidence plan.

## Status rules

- A phase or lifecycle slice completes only with a runnable increment and updated handoff/evidence.
- Compilation alone is not behavioural proof.
- Visual and behavioural failures override automated success.
- Persistent authored mutations require Undo/Redo and Save/Open evidence where applicable.
- Hosted CI limitations must be recorded rather than disguised as product passes or failures.
- Wicked remains pinned; do not modify Wicked source without an explicit, justified core-patch decision.
- New Wicked features enter the feature matrix at controlled sync points; they do not silently expand an active gate.
