# Story Flow Gate 6 — Journey View MVP

Status: implementation and exact Release owner acceptance passed on
`feature/story-flow-gate6-journey-view`; PR #85 closeout is pending.

Baseline: merged `main` commit
`c133221b63c744f737ec09da1fa2800158ae22ea` (PR #84).

## Purpose

Gate 6 makes Journey View the primary Story Flow authoring experience while
retaining Graph View as the exact topology/diagnostic projection of the same
authoritative `FlowDocument`.

There is no Journey semantic format. Cards, tracks, pan, zoom and manual visual
offsets are presentation state only. All node, route, outcome, priority,
condition, Level reference and Screen reference edits continue to use the
existing validated Story Flow authoring session.

## Locked implementation contract

1. Journey View is the default view for a newly built or migrated editor
   layout. The selected view persists with the project-local layout document.
2. Journey and Graph consume the same `StoryFlowAuthoringModel`, selection and
   authoring session. Switching views cannot save, reload or translate semantic
   Flow data.
3. The main reel follows the first deterministic outgoing route from Game
   Start. Additional deterministic routes create subordinate branch tracks.
   Merges and loops target the existing card rather than duplicating semantic
   nodes. Unreachable nodes remain visible on deterministic detached tracks.
4. Level and Screen cards expose stable identity/path context where available.
   Gate 6 does not fabricate image thumbnails when no preview asset exists.
5. Selecting a card exposes its exits in the Inspector. Selecting an exit uses
   the existing route editor, reconnect and condition boundaries. Creating a
   connection is source selection plus destination-card selection; no wire
   drawing is required in Journey View.
6. Double-clicking a Level card/node opens the existing 3D Level Editor through
   the accepted Gate 4 open path. Double-clicking a Screen card/node invokes the
   accepted Gate 5 Screen Editor handoff. Game Start and terminal nodes do not
   activate an editor.
7. A successful New Level, Existing Level or New Screen transaction selects and
   focuses the created card. It must not silently reset the visible selection to
   Game Start.
8. Journey-specific canvas state and per-card visual offsets persist separately
   from Graph canvas/node positions. Reading schema-v1 Graph-only layouts
   migrates them without losing existing Graph state.
9. Gate 6 retains the dedicated Story Flow render path. The 3D Level Editor is
   inactive until a Level is explicitly opened.

## Acceptance

Gate 6 is accepted only when all of the following are true on the exact tested
commit:

- a new/opened project lands in Journey View;
- Level, Screen, terminal and Game Start cards are visible on deterministic
  main/branch tracks;
- adding a Level or Screen gives immediate visible selection/focus feedback;
- routes can be created and edited in Journey View without drawing wires;
- Journey edits appear immediately in Graph View and Graph edits appear
  immediately in Journey View;
- selection survives Journey/Graph toggling;
- double-click opens Level and Screen destinations through their existing
  lifecycle boundaries;
- Journey and Graph presentation state survives close/reopen;
- semantic save/reopen and Runtime traversal remain unchanged;
- Windows Debug and Release compile/test suites pass;
- the packaged Release receives an owner visual/interaction test. Unit tests or
  a hand-written GUI ordering probe are not accepted as proof of rendered UI.

## Explicit exclusions

- visual Screen Editor implementation (Gate 8);
- removal of the legacy blank startup Scene/project template constraint (Gate
  7 completion);
- semantic zoom, minimap, search, branch collapse, chapters, auto-splice and
  large-project tooling (Gate 9);
- Runtime/build/standalone programme closeout (Gate 10);
- Wicked Engine source or file-format changes.

## Acceptance evidence

- PR implementation head:
  `9f827dd4ccf9675f3c237dd2dcf25f5ab6f3778d`.
- Renegade Studio run 668 and Windows baseline run 1247 passed Debug and
  Release.
- Owner-tested Release artifact:
  `renegade-studio-windows-x64-Release-92bc0cf4dcfa8e474104cae0c557de1306df6b90`.
- Artifact SHA-256:
  `d7f2f67b8c6e9cebc96d77274e6c0a646eba9111e05671980b1a7e7de13ff0e8`.
- On 2026-08-21 the project owner completed the packaged audit and reported
  PASS. Required nodes were created, Journey/Graph and persistence checks
  passed, and Level-card double-click activation opened the governed editor.

The owner found the MVP interaction difficult to understand because selection,
connection direction and completion feedback are weak. That is recorded as
future Journey UX work rather than a failure of the Gate 6 functional scope;
Gate 9 owns the advanced Journey usability and scale programme.
