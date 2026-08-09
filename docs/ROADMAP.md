# Roadmap

## Current milestone

**Phase 4 — Project and asset pipeline**

**Outcome:** Assets become repeatable project resources rather than ad-hoc file opens.

Phase 3 established the Renegade Studio and Runtime foundations needed to move into the asset pipeline: project/scene persistence, hierarchy and inspector workflows, undo/redo, environment and terrain authoring, native light authoring, model import and placement, stable project/document/entity identity, transactional Renegade-owned document writes, Runtime screens/flow, and the accepted LP04 Test Level workflow.

LP04 is complete and accepted. Studio can launch the real standalone Runtime from the current live, unsaved scene through a disposable snapshot, wait for an explicit Runtime READY handshake, and STOP the Runtime cleanly without forcing an authoritative scene save.

LP05 is complete and accepted. Its deterministic Renegade-owned dependency graph passed separate-process Debug/Release packaged proof at exact head `8abb9fad959268ee00c32e046ede13d852883fa4` and was squash-merged through PR #33 at `fec9b521884d1a8e9017b8bac574b0ef615ca6cd`.

The active bounded milestone is **LC01 Gate 1 — Stable Asset Record Contract**. It consumes LP05's accepted graph into Renegade-owned project asset records while remaining UI-free and read-only.

### LC01 Gate 1 target

LC01 Gate 1 should prove that Renegade can:

- assign UUID-v4 asset identity to each project-owned dependency node;
- retain asset identity when the same canonical path is refreshed;
- retain class, requirement, applicability, provider/version and source hash;
- project graph edges onto stable asset IDs;
- distinguish missing sources without inventing files or repairing content;
- exclude engine/runtime-support nodes from the project asset registry;
- report added, changed and removed records;
- serialize and reload a versioned registry byte-identically;
- reject invalid IDs, duplicate paths, cross-project registries and dangling asset relationships;
- leave the pinned Wicked source unchanged.

Gate 1 defines and proves the registry contract only. It does not yet write an authoritative registry into a project, infer moved assets, run import/reimport, copy/cook content, build a game or alter the Content Browser UI.

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

**LC01 Gate 1 — Stable Asset Record Contract**

Acceptance is bounded to converting an LP05 dependency graph into stable, validated project asset records; refreshing those records without changing their UUIDs; reporting source/closure changes; and round-tripping deterministic versioned JSON in Debug and Release.

The architectural rule is:

> Paths locate current content; UUIDs identify assets. LC01 consumes LP05's canonical paths and hashes but does not make a path the durable identity authority.

See `docs/LC01_ASSET_IDENTITY_SOURCE_TRACKING.md` for the contract, exclusions and evidence plan.

## Status rules

- A phase or lifecycle slice completes only with a runnable increment and updated handoff/evidence.
- Compilation alone is not behavioural proof.
- Visual and behavioural failures override automated success.
- Persistent authored mutations require Undo/Redo and Save/Open evidence where applicable.
- Hosted CI limitations must be recorded rather than disguised as product passes or failures.
- Wicked remains pinned; do not modify Wicked source without an explicit, justified core-patch decision.
- New Wicked features enter the feature matrix at controlled sync points; they do not silently expand an active gate.
