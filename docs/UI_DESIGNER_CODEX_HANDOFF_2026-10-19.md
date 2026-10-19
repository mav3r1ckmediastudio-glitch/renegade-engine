# Renegade UI Designer — Codex takeover (19 October 2026, 13:00 UK)

## Owner's objective — non-negotiable
Deliver the **complete, intuitive, beginner-friendly Basic UI Designer**, not another collection of green builds. Continue implementing and validating the native Designer through a standalone owner-test release. The owner is frustrated by discovering fundamental UX gaps element by element. Audit **all 22 wired toolbox element entries** and the whole create → configure → preview → save → reopen → runtime journey systematically. Advanced controls remain accessible but Basic Mode must make ordinary tasks obvious. Do not stop after individual milestones, and do not claim completion on source checks alone.

## Exact machine and worktree
- Work **on Windows device DESIGN**, not Codex's default Linux checkout.
- Active isolated worktree: `C:\Users\paulw\source\repos\renegade-ui-designer-complete`
- Branch: `feature/ui-designer-complete-20261017`
- HEAD baseline verified: `60634cec26feb7ffc2d0f941d3381865fa3b41c7`
- At handoff **195 tracked/untracked working changes** are intentional, including extensive earlier UI Designer work. **DO NOT reset, clean, stash, checkout another tree, overwrite or discard any of them.** Do not touch the original shared `renegade-engine` checkout or alter owner's project.
- **Do not commit, push, merge or open a PR without owner approval.**
- `AGENTS.md` is present. First read it, `HANDOFF.md`, `docs/UI_DESIGNER_BASIC_UX_AUDIT_2026-10-19.md`, `docs/UI_DESIGNER_FINAL_VALIDATION_2026-10-19.md`, original `docs/UI_DESIGNER_CODEX_FULL_HANDOFF_2026-10-17.md` and all referenced specs/checkpoints. Follow repository constraints and 29 approved requirements; avoid inventing extra features.
- Windows MSVC/CMake executable: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`. CTest in same directory. Existing build: `BUILD\ui-complete`.

## Recently implemented, compiled, but **NOT visually accepted**
1. `Studio/src/ScreenEditorLibraryPalette.h`: wired **Progress Bar** promoted to BASIC toolbox from obscure collapsed DATA category.
2. `Studio/src/RenegadeScreenEditorWorkspace.{h,cpp}`: Basic Inspector image element uses conspicuous 43-pixel **ADD / CHANGE IMAGE** action higher in Inspector; irrelevant generic TEXT field hidden; image Name repositioned. Native-orange accent-outline button style in `RenegadeStudioChrome.{h,cpp}` is opt-in rather than changing all Studio buttons.
3. **Custom Progress Bar artwork**:
   - `EngineBridge/include/renegade/bridge/ScreenService.h`: optional `progressFrameResourcePath` and `progressFillResourcePath` fields.
   - `EngineBridge/src/ScreenService.cpp`: backward-compatible optional `progress_frame_resource` / `progress_fill_resource` persistence; governed project-relative path validation.
   - `ScreenRenderer/src/ScreenRenderer.cpp`: optional image resources loaded using existing resource resolver; textured fill is revealed by clipping a **full-size** artwork at 0–100%, not stretching the image; decorative frame overlay drawn afterward. Legacy solid-colour bars retained.
   - `EngineBridge/include/renegade/bridge/ScreenAuthoringSession.h`: `UpdateProgressArtwork` governed mutation with validation and Undo/Redo.
   - `Studio/src/RenegadeScreenEditorWorkspace.{h,cpp}`: distinct Basic **FRAME IMAGE // CHOOSE/CHANGE** and **FILL IMAGE // CHOOSE/CHANGE** controls.
   - `Studio/src/RenegadeScreenEditorRenderPath.h` & `RenegadeScreenEditorAdvancedInspector.h`: both controls reuse the existing project-image picker and `CreatorTextureWorkflowService` import/mirror mechanism, capture widget/screen and slot to guard asynchronous import targeting, call governed authoring mutation and document-change callback.
   - `Tests/ScreenProgressTests.cpp`: save/reopen roundtrip of frame/fill paths, undo/redo and invalid outside-project path rejection.
4. Keep visual native checks honest: compilation **does not certify** actual displayed art/clipping/picker interaction or UI fit. Buttons are positioned at `HeaderHeight+467` and `+514`, and source geometry fits 768px-height; still require real native interactions.

## Verified evidence
- Full `RenegadeStudio` Release build completed exit 0; log: `BUILD/ux-progress-artwork-slots-studio-build.log`; exe: `BUILD/ui-complete/Studio/Release/RenegadeStudio.exe` (modified 19 October 12:50 UK).
- `BUILD/ux-progress-artwork-slots-tests.log`: focused 24/24 PASS.
- **Full 231/231 tests PASS**, `BUILD/ux-progress-artwork-full-tests.log`, 80.86 sec.
- Earlier progressive builds/tests also recorded in Basic audit and `HANDOFF.md`.
- Current owner-test packaged Studio is **not** this latest executable. Do not replace it or close a user's running Studio without consent.
- The existing disposable QA project under `BUILD/ui-native-qa/UI Designer QA` was used for prior native testing; ensure it is safe/disposable and do not modify actual owner project. You may set up a new disposable QA root.

## HIGH PRIORITY unfinished work (work through, not just inventory)
**P0 — Native UX acceptance and StoryFlow**
- Owner created a Screen named `Options`; it did not appear, yet recreating it said the name already existed. Diagnose source/project screen indexing/visibility, initial and cold reopen, StoryFlow graph/palette representation, search and focus. Never delete/mutate owner's Options screen as part of reproduction.
- Newly created StoryFlow screen must be **centered in current viewport**, not placed far away; first click on prepopulated title should select/replace all text, not append `MAIN MENUOptions`; minimap double-click should restore StoryFlow framing. Source fixes may exist but **not owner accepted**.
- Run actual beginner workflow: add screen, find it, rename, open editor, return to StoryFlow, save/reopen, no vanishing nodes.
**P1 — Finish Basic Inspector UX matrix for all 22 wired element entries**
- For each creator/action, test creation discoverability, sensible default, relevant properties, direct feedback, asset slots as needed, alignment, Undo/Redo, autosave, cold reopen, and genuine runtime binding/action.
- Remove misleading irrelevant fields and redundancies. Prioritize shared components/policies; avoid 22 bespoke UI patches. Do not treat catalogue placeholders as working: 52 ELEMENTS catalogue entries, only 22 wired; the native palette intentionally hides unwired entries. Avoid filler placeholder buttons.
- Button action, text/fonts, image asset import, HUD/gameplay binding, Progress Bar frame+fill, input controls, layout container navigation, runtime preview/Simulate.
**P1 — Progress artwork native verification**
- Actual distinct frame/fill chooser buttons visible and clickable in Basic. Choose existing asset and import PC image, verify project-contained path and undo/autosave/reopen. Confirm fill image is clipped rather than compressed at 0/25/50/100 and all directions, frame never shrinks; test selected/project image fit across 1366x768 and different resolutions.
- Check picker overlay remains reachable when Advanced Inspector isn't open, and Render/Update/Popup dispatch correctly route choices. If renderer contrast/accessibility path or scissor modifies clipping, fix carefully.
- Actual runtime **loading progress** is NOT proven: Runtime `Fill` binding supports explicit gameplay attribute ratios, but no verified `loading.progress` producer; don't claim a true loading screen automatically tracks loading. Implement a measured loading lifecycle source with robust semantics (or document a real blocker), bind Loading Screen appropriately and test runtime.
**P2 — Visual native parity and release**
- Test immediate editor changes, Undo/Redo, autosave, cold reopen and native built/standalone Studio. Avoid separate ad hoc preview implementations when existing `SIMULATE` already works; inspect actual functionality and improve discoverability.
- Execute full Release Studio/Runtime builds and regression CTest, plus native manual-equivalent QA on a disposable project.
- Produce finished standalone owner-test candidate **only after** native interaction validation. Record actual remaining failures without claiming total completion early.

## Working process
1. Read all handoffs/specs/AGENTS, inspect current changes, protect uncommitted work, identify real owner problems and reproduction.
2. Use existing DESIGN Windows build and native Studio, perform a repeatable *end-to-end* QA scenario. Preserve existing project and running processes; use disposable QA.
3. Implement fixes iteratively but continue across priorities, with measured progress and exact logs/results; never halt after each tiny change or ask owner to test unfinished pieces.
4. Use existing governed save/Undo and asset import pipelines; do not duplicate them.
5. Preserve the 29 approved requirements, complete robust UI Designer validation, keep clear evidence in `docs/UI_DESIGNER_BASIC_UX_AUDIT_2026-10-19.md`, `HANDOFF.md`, and final validation doc. Be candid about blockers.
