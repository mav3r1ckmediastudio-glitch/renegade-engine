# Story Flow Gate 9E — Concept Fidelity Owner Test

Gate 9E is a presentation-only pass. `StoryFlowAuthoringSession` remains the semantic authority and Graph Flow remains the only topology editor.

## Visual authority

Compare Journey Flow against the approved **Renegade Studio Story Flow Dashboard** concept. The runtime must be native/editable UI; the concept bitmap itself is never rendered as the interface.

## Owner acceptance

1. Open an existing project and enter Story Flow. The project handoff must remain clean with no Level Editor flash.
2. Confirm the approved **Renegade Engine fractured crest/wordmark** appears in the Story Flow header and is not stretched, clipped or replaced by generated text.
3. Confirm the left rail reads as the concept hierarchy: Hub, Story Flow, Levels, Screens, Assets, Variables, Test Play, with Story Flow visibly selected.
4. Journey cards must be image-led, compact and visually differentiated by destination type. Selected cards use the blue Story Flow selection treatment.
5. Main Journey and alternate tracks must read as an open Journey map rather than the old heavy boxed/debug layout.
6. Existing route relationships must appear as coloured, labelled presentation paths. They are read-only in Journey: no sockets, wire dragging, reconnecting or route deletion is exposed there.
7. Select several Journey cards. The right Inspector must update with destination identity, general metadata, Actions/Exits and validation state. Exit data must match the authoritative Graph topology.
8. The Story Overview/minimap must represent the Journey and the visible viewport without becoming an input/semantic authority.
9. Pan, zoom, FIT, START, FIND and card dragging must continue to work. Route presentation and overview must remain aligned after pan/zoom/card offsets.
10. Switch repeatedly between Journey and Graph. Graph must remain the proven ImNodes topology editor with no legacy Graph renderer, no duplicate route authority and no Journey presentation leaking into it.
11. Re-test Graph FIT and START. Neither may crash to desktop.
12. Re-test Screen/Level creation, Screen template dropdown, route create/delete/reconnect, undo/redo and double-click activation. Gate 9E must not regress Gate 9C/9D authoring behavior.
13. Save, close and reopen the project. Story Flow layout/pan/zoom must persist as before.
14. At both 1280×720 and a larger desktop size, header, rail, Inspector, cards, minimap and native controls must remain usable with no text/control overlap.

## Editable visual seam

The packaged file `Content/ui/story-flow-theme.cfg` owns Story Flow presentation values such as typography, shell dimensions, colours, route styling and asset paths. The approved logo is packaged as `Content/ui/renegade-story-flow-logo.png`. Editing presentation data must never change Story Flow runtime semantics.
