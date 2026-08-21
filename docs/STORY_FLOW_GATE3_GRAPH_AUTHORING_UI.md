# Story Flow Gate 3C — Native Graph Authoring Controls

## Purpose

Gate 3C binds the presentation-independent `StoryFlowAuthoringSession` created in the Gate 3 foundation to the dedicated Story Flow render path introduced by Gate 3B.

The Graph workspace is no longer a read-only proof surface. Semantic edits are executed through the existing validated Flow authoring session and therefore use the same Flow document, validation rules, history and transactional writer as Runtime.

## Implemented creator actions

- select and drag Graph nodes without changing Flow semantics;
- select routes directly from the Graph;
- rename an existing destination;
- delete a non-Game-Start node and its connected routes as one semantic edit;
- create terminal `Complete Game`, `Return To Main Menu` and `Quit` destinations;
- connect a selected non-terminal source to a destination;
- reconnect an existing route to a different destination while preserving route identity and source;
- edit route outcome, Player Entry and numeric priority in the Inspector;
- delete a route;
- Flow-specific Undo and Redo;
- explicit transactional Flow Save;
- visible dirty/saved state and fail-closed validation feedback;
- presentation diagnostics remain visible in the Inspector;
- Graph pan/zoom/node positions continue to persist only in the separate Story Flow layout document.

New route creation chooses deterministic valid defaults: Game Start emits `renegade.flow.start`; other sources use `next`; routes entering a Level receive `player_entry` until the creator edits it.

## Architectural boundaries

The native workspace never mutates `FlowDocument` directly. It delegates semantic operations to `StoryFlowAuthoringSession`, then rebuilds the shared `StoryFlowAuthoringModel` and reconciles the presentation-only layout document.

The permanent Game Start remains protected by the semantic service and the UI disables its delete action.

Level and Screen *document creation* is deliberately not invented in this slice. Existing Level and Screen nodes can be renamed, moved, connected, reconnected and deleted. Creating a new Level document and a new Screen document belongs to Gate 4 and Gate 5 respectively; those lifecycle gates will feed their stable document identities into the same Graph authoring session.

Route condition authoring remains the next bounded Gate 3 slice. Existing conditions are preserved by route edits and reported by count in the Inspector; this slice does not flatten or replace them.

Journey View remains Gate 6 and will consume the same semantic session/model rather than introducing a second graph or runtime format.

## Validation boundary

Every semantic edit is validated before it can enter `StoryFlowAuthoringSession` history. Invalid node/route states are rejected without changing the current Flow. Semantic Save uses the existing transactional `WriteFlowDocument` seam. Presentation layout writes remain separate and cannot alter Runtime traversal.

This stacked PR remains Draft. Full Studio Debug/Release and Windows baseline validation is intentionally deferred until the Gate 3 stack reaches a meaningful integrated checkpoint against `main`.
