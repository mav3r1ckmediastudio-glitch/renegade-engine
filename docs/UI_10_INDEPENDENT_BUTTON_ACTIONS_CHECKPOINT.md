# UI-10 — Screen Buttons get independent built-in actions

Date: 17 October 2026 (UK). Shared dirty branch `feature/p3-original-impact-vfx`, baseline HEAD `60634ce`. UI work only; do not checkout/rebase/push, touch user game project, WickedEngine, night-sky or local assistant sources.

## Problem

The UI-08 native Screen Editor's Add Button defaulted `actionId` to `document.actions.front().id`. On a Screen with no actions, creation immediately refused. Where there were two or more Buttons, adding another silently shared an existing action and destination route, which the UI-024 Inspector destination planner rightly rejected as ambiguous. Duplicating a Button copied that same action identity and caused the same problem. Normal buttons must never require Lua scripting.

## Implementation

- `EngineBridge/include/renegade/bridge/ScreenAuthoringSession.h`: add `CreateButtonWithUniqueAction` as a single validated Screen authoring command. It allocates an unused `ui_button_N` symbolic action, creates the Button and focus entry in the same candidate document, then commits once. Stable IDs are separate from symbolic action IDs.
- `EngineBridge/src/ScreenAuthoringSession.cpp`: deterministic candidate-local unique action name generator, 64-action schema guard; duplication of a Button now clones its styling and creates its **own outcome** instead of binding it silently to the original. Both operations use one Undo transaction, with no partial mutation on failure.
- `Studio/src/RenegadeScreenEditorWorkspace.cpp`: Add Button uses the owned outcome transaction and no longer reuses the first Screen action. When a new Button's Inspector opens the destination picker, current Screen and Flow are autosaved atomically before applying the route, avoiding a dependency on waiting 1.5 seconds for idle autosave. The existing StoryFlow graph retains exclusive navigation authority and normal buttons never need scripts.
- `Tests/ScreenAuthoringSessionTests.cpp`: verifies unique actions for new and duplicated Buttons, exactly one history entry, Undo/Redo of duplicated Button and action, persistence across save/reopen, and rejection at 64 actions without dirtying the original session.

## Validation

- Scoped `git diff --check` passed before target test build.
- Release test target build log: `BUILD/ui10-unique-button-tests-build.log`; final build/CTest status pending at checkpoint creation.
- Studio Release link and launch smoke test pending. Do not replace an open user's Studio executable or update Downloads launcher unless linked test executable is verified.

## Remaining acceptance limitations

Existing projects can intentionally share action IDs; UI-024 will still reject simple destination reassignment when an action is shared or uses multiple conditional StoryFlow routes. Such graph editing remains in the existing StoryFlow editor.

The broader UI-001..029 designer is not complete. Independent Button outcomes, user-custom-font import, image project import, Layers, Inspector, editor autosave and basic Screen routing are real implemented slices; editor zoom/pan, multi-select, full widget families, live gameplay bindings, global HUD, complete project templates and optional Screen-widget Lua remain future work. A successful automated test cannot replace a native GPU/keyboard/mouse acceptance pass.

## Final validation (17 October 2026, UK)

- `RenegadeEngineBridge` rebuilt Release static library successfully. The first native authoring test failed only because an older assertion expected a copied Button to share its original action. That assertion was updated to require a separate symbolic action.
- `RenegadeScreenAuthoringSessionTests` rebuilt and **PASS 1/1**, `BUILD/ui10-button-tests-fast-rebuild.log`.
- Screen/StoryFlow/ResourceAsset/Transaction regression **PASS 30/30** (10.48 seconds), `BUILD/ui10-30-screen-flow-regression.log`. Existing test binaries are included; the changed authoring test target was specifically rebuilt first.
- New isolated Studio Release build **PASS**, exit 0 (329.92 seconds), `BUILD/ui10-button-studio-release-build.log`.
- Verified file: `BUILD/renegade/Studio/Release/RenegadeStudioUIDesignerButtonTest.exe`, 28,877,312 bytes, modified 2026-10-17 02:12:20 +01:00.
- Scoped `git diff --check` **PASS**.
- Downloads launcher `C:\Users\paulw\Downloads\Renegade_UI_Designer_TEST_LOCAL.cmd` points to this UI-10 binary and checks **all** running `RenegadeStudio*` process names, including Aurora/MilkyWay preview variants.
- A separate `RenegadeStudioMilkyWayColourPreview` process was running from the night-sky worktree when the binary linked. It was intentionally neither closed nor touched. Startup smoke / actual GUI mouse-and-keyboard validation was deferred to avoid interfering with the owner's current preview. Do not claim that UI-10 has been visually owner-approved.

## First owner test for this slice

After saving and closing any Studio preview, use the Downloads UI Designer test launcher. In a disposable Screen, add Button A and Button B, assign different existing Screen destinations via Inspector, duplicate Button A and assign another destination to the duplicate. Confirm A, B and duplicate navigate independently and no Lua is required. Undo the duplication and verify its action disappears with it; redo restores both. Leave the Screen to trigger coordinated autosave, reopen and check that the two destinations remain distinct. Do not run multiple Studio variants against the same project.
