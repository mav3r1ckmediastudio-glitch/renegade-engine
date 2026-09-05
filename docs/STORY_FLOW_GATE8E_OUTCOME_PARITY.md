# Story Flow Gate 8E — Outcome and Package Parity

## Outcome

Gate 8E closes the functional seam between the accepted Gate 8D Screen Editor
and Story Flow routing without moving routing authority into Screen documents.

A Screen continues to author **symbolic action IDs** only. Story Flow continues
to own where each action goes. Gate 8E makes that relationship explicit,
validated and diagnosable across Studio, save/reopen, Runtime and packaged
standalone execution.

Gate 8E does **not** redesign Journey View. Gate 9 remains the Journey UI/UX and
large-project authoring programme. Gate 10 remains the broader final
Runtime/build/standalone closeout. There is no Gate 8F.

## Locked authority boundary

- `ScreenDocument.actions` declares the Screen's stable symbolic outcomes.
- Buttons bind to one declared Screen action ID.
- `FlowRoute.outcome` is the Story Flow-side reference to that symbolic action.
- `FlowRoute.destinationNodeId`, destination entry, conditions and priority stay
  exclusively in Story Flow.
- Renaming/deleting a Screen action never silently rewrites Story Flow routing.
  A mismatch becomes a creator-visible diagnostic that must be repaired
  explicitly in Story Flow.

## Cross-document parity audit

`StoryFlowScreenReferenceService::AuditScreenOutcomes` resolves the governed
Screen by stable document identity and checks the current Flow against it.

For one Screen node:

1. every outgoing Flow route must use an action currently declared by that
   Screen;
2. every action currently referenced by a Button must have at least one Story
   Flow route;
3. a declared action that no Button currently uses may remain unrouted;
4. multiple routes for the same valid Screen action remain legal so existing
   condition/priority semantics continue to belong to `FlowInterpreter`.

The audit never mutates either document.

## Studio behaviour

Story Flow now asks the governed Screen for its current action catalogue when a
route is created or its outcome is edited.

- A Screen-origin route cannot be changed to an arbitrary outcome that the
  Screen does not author.
- Creating a route from a Screen starts with a real authored Screen action
  rather than the old generic `next` fallback.
- Non-Screen source nodes retain the existing symbolic outcome workflow.
- The full parity audit reruns after validated Story Flow semantic changes,
  after content lifecycle reloads, on Flow reopen and after returning from the
  Screen Editor.
- A renamed/deleted Screen action therefore produces an immediate Story Flow
  diagnostic instead of surviving silently until play.

A mismatch is intentionally repairable authoring state. Studio does not rewrite
destination routing behind the creator's back.

## Runtime / standalone behaviour

`RuntimeFlowController::Initialize` performs the same parity audit before the
Flow interpreter starts.

Only Screen nodes that are potentially reachable from Game Start are required
to be runtime-ready. Detached Screen drafts remain legal project content.

A reachable Screen whose Button action has no Story Flow route, or whose Flow
route references an action no longer authored by the Screen, fails closed with
an explicit Screen outcome-parity error. The packaged standalone uses this same
Runtime path; no separate standalone routing implementation is introduced.

## Automated proof

`RenegadeStoryFlowGate8EOutcomeParityTests` proves:

- a valid Screen/Button/Flow mapping passes;
- unused declared Screen actions do not require routes;
- save/reopen preserves a valid mapping;
- Runtime accepts a valid mapping;
- renaming a Button-used Screen action without changing Story Flow exposes both
  the stale Flow outcome and the newly unrouted Button action;
- Runtime refuses that mismatched mapping;
- explicitly repairing the Flow outcome restores Studio/Runtime parity;
- deleting the last route for a Button-used action is diagnosed;
- multiple conditional routes for one valid Screen action remain legal.

The existing Gate 2 Screen semantics proof remains the end-to-end execution
foundation for `Screen action ID -> FlowInterpreter outcome -> Level/Screen/
terminal destination`.

## Packaged owner acceptance

Use the exact Release artifact produced by the final Gate 8E CI head.

1. Launch `Run-RenegadeStudio-DX12.cmd`, create/open a project and enter Story
   Flow.
2. Use a Screen with at least two Button actions and create valid Story Flow
   destinations for them. Save the Flow.
3. Open that Screen in Screen Editor. Rename one Button-used symbolic action
   (for example `options` to `settings`) and Save.
4. Return to Story Flow. The affected Screen must be selected/focused and the
   status must report that the old route outcome is no longer authored and the
   renamed Button action has no Story Flow destination.
5. Select the stale route. Attempting to apply the old action ID must be
   rejected because it is not an authored Screen action.
6. Change the route outcome to the renamed action ID and apply it. The parity
   warning must clear once every Button-used action has a valid Story Flow
   destination.
7. Save, return to the Hub or close Studio normally, reopen the same project and
   confirm the repaired routes remain synchronized.
8. Run the project through the existing Runtime/test workflow and activate the
   repaired Button. It must reach the Story Flow destination assigned to that
   action.
9. Build the project through Renegade's existing Windows Game build workflow,
   launch the packaged standalone, activate the same Button and confirm the same
   destination is reached.
10. Confirm the Screen Editor never asks for or stores a destination node/path;
    only Story Flow owns that mapping. Confirm no stock Wicked Editor window is
    exposed.

Any silent route rewrite, arbitrary invalid Screen route outcome, missing
return/reopen diagnostic, Runtime acceptance of a reachable broken mapping, or
standalone behaviour that differs from Runtime rejects Gate 8E.

Gate 8E is not merge-ready until exact-head Debug/Release CI and this packaged
owner audit pass.
