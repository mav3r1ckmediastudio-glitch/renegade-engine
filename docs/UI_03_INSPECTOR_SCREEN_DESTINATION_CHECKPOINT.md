# UI-03 — Screen Inspector destination authoring checkpoint

Date: 16 October 2026. Worktree: feature/p3-original-impact-vfx at initial HEAD 60634ce, shared dirty worktree; remote behind 13. Local/uncommitted.

## Scope
Continue approved UI-024: the normal Screen Inspector should offer **On Click → Open Screen → existing project Screen**. Do not create a second graph or route authority.

## Source implementation
- `EngineBridge/include/renegade/bridge/ScreenFlowDestinationPlan.h` (new): pure plan over validated ScreenDocument, FlowDocument, source Screen node, Button stable ID and target Screen node. Return a validated candidate FlowDocument and route identity; never mutate input documents or write to disk. Handle one route creation, one unconditional route update and idempotent no-op. Reject duplicate/shared Button action identity, multiple/conditional routes, wrong Source Screen identity, non-Screen destinations and invalid document data.
- `Tests/StoryFlowGate8EOutcomeParityTests.cpp`: candidate routing and non-mutation coverage, missing-route creation, idempotence, destination type, shared action rejection, conditional/branching route rejection, and mismatched source identity.
- `Studio/src/RenegadeScreenEditorWorkspace.h/.cpp`: new native destination picker under symbolic action selector for selected Buttons; source project Screen choices come from the previously-connected StoryFlow Screen library. Existing action picker and basic/advanced inspectors remain intact.
- `Studio/src/RenegadeScreenEditorRenderPath.h` route callback bridge: forwards the ScreenDocument, selected Button ID and destination node ID to the coordinator.
- `Studio/src/StoryFlowStudioIntegration.h`: one coordinator callback uses **the existing StoryFlowAuthoringSession** to AddRoute or UpdateRoute (single Flow history entry) after validation; refuses to route an unsaved new/changed Button action (rechecks on-disk Screen identity), updates the one StoryFlow model and existing Journey/Graph presentation. Roll back the Flow history entry if presentation refresh fails.
- `Studio/src/RenegadeStoryFlowWorkspace.h` and `RenegadeStoryFlowRenderPath.h`: expose narrow external mutation refresh wrapper instead of creating another routing cache/model.

## Verification
- Rebuilt Release target `RenegadeStoryFlowGate8EOutcomeParityTests`: **PASS**, exit 0 (log BUILD/ui03-route-plan-test-build.log).
- Focused rebuilt CTest: **PASS** (0.36 seconds).
- Scoped `git diff --check`: PASS before Studio source compile.
- Studio Release ClCompile for Inspector/coordinator: **PASS** (exit 0, 308.20 seconds), log `BUILD/ui03-destination-inspector-compile.log`.
- Full linked Studio Release target: **PASS** (exit 0, 8.29 seconds), `BUILD/ui03-destination-studio-release-build.log`.
- Final six focused Release Screen/Flow CTest suites: **PASS 6/6** (0.93 seconds). This includes the newly rebuilt outcome parity test.
- Scoped `git diff --check`: **PASS** (LF/CRLF conversion warning only).

## Limits / pending gates
- A Flow route is modified in Flow-authoring **memory** and marked dirty; the user must save the Flow document to persist. Do not claim multi-document disk atomicity or one cross-editor unified Undo. The Screen action ID is not modified by this command, so this is **one-Flow-document** routing, but Screen Edit history and Flow route history are still separate.
- Graph/Runtime parity, physical click/keyboard and save/reopen tests are still pending in a linked executable. Conditional/multiple routes must be edited through existing ImNodes Graph for now, not a silent rewrite.
- The approved UI framework is still **incomplete** (other widgets need real schema/Runtime support, template project scaffolds, responsive variants, persistent workspace settings, safe unified history, themes/accessibility, etc). Keep owner testing on hold until accepted interface is actually ready.
