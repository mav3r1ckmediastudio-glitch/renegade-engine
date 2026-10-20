# UI-01 first native slice — Screen layer-order controls

**Status:** local implementation candidate; owner visual and packaged acceptance pending.
**Date:** 16 October 2026.
**Branch / starting HEAD:** `feature/p3-original-impact-vfx`, `60634ce` (dirty worktree).
**Scope:** UI-029 only; minimal additive change to the accepted Screen authoring model. This is not the complete Concept 04 workspace, full UI-01, or permission to ship.

## Architecture findings (UI-00)

- `ScreenService.h` defines schema-v2 Image/Text/Button, stable IDs, parent hierarchy, absolute/anchored positions, symbolic actions and focus order.
- `ScreenAuthoringSession` is the only validated mutation/Undo/Redo/save authority used by the Screen Editor.
- `RenegadeScreenEditorWorkspace` already exposes direct drag/resize and absolute BACK/FRONT controls, but those were in the left creation area. The approved visual design calls for four distinct controls beneath the canvas.
- `StoryFlowAuthoringModel` / `FlowService` and the ImNodes Graph author authoritative Screen destinations separately; no new graph or routing service was introduced in this slice.
- The repository also contains unrelated pre-existing Qwen/assistant/Studio changes, with documented work in `HANDOFF.md`, `docs/ARCHITECTURE.md`, `docs/ROADMAP.md` and `docs/FEATURE_MATRIX.csv`. They are not edited here.

## Changed tracked files

1. `EngineBridge/include/renegade/bridge/ScreenAuthoringSession.h` — expose `MoveWidgetBackward` and `MoveWidgetForward`, plus private unified layer movement.
2. `EngineBridge/src/ScreenAuthoringSession.cpp` — validated, one-Undo-step layer reordering. Moves only among same-parent siblings; absolute back/front use the same authority. Boundary moves are no-ops without history noise.
3. `Studio/src/RenegadeScreenEditorWorkspace.h` — declare two incremental callbacks and native button controls.
4. `Studio/src/RenegadeScreenEditorWorkspace.cpp` — labelled `TO BACK`, `BACK 1`, `FORWARD 1`, `TO FRONT` buttons in a bottom-canvas control row, with correct disabled boundary states and status responses. Removed BACK/FRONT from left creation row.
5. `Tests/ScreenAuthoringSessionTests.cpp` — regression for incremental moves, Undo/Redo, no-op history, unchanged focus order, and nested parent isolation.

No changes to rendering, Runtime, gameplay, StoryFlow semantics, existing user projects, or the separate assistant work. All five files were clean at task start. **Scope limit:** these controls reorder one selected sibling, not a multi-selection or an entire linked component subtree; group-as-unit semantics and drag-reordering remain for later UI-01 gates. Do not mark all of UI-029 complete.

## Local verification record

Tool: Visual Studio 18 Build Tools (`v145`) on DESIGN. The earlier VS2022 `cmake.exe` installed on that machine is zero-byte and the 2022 MSBuild cannot load `v145`; those first attempted commands are not valid build results.

- `cmake --build BUILD/renegade --config Release --target RenegadeScreenAuthoringSessionTests -- /m:2 /verbosity:minimal` **PASS**, when run with the explicit VS18 Build Tools `cmake.exe`, first after basic one-step tests and then again after nested-parent tests.
- `BUILD/renegade/Tests/Release/RenegadeScreenAuthoringSessionTests.exe` **PASS** after both builds, including existing editor-authoring fixture Save/Open regressions.
- `git diff --check` for the five changed files **PASS**.
- `cmake --build BUILD/renegade --config Release --target RenegadeStudio` **BLOCKED**: existing separately modified `Studio/src/TestLevelRuntimeProcess.cpp:339` has compiler error `C2137: empty character constant`. This task intentionally did not edit that file.
- Studio **compilation-only PASS** via VS18 Build Tools MSBuild on `BUILD/renegade/Studio/RenegadeStudio.vcxproj` using `/t:ClCompile /p:SelectedFiles=RenegadeScreenEditorWorkspace.cpp /p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64`; command exit code 0 after 298.19s. The target also compiled other Studio source files. Output explicitly included `RenegadeScreenEditorWorkspace.cpp`; its generated `.obj` was newer than the modified source. This does NOT establish a successfully linked Studio executable or interactive visual parity.
- Native Screen Editor visual inspection **NOT RUN**, as no successful new Studio executable has been generated from this slice.
- Packaged Runtime/owner test **NOT RUN**; layer-ordering commands only affect Screen authoring and need save/reopen and visual approval.

## Acceptance checkpoint and next approved step

1. Build the entire Release Studio after the independent TestLevelRuntimeProcess issue is resolved by its owning task.
2. Owner opens a governed Screen in Studio and confirms four legible controls directly below the central canvas, at both 1920x1080 and 1366x768.
3. Move and resize a widget; confirm Back 1, Forward 1, To Back and To Front actually change visible stacking and that same-parent ordering is predictable.
4. Save, close and reopen Screen; verify exact authored layer order. Run Undo/Redo and confirm focus order and button actions remain intact.
5. Only after visual acceptance proceed with the next small UI-01 slice (configurable grid and snapping) and the rest of the frozen UI-001–029 requirements.

**Governance:** Do not merge, commit, overwrite the shared `HANDOFF.md` or claim Studio/Runtime visual success on this local test alone. Exact-head independent verification remains required under `AGENTS.md`.
