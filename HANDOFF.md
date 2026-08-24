# Renegade Engine — Current Handoff

**Date:** 2026-08-24

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Authoritative main:**
`1c1d580df3c40de2fbd68dee41c0c8a74e32f831`
(`Story Flow Gate 9D: navigation and Graph cleanup (#98)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Active work — Journey recovery Gate 9A

Draft PR: `#100`, branch `recovery/story-flow-journey-9a-shell`.

The recovery starts from accepted Gate 9D main, not rejected PR #99. Graph Flow
is frozen and out of scope. The owner-approved sequence remains 9A through 9F;
this branch is still an incomplete 9A implementation and is not an owner
candidate.

Implementation commit `09ac627cbebb7e9dc1b370e2baa071917653aa2f`
removes the legacy Level/Screen presentation rather than hiding it:

- deleted `RenegadeStoryFlowLevelPanel.h` and
  `RenegadeStoryFlowScreenPanel.h`;
- removed their integration-owned panel state, GUI registration, layout and
  lifecycle-layer reordering;
- added one compact Journey-native `Add Destination` sheet opened by the Levels
  or Screens rail item;
- retained governed New Level, Existing Level, New Screen, selected-card open,
  stable-ID resolution and Level/Screen Editor handoffs through the existing
  EngineBridge services;
- retained card double-click activation and Return to Story Flow;
- added a source contract that rejects reintroduction of either legacy panel or
  bypass of the accepted lifecycle/reference services.

Local evidence:

- `git diff --check` — PASS;
- Journey recovery source assertions using `test`/`rg` — PASS;
- `cmake -DRENEGADE_SOURCE_DIR="$PWD" -P Tests/StoryFlowJourneyRecovery9ASourceContract.cmake`
  — NOT RUN because CMake is unavailable in the Linux scratch environment.

Windows exact-head evidence and run IDs must be recorded after the source
contract/documentation follow-up commit. Visual acceptance remains explicitly
open: green CI proves compilation only. Required proof is a real packaged
Release screenshot at 1920x1080 and 1280x720, compared against the approved
concept. No merge or Gate 9A completion claim is permitted before owner visual
acceptance.

Next bounded work after this cleanup is the remaining 9A shell fidelity and
real top/left command wiring. 9B then owns the approved large rounded/shadowed
main reel cards and thumbnails; 9C owns role-coloured alternate branch rows and
Inspector routing; 9D owns constrained Journey navigation and the fixed-size
overview; 9E owns nonlinear Journey authoring; 9F owns diagnostics and final
acceptance.

## Gate 6 accepted evidence

Gate 5/PR #84 is merged and owner-accepted. New/Open projects enter the
dedicated Story Flow render path, governed Level and Screen lifecycle controls
remain visible, explicit Level open/return works, Screen Editor handoff resolves,
and Flow edits persist. Owner testing also established the remaining real UX
defects: double-click activation was absent and governed Level/Screen creation
could reset the visible workspace selection to Game Start.

Gate 6 implements the locked primary Journey experience without changing
Runtime semantics:

- deterministic main and alternate Journey tracks over the shared authoring
  model, including merge/cycle safety and detached unreachable content;
- Level/Screen cards with identity/path context where available;
- Inspector exit selection and existing route edit/reconnect boundaries;
- persisted Journey/Graph view toggle with shared selection and immediate
  semantic synchronization;
- schema-v2 presentation state with lossless schema-v1 Graph migration;
- presentation-only Journey card offsets and independent Journey canvas state;
- double-click dispatch to the accepted Level and Screen open boundaries;
- selection/focus feedback after governed content creation.

The UI-independent Gate 6 presentation test passes locally through a direct GNU
C++ build. Studio source and integration headers also pass local syntax checks.
Renegade Studio run 668 and Windows baseline run 1247 passed Debug and Release
for implementation head `9f827dd4ccf9675f3c237dd2dcf25f5ab6f3778d`.

The project owner completed the exact packaged Release audit on 2026-08-21 and
reported PASS. All required nodes were created, the remaining Journey/Graph and
persistence checks passed, and Level-card double-click activation opened the
governed editor. The tested artifact was:

`renegade-studio-windows-x64-Release-92bc0cf4dcfa8e474104cae0c557de1306df6b90`

SHA-256:
`d7f2f67b8c6e9cebc96d77274e6c0a646eba9111e05671980b1a7e7de13ff0e8`.

The owner found the current MVP interaction badly signposted. Selection,
connection direction and completion feedback remain weak, but this is recorded
for the Gate 9 Journey UX programme rather than treated as a Gate 6 functional
failure. The original owner-test instruction also incorrectly attempted to add
a second Game Start route; it now extends the seeded
`Game Start -> Main Level` journey from Main Level.

Gate 6 implementation, owner acceptance and PR #85 merge are complete.

Acceptance is locked in `docs/STORY_FLOW_GATE6_JOURNEY_VIEW_MVP.md`.

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

- **Gates 1-5:** accepted and merged through PR #84.
- **Gate 6:** accepted and merged through PR #85.
- **Gate 7:** accepted and merged through PR #86; Hub -> Story Flow by default,
  startup Flow + permanent Game Start, and no arbitrary blank startup Scene.
- **Gate 8 (active):** Screen Editor MVP; 8A and 8B are accepted, and 8C is
  the current authoring-shell slice. Gate 8 contains only 8A through 8E.
- **Gate 9:** implement the recovered approved Journey View concept as the real
  Story Flow UI/UX, then complete large/nonlinear Journey authoring and scale.
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
