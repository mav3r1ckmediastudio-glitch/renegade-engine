# Story Flow Gate 4C — Native Level Lifecycle Integration

**Status:** stacked implementation for Gate 4 review.

Gate 4C binds the Gate 4A/4B content lifecycle into the dedicated Story Flow render path and the existing 3D Level Editor.

## Native Story Flow controls

The Story Flow surface now exposes a bounded Level lifecycle strip:

- Level name input;
- `+ NEW LEVEL`;
- `+ EXISTING...` using the native file picker for WISCENE;
- `OPEN LEVEL` for a selected Level node.

The controls do not mutate `FlowDocument` directly. New Level creation delegates to `StoryFlowLevelLifecycleService`; existing Level adoption and open resolution delegate to `StoryFlowLevelReferenceService`.

## Story Flow -> Level Editor

Opening a selected Level:

1. resolves the WISCENE by stable Scene document ID, using the stored path only as a hint;
2. reports moved/missing/mismatched identity diagnostics fail-closed;
3. flushes presentation-only Story Flow layout state;
4. loads the governed WISCENE through the existing `StudioSession` / `SceneDocumentService` boundary;
5. switches the active Wicked render path to the existing 3D Level Editor;
6. leaves the Story Flow authoring session/model/layout alive but inactive.

The 3D editor therefore becomes active only when a Level is explicitly opened.

## Explicit Return to Story Flow

While a Level opened from Story Flow owns the 3D editor, a native `< STORY FLOW` control is hosted on the Level Editor GUI. It switches back to the dedicated Story Flow render path without discarding the Flow session or graph presentation state.

Normal Level saving remains the responsibility of the existing Level Editor Save/Save As/Reopen document controls; Gate 4 does not create a second Scene save path.

## Automated proof

`RenegadeStoryFlowGate4CLevelEditorLifecycleTests` proves that:

- a governed Level resolves by stable identity;
- the resolved WISCENE opens through `SceneDocumentService`;
- the active Scene document changes to the Level;
- the Story Flow authoring session remains loaded and semantically unchanged while the Level document is open.

## Remaining Gate 4 closeout

Gate 4D is a bounded integrated closeout slice: owner-test instructions, cumulative acceptance evidence, static audit of the A/B/C stack, and the exact-head CI checkpoint. It adds no new product architecture.
