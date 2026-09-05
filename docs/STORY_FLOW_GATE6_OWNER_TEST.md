# Story Flow Gate 6 — Packaged Release Owner Test

Use only the exact Release ZIP produced for the Gate 6 commit under review.
Record the outer ZIP SHA-256, inner package SHA-256, executable build identity
and tested commit before beginning. A green CI run is not visual acceptance.

Stop at the first mismatch and reject that artifact.

## 1. Project home and primary view

1. Launch `Run-RenegadeStudio-DX12.cmd`.
2. Create a new project.
3. Confirm the dedicated Story Flow surface opens directly.
4. Confirm `JOURNEY` is selected by default and the 3D Level Editor is not
   visible or ticking behind it.
5. Confirm the Level and Screen lifecycle rows are both visible and clickable.

## 2. Creation feedback and cards

1. Enter `Gate 6 Level` in the empty Level-name field and press `+ NEW LEVEL`.
2. Confirm a Level card appears, becomes selected and is brought into view.
3. Enter `Gate 6 Screen` in the empty Screen-name field, choose a template and
   press `+ SCREEN`.
4. Confirm a Screen card appears, becomes selected and is brought into view.
5. Add `Complete Game` from the Inspector and confirm its card is selected.

Creating content must never appear to do nothing or silently reset the visible
selection to Game Start.

## 3. Ordinary Journey authoring

The new-project fixture already contains `Game Start -> Main Level`. Extend that
existing main journey; do not attempt to add a second Game Start route.

1. Select `Main Level`, press `CONNECT`, then select `Gate 6 Level`.
2. Select `Gate 6 Level`, press `CONNECT`, then select `Gate 6 Screen`.
3. Select `Gate 6 Screen`, press `CONNECT`, then select `Complete Game`.
4. Select a card with exits. Confirm each exit is listed in the Inspector.
5. Click an exit, edit its outcome, apply it, then select another card and add
   an alternate exit using `CONNECT` plus destination-card selection.
6. Confirm the alternate destination appears on a subordinate track aligned
   beneath its source portion of the journey.

No Journey step should require dragging or drawing a graph wire.

## 4. Journey/Graph synchronization

1. Keep a card or exit selected and press `GRAPH`.
2. Confirm the same selection remains active and Graph shows the exact nodes,
   routes, destination and edited outcome authored in Journey.
3. Rename a node or reconnect a route in Graph.
4. Press `JOURNEY` and confirm the card/exit updates immediately without a save,
   reload or duplicate node.
5. Drag one Journey card to a small visual offset, switch to Graph and confirm
   Graph topology/positions were not semantically changed.

## 5. Double-click activation boundaries

1. In either view, double-click `Gate 6 Level`.
2. Confirm the existing 3D Level Editor opens. Press `< STORY FLOW` and confirm
   the same Story Flow session returns.
3. Double-click `Gate 6 Screen`.
4. Confirm the governed Screen Editor handoff is reported ready. Gate 8's visual
   Screen Editor is not required by Gate 6.
5. Double-click Game Start and each terminal kind. Confirm none opens an editor.

## 6. Persistence and regression

1. In Journey View, pan/zoom and leave a card visually offset. Save Flow.
2. Close and reopen the project from Recent Projects.
3. Confirm Story Flow opens, the selected Journey/Graph view and both independent
   canvas layouts persist, cards/routes persist and Runtime semantics are intact.
4. Re-run explicit `OPEN LEVEL` and `< STORY FLOW` to retain Gate 4 coverage.
5. Re-run explicit `OPEN SCREEN` to retain Gate 5 handoff coverage.

## Acceptance record

Record PASS/FAIL for every section plus screenshots of:

- default Journey project home;
- created/selected Level and Screen cards;
- main plus alternate tracks;
- matching Graph topology;
- reopened persisted Journey layout.

Gate 6 is accepted only on the exact commit and exact packaged Release that
passes this audit.

## Accepted implementation evidence

The project owner completed this audit on 2026-08-21 against PR #85 head
`9f827dd4ccf9675f3c237dd2dcf25f5ab6f3778d` using the Release artifact from
Renegade Studio run 668:

`renegade-studio-windows-x64-Release-92bc0cf4dcfa8e474104cae0c557de1306df6b90`

Outer artifact SHA-256:
`d7f2f67b8c6e9cebc96d77274e6c0a646eba9111e05671980b1a7e7de13ff0e8`.

All remaining owner-test sections passed. The owner specifically confirmed that
all required nodes were created and Level-card double-click activation opened
the governed Level Editor. The owner also identified the MVP interaction as
poorly signposted; that usability feedback does not invalidate the working
Gate 6 functional boundary and is retained for the Gate 9 Journey UX programme.
