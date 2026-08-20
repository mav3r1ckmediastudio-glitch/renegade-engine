# Renegade Engine Roadmap

**Current authoritative main:**
`02df129f96c860dd3a7d6b6e065c928bef0f8907`
(`hub: expose Exit Renegade action (#65)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Current product programme

The immediate active programme is **Renegade Story Flow**.

Story Flow is the project-level authoring home for the complete player journey.
It will use one authoritative semantic Flow document exposed through two
synchronized views:

- **Journey View** — primary/default creator surface;
- **Graph View** — secondary structural/logic surface.

Story Flow is also locked as a **first-class Studio render path/workspace**.
The finished product must not leave the 3D Level Editor continuously running
behind Story Flow. A Level or Screen editor is entered only when that content is
explicitly opened and returns control to Story Flow afterwards.

The full programme contract is in:

`docs/STORY_FLOW_JOURNEY_VIEW_IMPLEMENTATION_PLAN.md`

## Story Flow gate status

### Gate 1 — Shared Story Flow foundation and Studio workspace

**Status:** implementation and owner visual/interaction acceptance passed;
final documentation/exact-head PR closeout remains before merge.

Implemented on PR #66:

- read-only presentation-independent model over LP02 `FlowDocument`;
- stable node/route indexes and diagnostics;
- deterministic default presentation;
- separate layout persistence under `Saved/EditorState/StoryFlow/`;
- native wiGUI Story Flow surface;
- startup Flow resolution for an opened project;
- node/route rendering with outcome labels;
- selection, cursor-relative zoom, middle-mouse pan, `FIT` and `START`;
- controlled four-node owner-test fixture;
- tests proving presentation state does not change Runtime Flow traversal.

Exact implementation evidence:

- tested implementation head:
  `6f02f00519b344faa2fbe9a0f0d9d9174ad3f8d4`;
- Renegade Studio run 628: success;
- Windows baseline run 1156: success;
- owner Release acceptance on 2026-08-20: PASS;
- visible route:
  `Game Start -> Level One -> Level Two -> Complete Game`;
- owner confirmed node selection, zoom, pan, `FIT` and `START` all work.

The current Gate 1 workspace is intentionally hosted over the existing 3D
Studio render path. That condition is accepted only as the Gate 1 integration
scaffold. It is not the final Story Flow architecture.

### Gate 2 — First-class Screen semantics

Next after Gate 1 merge.

Extend the real Flow/runtime contract so `Screen` is a first-class executable
Flow destination backed by stable runtime-screen identity and named outcomes.
Acceptance is a real Runtime journey such as:

`Game Start -> Title Screen -> Level -> Victory Screen -> Complete Game`

### Gate 3 — Dedicated Story Flow render path, core editing and Graph View

Before Story Flow becomes the long-lived editable authoring surface, retire the
Gate 1 overlay scaffold and give Story Flow its own render path/workspace.
Then add semantic Flow mutation, Graph View, Flow-specific Undo/Redo, dirty
state, transactional Save/Open and route/node editing.

The 3D Level Editor must be inactive while Story Flow owns the surface.

### Gate 4 — Level lifecycle integration

Add New/Existing Level, governed Scene creation/identity, moved/missing
resolution, Level open into the 3D editor and explicit Return to Story Flow.

### Gate 5 — Screen lifecycle integration

Add governed Screen creation/templates/identity, Screen outcome exposure and the
open boundary for the Screen Editor.

### Gate 6 — Journey View MVP

Build the primary creator experience on the dedicated Story Flow render path:
main journey reel, Level/Screen cards, branch tracks, previews, Inspector-driven
exits and Journey/Graph synchronization.

### Gate 7 — New Project / project-home lifecycle

Make `Project Hub -> Story Flow` the normal project-open/create journey.
New projects receive a startup Flow and permanent Game Start rather than landing
in an arbitrary blank `Main.wiscene` purely to satisfy the old scene-first
lifecycle.

### Gate 8 — Screen Editor MVP

Native Screen Editor with image/background/text/button/layout/action authoring,
preview, validation, Save/Undo/Redo and Return to Story Flow.

### Gate 9 — Advanced Journey authoring and scale

Semantic zoom, minimap, branch collapse, search/focus, chapters/groups,
auto-splice, loops/hubs/returns and large-project diagnostics.

### Gate 10 — Runtime/build/standalone closeout

Prove end-to-end semantic parity:

`Hub -> Story Flow -> Screens/Levels -> save -> reopen -> Runtime -> Windows build -> standalone`

No editor/runtime disagreement is acceptable.

## Accepted platform foundations on current main

The old roadmap stopped around LP06. Current main is substantially further
advanced.

### LP04 — Unsaved Test Level Snapshot

Accepted. Studio can snapshot unsaved authoring state and launch the real
separate Runtime without forcing an authoritative save.

### LP05 — Representative Dependency Extraction

Accepted. Renegade owns deterministic dependency discovery across project,
Story Flow, Runtime Screen, WISCENE, model resources and explicit references.

### LC01 — Asset Identity and Source Tracking

Accepted. Stable project asset IDs, transactional asset registry persistence,
source/import provenance and moved/missing recovery are established.

### LP06 — Named Standalone Windows Build / safe rebuild

Accepted. Renegade can build a named Windows game package with deterministic
closure, integrity validation, safe staging/promotion and direct standalone
launch. Distribution hardening/signing/installer policy remains later work.

### LP07 — Reusable Project Asset Workflow

**Complete and accepted through PR #51.**

Current main includes the full reusable model-asset lifecycle:

- format-neutral importer seam;
- pinned Wicked FBX/ufbx path plus GLB/GLTF regression;
- stable registry-backed catalogue;
- governed `.rasset` import transaction;
- explicit stable reimport;
- creator Asset Browser workflow;
- repeatable placement;
- packaged Runtime acceptance.

### LP08 — Governed non-model resource pipeline

**Gates 1–5 accepted through PR #56.**

Current main includes the governed resource seam and texture workflow,
transactional resource assets, material texture binding, stable resource
reimport and packaged Runtime resource resolution/cache identity.

Broader audio/video/script/font creator workflows remain future work on the
accepted common resource foundation.

### Creator workflow recovery — PR #57

Merged as `a7775f31c5ec1ff61463d495e7db6ac4a5d63258`.

The creator model workflow now includes the repaired preview/import/placement
lifecycle, governed textures/materials, creator transforms and scale handling,
asset browser thumbnails/drag placement, atomic package writes and the broader
acceptance fixes proven during PR #57.

### Project Hub and project lifecycle — PR #58 through #65

Current main includes the rebuilt native Project Hub and the post-PR58 lifecycle
hardening:

- startup reveal / identity / Hub presentation;
- visual recent-project cards;
- staged project adoption after startup-scene validation;
- unsaved-scene protection across project/exit transitions;
- responsive real-phase project loading overlay;
- governed texture restore deduplication;
- explicit Exit Renegade action.

These foundations are the launch point for Story Flow.

## Product architecture rules carried forward

- Renegade Studio owns the UX; do not expose stock Wicked Editor windows.
- Wicked functionality is consumed through Renegade-owned services and native
  Renegade UI.
- Stable IDs are authoritative; paths are hints.
- Semantic documents and editor presentation/layout remain separate.
- Persistent creator mutations require Undo/Redo plus Save/Open proof.
- Multi-document writes use transactional/fail-closed discipline.
- Hosted/local compilation alone is never creator acceptance.
- GitHub CI remains authoritative when owner-machine CPU instability makes local
  compilation unreliable.
- Owner visual/behavioural findings override green CI.
- Wicked remains pinned unless a deliberate, justified core-patch decision is
  made.

## Broader phase position

| Phase | Outcome | Current position |
|---|---|---|
| 0. Charter and baseline | Traceable target and pinned Wicked source | Complete |
| 1. Reproducible build | Reliable Wicked/Renegade Windows build | Complete |
| 2. Architecture/UI proof | Branded Studio/Runtime architecture proof | Complete |
| 3. Studio foundation | Dependable project/scene/editor workflows | Substantially complete |
| 4. Project and asset pipeline | Repeatable governed assets/import/reimport/browser | Substantially advanced |
| 5. Scene/render exposure | Core visual/world systems authorable | Partially advanced |
| Story Flow programme | Player-journey authoring/project home | **Active** |
| 6. Physics/audio/gameplay | Small interactive packaged game | Future |
| 7. Advanced systems | Animation, particles and specialist systems | Future |
| 8. Scripting/runtime/export | Runtime/export maturity | Partially advanced |
| 9. Exposure audit/platforms | Coverage/performance/platform audit | Future |
| 10. Beta and V1 | Hardened supportable release | Future |

## After Story Flow

The remaining high-level engine roadmap still includes:

- broader governed audio/video/script/font creator workflows;
- remaining materials/render/world exposure gaps;
- physics and character controller authoring;
- input and audio gameplay foundations;
- animation/retargeting/IK workflows;
- particles/effects;
- generic projectile/magic framework;
- scripting/gameplay framework;
- richer Runtime state/save systems;
- networking/multiplayer where justified;
- distribution/signing/installer/update hardening.

Higher-level systems should consume the accepted project, asset, Flow, Runtime
and build contracts rather than introducing parallel file/runtime models.

## Status rules

- A gate completes only with runnable evidence appropriate to the feature.
- Compilation alone is not behavioural proof.
- Visual/behavioural failure overrides green CI.
- Persistent creator mutations require Undo/Redo and Save/Open evidence.
- Hosted limitations are recorded, not hidden by weakening the product contract.
- New Wicked features enter through deliberate roadmap/matrix review.
