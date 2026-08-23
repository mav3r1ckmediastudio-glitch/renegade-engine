# Story Flow Gate 9C — Graph Flow Node Editing

## Baseline

Gate 9C is based on merged Gate 9B main:

`f72a7b992d42bd5697ec4414d1c0edb32205e938`

Gate 9B established the native Journey reel/card/lane surface and governed Level thumbnails. Gate 9C does **not** add wires to Journey View. It upgrades the separate technical **Graph View** into a real node editor.

## Hard product split

### Journey View

Journey remains the higher-level creator view established in Gate 9B:

- ordered cards/reels/lanes;
- Level thumbnails and Screen/Level activation;
- no visible route wires;
- no input/output sockets;
- no link dragging or node-graph interaction.

### Graph View

Graph is the only Story Flow view with wired nodes. Its behavioural reference is Blender's node editor: not Blender's skin, but its predictable node/socket/link interaction model.

The creator must be able to:

- see Story Flow nodes as real node-editor nodes;
- see authoritative `FlowRoute`s as links;
- drag from an output socket to a destination input to create one route;
- grab the destination end of an existing link, detach that end, and attach the **same route** to another destination;
- drop a detached existing route on empty/invalid space and get the original route back unchanged;
- never create a semantic route merely by dragging an input/destination socket;
- select nodes and links;
- select a link and inspect/edit that exact route in the existing Renegade Inspector;
- delete selected links and restore/re-delete them through Story Flow Undo/Redo;
- drag nodes and persist Graph layout;
- pan the graph canvas;
- double-click Level/Screen nodes and retain the existing editor handoff behaviour.

## Established node-editor foundation

Gate 9C uses the upstream MIT-licensed **Nelarius/imnodes** library as a pinned Studio-only dependency.

Pinned revision:

`eb36902c892548ef94f88f51ad7e7c9c7058a71c`

This is the same class of node-editor foundation used by GameGuru MAX. Renegade does not copy MAX source or MAX UI.

ImNodes supplies established node-editor mechanics such as:

- nodes and input/output attributes;
- links;
- link/node selection and hit testing;
- link-start/create/drop/destroy events;
- `ImNodesAttributeFlags_EnableLinkDetachWithDragClick` for dragging an existing connected link end;
- panning and editor context state.

## UI architecture

The accepted Renegade UI decision remains unchanged:

- Wicked `wiGUI` is the production editor UI foundation;
- Renegade owns the visual language, navigation, Inspector, toolbars and workflows;
- EngineBridge remains UI-toolkit independent.

Gate 9C introduces a tightly bounded exception only for the Graph canvas:

`Renegade wiGUI shell -> Graph viewport -> Dear ImGui + ImNodes mechanics -> Wicked renderer`

Dear ImGui sources are the already-pinned copy present in Wicked's `Example_ImGui` sample. Renegade supplies a small Wicked renderer/input bridge and Renegade-owned HLSL shaders. ImNodes is linked only into `RenegadeStudio`.

Runtime and standalone game targets do **not** link Dear ImGui or ImNodes.

## One route authority

Graph UI never owns Runtime route semantics.

All semantic changes continue through `StoryFlowAuthoringSession`:

- output socket -> input socket: `AddRoute`;
- existing destination-end reconnect: `UpdateRoute`;
- delete selected link: `DeleteRoute`;
- history: existing Story Flow Undo/Redo.

The Inspector, Graph, Journey projection, Runtime and packaging therefore continue to read one Flow document.

## Transactional reconnect rule

The destination-end drag is deliberately transactional.

When ImNodes reports that an existing link was detached, Renegade records the stable `FlowRoute` ID but does **not** delete or mutate the route yet.

- valid new destination -> `UpdateRoute(existingRouteId, ...)`;
- invalid/empty drop -> no semantic mutation; the authoritative route is rendered again;
- no delete+create substitution is permitted.

A successful rewire preserves:

