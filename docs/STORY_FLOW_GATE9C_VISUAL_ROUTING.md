# Story Flow Gate 9C — Visual Routing, Branch Lanes & Inspector

## Baseline

Gate 9C is based on merged Gate 9B main:

`f72a7b992d42bd5697ec4414d1c0edb32205e938`

Gate 9B established native Journey lane/card objects and governed Level thumbnails. Gate 9C adds route authoring to that same native Journey surface. It does not introduce a second Flow model or use the approved concept image as rendered media.

## Product contract

Journey View must make Story Flow routing visible and directly understandable. Route wires are not decoration: each visible wire represents one authoritative `FlowRoute` from the active `StoryFlowAuthoringSession`.

The creator must be able to:

- see every authoritative route in Journey View;
- distinguish the primary continuation from secondary branch exits;
- hover and select a route wire;
- inspect/edit the selected route through the existing Story Flow Inspector;
- drag a route destination handle to another valid destination while preserving route identity and metadata;
- drag from a source output/action port to create a new authoritative route;
- delete a selected route and restore/re-delete it through normal Story Flow Undo/Redo;
- switch to Graph View and observe the exact same topology;
- continue using Graph connection authoring without the Gate 9B header collision hiding the control.

## One route authority

Gate 9C does not serialize route geometry or route semantics in the Journey UI.

All semantic mutations continue through `StoryFlowAuthoringSession`:

- create: `AddRoute`
- rewire/edit: `UpdateRoute`
- delete: `DeleteRoute`
- history: existing Story Flow Undo/Redo

Journey View, Graph View, Inspector, Runtime and packaging therefore continue to read one Flow document.

A destination-handle rewire changes only the destination fields required by the destination convention. The existing route ID, source node, outcome/action, priority and conditions remain on the same route object.

## Native Renegade routing objects

Gate 9C introduces `RenegadeStoryFlowJourneyRoutingOverlay`, composed from Renegade-owned route and port widget objects using Wicked's low-level GUI/render/input infrastructure.

The following hard rules apply:

- no concept-art background is loaded or rendered;
- no route hotspots are placed over a screenshot;
- no stock Wicked Editor node/graph UI is exposed;
- every visible Journey route is represented by a route object bound to a stable `FlowRoute` ID;
- every visible source port is represented by a port object bound to a stable source node ID and exact outcome/action string.

## Source ports

Source ports expose existing semantic conventions rather than inventing Runtime behaviour:

- Game Start: `renegade.flow.start`
- Level: `next`
- Screen: current authored Screen actions returned through the existing Screen outcome query
- terminal nodes: no output ports

For Screen nodes, the selected port's authored action becomes the new route outcome. The action is revalidated when the drag is committed so an action changed during the gesture cannot silently create a stale route.

## Wire presentation

Journey routes use clean orthogonal routing rather than free diagonal lines.

- primary continuation: restrained route colour;
- secondary branch: branch treatment;
- hover: raised neutral emphasis;
- selected: Renegade Forge orange;
- selected/hovered wires expose their outcome label;
- destination handles are visually explicit and draggable.

Backward/return relationships use an outside gutter path instead of cutting directly through card content.

This is the Gate 9C routing foundation. Gate 9D still owns semantic zoom, Story Overview/minimap, search and navigation polish. Gate 9E owns broader nonlinear authoring conveniences such as advanced splicing/hubs/loop workflows.

## Gate 9B Connect regression closure

Gate 9B did not delete Graph `CONNECT`; the new 9A workspace inset caused the old hardcoded Level lifecycle name field to cover it.

Gate 9C must restore accessible Graph connection authoring. The 9C implementation supplies a Graph-only Renegade `CONNECT / CANCEL LINK` control in clear Inspector space and delegates directly to the existing `BeginConnect`/`CommitConnectionTo` path. The old covered toolbar control is not treated as the user-facing solution.

Gate 9C fails if Graph View still has no accessible way to create a route.

## Automated proof

`RenegadeStoryFlowGate9CVisualRoutingTests` proves the semantic boundary independently of UI rendering:

1. a branched Flow projects every authoritative route into Journey exits;
2. a secondary Screen route is projected as a branch rather than the primary continuation;
3. rewiring a route preserves its stable ID, source, outcome, priority and conditions;
4. Journey projection immediately reflects the rewired destination;
5. deleting the route removes it from both Flow and Journey;
6. Undo restores the same route identity/topology;
7. Redo removes it again from both views.

## Owner Release acceptance

Use the packaged Release build from the exact passing PR head.

1. Open a project containing at least Game Start, one Screen and two Level/destination nodes.
2. In **Graph View**, select a non-terminal node. Confirm an accessible `CONNECT` control exists and is not covered by Level/Screen lifecycle controls.
3. Use Graph `CONNECT` to create a route. Confirm the route appears in Graph and Journey without reopening the project.
4. Switch to **Journey View**. Confirm every existing route is visibly wired between cards.
5. Hover a wire. Confirm it highlights without selecting or moving a card.
6. Click a wire. Confirm the Inspector switches to that exact route and its outcome/destination metadata are editable.
7. Drag that wire's destination handle to another destination card. Confirm the wire moves, the Inspector remains on the same route, and Graph View shows the same new destination.
8. From a Level `NEXT` port, drag to a destination and confirm a new `next` route is created.
9. For a Screen with authored actions, confirm output ports represent those actions. Drag from a specific action port and verify the selected route's outcome equals that action.
10. Select a wire and press **Delete**. Confirm it disappears from Journey and Graph.
11. Press **Undo**. Confirm the same route returns. Press **Redo** and confirm it disappears again.
12. Create or inspect a branched route and confirm secondary branch wiring is visually distinguishable from the primary continuation.
13. Save, close Renegade, reopen the project and confirm the authored topology persists identically in Journey and Graph.
14. Recheck Gate 9B behaviour: Level thumbnails remain visible/persistent, Level double-click opens the Level Editor, and Screen double-click opens the Screen Editor.
15. Confirm no approved concept image is present as a rendered background and no stock Wicked Editor window has appeared.

Gate 9C passes only when exact-head Debug/Release CI is green **and** the packaged Release owner audit above passes.

## Gate boundary

Gate 9C does not close Gate 9 visual polish. Current cards/typography/spacing may still be visibly pre-polish. The gate closes routing comprehension and manipulation on the real Renegade Journey component architecture so later visual refinement is applied to the correct interaction model.
