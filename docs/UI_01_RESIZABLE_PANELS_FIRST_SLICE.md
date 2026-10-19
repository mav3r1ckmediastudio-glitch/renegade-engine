# UI-01 — Native Screen Editor resizable panels (first slice)

**Status:** implementation candidate, local tests and native Studio source compilation passed; linked Studio and visual acceptance pending.
**Date:** 16 October 2026.
**Branch / HEAD:** `feature/p3-original-impact-vfx` / `60634ce`, with unrelated uncommitted assistant work.

## Owner requirement

UI-027, based on approved Concept 04: independently resizable left Screen/Elements/Layers panel and right contextual Inspector; central canvas gains the available width. Keep the current Screen document, graph, shared preview renderer and gameplay logic authoritative.

## Scoped changes

- `Studio/src/ScreenEditorPanelLayout.h`: pure tested workspace geometry (default left 260, right 360, 14px gaps, 200px minimum left, 340px minimum right, 380px reserved canvas on supported medium/large desktop sizes). Drag positions clamp without silently resizing the opposite pane; narrower layouts reduce the canvas reservation before overlapping panels.
- `Studio/src/RenegadeScreenEditorWorkspace.h/.cpp`: replace hardcoded side widths, add 12px-wide draggable divider hit regions and 2px subtle divider visuals, resize the left toolbox buttons and hierarchy text, notify the owning render path about viewport changes. Start-up at provisional 1px dimensions preserves real desktop pane defaults.
- `Studio/src/RenegadeScreenEditorRenderPath.h`: on splitter drag update the shared Screen renderer's *viewport* via `SetViewport` and `ApplyLayout` (no document mutation or resource reload); update Advanced Inspector width. On actual desktop resize refresh advanced width after clamping panels.
- `Studio/src/RenegadeScreenEditorAdvancedInspector.h`: support a preferred Inspector width rather than forcing a fixed 360px overlay, so the advanced controls continue to follow right-side resizing.
- `Tests/ScreenAuthoringSessionTests.cpp`: deterministic panel initial state, independent width dragging, 1024px constraints, max drag clamping and narrow 800px geometry. Existing Screen authoring, grid and sibling alignment tests remain part of the same target.

## Boundaries and acceptance

No change to ScreenDocument schema, gameplay, StoryFlow, Runtime, Wicked source or project assets. Only workspace geometry is affected. No global layout preference persistence or panel collapse shortcuts yet; those remain in the frozen UI-027 requirement, not in this slice.

**Critical acceptance not yet performed:** visual inspection of native Studio at 1920x1080, 1366x768 and a narrow desktop; mouse-drag splitters, preview alignment, advanced Inspector overlap, support of the existing 4-step layer-order bar, undo and saved Screen layout unaffected.

## Local verification

- VS18 Build Tools CMake target `RenegadeScreenAuthoringSessionTests` Release: **PASS**.
- `BUILD/renegade/Tests/Release/RenegadeScreenAuthoringSessionTests.exe`: **PASS**, including new panel layout tests.
- Scoped `git diff --check`: **PASS** before source compilation.
- Native Studio source **compilation-only PASS**: VS18 Build Tools MSBuild `BUILD/renegade/Studio/RenegadeStudio.vcxproj /t:ClCompile /p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64 /m:2 /verbosity:minimal` exited **0** after **282.96s**. `RenegadeScreenEditorWorkspace.cpp` and `StudioApplication.cpp` both compiled with new declarations; the Screen Editor object is newer than its final source. This is not a linked executable or visual test. Full Studio linking remains blocked by a separate dirty `Studio/src/TestLevelRuntimeProcess.cpp:339` containing `mutableCommand.push_back(L'');` (MSVC C2137). Do not modify that file in this task.
- No linked new Studio artifact, on-screen owner acceptance, independently verified release gate, commit, or push claimed.

## Next step

Complete Studio compilation-only check and document its outcome. Once the separate launcher error is resolved by its owner, build Studio Release and do the owner visual test. Continue towards Concept 04's four-tab toolbox and Inspector-first navigation only after basic layout responsiveness is validated.
