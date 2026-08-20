# Renegade Engine — Current Handoff

**Date:** 2026-08-20

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Authoritative main:**
`02df129f96c860dd3a7d6b6e065c928bef0f8907`
(`hub: expose Exit Renegade action (#65)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Active work — Story Flow PR #66

PR #66:
`Story Flow Gate 1: shared foundation for Journey + Graph views`

Branch:
`agent/story-flow-gate1-foundation`

Gate 1 implementation and owner visual/interaction acceptance have passed.
Final documentation reconciliation has now been applied on the branch; the live
PR head must be verified and exact-head CI must pass again before merge.

Do not treat the older PR #57/importer handoff that previously occupied this
file as current. PR #57 is already merged on main as
`a7775f31c5ec1ff61463d495e7db6ac4a5d63258`, followed by LP08/project-lifecycle
work through PR #65.

## Story Flow locked product architecture

Story Flow is the project-level authoring home for the complete player journey.
It is not just a level-sequencing graph.

There is one authoritative semantic Flow model presented through two
synchronized views:

- **Journey View** — primary/default creator experience, based on a readable
  journey reel/track with Level and Screen cards, previews, branch tracks and
  Inspector-driven exits;
- **Graph View** — secondary structural/logic view over the exact same nodes and
  routes for complex branching, inspection and advanced editing.

There is never a second Journey-specific Runtime format. Presentation/layout
state is editor state only and must not alter semantic Flow/runtime behaviour.

### Render-path requirement

Story Flow is locked as a **first-class Studio render path/workspace separate
from the 3D Level Editor**.

Target lifecycle:

`Project Hub -> Story Flow`

`Story Flow -> Level Editor -> Story Flow`

`Story Flow -> Screen Editor -> Story Flow`

The inactive 3D Level Editor must not continue rendering/ticking a scene behind
the finished Story Flow surface.

Gate 1 deliberately uses the existing 3D Studio render path as a temporary host
to prove the Flow-loading/presentation boundary without also replacing the
project lifecycle. The owner has explicitly accepted that temporary scaffold
for Gate 1 only.

The dedicated Story Flow render path is now a required **Gate 3** deliverable,
before the long-lived editable Graph/Journey UI is built on top of it.

Canonical programme document:
`docs/STORY_FLOW_JOURNEY_VIEW_IMPLEMENTATION_PLAN.md`

## Gate 1 — delivered foundation

Gate 1 remains read-only at the semantic Flow boundary.

Delivered:

- presentation-independent `StoryFlowAuthoringModel` over LP02 `FlowDocument`;
- stable-ID node/route indexes;
- deterministic presentation ordering;
- reachability/diagnostic projection;
- separate Story Flow layout document/persistence;
- deterministic layout reconciliation;
- native wiGUI `RenegadeStoryFlowWorkspace`;
- LP02 node/route rendering and outcome labels;
- permanent Game Start presentation;
- node selection;
- cursor-relative mouse-wheel zoom;
- middle-mouse pan;
- `FIT` and `START` presentation controls;
- startup Flow resolution after a project opens;
- project-ID/document-ID fail-closed validation;
- controlled Gate 1 owner-test fixture;
- tests proving presentation/layout cannot alter Runtime Flow traversal.

The temporary integration adapter displays the Story Flow surface over the
central Studio viewport after an existing project with valid `startup_flow`
finishes opening. It hides while the Project Hub/loading overlay owns the
surface and persists only presentation state.

## Gate 1 exact implementation evidence

Owner-tested implementation head:
`6f02f00519b344faa2fbe9a0f0d9d9174ad3f8d4`

Authoritative CI on that implementation head:

- Renegade Studio run **628** — success;
- Windows baseline run **1156** — success.

Owner Release acceptance on 2026-08-20:

- controlled fixture opened successfully;
- visible Flow:
  `Game Start -> Level One -> Level Two -> Complete Game`;
- all four destinations/routes rendered;
- node selection worked;
- zoom worked;
- middle-mouse pan worked;
- `FIT` worked;
- `START` worked.

Owner test record:
`docs/STORY_FLOW_GATE1_OWNER_TEST.md`

A first manually prepared external fixture ZIP was correctly rejected because it
accidentally carried an `AssetRegistry.renegade-assets` belonging to the source
project. The corrected fixture used one consistent project identity and opened
normally. This demonstrated the existing project validation failing closed; it
was not a Story Flow implementation defect.

## Gate 1 closeout rule

The code/behavioural Gate 1 acceptance is passed.

Before PR #66 is merge-ready:

1. documentation reconciliation must be present on the final branch head;
2. exact-head Renegade Studio/Windows baseline CI must be green after those
   closeout commits;
3. verify PR #66 remains mergeable and no unexpected main movement conflicts
   with the recorded `02df129...` base;
4. do not add semantic editing or later-gate scope to Gate 1 merely to avoid
   starting Gate 2 cleanly.

Do **not** merge automatically without the owner's normal merge decision.

## Next gate — Gate 2 first-class Screen semantics

After Gate 1 merge, Gate 2 extends the real Flow/runtime contract so `Screen`
is an executable Story Flow destination.

Required direction:

- Screen is a first-class Flow node/reference, not a visual-only card;
- stable runtime-screen document identity remains authoritative;
- path is only a hint;
- creator-facing purposes/templates may include Title, Loading, Options, Death,
  Victory, Save/Load, Credits and Custom while retaining one extensible Screen
  document/runtime concept;
- authored Screen actions become named Story Flow outcomes;
- Runtime enters a Screen, receives an action/outcome and returns to the same
  route-selection machinery;
- dependency/build discovery follows Screen references;
- representative proof:
  `Game Start -> Title Screen -> Level -> Victory Screen -> Complete Game`.

No Graph editing, Level creation or Journey polish should be smuggled into Gate
2 unless a concrete semantic dependency requires it.

## Gate 3 — dedicated render path + editing/Graph View

Gate 3 now explicitly owns the transition away from the temporary Gate 1
3D-editor overlay scaffold.

It must establish:

- first-class Story Flow render path/workspace;
- 3D Level Editor inactive while Story Flow is active;
- shared project/session/document state retained across transitions;
- Graph View;
- node/route create/delete/reconnect;
- outcome, Player Entry, priority and condition editing;
- rename;
- permanent Game Start protection;
- Flow dirty state;
- Flow-specific Undo/Redo;
- transactional Flow Save/Open;
- validation/diagnostics;
- graph layout persistence separate from semantics.

## Remaining Story Flow programme

- **Gate 4:** Level lifecycle — Add New/Existing Level, governed Scene identity,
  open Level Editor, Return to Story Flow.
- **Gate 5:** Screen lifecycle — governed Screen creation/templates/identity and
  Screen Editor open boundary.
- **Gate 6:** Journey View MVP — reel/cards/branches/Inspector and Journey/Graph
  synchronization on the dedicated Story Flow render path.
- **Gate 7:** New Project/project-home lifecycle — Hub -> Story Flow by default,
  startup Flow + permanent Game Start, remove arbitrary blank startup Scene
  requirement safely.
- **Gate 8:** Screen Editor MVP.
- **Gate 9:** large/nonlinear Journey UX.
- **Gate 10:** Runtime/persistence/build/standalone parity closeout.

## Existing accepted foundations on main

Current main is much further advanced than the previous handoff/roadmap stated.
Do not regress or duplicate these systems.

### LP04

Unsaved Test Level snapshot/Runtime handoff accepted.

### LP05

Representative dependency extraction accepted. Story Flow/Runtime Screens/Scene
and governed asset dependency discovery are established.

### LC01

Stable project asset identity, transactional registry/provenance and
moved/missing recovery accepted.

### LP06

Named standalone Windows build, package integrity, safe staging/promotion and
owner standalone launch accepted.

### LP07

Reusable model-asset workflow complete through PR #51:

- format-neutral import seam;
- Wicked FBX/ufbx plus GLB/GLTF regression;
- registry-backed catalogue;
- governed `.rasset` transaction;
- explicit stable reimport;
- creator Asset Browser;
- repeatable placement;
- packaged Runtime acceptance.

### LP08

Governed non-model resource pipeline advanced through Gate 5 / PR #56:

- common resource seam;
- governed resource assets;
- texture/material workflow;
- resource reimport;
- packaged Runtime resource resolution/cache identity.

### PR #57 creator workflow recovery

Merged. The creator import/preview/material/thumbnail/drag-placement workflow and
its acceptance repairs are part of current main.

### PR #58 through #65 project lifecycle

Current main includes:

- rebuilt native Project Hub;
- startup reveal/identity flow;
- visual Recent Projects cards;
- staged project adoption;
- unsaved-scene protection across destructive transitions;
- responsive real-phase loading overlay;
- governed texture restore deduplication;
- Exit Renegade action.

Story Flow must build on these accepted seams rather than replacing them with
parallel lifecycle systems.

## Critical owner-machine safety rules

The owner's local clone has historically contained an unrelated local
modification to `Tools/Windows-Build.Common.ps1` plus occasional untracked
helper/patch files.

When working on the owner clone:

- never reset/restore/discard that local build-script modification unless the
  owner explicitly asks;
- never use `git clean`;
- never use `git add .`;
- never silently move the Wicked submodule pin;
- preserve unrelated untracked files.

Owner hardware context remains relevant: Windows 10, 128 GB RAM, confirmed CPU
instability affecting local native compilation. GitHub CI Debug/Release remains
the authoritative build proof when local compiler failures may be hardware
related.

## Architecture rules

- Renegade owns the UX; never expose stock Wicked Editor windows.
- Consume Wicked functionality through Renegade-owned services/native UI.
- Stable IDs are authoritative; paths are hints.
- Semantic documents and presentation/layout are separate.
- Exactly one permanent Game Start unless the architecture is deliberately
  revised with migration/runtime proof.
- Persistent semantic mutations require Undo/Redo and Save/Open evidence.
- Multi-document writes use transactional/fail-closed persistence.
- Runtime/editor disagreements fail closeout.
- Visual/behavioural owner failure overrides green CI.
- Wicked remains pinned unless an explicit core-patch decision is justified and
  documented.

## Immediate continuation procedure

A new implementation session should:

1. inspect current `main` and PR #66 live head;
2. verify final closeout docs are present;
3. verify exact-head CI after the documentation commits;
4. report whether PR #66 is genuinely merge-ready;
5. stop for the owner's merge decision;
6. after merge, start Gate 2 from the new exact main and re-audit Flow/Screen
   semantics before editing.
