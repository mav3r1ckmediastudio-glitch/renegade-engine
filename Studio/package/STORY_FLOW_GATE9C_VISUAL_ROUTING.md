# Gate 9C — Packaged Release Owner Check

This package is a Gate 9C candidate only if it was built from the exact passing PR head.

## Required owner check

1. Open Story Flow and inspect **JOURNEY** first. Confirm it remains the Gate 9B card/reel view with **no wires and no sockets**.
2. Switch to **GRAPH**. Confirm nodes have input/output sockets and existing routes appear as node-editor links.
3. Drag a Level **NEXT** output socket onto another valid node's input. Confirm exactly one route/link is created.
4. Grab the **destination/input end of that existing link** and drag it away. Confirm that end detaches from the node; it must not spawn another route.
5. Drop the detached end onto another valid input. Confirm the **same route** is rewired and no duplicate link remains.
6. Detach the destination end again and release in empty graph space. Confirm the original route returns unchanged.
7. Try to start/commit a new route from an input socket. Confirm no Story Flow route is created.
8. On a Screen node, confirm output sockets correspond to its authored Screen action IDs. Create a link from one and confirm the Inspector outcome matches it.
9. Click a link. Confirm the Renegade Inspector selects that exact route.
10. Press **Delete** on a selected link. Confirm it disappears. **Undo** restores the same route; **Redo** removes it again.
11. Drag Graph nodes and pan the canvas. Save, close Renegade and reopen the project. Confirm Graph topology/layout remain correct.
12. Double-click Level and Screen nodes in Graph. Confirm the established Level Editor / Screen Editor handoffs still work.
13. Return to **JOURNEY**. Confirm it still contains no wires, and Gate 9B Level thumbnails plus Level/Screen activation still work.
14. Confirm there is no concept-art background, no stock Wicked Editor window and no visible generic ImGui window chrome.

Report **PASS** only if every item succeeds. Otherwise report the first failing step and what happened.
