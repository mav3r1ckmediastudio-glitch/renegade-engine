# Renegade UI Designer — first owner test build (UI-06)

**Date:** 16 October 2026. **Branch:** \`feature/p3-original-impact-vfx\`, HEAD at start \`60634ce\`. Local dirty tree is shared with assistant/Studio and other work; no commit/push/checkout/rebase done.

## Scope of first owner test

The native Studio Screen Editor now provides the approved four-tab **Screens / Gameplay / Elements / Globals** toolbox, category search and collapse, real StoryFlow Screen list, a persistent on-screen Layers region with one-step/front/back reordering and drag-to-reorder, native canvas move/resize selection, independent grid/snap, align-to-peers, and real creation paths for Text, Button, Image, Background, Panel and Heading. The Basic/Advanced Inspector handles element naming, text, visibility/enabled, typography/styles, imagery, layout and symbolic button actions, with Inspector-first selection of existing StoryFlow Screen destinations.

The Screen Editor uses one chronological Undo/Redo dispatcher for Screen and Flow route changes. Simple Inspector destination edits stay governed by the StoryFlow authoring authority. Save is journaled across current Screen and StoryFlow documents with a rollback/recovery-capable project transaction, external-change protection and history preservation. **ASSETS in StoryFlow opens the Level Editor's existing Asset Browser, not the UI element library.**

## Final UI-06 workspace polish

- Ctrl+S invokes Screen Editor coordinated Save. Save is enabled even when the **only** dirty document is StoryFlow.
- Focus/Panels toggle expands the central canvas and restores both panels without changing authored Screen geometry.
- Panel widths, palette tab, grid visibility, snapping and spacing persist in \`Intermediate/Editor/ScreenWorkspace.txt\` inside the project (editor-only, not game Runtime data). Bad or out-of-range preferences fail closed.
- Alt temporarily bypasses canvas snap (matching approved UX).
- [ and ] move a selected layer one step, and Shift+[ / Shift+] send to back/front, through the same validated Undo-authority.
- Unsupported toolkit entries deliberately have **no active creation command**; they cannot fabricate gameplay bindings, UI screen composition, animations or Runtime widgets.

## Evidence / acceptance gates

- Rebuilt \`RenegadeScreenAuthoringSessionTests\` Release target + test: PASS. Includes UI-06 preference roundtrip and corrupt-setting rejection.
- UI-01/02/03/04/05 prior Screen/Flow tests: prior targeted suites PASS; re-run six tests after UI-06 build.
- UI-06 **linked Studio Release PASS**, exit 0 (344.48 s), log \`BUILD/ui06-test-ready-studio-release.log\`. Executable: \`BUILD/renegade/Studio/Release/RenegadeStudio.exe\` (2026-10-16 17:28:45 +01:00).
- Six targeted Screen/Flow Release CTests **PASS 6/6**, including atomic-save rollback and UI-06 editor preferences. Scoped git diff check PASS.
- Launcher created: \`C:\Users\paulw\Downloads\Renegade_UI_Designer_TEST_LOCAL.cmd\`. The launcher refuses to start while another Studio is running. An older CI-downloaded Studio was open and intentionally not terminated.
- Live GPU-rendered visual/keyboard checks still need the first hands-on pass. The successful build and tests cannot certify input hit targets, DPI, canvas overlap or actual click transitions.

## Manual first-acceptance script (isolated sample project; don't use irreplaceable game content)

1. Open **Renegade Studio Release**, enter an existing sample project, StoryFlow, then a Screen node. Confirm four tabs / Layers / Inspector are visible and the canvas preview is centered without overlapping the chrome.
2. Try toolbox search, categories, basic element creation and Layers selection/order, both drag and [ / ] shortcuts. Confirm Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y undo/redo correctly.
3. Test panel-resize handles and Focus/Panels, adjust Grid/Snap and spacing; leave/reopen Screen and confirm preferences restore.
4. Edit Text and a Button; select On Click action and a real Screen destination. Confirm StoryFlow Graph's existing single route updates, no fake Screen node appears, and shared/conditional actions refuse silent overwrite.
5. Make *only* a Flow destination edit; verify Save is enabled, use Ctrl+S, leave/reopen Screen/Flow and verify both persisted. Confirm undo/redo still works and project assets remain unchanged.
6. Verify unsupported entries are visibly disabled, no unbounded horizontal overflow at 1366×768, and Advanced Inspector overlays don't accidentally mutate the Basic Inspector.

## Explicitly not part of this test

Full UI-001–UI-029 Runtime widget support is not implemented. Binding gameplay attributes, live HUD composition and visibility, full animation timeline, additional input widgets/VR/accessibility, global themes, reusables and automated project template scaffolding remain real future engine features. **Do not describe the first native UI testing build as a fully complete UI framework.**

No owner game assets, WickedEngine submodule, or unrelated assistant/projectile files edited. No CI push or PR created.
