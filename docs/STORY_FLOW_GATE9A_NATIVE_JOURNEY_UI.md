# Story Flow Gate 9A — Native Journey UI Foundation

## Status

Implementation branch: `feature/story-flow-gate9a-native-journey-ui`

Exact base: `a1288c6e4fcf74eb5d85ae8944ede789802813b2`
(`Story Flow Gate 8E: outcome and package parity (#94)`).

Gate 9A is the first UI/UX slice of Gate 9. It changes presentation only. The
accepted Story Flow semantic document, Screen action/outcome ownership,
transactional authoring session, Runtime traversal and standalone behaviour are
not replaced or forked.

## Product lock

The approved Story Flow concept art is **design reference only**.

It MUST NOT be loaded as a Story Flow background, used as a flattened UI plate,
or combined with invisible/hotspot controls. Gate 9 is built natively.

Every persistent visible Story Flow UI element is implemented as a real object:
workspace regions, navigation rail, toolbar controls, Journey lanes/cards,
Inspector controls, route controls, overview/minimap and zoom/navigation
controls. Later Gate 9 slices may progressively replace the remaining Gate 6
hand-rendered Journey presentation, but no new Gate 9 feature may be implemented
as painted concept media.

## Gate 9A purpose

Establish the real native shell that later Journey authoring slices build into,
without destabilizing the already accepted Flow behaviour.

Gate 9A owns:

- a native Story Flow navigation rail;
- a native top-bar frame/host;
- a native Journey viewport frame/host;
- a native Inspector frame/host;
- native reserved hosts for the later Story Overview and zoom/navigation tools;
- responsive shell geometry rather than concept-resolution coordinates;
- explicit Wicked GUI layering so the semantic workspace remains behind the new
  Journey chrome while existing lifecycle controls remain usable;
- preservation of the first-class Story Flow render path and dormant Level
  Editor architecture.

The existing Story Flow workspace is inset into this shell. Gate 9A does not
rewrite `FlowDocument`, `StoryFlowAuthoringSession`, `StoryFlowJourneyModel`,
Screen outcome parity, Level/Screen lifecycle services or Runtime.

## Gate 9A non-goals

The following belong to later Gate 9 slices and are not reasons to widen 9A:

- final thumbnail/media cards and local-image thumbnail selection (9B);
- directly manipulable Journey route wires and output ports (9C);
- semantic zoom, Story Overview/minimap, search/focus and branch collapse (9D);
- auto-splice, safe rewiring, loops/hubs/returns and chapters/groups (9E);
- representative large-project diagnostics/hardening and final Gate 9 visual
  acceptance (9F);
- editor-viewport screenshot capture for Level cards (post-beta enhancement).

The inactive rail entries in 9A are structural chrome only. Gate 9A must not
invent duplicate Project/Assets/Build implementations just to make the first
visual shell appear complete.

## Architecture

`RenegadeStoryFlowRenderPath` remains the first-class Story Flow render path.

Gate 9A introduces `RenegadeStoryFlowJourneyChrome`, which owns independent
native Wicked GUI objects for the visible shell regions. The semantic
`RenegadeStoryFlowWorkspace` is positioned to the right of the rail and remains
the authority for selection, Journey/Graph projection, Flow mutations,
Inspector values and existing authoring controls.

Wicked GUI reverse-registration order is explicitly maintained as:

1. semantic Story Flow workspace (back);
2. native Journey chrome;
3. condition editor;
4. external Level/Screen lifecycle controls (front).

The chrome is presentation-only and does not consume Story Flow input in 9A.

## Required acceptance

Gate 9A is accepted only when the exact packaged Release build proves all of the
following:

1. Open/create a project and land in Story Flow normally.
2. The new native left navigation rail is visible and the existing Story Flow
   workspace is correctly inset beside it.
3. The shell remains usable at normal supported Studio sizes and resizing does
   not overlap/collapse the Inspector or Journey viewport.
4. Journey and Graph toggles still work.
5. Existing pan, zoom, Fit and Start navigation still work.
6. Existing node selection and double-click Level/Screen activation still work.
7. Existing Add Level/Add Existing Level/Add Screen/Open controls remain visible
   and clickable above the Story Flow layers.
8. Existing Flow Save/Undo/Redo and Inspector edits still work.
9. Gate 8E Screen action/outcome diagnostics and routing behaviour are unchanged.
10. Returning from Level Editor or Screen Editor returns to the same Story Flow
    project context.
11. No stock Wicked Editor window appears.
12. Repository/build inspection confirms the approved concept image is not
    copied, loaded or rendered by Story Flow.

A green compile is not owner acceptance. Any visual overlap, obscured lifecycle
control, broken Story Flow interaction, or fake concept-background implementation
fails Gate 9A.

## Gate 9A completion boundary

Gate 9A completes when the native shell is owner-accepted and merged. Gate 9B
then replaces/enriches the Journey reel/card presentation inside this established
shell, including governed Level-card thumbnails and the small Choose/Change
Image control.
