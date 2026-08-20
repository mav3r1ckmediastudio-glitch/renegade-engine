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

## Preconditions before owner testing

Do not begin the owner test merely because the fixture exists. The Story Flow Studio integration slice must first pass exact-head CI and must support opening a project's valid `startup_flow` into the native Story Flow workspace.

The Release build used for the test must correspond to that exact PR head.

## Owner acceptance steps

1. Generate the fixture from any known-good existing Renegade project.
2. Open `StoryFlowGate1Fixture.renegade` in the exact-head Release build.
3. Confirm the project reaches the native Story Flow workspace rather than silently falling back to an unrelated blank editor state.
4. Confirm exactly four semantic destinations are visible: Game Start, Level One, Level Two and Complete Game.
5. Confirm the three routes are visually present and connect in the expected order.
6. Select each destination and confirm selection feedback follows the clicked destination.
7. Pan the canvas with the implemented pan gesture.
8. Zoom in and out and confirm the cursor-relative zoom remains stable and usable.
9. Use fit/centre and confirm the complete fixture returns to a readable framing.
10. Close and reopen the project after changing only presentation layout. Confirm runtime/semantic Flow content has not changed.
11. Report any clipping, unreadable labels, broken routing, input conflicts, incorrect workspace visibility, unexpected scene/editor overlays or other visual defects.

## Pass condition

Gate 1 owner acceptance passes only when the controlled fixture renders and behaves correctly in the Release build and no presentation operation changes the authoritative Flow document or runtime traversal.

Gate 1 does **not** require Level creation, Screen nodes, Flow mutation, route authoring, Screen Editor, Journey View polish or the New Project -> Story Flow lifecycle. Those remain later gates in `STORY_FLOW_JOURNEY_VIEW_IMPLEMENTATION_PLAN.md`.
