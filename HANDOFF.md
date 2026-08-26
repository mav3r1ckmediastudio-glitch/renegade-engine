# Renegade Engine — Current Handoff

**Date:** 2026-08-26

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Authoritative main:**
`628cf26574a4a2a6e8eb0a5a522d94966ad8917e`
(`Scene UI Gate 4: recover Asset Browser and placement UX (#104)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Active work — Scene UI Gate 5 Environment and Terrain recovery

Draft PR: `#105`, branch
`recovery/scene-ui-gate5-environment-terrain`.

The published foundation head `ac2827ac1bfc36d21fb285925661bf8c16cf84bf`
passes Windows baseline and Renegade Studio Debug/Release. It contains the
one-metre terrain spacing, three 1024 DDS default-grass maps, native Stars and
the dedicated snowflake DDS while retaining the accepted snow movement.

The continuation candidate adds the part that was still absent:

- standard radius 9 gives 19x19 chunks and an honest 1.254 km square;
- the raw generation-radius slider is removed from creator UI;
- the Terrain panel reports current dimensions;
- `EXPAND TERRAIN // +1 RING` increments the finite authored radius without
  `Generation_Restart()`;
- expansion Undo cancels generation and removes only the added outer ring;
- Redo asks Wicked to regenerate only missing ring coordinates;
- `RenegadeTerrainTests` covers dimensions and inner-chunk preservation;
- a Gate 5 source contract locks the DDS/Stars/snow/expansion seams;
- architecture, terrain documentation and `REN-WLD-001` record the boundary.

Pinned Wicked source inspection also found that stock distant removal calls
`chunks.erase(it)`. Those chunk entries contain the serialized height and blend
authority, so enabling `centerToCamera/removeDistantChunks` would destroy
sculpting rather than safely unload it. Gate 5 keeps authored CPU chunks
resident while native LOD, frustum/occlusion culling and the smaller physics
radius remain active. A true edited-chunk residency cache is a later terrain
architecture task; no false streaming claim is made in this gate.

Local evidence for the continuation:

- `git diff --check` — PASS;
- source/path assertions for radius 9, expansion ownership, removed raw radius,
  DDS packaging, Stars and snow — PASS;
- CMake/Windows compile — not run locally because CMake is unavailable;
- exact-head PR CI — pending publication;
- packaged Release visual/interaction/save/reopen/Runtime owner acceptance —
  pending and required before merge.

Acceptance and the exact owner sequence are in
`docs/SCENE_UI_GATE5_ENVIRONMENT_TERRAIN.md`.

## Active work — Journey recovery Gate 9A

Draft PR: `#100`, branch `recovery/story-flow-journey-9a-shell`.

The recovery starts from accepted Gate 9D main, not rejected PR #99. Graph Flow
is frozen and out of scope. The owner-approved sequence remains 9A through 9F.
The implementation has now replaced the failed legacy-scale presentation, but
the branch is not an owner candidate until exact-head Windows CI and packaged
Release visual acceptance pass.

The current recovery implementation:

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
- installs a fixed 70-pixel top bar, 96-pixel rail, bounded Inspector, fixed
  bottom navigation and fixed Story Overview;
- renders 164x214 rounded/shadowed image-led main cards with neutral borders,
  cover-cropped governed thumbnails and blue selection only;
- renders compact alternate cards inside purple/turquoise/red role rows and
  uses the same role classification for Inspector bullets;
- authors new actions and stable-ID destination rewires directly in the
  Inspector without opening Graph or drawing Journey wires;
- wires Journey search/focus, detached filtering, fit/start navigation,
  82-118% semantic zoom including the visible slider, Preview, Save and
  Undo/Redo;
- launches Story Flow Preview from the governed project descriptor through the
  supervised Runtime process without creating or cleaning an LP04 snapshot;
- replaces letter-placeholder rail/toolbar glyphs with native icon shapes;
- adds source/layout contracts that reject legacy panels, coloured card frames,
  thumbnail overflow, tiny zoom and Journey-to-Graph exit routing.

Current local evidence:

- `git diff --check` — PASS;
- `g++ -std=c++17 -IStudio/src Tests/StoryFlowJourneyUiLayoutTests.cpp ...`
  — PASS at 1680x945, 1920x1080 and 1280x720;
- Journey recovery and updated Gate 9D source assertions using `rg` — PASS;
- `cmake -DRENEGADE_SOURCE_DIR="$PWD" -P Tests/StoryFlowJourneyRecovery9ASourceContract.cmake`
  — NOT RUN because CMake is unavailable in the Linux scratch environment.

Visual acceptance remains explicitly open: green CI proves compilation only.
Required proof is a real packaged Release screenshot at 1920x1080 and
1280x720, compared against the approved concept. No merge or Gate 9A completion
claim is permitted before owner visual acceptance.

The locked recovery contract and stage ownership are recorded in
`docs/STORY_FLOW_JOURNEY_RECOVERY_9A_9F.md`.

### Gate 9F authoritative logo replacement

Implementation commit
`3d66de2983d3a1c1347aacd84fcd8f02cfb98af4` replaces the superseded narrow
RGB wordmark with the project owner's 2026-08-25 transparent fractured-crest
logo in Journey, the shared Studio chrome and Project Hub. The committed RGBA
asset removes only empty transparent margins from the supplied PNG, preserves
the complete visible artwork and is fitted proportionally into each header.
All three consumers use alpha blending; the former additive Journey/Studio
rendering is removed.

Changed seams:

- `Studio/assets/renegade-engine-fractured-crest-logo.png` and asset README;
- Studio, Journey and Project Hub logo loading/rendering;
- Studio package copy rule;
- Journey recovery source/hash contract and `REN-UI-001` evidence.

Local evidence for implementation tree
`84dcb015122efd197c3d5680044bab40a247da72`:

- supplied source SHA-256:
  `1bd906dc2fabb4ef6152edcaa4764c3cab5a247ac6ef9a4a8d0de86ab3a06cb1`;
- committed alpha-trimmed asset SHA-256:
  `9acc347e3e46602142ec9cdceeb846d3eb96fddac0b07d10ebc33a0a912e2a05`;
- `git diff --check` — PASS;
- direct logo hash/path/alpha/package/legacy-removal assertions — PASS;
- GNU C++17 `StoryFlowJourneyUiLayoutTests` — PASS with the pre-existing
  C++20 `concept` identifier compatibility warning;
- exact 184x70 Journey header-slot composition inspected locally — PASS;
- CMake source contract — NOT RUN locally because CMake is unavailable.

Required closeout remains exact-head Windows Debug/Release CI followed by the
owner's packaged Release visual check. A successful build does not prove the
logo is acceptably sized on the owner's display.

### Gate 9F consolidated UX-hardening candidate

Implementation commit
`20cf9a23ba7f2a873b71c09bed5064834e7fa550` (tree
`2533598922d8d179780e03db3f4745c49c3e13cb`) supersedes the earlier
logo-only build as a Gate 9F candidate. The supplied fractured-crest logo is
one item in this consolidated pass, not a separate gate.

The candidate also:

- raises the remaining Journey toolbar, rail, overview and Inspector copy to a
  readable high-contrast hierarchy;
- replaces raw validation codes with creator-facing diagnostics and a bounded
  honest remaining-issue count;
- removes the decorative unwired Inspector close glyph;
- routes Settings and Main Menu to explicit staged-unavailability reasons and
  visibly disables Journey-only Filter while Graph is active;
- gives an unavailable Add Action control an explicit reason (`TERMINAL`,
  `ENTRY SET`, `NO ACTIONS`, `LIMIT REACHED` or `UNAVAILABLE`);
- retains dirty-Flow save-before-Preview and blocks Runtime launch when save
  fails;
- strengthens 1920x1080 and 1280x720 hard-boundary and fixed-chrome layout
  regressions; and
- records the control and owner-acceptance matrix in
  `docs/STORY_FLOW_GATE9F_UX_HARDENING.md`.

Local evidence for that implementation tree:

- `git diff --check` — PASS;
- GNU C++17 `StoryFlowJourneyUiLayoutTests` — PASS at the locked concept,
  1920x1080 and 1280x720 geometries;
- manual Gate 9F source assertions for all surfaced shell actions, logo hash,
  Preview lifecycle, readable diagnostics and removed fake controls — PASS;
- CMake source contract — NOT RUN locally because CMake is unavailable and the
  Wicked submodule is not initialized in the Linux scratch workspace.

Exact-head Windows Debug/Release CI is intentionally deferred until the
handoff/documentation commit is published, so the next workflow cycle tests the
complete Gate 9F candidate once. Green CI will still leave owner packaged visual
and interaction acceptance open; PR #100 remains draft and must not be merged
automatically.

### Gate 9F wrapped Inspector-message correction

Owner testing of the consolidated candidate found that Status and Validation
used hard 48-character ellipsizing, leaving no way to read the complete
message. Implementation commit
`122cea3308fac6ad40edb3d77e95090539ec6aae` (tree
`0c09b8af48232a03fc6eab38b498caa1ce32cd92`) removes both truncation paths.

The correction:

- preserves complete Status and visible Validation text and wraps it at word
  boundaries inside the real Inspector width;
- safely splits a single long identifier/path rather than allowing it to clip;
- moves Graph Status directly below Validation instead of leaving it isolated
  at the bottom of the Inspector;
- computes a non-overlapping Validation/Status stack for Journey, including
  compact height; and
- adds deterministic reconstruction and 1920x1080/1280x720 placement tests plus
  source-contract rejection of the former ellipsis calls.

Local evidence for that implementation tree:

- `git diff --check` — PASS;
- GNU C++17 layout/wrapping test with `-Wall -Wextra -Werror` — PASS;
- complete ordinary-message and long-identifier reconstruction — PASS;
- manual wrapped-message source contract — PASS;
- CMake/Windows compile and packaged visual confirmation — pending exact-head
  CI and owner retest.

The previous consolidated head `27d260b8fd43e5efd938c8bfcd2db7f32914d88d`
passed Studio Debug/Release and baseline Debug/Release, including both Studio
startup checks, but is superseded because it still contained the owner-reported
message truncation. Gate 9F remains unaccepted until the corrected packaged
Release passes owner visual confirmation.

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

1. inspect current `main` and draft PR #100 live head;
2. confirm Graph files have not drifted beyond shared synchronization seams;
3. run exact-head Windows Debug and Release CI;
4. download the packaged Release artifact and capture Journey at 1920x1080 and
   1280x720 using the owner's representative project;
5. compare those screenshots against `storyflow concept(3).png` and the locked
   rules in `docs/STORY_FLOW_JOURNEY_RECOVERY_9A_9F.md`;
6. fix every visible mismatch before requesting owner acceptance;
7. keep PR #100 draft and stop for the owner's merge decision.
