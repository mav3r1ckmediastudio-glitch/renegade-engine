# Manual Screen geometry, 27 November 2026

The owner reported that the Title Continue button could not be resized. Its Actions parent used a Grid container: the shared layout stretched its cell across most of the screen, the basic Inspector disabled all geometry fields, and the canvas hid/blocked resize handles. The preceding vial review left this usability failure unresolved despite showing the large button in Runtime.

## Behaviour and ownership

Single-element dragging, corner resizing and basic numeric geometry edits now use ScreenAuthoringSession::UpdateWidgetManually. A real geometry change inside an automatic container atomically sets that element to schema-3 manual layout and writes the requested parent-relative rectangle. The element retains its identity, parent, action, style, bindings and inherited state. Its original automatic slot stays reserved so sibling allocations remain unchanged. Content-only edits keep automatic layout. Ordinary UpdateWidget and other automatic-container safeguards retain their prior behaviour.

ScreenLayoutMode::Manual is serialized as layout_mode=manual and resolved through the same EngineBridge geometry function used for rendering and pointer hit tests. Both Studio and Runtime must use the updated bridge. A gesture coalesces the layout change and all intermediate rectangles into one Undo operation; Undo restores the original automatic mode as well as geometry. Manually positioned cells also support translation and existing batch geometry operations. Parent container settings and unrelated controls are not rewritten. Existing size constraints still apply.

The Advanced layout selector exposes Manual alongside Absolute and Anchored. Selecting Absolute or Anchored again lets an automatic parent resume controlling the element. Batch edits of still-automatic cells and grouping inside managed parents retain their existing guardrails.

New Title templates use a freely editable root Continue button at design rect (460,592,360,64), matching the established free Pause-button approach. Existing project templates are not silently migrated. The reported review Screen is verified through the actual native Inspector after rebuilding; temporary test changes are undone to preserve the owner's saved geometry.

## Automated verification

Windows x64 DX12 Release. Explicitly recompiled consumers of changed Screen headers because this reused build has incomplete header tracking. Built EngineBridge, ScreenRenderer, RuntimeBootstrap, Studio, Runtime and these seven executables using configured VS18 MSBuild with /t:Build /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:CL_MPCount=3 /m:1 /v:minimal /nologo and CL=/MP3:

- RenegadeScreenTests
- RenegadeScreenAuthoringSessionTests
- RenegadeScreenLibraryTests
- RenegadeScreenHealthPrefabTests
- RenegadeScreenRendererTests
- RenegadeRuntimeScreenTests
- RenegadeProjectUiScaffoldTests

All seven suites passed. The amended authoring suite verifies explicit manual resize versus ordinary managed edits, content-only edit preservation, unchanged sibling rectangles and parent identity, one-gesture Undo/Redo, mode/size save-reload, subsequent movement and batch geometry. Scaffold integration verifies the new Title button's free geometry. Existing serialization, renderer, runtime focus/action, library and health/mana prefab suites passed.

Native visual checks and final source checkpoint are recorded below and in HANDOFF.md. No hosted CI, Debug, exported-game packaging or independent release approval is claimed.

## Preview return lifecycle correction

During native checks the owner reported a clipped/offset canvas after closing interactive Preview; restarting cleared the view. The close path reattached the editor renderer then called a dimension-cached LayoutWorkspace, which could return without restoring the editor viewport. Closing now invalidates that cache, restores the editor viewport and clip explicitly and reapplies layout. The workspace's existing camera and panel configuration are retained.

Preview entry/exit also finishes an active canvas gesture and clears transient panning, panel dragging, layer dragging and pending palette input. Basic native text fields relinquish typing ownership before the workspace is hidden. After Preview closes, editor input waits for held mouse buttons/Escape to be released and consumes the return frame so the close gesture cannot begin another editor edit. These changes do not rewrite Screen data.

The Preview correction compiled and passed the native checks below. Preliminary resizing/Undo attempts overlapped active owner testing and are excluded from final verification evidence. The owner then explicitly authorized a restart and uninterrupted checks.

## Final native receipt

The initial full Release build and seven suites passed in 538.70 seconds. Preview-return changes explicitly recompiled 39 Studio translation units using the same configured MSBuild options with /t:ClCompile. A final /t:Build /p:TargetName=RenegadeStudioPreviewReview compiled the final workspace input cleanup and linked successfully (14.71 seconds), without replacing the owner's running application. After the owner authorized restart, that built executable was copied to the normal Studio executable location and launched there. RenegadeStudioUiPolicyTests.exe and git diff --check passed. No additional Screen model changes were made after the seven suites passed.

Actual native checks used the existing health-prefab-review-1127/UI Starter project, preserving the owner's current Title Screen before testing. Mouse dragging the Continue bottom-right handle changed its size from431.2x96.8 to368x80 without moving its top-left. One native Undo followed by Save restored the owner's Screen bytes exactly. Redo/Undo worked. Numeric width320 and height64 applied immediately and saved; comparing documents showed that only those two Continue properties changed, retaining all IDs, parent, action, styles, bindings and vial layers. Two native Undo operations restored the exact owner bytes. Returning to Story Flow and reopening Title retained the owner's manual431.2x96.8 geometry at(448,512).

Five interactive Preview return checks passed: normal Close preview; Continue route to Main Menu followed by Escape; focused canvas with wheel zoom and middle-button pan followed by Escape; the same view followed by Close preview; and another Escape cycle. Both normal and focused/panned views returned to their original geometry. Static cork crops before/after matched pixel for pixel (normal304,184..344,208; focused240,128..296,152), while liquid bubbles continued animating. Reset the view to fitted canvas with both panels visible. Save then matched the owner baseline SHA256 516e90b22d4ea6101da4f930e051edb775a8ad7ff6a5473d0e9961329e1ad7c0 exactly. This confirms the test sequence did not alter saved Screen content.

Standalone RenegadeRuntime.exe --project "BUILD/health-prefab-review-1127/UI Starter/UI Starter.renegade" showed the saved compact manual Continue at75 FPS. Clicking it opened Main Menu. Runtime was closed and the final Studio left open with Continue selected, its editable dimensions visible and the saved owner content retained. The owner was told control was returned before documentation/commit work continued.

Screenshots are under docs/evidence/button-resize-2026-11-27. This is scoped Windows DX12 Release verification. Debug, exported-package playback, small-window handling and independent exact-commit acceptance remain unverified. No GitHub push, CI, merge or release. Pre-existing Wicked changes and root log.txt are excluded.