- stable route ID;
- source node;
- outcome/action;
- priority;
- conditions;
- every other route field except destination/destination-entry fields required by the new destination convention.

This directly prevents the failed prototype behaviour where dragging either end merely spawned another wire.

## Socket conventions

### Inputs

Every valid destination node gets an input socket. Permanent Game Start cannot be a destination.

### Outputs

- Game Start: `renegade.flow.start` displayed as `START`;
- Level: `next` displayed as `NEXT`, plus existing authored outcome slots needed to represent current topology;
- Screen: actual authored Screen action IDs from the established Screen outcome query;
- terminal nodes: no outputs.

A stale Screen route outcome remains visible so existing topology is never hidden, but its stale socket is not authorable as a new route source.

## Gate 9B Connect regression closure

The old `CONNECT` button was not deleted in Gate 9B; the 9A rail inset caused the hardcoded Level lifecycle field to cover it.

Gate 9C supersedes that old two-step button workflow with direct Graph socket linking. The legacy `CONNECT` control is hidden while the Graph node editor is active rather than moved to another arbitrary location.

Graph View therefore regains connection authoring through the normal node-editor interaction itself.

## Rendering and performance boundary

The Graph node editor is editor-only and does not affect packaged-game performance.

Renegade uses Wicked's native graphics device to render ImGui draw data and keeps the graph bounded to the Graph viewport. Journey never creates or renders ImNodes content.

The implementation intentionally does not add Node.js, Electron, a web view or a browser runtime.

## Automated proof

`RenegadeStoryFlowGate9CVisualRoutingTests` proves the semantic boundary independently of UI rendering:

1. Graph-style link creation creates exactly one authoritative route/history mutation;
2. destination rewiring keeps the same stable route ID and route count;
3. source, outcome, priority and conditions survive the rewire;
4. the authoring model immediately mirrors the new destination;
5. a cancelled visual detach requires no Story Flow mutation;
6. link Delete removes the route;
7. Undo restores the same rewired route identity/metadata;
8. Redo removes it again.

## Owner packaged Release acceptance

Use the packaged Release build from the exact passing PR head.

1. Open Story Flow and inspect **Journey** first. Confirm it remains the Gate 9B card/reel view and contains **no wires or sockets**.
2. Switch to **Graph**. Confirm nodes have clear input/output sockets and routes are visible as node-editor links.
3. Drag from a Level `NEXT` output socket to another valid destination input. Confirm exactly one route/link is created.
4. Grab the **destination/input end of that existing link** and drag it away. Confirm that end detaches from the node instead of creating another link.
5. Drop that detached end on another destination input. Confirm the same link is rewired and no duplicate link remains.
6. Repeat the detach, but release in empty graph space. Confirm the original link snaps/restores and no route is created/deleted.
7. Start a new drag from an input/destination socket. Confirm it cannot commit a new Story Flow route.
8. On a Screen node, confirm output sockets correspond to that Screen's authored action IDs. Create a link from one action and confirm the Inspector outcome matches it.
9. Click a link. Confirm the existing Renegade Inspector selects that exact route.
10. Press **Delete** on a selected link. Confirm it disappears. Press **Undo** and confirm the same route returns; **Redo** removes it again.
11. Drag Graph nodes and pan the canvas. Save/close/reopen and confirm node layout/topology remain correct.
12. Double-click Level and Screen Graph nodes and confirm the established Level Editor / Screen Editor handoffs still work.
13. Return to Journey and confirm it still has no wires and Gate 9B thumbnails/activation remain intact.
14. Confirm there is no concept-art background, no stock Wicked Editor window, and no visible generic ImGui window chrome.

Gate 9C passes only when exact-head Debug/Release CI is green **and** this packaged Release owner audit passes.

## Gate boundary

Gate 9D still owns semantic zoom, Story Overview/minimap, search and navigation polish. Gate 9E owns broader nonlinear authoring conveniences. Gate 9C is specifically the Graph node/link interaction foundation.
