# Renegade Engine — Current Handoff

**Date:** 2026-09-01

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Authoritative main:**
`0b3a162b88a951097413b029d262487d892d7a6a`
(`Phase 5 Gate 5: post-processing and image quality (#113)`).

**Wicked pin:**
`3a800b7134aafe58461093c8abb2e274d4e64033`

## Active recovery — editor frame-loop performance

Branch: `perf/editor-frame-loop-recovery`.

Base: unmerged `wd01/wicked-vegetation-clean` head
`b54a22104b965f7b5d463056b76cb2194acd33b7`.

Implementation commit:
`67ed669ab58e5c4c53967bc38e3976407df61b56`.

The owner-reported Level fell from the 75 Hz VSync cap to approximately 30 FPS.
The static audit found project/resource repair, synchronous diagnostics and
whole-system synchronization in ordinary frame paths. This recovery removes
those architectural defects without lowering terrain, vegetation or render
quality.

Implemented:

- removed governed texture restoration from
  `CreatorAssetStudioChrome::Update`; explicit Scene open and staged project
  adoption restoration remain intact;
- retired synchronous `PR58Gate1Lifecycle.log` writes while retaining
  operation-scoped Wicked backlog timings;
- removed ordinary Studio ownership of native environment-probe debug drawing;
  Gate 9 Diagnostics remains sole authority;
- added `SceneService::Revision()` and revision-gated Studio/Runtime render and
  LUT synchronization;
- replaced WD01's idle whole-terrain synchronization with an O(1) terrain
  identity/chunk-count gate, retaining native Wicked grass painting and
  completed-stroke rebuild behavior; and
- added `RenegadeEditorFrameLoopRecoverySourceContract`, corrected the older
  Gate 3/Gate 4 contracts that required the bad behavior, and added a bridge
  revision assertion.

Changed files are the 20 paths recorded by commit `67ed669`, bounded to
`SceneService`, Studio/Runtime frame lifecycle, WD01 synchronization, PR58
diagnostics, source contracts and recovery documentation. The Wicked submodule
pin is unchanged.

Local evidence:

- `git diff --cached --check` — PASS before implementation commit;
- fixed-string negative checks for per-frame texture restore, ordinary Studio
  probe-debug ownership and PR58 file I/O — PASS;
- fixed-string positive checks for Studio/Runtime Scene revision gates, Gate 9
  probe ownership and WD01 chunk lifecycle gating — PASS;
- modified-document local-link validation — PASS;
- CMake source-contract execution — NOT RUN because CMake is unavailable in
  this Linux worker;
- Windows native compile and GPU profiling — pending authoritative CI/owner
  evidence.

Owner performance result and remaining blocker:

- draft PR #122 targets `main`; all four required checks passed;
- the owner measured 75 FPS in both an empty and populated Level, restoring the
  75 Hz VSync cap without reducing scene or vegetation quality;
- `wd01/wicked-vegetation-clean` remains unmerged because affected menus do not
  react to the mouse and Hierarchy selection works while collapse/expand does
  not; and
- `fix/wd01-ui-input-lifecycle` now carries a bounded source repair. The
  vegetation handler's outside-viewport stroke-release path was unreachable
  whenever native GUI held focus because `StudioRenderPath::Update()` returned
  at `GetGUI().HasFocus()` first. The repair evaluates vegetation release before
  shortcut, pending-action, chrome-consumption and native-focus exits, then
  preserves the existing ownership return before scene/gizmo input. The WD01
  source contract locks this ordering. Owner interaction testing is still
  required; compile-only CI cannot prove the mouse behaviour.

Next required work:

1. publish the UI input-lifecycle repair on top of PR #122's exact head and run
   Windows x64 Studio Debug/Release plus baseline Debug/Release CI;
2. owner-test top menus, dropdowns, Hierarchy category/entity disclosure,
   selection, and Inspector controls before and after a grass stroke whose
   release occurs over the top bar, Hierarchy, Inspector and bottom drawer;
3. re-confirm the populated Level remains at the 75 Hz VSync cap;
4. confirm multi-chunk grass paint/delete/Undo/Redo/save/reopen/Runtime parity;
   and
5. keep PR #122 draft and do not merge to `main` until the owner explicitly
   accepts both interaction and performance on the exact tested head.

## Active work — Phase 5 Gate 6 AO / GI / Reflections

Branch: `phase5/scene-render-gate6-ao-gi-reflections`.

Gate 5 is owner-accepted and merged. The exact accepted Gate 5 product tree is
on `main`; LUTs and the complete post-processing workspace were exercised in a
real Release build, including DOF and motion blur under their required camera /
motion conditions. HDR output calibration is correctly inert on SDR and its
wording is deferred to the later global UI polish pass.

Gate 6 extends the same `RenderSettingsState`, hidden WISCENE Metadata carrier,
`SetRenderSettingsCommand`, `ApplyRenderSettingsToPath()` seam and RENDER
workspace. There is no second render-state model and no stock Wicked Editor UI.

Current implementation commit:
`4c77f3a9e97edb35c5570365bde7e1f0c92fa388`.

Implemented Gate 6 boundary:

- AO: OFF / SSAO / HBAO / MSAO, power, SSAO range and sample count;
- screen-space GI: SSGI, depth rejection and GI Boost;
- reflections: SSR toggle/quality/roughness cutoff plus planar reflection
  toggle/resolution scale/MSAA;
