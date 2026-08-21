# Renegade Engine Roadmap

**Current authoritative main:**
`c133221b63c744f737ec09da1fa2800158ae22ea`
(`Story Flow Gate 5 recovery: make Story Flow the project home (#84)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Current product programme

The immediate active programme is **Renegade Story Flow**.

Story Flow is the project-level authoring home for the complete player journey.
It uses one authoritative semantic Flow document exposed through two
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

**Status:** accepted and merged.

Shared model, deterministic presentation, separate editor layout persistence and
the first native Story Flow proof surface are established.

### Gate 2 — First-class Screen semantics

**Status:** accepted and merged. Screen is a first-class executable Flow
destination backed by stable runtime-screen identity and named outcomes.

### Gate 3 — Dedicated Story Flow render path, core editing and Graph View

**Status:** accepted and merged. Story Flow has a dedicated render path plus
Graph authoring, transactional Save/Open, validation and Flow history.

### Gate 4 — Level lifecycle integration

**Status:** accepted and merged. New/Existing Level, governed identity,
resolution, 3D editor open and Return to Story Flow are present.

### Gate 5 — Screen lifecycle integration

**Status:** accepted and merged through PR #84. Governed Screen lifecycle and
the Screen Editor handoff are present; New/Open project entry now uses Story
Flow as the interim project home while retaining the legacy startup WISCENE.

### Gate 6 — Journey View MVP

**Status:** implementation and exact packaged Release owner acceptance passed
on PR #85 head `9f827dd4ccf9675f3c237dd2dcf25f5ab6f3778d`; documentation-only closeout and
final exact-head CI remain before merge.

Build the primary creator experience on the dedicated Story Flow render path:
main journey reel, Level/Screen cards, deterministic branch tracks,
Inspector-driven exits, Journey-specific layout state, double-click activation,
creation feedback and immediate Journey/Graph synchronization. Acceptance is
locked in `docs/STORY_FLOW_GATE6_JOURNEY_VIEW_MVP.md`.

The owner confirmed the functional Gate 6 boundary while reporting that the
MVP's selection, connection and completion feedback is difficult to understand.
That feedback is retained for Gate 9 rather than retroactively widening Gate 6
into the advanced Journey UX programme.

### Gate 7 — New Project / project-home lifecycle

Complete the project-home transition begun by the Gate 5 recovery. Remove the
arbitrary blank `Main.wiscene` requirement from new-project construction rather
than merely adopting it into a canonical Flow, and finish dedicated project
templates/migration policy around permanent Game Start.

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
