# Renegade Story Flow — Journey View Implementation Plan

**Status:** Locked product direction; implementation programme starts with Gate 1.

**Prepared:** 2026-08-20

**Repository:** `mav3r1ckmediastudio-glitch/renegade-engine`

**Audited implementation baseline:**
`02df129f96c860dd3a7d6b6e065c928bef0f8907`
(`hub: expose Exit Renegade action (#65)`).

**Wicked pin:** `3a800b7134aafe58461093c8abb2e274d4e64033`

## 1. Locked product definition

Story Flow is the project-level authoring home for the complete player journey,
not only a level-sequencing graph.

The normal creator journey is:

`PROJECT HUB -> CREATE/OPEN PROJECT -> STORY FLOW -> SCREEN/LEVEL -> EDITOR`

Opening or creating a Renegade project should ultimately land in Story Flow.
A Level is opened from Story Flow into the existing 3D Level Editor. A Screen
is opened from Story Flow into the future Screen Editor. Returning from either
content editor returns the creator to Story Flow.

Story Flow represents the executable journey from application start to game
completion/return/quit, including player-facing game screens such as splash,
title/main menu, loading, save/load, options, pause, death/failure, victory,
credits and custom screens.

HUD authoring is deliberately excluded from this programme. HUD/player-overlay
architecture will be designed separately after a robust UI/runtime system exists.

## 2. Two synchronized views, one authoritative model

Renegade Story Flow has two presentations over one authoritative Story Flow
document.

### Journey View — primary/default

Journey View is the creator-facing surface. It should feel like arranging the
player experience rather than programming a graph.

The intended visual language is a reel/track-based journey:

- a main journey reel;
- large Level and Screen destination cards;
- thumbnails/previews where available;
- alternate branches on subordinate tracks;
- exits/actions edited primarily through the Inspector;
- minimal or no visible graph wires;
- strong zoom/navigation for large projects;
- Renegade Studio visual language, not a copy of GameGuru MAX.

The visual concept produced during design discussion is directional reference,
not a pixel-for-pixel implementation contract. The shipped workspace must use
Renegade's existing dark technical styling, typography, panel geometry, native
widgets and interaction conventions.

### Graph View — secondary/logic view

Graph View exposes the same nodes and routes explicitly as a conventional
connection graph. It exists for complex branching, inspection, debugging and
advanced editing.

Graph View is not a second Story Flow format and must never become an alternate
runtime truth.

### Non-negotiable synchronization rule

There is exactly one semantic node, route, Screen reference and Level reference.

A rename, route change or deletion made in either view changes the same
underlying Story Flow document. Journey View and Graph View are projections of
that shared model and must update each other immediately.

Presentation state such as card position, graph position, track arrangement,
pan, zoom, collapsed groups and future chapter regions is editor state only and
must never alter Runtime semantics.

## 3. Existing foundations retained

LP02 already provides the authoritative Story Flow/runtime foundation:

- `renegade-document` schema v1, type `story-flow`;
- stable document, node and route IDs;
- node kinds `GameStart`, `Level`, `CompleteGame`, `ReturnToMainMenu`, `Quit`;
- Level references by stable Scene identity with project-relative path hint;
- named route outcomes;
- destination Player Entry;
- integer priority;
- state conditions;
- deterministic route selection;
- fail-closed ambiguity handling;
- Runtime traversal through `FlowInterpreter`.

That contract is extended where required; it is not replaced by visual-editor
state or by a separate Journey format.

LP03 already provides a separate `runtime-screen` document/runtime boundary.
The Story Flow programme will extend the real Flow/runtime contract so a Screen
becomes a first-class Flow destination rather than drawing fake screen nodes
that Runtime cannot execute.

## 4. Screen model direction

Creator-facing screen purposes such as Title, Loading, Options, Death, Victory,
Save/Load and Credits should be variants/templates of one underlying Screen
document concept rather than independent engine architectures.

The creator sees names such as `TITLE SCREEN` and `LOADING SCREEN`; the backend
retains one extensible Screen contract.

A Screen's authored actions become named Story Flow outcomes. For example a
Title Screen may expose:

- `new_game`;
- `load_game`;
- `options`;
- `credits`;
- `quit`.

Those outcomes route through the same stable LP02 route contract used by Levels.

## 5. Level model direction

A Level node/card remains a reference to a governed Scene/WISCENE document.
It does not embed a separate Level editor.

