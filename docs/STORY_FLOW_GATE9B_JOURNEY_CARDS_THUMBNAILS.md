# Story Flow Gate 9B — Journey Reel, Cards & Level Thumbnails

## Status

Gate 9B implementation candidate. Base: merged Gate 9A (`1a6bad15b7734de8325e26cce9e3e7d3dc15dc9a`).

## Purpose

Replace the primitive Journey MVP card presentation with real Renegade-owned native Journey objects while preserving the one authoritative Story Flow semantic model.

## Product locks

- The approved Journey concept image is design reference only. It is never loaded, rendered, composited or used as a background/hotspot plate.
- Each Journey lane is a native `RenegadeStoryFlowJourneyLane` object.
- Each semantic Story Flow node is represented by a native `RenegadeStoryFlowJourneyCard` object.
- Graph View and Journey View continue to consume the same `StoryFlowAuthoringModel`; no second route or runtime model is introduced.
- Level/Screen activation remains the governed lifecycle path accepted in earlier gates.
- Visible/manipulable Journey route wires are intentionally Gate 9C, not 9B.

## Journey card presentation

Cards expose a creator-facing information hierarchy rather than the old primitive block:

- journey sequence number;
- Flow destination type;
- display name;
- Level/Screen governed document hint as a human-readable subtitle;
- media/thumbnail region;
- outgoing-exit count;
- selected state;
- validation warning/error/ready state.

At low zoom the same card object reduces to a compact presentation rather than creating a separate semantic representation.

## Governed Level thumbnail slot

Gate 9B provides the beta/manual Level thumbnail workflow through the small `IMG` utility button on Level cards.

The creator chooses a JPG/JPEG/PNG/BMP/TGA image. Renegade copies the selected image into the project-owned deterministic slot:

`Content/StoryFlow/Thumbnails/<stable-story-flow-node-id>.<extension>`

Rules:

- the original machine-local source path is never persisted;
- stable Story Flow node identity determines the thumbnail slot;
- changing source image replaces the existing governed resource;
- changing image format removes stale managed variants only after successful promotion;
- more than one managed variant is treated as ambiguous/fail-closed;
- no thumbnail is a valid normal state and renders the native Renegade placeholder;
- thumbnail resources are presentation-only and do not participate in Runtime Flow semantics;
- the future post-beta Level Editor capture workflow must write to this same stable-ID resource slot rather than introducing another thumbnail system.

The `IMG` utility hit region is evaluated before card-body activation. Choosing/changing a thumbnail therefore cannot count as a Level double-click, card drag or route interaction.

## Automated proof

`RenegadeStoryFlowGate9BJourneyThumbnailTests` proves:

- project root recovery from a governed Story Flow document path;
- supported/unsupported extension policy;
- deterministic stable-ID managed destination;
- imported content is copied into the project;
- managed thumbnail resolves after reopen;
- same-format replacement replaces the governed resource;
- format-changing replacement leaves one governed variant;
- unsupported input is rejected;
- ambiguous duplicate managed variants fail closed.

Existing Story Flow, Runtime and Gate 8E tests remain in the same CI suite and therefore continue to guard semantic/runtime parity.

## Owner packaged Release acceptance

Gate 9B passes only if all of the following are observed in the packaged Release build:

1. Story Flow opens in Journey View with native Renegade lane/card presentation rather than the old primitive giant card style.
2. No approved concept artwork appears as a background or interaction plate.
3. Main Journey cards remain selectable and draggable.
4. Double-clicking a governed Level card opens the correct Level Editor.
5. Double-clicking a governed Screen card opens the correct Screen Editor.
6. A Level card exposes a small `IMG` utility control when Journey detail is visible.
7. Clicking `IMG` opens image selection and does **not** open the Level or begin a card drag.
8. Selecting a supported image displays it in that Level card.
9. Replacing the image refreshes the displayed thumbnail.
10. Navigating Level Editor -> Story Flow preserves the thumbnail.
11. Closing and reopening the project resolves the same managed thumbnail from the stable node ID.
12. Moving/reordering the Journey card does not lose the thumbnail.
13. A Level with no assigned image displays a native Renegade placeholder.
14. Journey/Graph switching, Save, Undo/Redo, lifecycle controls and Gate 8E Screen action/routing behavior remain intact.

Any crash, interaction overlap, lost Level/Screen activation, machine-absolute thumbnail dependency, concept-background implementation or Story Flow semantic regression fails Gate 9B.
