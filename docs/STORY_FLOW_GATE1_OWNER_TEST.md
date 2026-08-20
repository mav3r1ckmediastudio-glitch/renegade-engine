# Story Flow Gate 1 owner test

This document defines the controlled owner-facing visual/interaction test for Story Flow Gate 1.

Gate 1 remains read-only at the Story Flow semantic boundary. The purpose of this test is to prove that a real LP02 Flow can be opened and rendered natively in Renegade Studio, that presentation interaction works, and that presentation state remains separate from runtime Flow semantics.

## Why a dedicated fixture is required

Existing creator projects are not expected to contain multi-level Story Flow documents because Renegade does not yet expose a user-facing level/route authoring workflow. Gate 1 therefore must not depend on the owner already having a suitable project.

The fixture generator creates a separate project by copying the source project's existing startup scene twice. It never edits the source project.

The generated Flow is:

`GAME START -> LEVEL ONE -> LEVEL TWO -> COMPLETE GAME`

This is deliberately small but gives enough structure to inspect node rendering, route rendering, selection, panning, zooming, fit/centre behaviour, deterministic layout and persisted presentation state.

## Fixture creation

`Tools/StoryFlow/Create-Gate1OwnerFixture.ps1` is a maintainer/developer convenience for creating the controlled fixture from any known-good Renegade project. The source project may contain only one real level; the script uses that scene as a safe visual seed for both fixture Level nodes.

Owner acceptance does **not** require the owner to use PowerShell, Git or a terminal. A prebuilt fixture may be supplied directly for the owner-facing test.

The default generated fixture is:

`Renegade-StoryFlow-Gate1-Fixture/StoryFlowGate1Fixture.renegade`

Existing destinations are never replaced unless the generator is explicitly run with its force option.

## Gate 1 Studio behaviour under test

When Studio finishes opening a project with a valid stable `startup_flow`, the Gate 1 lifecycle adapter resolves that Flow through the existing LP02 stable-ID contract, reads and validates it, builds the shared `StoryFlowAuthoringModel`, restores or creates the separate Story Flow layout document, and displays the native Story Flow workspace over the central Studio viewport.

The current startup Scene is still prepared and adopted underneath because changing the project-home/startup lifecycle is explicitly a later gate. Gate 1 does **not** change that contract.

This overlay is a temporary Gate 1 integration scaffold, not the target architecture. The locked product architecture requires Story Flow to become its own first-class Studio render path/workspace; the 3D Level Editor must only run when a Level is explicitly opened from Story Flow and must return control to Story Flow afterwards.

The Story Flow header exposes two presentation-only controls for owner acceptance:

- `FIT` frames the entire current Flow.
- `START` centres the canvas on the permanent Game Start destination.

Mouse wheel zoom is cursor-relative. Middle-mouse drag pans the canvas. Node selection is left-click. Presentation changes are persisted under `Saved/EditorState/StoryFlow/<flow-document-id>.renegade-flow-layout`; they never write the semantic `.renegade-flow` document.

A missing or invalid semantic Flow is an error and is not silently replaced. A missing or damaged **layout** may be rebuilt because layout is non-semantic presentation state.

## Preconditions before owner testing

The exact implementation head used for owner acceptance was:

`6f02f00519b344faa2fbe9a0f0d9d9174ad3f8d4`

Authoritative CI for that implementation head:

- Renegade Studio run **628** — success in Debug/Release;
- Windows baseline run **1156** — success.

The owner used the Release artifact from Studio run 628.

## Owner acceptance steps

The controlled owner-facing interaction acceptance is:

1. Open `StoryFlowGate1Fixture.renegade` in the exact implementation Release build.
2. Confirm exactly four semantic destinations are visible: Game Start, Level One, Level Two and Complete Game.
3. Confirm the three routes are visually present in the expected order and show their LP02 outcome labels when sufficiently zoomed in.
4. Select Level One, Level Two and Complete Game and confirm selection feedback follows the clicked destination.
5. Zoom with the mouse wheel and confirm the Story Flow canvas scales correctly.
6. Pan with middle-mouse drag.
7. Click `FIT` and confirm all four destinations return to a readable framing.
8. Click `START` and confirm Game Start is re-centred/focused.

Automated Gate 1 tests separately cover deterministic layout round-trip/reconciliation and prove that presentation state does not change the authoritative Flow traversal semantics.

## Owner acceptance result — PASS

**Accepted:** 2026-08-20, owner Release test.

The corrected controlled fixture opened successfully in the exact implementation Release build and visibly rendered:

`GAME START -> LEVEL ONE -> LEVEL TWO -> COMPLETE GAME`

The owner confirmed all five requested interaction checks passed:

- node selection;
- mouse-wheel zoom;
- middle-mouse pan;
- `FIT`;
- `START`.

The screenshot also confirmed the expected temporary Gate 1 condition: the existing Level Editor chrome/scene remains around and underneath the Story Flow surface. That is accepted only as the Gate 1 integration scaffold and must not become the finished Story Flow architecture.

An earlier manually prepared fixture package was correctly rejected because it accidentally carried an asset registry belonging to the source project. The corrected fixture used consistent project identity and opened normally. This was fixture packaging error, not a Studio/Story Flow failure; the fail-closed project validation behaved correctly.

## Pass condition

Gate 1 owner visual/interaction acceptance is **passed**.

Gate 1 still requires final documentation reconciliation and exact-head closeout CI after those documentation commits before the PR itself is considered ready to merge.

Gate 1 does **not** require Level creation, Screen nodes, Flow mutation, route authoring, Screen Editor, Journey View polish or the New Project -> Story Flow lifecycle. Those remain later gates in `STORY_FLOW_JOURNEY_VIEW_IMPLEMENTATION_PLAN.md`.
