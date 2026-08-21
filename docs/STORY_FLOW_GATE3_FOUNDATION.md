# Story Flow Gate 3 — Dedicated Workspace and Authoring Foundation

**Status:** stacked implementation in progress; Gate 2 remains the prerequisite branch until merged.

**Stacked base:** `agent/story-flow-gate2-screen-semantics`

## Purpose

Gate 3 retires the temporary Gate 1 overlay architecture and makes the authoritative Story Flow fully authorable through a native Graph workspace. This document records the first bounded implementation slice; it does not declare Gate 3 complete.

## First implemented slice — semantic authoring session

`StoryFlowAuthoringSession` is presentation-independent and owns:

- the currently validated `FlowDocument`;
- Flow-specific Undo/Redo history;
- dirty/saved history position;
- transactional save through the existing `WriteFlowDocument` seam;
- reopen/reload;
- rename, node add/delete and route add/update/delete mutations;
- permanent Game Start creation/deletion protection;
- atomic removal of routes attached to a deleted node;
- validation before any mutation enters history.

Layout is deliberately absent from this session. Graph/Journey positioning continues to use the separate Story Flow layout document, preserving the programme-wide rule that presentation state cannot become Runtime truth.

## Automated proof in this slice

`RenegadeStoryFlowAuthoringSessionTests` verifies:

- clean open;
- rename -> dirty -> undo -> clean -> redo;
- transactional save and reopen;
- terminal node creation;
- route creation and priority/condition editing;
- atomic node/route deletion;
- permanent Game Start protection;
- invalid semantic edits fail before history mutation;
- Screen nodes/routes participate in the same history;
- redo history and clean reload boundaries.

## Dedicated render-path promotion

The locked Gate 3 architecture remains:

`Project Hub -> Story Flow render path`

`Story Flow -> Level Editor -> Story Flow`

`Story Flow -> Screen Editor -> Story Flow`

The Story Flow path will be a native Wicked 2D render path. When it is active, the 3D Level Editor must not continue its normal update/render loop underneath Story Flow. Shared project/session/document services may remain alive across the transition.

This render-path switch is the next Gate 3 implementation slice. It is deliberately not represented by a dormant or fake render-path class before the application coordinator is wired to activate it.

## Still required before Gate 3 acceptance

Gate 3 is not complete until the following are integrated and proven:

- dedicated first-class Story Flow render path activated by Studio;
- temporary Gate 1 overlay removed;
- native Graph mutation controls over the shared model;
- route reconnection and Inspector editing for outcomes, Player Entry, priority and conditions;
- semantic Save/dirty affordances;
- Graph node movement and presentation persistence;
- validation/diagnostic display;
- Level Editor inactive while Story Flow owns the application render path;
- owner acceptance of the dedicated workspace and core editing behaviour.

Journey View remains Gate 6 and must not be conflated with this Graph/editor foundation.
