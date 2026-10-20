# UI-08 Native Designer — first integrated acceptance checklist

Date: 16 October 2026. Shared dirty worktree: branch feature/p3-original-impact-vfx; initial HEAD 60634ce, remote ahead by 13 commits. No checkout, reset, push or edits to unrelated engine/assistant/projectile branches or user's project assets.

## Test build contract

This is a **native editor workflow test**, not the completed UI-001 through UI-029 Runtime/UI Framework. The accepted Concept 03 visual direction has not had a full owner-side pixel/hit-test acceptance. Do not claim Script->Screen/widget Runtime execution or non-implemented HUD/gameplay widgets.

UI-08 source work:
- Selected element Delete and Backspace dispatch the same validated DeleteWidget transaction (when GUI text fields are not in edit mode); Undo/Redo controls preserve the deletion history.
- Main Inspector gives Name, Text, X/Y, Width/Height, visibility, enable, Apply, Copy and Delete, symbolic button action plus authoritative StoryFlow destination; Style/Font shortcut opens existing richer Inspector. Invalid/negative/nonfinite geometry is rejected.
- Main Inspector image control opens project Image picker with `Import from this Computer` option. Native dialog delegates to CreatorTextureWorkflowService, which retains/records LP08 source and product; a validated Content/UI/Images mirror is transactionally committed and assigned as a Runtime-resolvable Screen image. Picker enumerates only imported images whose mirror actually exists. Existing governed font choices remain unchanged.
- Left toolbox hides unavailable/unimplemented creator actions instead of offering inert controls. Screens, Gameplay, Elements and Globals tabs remain navigable; only implemented actions are actionable.
- Script attachment is explicitly optional advanced development. Existing Lua system attaches to Scene/Entity contexts, not Screen widgets. No inert script control is exposed as working, and no arbitrary Lua file is executed.

## Owner acceptance scenario (use a disposable test project)

1. Open current test build, StoryFlow -> an existing Screen. Four-tab toolbox, canvas, Layers and Inspector visible. Capture 1366x768 and 1920x1080 native screenshots; check Inspector input hit targets and Advanced overlay behaviour.
2. Add Text. Select from canvas and Layers; rename; change text; edit X/Y, Width/Height in main Inspector; Apply/Enter; verify element moves/resizes; Undo/Redo returns both.
3. With selection, press Delete; verify removed and Undo restores. Repeat with Backspace, but when a text field is actively typing, Backspace should edit text rather than delete element.
4. Add Image, choose project media; if none, select Import From This Computer; use a valid local PNG/JPG. Check import is visibly committed, assigned and preview shows image. Save and reopen. Confirm raw Content/UI/Images source and governed asset remain in disposable project, no absolute local path leaks into Screen documents.
5. Add Button, choose symbolic Action, choose a real Screen destination; verify same one authoritative StoryFlow route (no second graph), Save, reopen route. Undo and edit a different action; autosave after 1.5s idle. Do not silently repoint shared action/conditional routes.
6. Verify layer dragging and [ / ], Shift+[ / ], snapping/grid/Alt bypass, panel resizing and Focus/Panels restore. Close/reopen restores editor-only preferences.
7. Screens tab switches between saved Screens. Pending unsaved changes trigger guarded atomic Save; failed journal transaction cannot silently discard them.
8. Unsupported Gameplay/Globals/widgets should not appear as ready-to-use creator actions; scripting not yet wired to Runtime is clearly out of scope.

## Verification matrix

- UI-08 Release Studio source compile: PASS for initial Inspector + local media work; second source compile including mirrored Content paths and font preservation: PASS.
- Seven existing Screen, Flow, ScreenRenderer, ScreenAuthoringSession and ResourceAsset Release CTests: PASS 7/7 (2.53s).
- Standalone linked test build with different executable name (existing Studio process open): pending, BUILD/ui08-standalone-test-link.log. Do not substitute a previous UI-07 executable.
- Actual Windows GPU-rendered native visual and click-through testing: pending. Do not claim all controls work from compilation alone.
- Full UI-001..029 feature parity: NOT DONE; require new widget schemas, Runtime, manual gameplay bindings, templates, HUD/theme/accessibility and proper optional per-widget Lua event bridge.

The test launcher may be updated only after a successful standalone binary link and verified file. Do not close user's running RenegadeStudio.exe without coordination.

## Final native test build verification

- Existing Studio process is still running from `BUILD/renegade/Studio/Release/RenegadeStudio.exe`; not closed or modified.
- Initial Studio source compile PASSED and additional compiled final source includes Content/UI/Images and protected Font catalogue logic.
- First standalone link failed with missing `OnAppearanceRequested` because the large compilation had captured an older in-flight workspace translation unit. Root cause: file edited while compilation was underway; not an unresolved source definition.
- Recompiled final `RenegadeScreenEditorWorkspace.cpp` after source settled, and **standalone linked UI-08 Release build PASSED**, exit 0, `BUILD/ui08-standalone-test-link-retry.log`. Exact file: `BUILD/renegade/Studio/Release/RenegadeStudioUIDesignerTest.exe`, 28,853,760 bytes, modified 2026-10-16 18:56:13 +01:00.
- SHA-256 `C30BD04D851B35BDAD44291D7E5228CFC44D7C56B509133B64596FAAB907F7B5`. File path and checksum verified on DESIGN computer.
- **30/30 Release CTests PASSED**, `BUILD/ui08-acceptance-regression-30.log`, covering Screen, StoryFlow, ResourceAsset package/reimport/rollback and ProjectDocumentTransaction. No native GUI interaction automated by those tests.
- Scoped `git diff --check` PASSED after source settled.
- Downloads launcher `C:\Users\paulw\Downloads\Renegade_UI_Designer_TEST_LOCAL.cmd` now points specifically to the verified `RenegadeStudioUIDesignerTest.exe`; it refuses to start while RenegadeStudio.exe or another test instance is already open.
- The script-import/runtime widget hook is deliberately NOT available as a standard button action. Ordinary Screen button actions use governed symbolic outcomes and StoryFlow.
- **Acceptance:** usable test candidate for implemented editor workflows; **not** proof that all 29 framework requirements are built or that mouse hit-testing/visual styling and packaging of newly imported media have been visually accepted. Test only on a disposable project until these are checked.
