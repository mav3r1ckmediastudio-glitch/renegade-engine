# Renegade Engine Roadmap

**Current authoritative main:**
`1e0470a9e530dd20c42ddf16662c3771aaede825`
(`Scene UI Gate 5: recover Environment and Terrain (#105)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Immediate stabilization programme

**Scene UI Gate 6 — Consolidated Whole-Editor Acceptance is active.**

Gate 6 is the final cross-workspace hardening pass over the accepted Scene UI
recovery. It runs exact-head four-way CI and one packaged Release through
Project Hub, Story Flow, Level and Screen editors, Asset Browser/import/place,
Environment/Terrain, save/reopen, Test Level and Build Windows Game at
1280x720, 1680x945 and 1920x1080. Only concrete integration defects found by
that pass may change code.

Horizon visibility, terrain/grid datum alignment and +/- terrain-ring controls
remain a later editor-polish gate rather than expanding Gate 6.

Contract: `docs/SCENE_UI_GATE6_CONSOLIDATED_ACCEPTANCE.md`.

## Broader product programme

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

**Status:** accepted and merged through PR #85. Journey View is the default
projection; Graph remains synchronized; creation focus and double-click
activation passed the exact packaged Release owner audit.

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

**Status:** accepted and merged through PR #86. New Project creates a native
Flow project home without an invented placeholder Scene; Runtime, dependency
closure and Project Hub all accept that launch root.

Complete the project-home transition begun by the Gate 5 recovery. Remove the
arbitrary blank `Main.wiscene` requirement from new-project construction rather
than merely adopting it into a canonical Flow, and finish dedicated project
templates/migration policy around permanent Game Start. The locked contract is
`docs/STORY_FLOW_GATE7_PROJECT_HOME_LIFECYCLE.md`.

### Gate 8 — Screen Editor MVP

Native Screen Editor with image/background/text/button/layout/action authoring,
preview, validation, Save/Undo/Redo and Return to Story Flow.

**Status:** Gates 8A, 8B, 8C and 8D are accepted and merged. Gate 8E is
active on `feature/story-flow-gate8e-outcome-parity`.

Gate 8 is delivered as bounded internal slices so the editor is not built on
the old runtime-proof schema:

1. **8A — Screen contract v2:** fully serialized canvas scaling, parent/anchor
   layout, editable normal/hover/pressed/focused/disabled appearance, explicit
   built-in or project `.ttf` font identity, lossless v1 migration and Runtime
   consumption. No visual property may be supplied by hidden Runtime styling.
2. **8B — shared Screen renderer (accepted):** one Renegade-owned
   layout/presentation core used by editor preview and Runtime, including
   remaining shape/border fidelity and removal of Wicked's implicit disabled
   fade.
3. **8C — authoring shell (accepted):** first-class Screen Editor path,
   hierarchy, stable selection, exact shared Runtime preview, Screen-specific
   history, transactional Save/Open and guarded Return to Story Flow. Contract:
   `docs/STORY_FLOW_GATE8C_SCREEN_EDITOR_SHELL.md`.
4. **8D — creator controls (accepted):** direct move/resize, editable
   image/background/text/button controls, complete visual states/typography,
   governed resource selection, symbolic actions, focus order, parent/layout,
   presets/components and creator Undo/Redo/Save/Open. Contract:
   `docs/STORY_FLOW_GATE8D_CREATOR_CONTROLS.md`.
5. **8E — Story Flow and packaging acceptance (active):** Screen Button action
   IDs remain symbolic while Story Flow exclusively owns destinations; invalid
   action/route mappings are diagnosed in Studio, fail closed for reachable
   Runtime Screens and are proven through save/reopen and packaged standalone
   parity. Contract: `docs/STORY_FLOW_GATE8E_OUTCOME_PARITY.md`.

### Gate 9 — Journey View UI/UX implementation and scale

Implement the recovered approved Journey View concept as the real Story Flow
creator experience: Renegade Studio chrome, navigation rail, journey reels and
branch lanes, visual cards, Inspector, overview/minimap and the complete
interaction language. Concept fidelity is owner-reviewed UI/UX work, not an
implicit side effect of the functional gates.

The same gate adds semantic zoom, branch collapse, search/focus,
chapters/groups, auto-splice, loops/hubs/returns and large-project diagnostics.

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