Double-click/open on a Level transitions to the existing 3D Level Editor. The
Level reference remains stable-ID authoritative, reports moved/missing identity
failures clearly, and returns to the same Story Flow project context.

## 6. GameGuru MAX reference policy

GameGuru MAX is a useful public implementation reference for solved creator
workflows. Its Storyboard demonstrates the value of project-wide visual flow,
Screen/Level differentiation, a Screen Editor, thumbnails and action-driven
outputs.

Renegade will not port MAX's Storyboard implementation, data structures,
fixed-capacity node model, ImGui/imnodes UI, Lua-special-case routing or visual
layout. MAX is used to understand workflow and edge cases while Renegade keeps
its own stable-ID, transactional and runtime architecture.

## 7. Implementation gates

### Gate 1 — Shared Story Flow foundation and Studio workspace

**Purpose:** establish the presentation-independent foundation required by both
Journey View and Graph View.

Deliverables:

- re-audit exact `main` before editing and record material changes;
- UI-independent read-only Story Flow authoring/presentation model over the
  existing `FlowDocument`;
- stable-ID-indexed nodes/routes and diagnostics;
- deterministic presentation ordering/default layout;
- separate Story Flow editor-layout persistence under
  `Saved/EditorState/StoryFlow/`;
- layout state contains presentation only, never routes/runtime semantics;
- native Renegade Studio Story Flow workspace boundary;
- render the existing LP02 node kinds/routes;
- pan, zoom, select and fit/centre;
- permanent Game Start representation;
- open Story Flow for an already-open project with a valid startup Flow;
- tests proving layout cannot alter Runtime traversal;
- reconcile stale `HANDOFF.md` / `docs/ROADMAP.md` statements.

**Explicit exclusions:** no New Project lifecycle change, no automatic startup
Flow creation, no Flow mutation, no Level creation, no Screen Flow node yet, no
Screen Editor, no auto-splice, no importer work and no Wicked modification.

**Acceptance:** a real existing LP02 Story Flow renders natively in Studio and
its Runtime semantics are byte/behaviourally unaffected by layout operations.

### Gate 2 — First-class Screen semantics

**Purpose:** extend the real Flow/runtime contract so Screen is executable.

Deliverables:

- first-class `Screen` Flow node/reference;
- stable Screen document identity and project-relative hint;
- creator-facing Screen purpose metadata where justified;
- Flow validation/resolution for Screen references;
- Runtime enters a Screen and receives an authored action/outcome;
- that outcome returns to the existing route-selection machinery;
- dependency/build discovery follows Screen references correctly;
- tests for `Game Start -> Title Screen -> Level -> Victory Screen -> Complete`.

**Acceptance:** Screen nodes are real Runtime destinations, never decorative
editor-only nodes.

### Gate 3 — Core Flow editing and Graph View

**Purpose:** make the authoritative Story Flow fully authorable and expose its
exact topology.

Deliverables:

- Graph View over the shared model;
- add/delete supported nodes;
- add/delete/reconnect routes;
- route outcome, destination entry, priority and condition editing;
- rename operations;
- legal-deletion rules and permanent Game Start protection;
- Flow dirty state;
- Flow-specific command history / Undo / Redo;
- transactional Flow Save/Open;
- validation and diagnostics;
- Graph presentation persistence independent of semantic Flow data.

**Acceptance:** the LP02 four-node proof and a Screen-containing proof can be
authored entirely inside Studio.

### Gate 4 — Level lifecycle integration

**Purpose:** make Level cards/nodes real governed Renegade content.

Deliverables:

- Add New Level;
- Add Existing Level;
- governed Scene/WISCENE creation;
- `.wiscene.rmeta` stable Scene identity;
- stable-ID Level reference;
- moved/missing Level diagnostics;
- double-click/open Level into the existing 3D Level Editor;
- explicit Return to Story Flow;
- atomic orchestration so Flow, Scene identity and project state cannot be
  half-created.

**Acceptance:** Story Flow -> Add Level -> edit/save Level -> return -> reopen
retains the same stable Level and Flow relationship.

### Gate 5 — Screen lifecycle integration

**Purpose:** give Screen nodes a governed project-content lifecycle.

Deliverables:

- Add Screen;
- choose purpose/template;
- governed runtime-screen document creation;
- stable Screen identity;
- moved/missing diagnostics;
- expose authored Screen actions back to Story Flow;
- double-click/open boundary for Screen Editor.

**Acceptance:** Screens persist, resolve, execute and expose real named outcomes.

