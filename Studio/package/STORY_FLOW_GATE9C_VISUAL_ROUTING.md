# Gate 9C — Packaged Release Owner Check

This package is a Gate 9C candidate only if it was built from the exact passing PR head.

## Required owner check

1. Open an existing Story Flow with Game Start, a Screen and at least two destinations.
2. Switch to **GRAPH**. Select a non-terminal node and confirm an accessible **CONNECT** control is visible in the Inspector area. It must not be hidden beneath the Level lifecycle controls.
3. Use **CONNECT**, then select a destination node. Confirm a real route is created.
4. Switch to **JOURNEY**. Confirm that route and all other Flow routes are visible as wires between cards.
5. Hover a wire: it should highlight. Click it: the Inspector should select that exact route.
6. Drag the selected wire's destination handle to another destination card. Switch to Graph and confirm the exact same route now terminates at that destination.
7. Drag from a Level **NEXT** output port to a destination card. Confirm a new `next` route is created.
8. On a Screen with authored actions, confirm its output ports correspond to those actions. Create a route from a chosen action and verify the Inspector outcome matches it.
9. Select a wire and press **Delete**. Confirm it disappears. **Undo** must restore it; **Redo** must remove it again.
10. Confirm secondary branch wiring is visually distinguishable from the primary continuation.
11. Save, close Renegade, reopen the project, and confirm Journey and Graph show the same persisted topology.
12. Recheck Gate 9B: Level thumbnails still persist, Level cards still open the Level Editor, and Screen cards still open the Screen Editor.
13. Confirm there is no concept-art background and no stock Wicked Editor window.

Report **PASS** only if all items succeed. Otherwise report the first failing step and what happened.
