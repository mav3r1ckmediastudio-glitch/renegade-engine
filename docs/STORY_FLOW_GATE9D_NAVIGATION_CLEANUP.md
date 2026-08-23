# Story Flow Gate 9D — Navigation and Graph Ownership Cleanup

## Baseline

Gate 9D starts from merged Gate 9C `main`:

`798dbcd82d455483093a03ae03bb30c34a0681e4`

Gate 9C established the real ImNodes Graph editor and proved direct route creation, reconnect, deletion, node dragging, governed Level/Screen creation and owner-visible recovery. Gate 9D deliberately begins by **removing transitional ownership paths** before adding navigation polish.

## Non-negotiable ownership split

### Story Flow model/session

`StoryFlowAuthoringSession` remains the only semantic/history authority. Neither Journey, ImNodes nor the overview owns Flow nodes or routes.

### Graph

Graph is the only topology editor.

- ImNodes is the sole Graph canvas renderer and pointer owner.
- Graph creates routes from output sockets.
- Graph reconnects existing route destinations by dragging links.
- Graph selects/deletes routes and non-Game-Start nodes.
- Graph Inspector may edit authoritative route properties.
- The pre-ImNodes primitive Graph renderer, line hit testing and two-step CONNECT/RECONNECT workflow are not surfaced.

### Journey

Journey remains the high-level creator overview.

- cards, lanes, thumbnails, ordering and reachability remain native Renegade presentation;
- Journey has no route wires or sockets;
- Journey has no route/node Delete command;
- Journey exit rows are read-only summaries with a navigation affordance: clicking an exit switches to Graph with that route selected;
- Journey may still select/rename/open content because those are content/navigation operations rather than topology wiring.

## Native navigation controls

Gate 9D promotes Story Flow navigation out of the old painted workspace hit targets into real native wiGUI controls:

- **JOURNEY** — activate Journey presentation;
- **GRAPH** — activate Graph topology editor;
- **FIT** — frame the active view;
- **START** — center Game Start;
- **FIND** — exact node-name match first, deterministic partial-name match second.

The FIND field owns a live draft string so it does not depend on text-focus commit timing.

FIT/START live in the lower-left canvas navigation host rather than competing with the two governed Level/Screen lifecycle rows at the top of the 1280x720 owner layout.

## Graph overview

Graph gains a small presentation-only overview in the lower-right of the Graph viewport.

The overview is derived each frame from:

- `StoryFlowAuthoringModel::Nodes()` / `Routes()`;
- persisted Story Flow node layout;
- current Graph pan/zoom;
- current shared node selection.

It draws no semantic state and never mutates the Flow document. It therefore cannot become a second graph authority.

## Single Graph runtime path

`RenegadeStoryFlowRenderPath` leaves the shared workspace visible in Graph mode only for the common header/Inspector presentation, but disables it as a canvas interaction owner. ImNodes receives Graph canvas interaction.

`RenegadeStoryFlowWorkspace::Render()` renders Journey cards only. It does not draw primitive Graph nodes or route lines.

`RenegadeStoryFlowWorkspace::Update()` performs canvas gestures only in Journey. Graph pointer gestures never enter the old workspace route/node hit-test path.

This removes the class of 9C failures caused by two overlapping Graph implementations rather than adding more arbitration code between them.

## Regression proof

`RenegadeStoryFlowGate9DSourceContract` is a narrow architecture regression check. It fails if source reintroduces:

- `EXITS // CLICK TO EDIT` in Journey;
- the primitive legacy Graph route renderer;
- old painted FIT/START hit testing;
- Journey-level keyboard Delete;
- surfaced legacy CONNECT/RECONNECT controls;
- route editing or node deletion outside Graph;
- loss of native FIND / overview APIs.

Behaviour that depends on actual Wicked/ImNodes pointer interaction remains owner-tested in the packaged Release build.

## Owner packaged Release acceptance

Use the packaged Release artifact from the exact passing PR head.

1. Open Story Flow in **Journey**. Confirm the Gate 9B cards/lanes/thumbnails remain intact and there are no wires or sockets.
2. Confirm JOURNEY and GRAPH are real clickable controls and repeated switching does not expose any old Graph UI.
3. In Journey, select a Screen/Level with exits. Confirm the Inspector reads **EXITS // OPEN IN GRAPH**. Click one exit and confirm Graph opens with that exact route selected.
4. Confirm Journey exposes no Delete route/node command and the Delete key does not remove topology there.
5. In Graph, confirm ImNodes is the only visible Graph: no orange legacy rectangles/lines may leak into the Inspector or appear behind dropdowns.
6. Re-run the accepted 9C interactions: move nodes, pan/zoom, create routes, reconnect, cancel reconnect, delete, Undo/Redo, and Screen/Level lifecycle creation.
7. Confirm FIT frames Graph content and START centers Game Start. Repeat in Journey and confirm they operate on the Journey presentation instead.
8. Search by an exact node name; confirm that node is selected and centered. Search by a unique partial name and confirm deterministic focus. Search for a missing name and confirm a clear non-destructive status message.
9. In Graph, confirm the lower-right overview mirrors nodes/routes, highlights the selected node, and shows the current visible viewport while panning/zooming.
10. Open the Screen template dropdown repeatedly in Graph. The modern Graph must remain visible beneath it and node dragging must still work after the dropdown closes.
11. Save/close/reopen and confirm Graph node layout, Journey offsets, pan/zoom and topology remain authoritative and synchronized.
12. Confirm Level and Screen creation still require one click and the created node appears immediately.

Gate 9D passes only when exact-head Debug/Release Studio and Windows baseline CI are green **and** this owner audit passes.

## Deliberate boundary

Gate 9D does not introduce chapters/groups, branch collapse, route bundling, mass editing or a second semantic projection. Those broader nonlinear authoring conveniences remain later work. This gate is primarily about clean ownership, navigation and scale visibility on top of the proven Gate 9C Graph foundation.