### Gate 6 — Journey View MVP

**Purpose:** deliver the locked primary creator experience.

Deliverables:

- main journey reel;
- Level and Screen cards;
- previews/thumbnails where available;
- alternate branch tracks;
- contextual Inspector;
- Inspector-driven action/exit destinations;
- deterministic derivation of Journey tracks from authoritative routes;
- presentation-only manual ordering where required;
- Journey-specific layout state;
- Journey / Graph view toggle;
- immediate synchronization between both views.

**Acceptance:** ordinary Story Flow authoring can be completed in Journey View
without drawing graph wires, while Graph View displays the exact same topology.

### Gate 7 — New Project and project-home lifecycle

**Purpose:** make Story Flow the actual project home.

Current audited behaviour creates/copies `Content/Scenes/Main.wiscene` and enters
the 3D editor while `startup_flow` is empty. This gate deliberately changes that
only after the Flow workspace/content lifecycles are proven.

Deliverables:

- New Project creates/owns a startup Flow;
- exactly one permanent Game Start;
- new projects land in Journey View;
- no arbitrary blank `Main.wiscene` solely to satisfy the old startup path;
- resolve the current `startup_scene` project-validation constraint safely;
- failure rollback leaves no half-created project/Flow/Scene content;
- existing and legacy projects remain safe.

**Acceptance:** Hub -> Create/Open Project -> Story Flow is the normal creator
journey.

### Gate 8 — Screen Editor MVP

**Purpose:** make double-clicking a Screen a real authoring operation.

Minimum deliverables:

- native Screen Editor workspace;
- screen canvas;
- background/image;
- text;
- buttons;
- basic layout;
- preview;
- action authoring;
- save/dirty/undo/redo;
- validation;
- return to Story Flow;
- action changes synchronize to Story Flow outcomes and diagnose invalid routes.

**Acceptance:** a creator can build a simple Title Screen without manual source
or Lua editing and route its actions in Story Flow.

### Gate 9 — Advanced Journey authoring and scale

**Purpose:** make Journey View remain usable for large nonlinear projects.

Deliverables include:

- semantic zoom;
- compact far-zoom representation;
- richer close-zoom card detail;
- minimap/overview;
- route/action highlighting;
- branch collapse/expansion;
- search/focus navigation;
- presentation-only chapters/groups;
- auto-splice/insertion;
- safe rewiring;
- loops/hubs/returns;
- `Next`, `Restart`, `Failed` creator conventions plus custom valid outcomes;
- clear destination Player Entry editing;
- large-project readability/diagnostics.

**Acceptance:** a representative nonlinear project remains understandable and
editable primarily in Journey View.

### Gate 10 — Runtime, persistence, build and standalone closeout

**Purpose:** prove that the creator-facing Story Flow is the game Runtime uses.

End-to-end proof:

`Hub -> Story Flow -> Screens/Levels -> save -> close -> reopen -> Runtime -> Build Windows Game -> standalone`

Required verification:

- Journey and Graph views remain semantically identical;
- stable identities survive move/reopen;
- layout survives without changing Flow bytes/semantics;
- Screen/Level dependencies are complete;
- missing/ambiguous documents fail closed;
- transactional rollback/recovery is proven;
- standalone follows exactly the authored journey;
- exact-head Debug/Release CI;
- owner visual/behavioural Release-artifact acceptance;
- independent exact-head review where required.

## 8. Programme-wide invariants

1. Story Flow is Runtime truth; editor presentation never becomes a second
   executable format.
2. Journey View is primary; Graph View is secondary but fully synchronized.
3. Stable IDs are authoritative; paths remain hints.
4. Presentation layout is separate from semantic Story Flow data.
5. Game Start is unique and permanent.
6. Failures are visible and fail closed; no silent fallback to an arbitrary
   Scene or Screen.
7. Persistent semantic mutations require command-history and Save/Open proof.
8. Multi-document mutations use project transactions/rollback rather than
   sequential best-effort writes.
9. Wicked remains pinned unless a separate explicit core-patch decision is made.
10. HUDs remain outside this programme until their architecture is designed.

## 9. Definition of complete

Story Flow is complete when a creator can launch Renegade, create/open a project
from the Hub, land in Journey View, create and edit Screens and Levels, author
branches/loops/exits, optionally inspect the same topology in Graph View, save
and reopen without identity/layout loss, run the project and build a standalone
Windows game whose executable follows exactly the same authored player journey.