- render settings schema v2 with lossless schema-v1 Gate 5 migration;
- Studio/Runtime shared application, command history and deterministic defaults;
- focused Gate 6 backend regression and source contract; and
- RTAO/ray-traced reflections/ray-traced diffuse/path tracing explicitly kept
  out of Gate 6 for Gate 7.

The guarded implementation passed `git diff --check`, the retained Gate 5
source contract and the new Gate 6 source contract before the temporary staging
workflows self-cleaned. The production branch history was then reparented to
remove those staging workflows completely.

Next required evidence is one real PR cycle: Studio Debug/Release and Windows
baseline Debug/Release on the complete Gate 6 candidate, followed by one owner
Release build. Green compilation alone does not accept Gate 6. Do not merge
until the owner verifies AO/GI/reflection behaviour and Gate 5 regression parity.

Contract: `docs/PHASE5_GATE6_AO_GI_REFLECTIONS.md`.

## Historical work — Scene UI Gate 6 consolidated acceptance

Branch: `recovery/scene-ui-gate6-consolidated-acceptance`.

Scene UI Gate 5 is complete. Corrected head
`93659c274fa1ce6edf68e2478dfbfa8c9e3298a4` passed all four required
Windows checks, the packaged Release passed project-owner runtime testing, and
PR #105 merged as `1e0470a9e530dd20c42ddf16662c3771aaede825` on
2026-08-27. The rejected truncated-package heads remain failure evidence only.

### Build Windows Game blocker repaired in this candidate

Implementation commit:
`a2f65c3173d796f8dda08f8a8a7332017178408f` (tree
`ac121145dee8c76907847636ce736fec572c16ca`). Changed code and regressions are
bounded to `EngineBridge` standalone project preparation, the Studio chrome /
application / Story Flow build handoff, the Windows build controller and the
two focused build/source-contract tests.

The owner found two independent standalone-build blockers during Gate 6
acceptance. The Scene BUILD menu bypassed authoring saves and read a stale
on-disk Story Flow, while generated Terrain scenes referenced Renegade's
default grass beside Studio and were correctly rejected as outside-project.

The recovery candidate:

- routes both Scene and Story Flow Build buttons through one save-first Scene
  then Story Flow handoff;
- blocks packaging on save cancellation/failure;
- makes Story Flow `Runtime Ready` and saved build preparation use the same
  deterministic Game Start-to-Complete Game route authority;
- admits only exact filesystem-equivalent Scene references to four declared
  Renegade Terrain/weather resources;
- hashes and stages those same declarations at package-root `Content/...`
  destinations; and
- retains fatal rejection for arbitrary outside-project dependencies.

Focused tests cover the shared route, stale direct-call rejection, exact-file
admission, unrelated-file rejection and package-plan destination. PR #106 must
not merge until the sequence in
`docs/SCENE_UI_GATE6_BUILD_GAME_RECOVERY.md` passes.

Gate 6 is the final whole-editor integration/acceptance pass, not another
feature gate. The current source audit confirms the established wiring remains:

- explicit Story Flow, Level Editor and Screen Editor transitions;
- dirty-state protection before returning from Screen Editor;
- Hierarchy/Inspector/selection/gizmo/navigation ownership;
- object/Terrain surface picking and command-owned reusable asset placement;
- transient-safe Scene Save plus Reopen;
- independent command-owned Environment and Terrain creation/authoring;
- unsaved Test Level and governed Story Flow Preview Runtime paths; and
- the accepted Windows game build workflow.

The implementation adds one consolidated source contract while retaining every
Scene UI Gate 2-5 contract. It also reconciles the stale README, roadmap,
handoff and feature evidence that still described Gate 4 or the rejected Gate 5
candidate as current.

Implementation commit:
`19114d586531748f9a39b9033703c71872a82202`.

Changed files:

- `CMakeLists.txt`;
- `Tests/SceneUiGate6.cmake`;
- `Tests/SceneUiGate6SourceContract.cmake`;
- `docs/SCENE_UI_GATE6_CONSOLIDATED_ACCEPTANCE.md`;
- `docs/SCENE_UI_GATE5_ENVIRONMENT_TERRAIN.md`;
- `README.md`;
- `docs/ROADMAP.md`;
- `docs/FEATURE_MATRIX.csv`; and
- `HANDOFF.md`.

Local evidence:

- `git diff --check` — PASS;
- manual execution of every Gate 6 source-contract assertion with fixed-string
  `rg` checks — PASS;
- `docs/FEATURE_MATRIX.csv` parsed as 36 structurally consistent 16-column rows
  — PASS;
- modified Markdown local-link target validation — PASS;
- direct Gate 6 Build Game recovery source-seam assertions with `rg -Fq` — PASS;
- direct GNU C++17 Journey fixed-shell/role regression — PASS;
- local CMake/Windows build and `RenegadeWindowsGameBuildProjectTests` — not run
  because CMake, Wicked and the Windows toolchain are unavailable in this Linux
  scratch environment; exact-head CI is authoritative.

Required next evidence:

1. require exact-head Studio Debug/Release and baseline Debug/Release CI;
2. download one packaged Release;
3. run the consolidated owner sequence at 1280x720, 1680x945 and 1920x1080 by
   resizing the same build; and
4. do not merge before explicit project-owner acceptance.

Horizon visibility, terrain/grid datum alignment and +/- terrain-ring controls
are deliberately deferred to a subsequent editor-polish gate.

The complete boundary and owner sequence are in
`docs/SCENE_UI_GATE6_CONSOLIDATED_ACCEPTANCE.md`.

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
