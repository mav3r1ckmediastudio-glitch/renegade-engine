# Roadmap

## Current milestone

**Phase 4 — Project and asset pipeline**

**Outcome:** Assets become repeatable project resources rather than ad-hoc file opens.

Phase 3 established the Renegade Studio and Runtime foundations needed to move into the asset pipeline: project/scene persistence, hierarchy and inspector workflows, undo/redo, environment and terrain authoring, native light authoring, model import and placement, stable project/document/entity identity, transactional Renegade-owned document writes, Runtime screens/flow, and the accepted LP04 Test Level workflow.

LP04 is complete and accepted. Studio can launch the real standalone Runtime from the current live, unsaved scene through a disposable snapshot, wait for an explicit Runtime READY handshake, and STOP the Runtime cleanly without forcing an authoritative scene save.

The next bounded milestone is **LP05 — Representative Dependency Extraction**. LP05 is an asset-pipeline foundation slice: establish a deterministic Renegade-owned dependency graph for representative project/runtime content before asset cooking, copying, packaging, or a full Content Browser workflow is attempted.

LP05 must use a **Renegade-owned, read-only typed walker** over the pinned Wicked ECS/component structures. Do not modify Wicked source merely to expose its internal serializer resource-registration state.

### LP05 target

LP05 should prove that Renegade can:

- identify representative authored roots and collect the native resources they require;
- extract WISCENE dependencies from the relevant Wicked components through typed ECS queries;
- normalize and classify dependency paths without rewriting the source scene;
- represent dependencies with stable Renegade-owned graph/path structures;
- deduplicate repeated references deterministically;
- preserve enough provenance to explain which authored object/component produced each dependency;
- report deliberately missing or unresolved dependencies as structured evidence rather than silently dropping them;
- cover representative imported model content and nested material/texture references;
- remain read-only with respect to the authoritative project and WISCENE files;
- leave the pinned Wicked source unchanged.

LP05 is deliberately narrower than a complete asset database or cooker. It establishes trustworthy dependency discovery first; later lifecycle slices can build asset identity, source tracking, reimport, copying/cooking, recovery and Content Browser workflows on top of that evidence.

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

**LP05 — Representative Dependency Extraction**

Acceptance is bounded to dependency discovery and evidence. It must not silently expand into the full Content Browser, asset cooker, packaging pipeline or a Wicked source modification.

The architectural rule for WISCENE native-resource extraction is:

> Renegade owns the extraction surface. Read the required Wicked component fields through the public/native scene and ECS structures, build Renegade-owned dependency records, and keep the pinned Wicked source untouched unless a later gate proves there is no viable external route.

After LP05, continue the lifecycle in dependency order rather than jumping directly to broad UI work. Asset identity/source tracking and subsequent packaging/cooking work should consume the dependency model proved here.

## Status rules

- A phase or lifecycle slice completes only with a runnable increment and updated handoff/evidence.
- Compilation alone is not behavioural proof.
- Visual and behavioural failures override automated success.
- Persistent authored mutations require Undo/Redo and Save/Open evidence where applicable.
- Hosted CI limitations must be recorded rather than disguised as product passes or failures.
- Wicked remains pinned; do not modify Wicked source without an explicit, justified core-patch decision.
- New Wicked features enter the feature matrix at controlled sync points; they do not silently expand an active gate.
