# UI-01 — Element-to-element snapping and editor alignment guides

**Status:** bounded local implementation candidate, owner visual acceptance pending.
**Date:** 16 October 2026.
**Local base:** `feature/p3-original-impact-vfx` at `60634ce` with unrelated dirty assistant/Studio work.
**Design authority:** UI-026 and Concept 04 under the frozen UI-001–UI-029 requirements.

## Change summary

- `Studio/src/ScreenElementSnap.h`: pure design-space alignment calculation. Move supports left/centre/right and top/centre/bottom against nearby peer edges and centres. Resizing aligns only dragged edges, preserving the fixed opposite edge and minimum size. Grid result remains fallback independently per axis.
- `Studio/src/RenegadeScreenEditorWorkspace.cpp/.h`: integrate the calculation into existing direct dragging through `ScreenAuthoringSession::UpdateWidget`; consider only visible sibling elements with the same parent, whose final authored positions are resolved through `ResolveScreenWidgetRect`. Display temporary orange horizontal and vertical guide lines in the canvas, with coordinates transformed by the shared Screen-service viewport geometry.
- The magnetic threshold is 8 *screen pixels*, converted to design units for the current preview scaling. The existing SNAP control governs grid and element snapping; Shift bypasses both. Grid visibility remains independent. Guides clear on release.
- `Tests/ScreenAuthoringSessionTests.cpp`: deterministic alignment test cases for independent axis matching, centre alignment, moving versus resizing, opposite-edge preservation, and unmatched-axis grid fallback.

## Architecture and safety

No ScreenDocument schema, renderer authority, gameplay service or StoryFlow route changes; no changes to saved existing project assets. Screen Undo/Redo and shared Runtime preview remain authoritative. No upstream Wicked source modifications. No writes to the other work's dirty TestLevelRuntimeProcess, StudioApplication, assistant code or shared HANDOFF files.

This is **not** the complete UI Designer or all of UI-026. Project/per-screen grid preference persistence, full resizable Concept 04 workspace, and owner visual evaluation remain outstanding. Multi-selection snapping is not implemented.

## Verification

- Release `RenegadeScreenAuthoringSessionTests` target compilation: **PASS** using VS18 Build Tools CMake executable and MSBuild v145.
- Release `RenegadeScreenAuthoringSessionTests.exe`: **PASS**, including newly added alignment assertions and existing document/undo/save/reopen regressions.
- Modified tracked-file `git diff --check`: **PASS**.
- Native Studio **source compilation PASS** via VS18 MSBuild `BUILD/renegade/Studio/RenegadeStudio.vcxproj /t:ClCompile /p:SelectedFiles=RenegadeScreenEditorWorkspace.cpp /p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64 /m:2 /verbosity:minimal` (exit 0 in 305.04 seconds). Output includes `RenegadeScreenEditorWorkspace.cpp`; this is compilation only, not linked Studio or on-screen acceptance. Full Studio Release build remains blocked independently by dirty `Studio/src/TestLevelRuntimeProcess.cpp:339`, `C2137: empty character constant`.
- No current full Studio executable build, visual acceptance or packaged Runtime test claimed.

## Next

Confirm native Screen Editor source compilation; await the owning assistant fix for the separate Test Level launcher build blocker. Then inspect actual Studio native interaction at desktop and smaller layouts, check grids over Runtime preview, 8px magnet guides, anchored elements, layer changes, Undo, Save/Open and controls' visibility. Require independent verification before declaring release gate complete. Keep source changes uncommitted until reviewed.
