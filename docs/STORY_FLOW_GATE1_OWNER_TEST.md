# Story Flow Gate 1 owner test

This document defines the controlled owner-facing visual/interaction test for Story Flow Gate 1.

Gate 1 remains read-only at the Story Flow semantic boundary. The purpose of this test is to prove that a real LP02 Flow can be opened and rendered natively in Renegade Studio, that presentation interaction works, and that presentation state remains separate from runtime Flow semantics.

## Why a dedicated fixture is required

Existing creator projects are not expected to contain multi-level Story Flow documents because Renegade does not yet expose a user-facing level/route authoring workflow. Gate 1 therefore must not depend on the owner already having a suitable project.

The fixture generator creates a separate project by copying the source project's existing startup scene twice. It never edits the source project.

The generated Flow is:

`GAME START -> LEVEL ONE -> LEVEL TWO -> COMPLETE GAME`

This is deliberately small but gives enough structure to inspect node rendering, route rendering, selection, panning, zooming, fit/centre behaviour, deterministic layout and persisted presentation state.

## Create the fixture

From the repository root in PowerShell:

```powershell
.\Tools\StoryFlow\Create-Gate1OwnerFixture.ps1 -SourceProject "C:\path\to\YourProject.renegade"
```

The source project may contain only one real level. The script uses that scene as a safe visual seed for both fixture Level nodes.

By default the fixture is created beside the source project as:

`Renegade-StoryFlow-Gate1-Fixture\StoryFlowGate1Fixture.renegade`

Use `-DestinationRoot` to choose another location. Existing destinations are never replaced unless `-Force` is supplied explicitly.

## Gate 1 Studio behaviour under test

When Studio finishes opening a project with a valid stable `startup_flow`, the Gate 1 lifecycle adapter resolves that Flow through the existing LP02 stable-ID contract, reads and validates it, builds the shared `StoryFlowAuthoringModel`, restores or creates the separate Story Flow layout document, and displays the native Story Flow workspace over the central Studio viewport.

The current startup Scene is still prepared and adopted underneath because changing the New Project / startup lifecycle is explicitly a later gate. Gate 1 does **not** change that contract.

The Story Flow header exposes two presentation-only controls for owner acceptance:

- `FIT` frames the entire current Flow.
- `START` centres the canvas on the permanent Game Start destination.

Mouse wheel zoom is cursor-relative. Middle-mouse drag pans the canvas. Node selection is left-click. Presentation changes are persisted under `Saved/EditorState/StoryFlow/<flow-document-id>.renegade-flow-layout`; they never write the semantic `.renegade-flow` document.

A missing or invalid semantic Flow is an error and is not silently replaced. A missing or damaged **layout** may be rebuilt because layout is non-semantic presentation state.

## Preconditions before owner testing

Do not begin the owner test merely because the fixture exists. The exact PR head containing the Studio `startup_flow` integration must first pass both Windows baseline and Renegade Studio Debug/Release CI.

The Release build used for the test must correspond to that exact PR head.

## Owner acceptance steps

1. Generate the fixture from any known-good existing Renegade project.
2. Open `StoryFlowGate1Fixture.renegade` in the exact-head Release build.
3. Confirm the project reaches the native Story Flow workspace rather than silently falling back to an unrelated blank editor state.
4. Confirm exactly four semantic destinations are visible: Game Start, Level One, Level Two and Complete Game.
5. Confirm the three routes are visually present in the expected order and show their LP02 outcome labels when sufficiently zoomed in.
6. Select each destination and confirm selection feedback follows the clicked destination.
7. Pan the canvas with middle-mouse drag.
8. Zoom in and out with the mouse wheel and confirm cursor-relative zoom remains stable and usable.
9. Click `FIT` and confirm the complete fixture returns to a readable framing.
10. Pan away, then click `START` and confirm Game Start is centred without altering the Flow.
11. Change only presentation framing, return to the Project Hub, reopen the fixture, and confirm the saved pan/zoom presentation is restored.
12. Confirm the semantic Flow still contains the same four destinations and three routes after that reopen.
13. Report any clipping, unreadable labels, broken routing, input conflicts, incorrect workspace visibility, unexpected scene/editor overlays or other visual defects.

## Pass condition

Gate 1 owner acceptance passes only when the controlled fixture renders and behaves correctly in the Release build and no presentation operation changes the authoritative Flow document or runtime traversal.

Gate 1 does **not** require Level creation, Screen nodes, Flow mutation, route authoring, Screen Editor, Journey View polish or the New Project -> Story Flow lifecycle. Those remain later gates in `STORY_FLOW_JOURNEY_VIEW_IMPLEMENTATION_PLAN.md`.
