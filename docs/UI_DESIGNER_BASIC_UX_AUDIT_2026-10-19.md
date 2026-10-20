# Renegade UI Designer: beginner-first UX audit (working document)

Date: 2026-10-19. Scope: isolated UI Designer worktree only. No release acceptance implied.

## Principles approved by owner
- Basic is the primary novice workflow; Advanced extends it without duplicating engine systems.
- A primary action appears near the top of the context-sensitive Inspector, with a prominent semi-transparent native Renegade-orange treatment, bright outline and bold white label.
- For Image, label the existing action ADD / CHANGE IMAGE, not ADD NEW ELEMENT (which implies creating a second widget).
- Retain native Windows file picker and existing governed asset pipeline. No bespoke asset browser or drag-and-drop prerequisite.
- Creating a widget places it in the current visible canvas, selected, ready to move/resize.
- Text/label properties should be directly editable, refresh immediately, support Undo/Redo and autosave; avoid mandatory Apply for routine changes.
- Simulate is already implemented: inspect its present behaviour before considering any additional preview feature.
- Audit complete journeys from the perspective of a beginner, returning user, experienced user and frustrated user; prioritise discoverability and recovery.

## Source-confirmed findings (not yet live UI certified)
| ID | Priority | Finding | Evidence | Desired acceptance |
|---|---|---|---|---|
| UX-01 | P1 | Image chooser sits low in Inspector: HeaderHeight+423, after name/text/position/size and Apply. | Studio/src/RenegadeScreenEditorWorkspace.cpp:862-871 | Image primary action appears directly under header, enlarged, orange, clear ADD / CHANGE IMAGE label; invokes unchanged asset action. |
| UX-02 | P1 | Generic TEXT label is drawn for all non-progress widgets, including Image. Text field visibility also includes Image, merely disabled. | Same file:1998-2005,1001-1005 | Show text label/field only when relevant and editable; otherwise hide it and reclaim space. |
| UX-03 | P2 | APPLY ELEMENT remains visible, although many inputs already apply on InputAccepted; text applies on acceptance, not necessarily each keystroke. | Same file:679-716,990-995 | Retain transactional semantics but make routine edits immediate enough and undoable; only show Apply where truly necessary. |
| UX-04 | P1 | '+ IMAGE' creates placeholder and asset import is a separate control (already functional). | Same file:569-572,641-648 | Primary Basic Inspector chooser obvious after selection; do not rebuild importing. |
| UX-05 | P1 | First-click title replacement, viewport node placement and minimap camera changes exist, but interactive owner validation is outstanding. | Studio/src/StoryFlowTitleDraft.h; StoryFlowOverviewPolicy.h; RenegadeStoryFlowGraphEditor.cpp | Reproduce original failures and verify after cold reopen. |
| UX-06 | P0 | Owner reports 'Options' screen exists by name but is not visible/discoverable; duplicate name blocks recreation. | Owner observation, 2026-10-19 | Screen discoverable in project list/StoryFlow, with clear adopt/open action and no loss. Root cause not yet established. |

## Audit stages
1. Complete pending StoryFlow blocker fixes and native tests on safe project copies.
2. Walk Basic Screen Editor end-to-end: new Screen, image, text, button action, HUD, existing asset, save/reopen, Simulate and return.
3. Inspect Inspector states per widget and identify irrelevant/disabled controls, vertical ordering, primary actions and confusing feedback.
4. Inspect navigation, layer selection, keyboard shortcuts, minimap and recovery paths.
5. Prioritise small surgical interface changes over new subsystems; test at representative resolution and verify Undo/Redo/autosave.

## Evidence status
- Existing current Studio Release rebuild succeeded (exit 0) in BUILD/ui-complete during this continuation.
- 20/20 existing RenegadeStoryFlow tests passed; those tests do not demonstrate owner-level usability.
- This document records an initial source-based audit, **not** a completed comprehensive visual audit.
- All substantial source changes remain uncommitted; do not reset, clean, push or modify shared original checkout.

## Additional source audit: Basic Inspector and Simulate
- Basic Inspector visible widget loop shows name, numeric geometry, visibility/enabled and Apply for every selected widget (Workspace.cpp:981-995); text is hidden only for Progress and disabled for kinds not supporting text (1001-1005). Fix must avoid empty, irrelevant slots and misleading generic labels.
- Existing OnInputAccepted hooks apply element name, text and numeric geometry (Workspace.cpp:679-716). They do NOT by themselves prove keystroke-by-keystroke live updates. Preserve transaction/Undo semantics; measure actual interaction before changing dispatch.
- Renegade's native orange is wi::Color(222,91,29,255) in Workspace.cpp:33. Use this brand family rather than cyan for primary action, with sufficiently contrasting bold text.
- Existing imageAssetButton_ invokes imageAssetRequested_ (Workspace.cpp:641-648). Move/resize/relabel that exact action, not a duplicate importer; verify project-existing and local-PC paths both still work.
- SIMULATE is implemented in Advanced Inspector (RenegadeScreenEditorAdvancedInspector.h:149-152,620-647) with isolated samples/live data. A game-like fullscreen interactive visual Preview is NOT confirmed by that fact. Inspect running UI before concluding another feature is required.
- Proposed source-safe rollout: (1) remove irrelevant text field and label for Image, (2) put existing primary image action high with reflowed Inspector properties and orange styling, (3) test accepted input, autosave and Undo/Redo, (4) test Simulate as actual end-user journey.

## Implementation slice underway (19 Oct)
- Reclassified Progress as **BASIC / Progress Bar**; removed duplicate DATA/Progress entry. Existing action/renderer remains unchanged.
- Relabelled existing Image asset action **ADD / CHANGE IMAGE**, positioned near top of Inspector with 43 px height. Name field moved lower for Image selection.
- Changed Basic text visibility and label to avoid displaying disabled TEXT on non-text elements. Existing edit handlers unchanged.
- Release RenegadeStudio build started against BUILD/ui-complete; must inspect final exit, run relevant tests and inspect native layout before calling this accepted.
- Not yet implemented: orange button visual state, unified per-widget layout, dual progress artwork textures/clipping, custom loading progress binding, full journey UX validation. These remain pending, not completed.

## 19 October implementation and verification (first slice)
- Progress renamed **Progress Bar** and promoted to BASIC; duplicate DATA creator entry removed.
- Existing **ADD / CHANGE IMAGE** chooser moved near the top (43 px), with its existing import/project-image callback retained.
- Created an opt-in shared RenegadeButton accent-outline style: native orange #DE5B1D with alpha 84 idle / 146 engaged, brighter orange outline, white text; enabled for Image chooser only. No other button styling changed.
- Image selection moves Name beneath the primary action. Basic Inspector no longer shows a disabled generic text input/label for Image and other non-text widgets. Text/interactive fields are still editable, with 'DISPLAYED TEXT' label.
- Final Release Studio build: BUILD/ux-basic-inspector-orange-build.log, exit code 0. Prior layout build: BUILD/ux-basic-inspector-build.log, exit code 0.
- Targeted CTest: BUILD/ux-basic-regression-tests.log, 24/24 passed in 13.44 seconds (StoryFlow, Screen Authoring, Canvas Camera, Progress, Authoring Completion). Source changes are not yet native visually certified.
- Toolbox inventory from ScreenEditorLibraryPalette.h: 52 named element catalogue entries, 22 wired creator actions, 30 unimplemented catalogue placeholders. Runtime palette deliberately hides None-action placeholders; do not misrepresent these as working elements. Category wiring: BASIC 10/10, LAYOUT 4/7, MEDIA 0/4, DATA 1/5, INPUT 4/8, NAVIGATION 0/4, GAMEPLAY 3/7, FEEDBACK 0/5, REUSABLE 0/2.
- Follow-up P0/P1: native layout check at standard viewport, undo/autosave/cold-reopen, fixed StoryFlow screen discovery, full per-widget contextual Inspector matrix, progress bar separate frame/fill assets and clipping, loading binding and UX, Basic preview of existing simulate functionality.

## Continuation validation — source layout cleanup
- Removed stale low-position image-import label at HeaderHeight+403 after moving the orange chooser to the Inspector top. This avoids misleading duplicate presentation below ordinary property controls. The chooser remains at HeaderHeight+69 and Image Name at HeaderHeight+141.
- Verified current build `BUILD/ux-basic-followup-build.log`: RenegadeStudio Release exit 0 (14.04 seconds). Targeted regressions `BUILD/ux-basic-followup-tests.log`: 24/24 PASS, 13.15 seconds.
- Running owner-test Studio is a separate packaged executable and was not replaced or closed. The changed source is in BUILD/ui-complete/Studio/Release/RenegadeStudio.exe, not yet packaged for owner testing.
- Outstanding: widget-by-widget contextual layout, native visual inspection, actual field editing and save/reopen, StoryFlow P0 invisible-screen issue, separate frame/fill image implementation. No commit or push.

## 2026-10-19 continuation: Progress custom artwork foundation
- Added optional `progressFrameResourcePath` and `progressFillResourcePath` to `ScreenWidget` in `EngineBridge/include/renegade/bridge/ScreenService.h`. The existing colour-based bars remain the default with empty paths.
- `EngineBridge/src/ScreenService.cpp` writes and reads optional `progress_frame_resource` / `progress_fill_resource` keys; validation rejects non-project-relative paths, retaining backward compatibility with existing v3 progress documents.
- `ScreenRenderer/src/ScreenRenderer.cpp` loads those resources through the existing governed image loader, draws a full-size fill texture clipped using computed progress geometry (avoids stretching the fill artwork), and overlays the frame image; existing solid-colour path retained.
- `Tests/ScreenProgressTests.cpp` explicitly tests save/edit/reload of both asset fields. Target `RenegadeScreenProgressTests` Release build PASS exit0 (`BUILD/ux-progress-artwork-contract-build.log`, `BUILD/ux-progress-artwork-roundtrip-build.log`, `BUILD/ux-progress-artwork-render-build.log`); CTest 1/1 PASS with round-trip and renderer source compiled.
- NOT yet user-available: two named Inspector image slots with existing picker/import, governed undo transaction for setting art paths, native visual runtime certification, complete Studio/Runtime rebuild, actual loading-progress source binding. Do not claim finished. Preserve all other uncommitted work.

## 2026-10-19 continuation: Progress artwork Basic Inspector slots wired
- Added governed `ScreenAuthoringSession::UpdateProgressArtwork(widgetId, framePath, fillPath, error)` as one validated Undo/Redo transaction (also works with linked components' override bit). `Tests/ScreenProgressTests.cpp` now checks incremented Undo, Undo/Redo paths, rejects invalid outside-project path without revision mutation, and roundtrip save/reopen.
- In `Studio/src/RenegadeScreenEditorAdvancedInspector.h`, `OpenProgressArtworkForSelection(bool frame)` reuses `ImageChoices(true)` for existing project resources or + IMPORT IMAGE FROM THIS COMPUTER. Computer import reuses `CreatorTextureWorkflowService`, safe-point guards, and governed Content/UI/Images packaging; target slot is captured to avoid assignment to the wrong field.
- `Studio/src/RenegadeScreenEditorWorkspace.{h,cpp}` adds two Basic Progress buttons `FRAME IMAGE // CHOOSE/CHANGE` and `FILL IMAGE // CHOOSE/CHANGE` (accent orange). Registered in Create/Update/Render loops, visible on Progress only; `Studio/src/RenegadeScreenEditorRenderPath.h` routes requests to Advanced Inspector picker without forcing Advanced Mode.
- Release full RenegadeStudio build `BUILD/ux-progress-artwork-slots-studio-build.log`: exit 0 after 412.95 seconds. Focused `BUILD/ux-progress-artwork-slots-tests.log` 24/24 pass, 13.40 seconds. `git diff --check` on changed source returned no errors.
- IMPORTANT: source builds and automated tests do NOT prove native interaction or image clipping visuals. Two-slot import, actual displayed frame/fill alignment, fill direction, reopen, 1366x768 Inspector fit and real loading-progress gameplay binding still require native owner-like QA. Existing owner project was not opened or modified; no commit/push/merge.

## 2026-10-19 loading-progress binding investigation
- `Runtime/src/RuntimeScreen.cpp` supports explicit progress `Fill` bindings through `ScreenBindingSnapshot`/`ScreenBindingContext` and the shared screen renderer. Existing tests exercise health-current / health-maximum ratio binding.
- Search through Runtime/EngineBridge/Studio found no verified `loading.progress` gameplay attribute producer, percentage signal or ready-made Loading Screen progress binding. The UI must NOT silently promise genuine game-load progress just because Progress Bar renders a fill value.
- Implement a loading lifecycle/progress authority separately, including measured stage/asset progress, safe transitions/unknown duration, Runtime binding production, and loading-screen template defaults. Then validate from actual runtime load events. This remains OPEN.
\n## 2026-10-19 full regression verification\n- Executed full 231-test Release CTest suite after Frame/Fill Inspector integration: `BUILD/ux-progress-artwork-full-tests.log`, **231/231 PASS**, 0 failed, 80.86 seconds. This covers previously targeted UI tests plus broader Engine, Runtime, Screen and StoryFlow coverage.\n- Source geometry check: `HeaderHeight=64`, `FooterHeight=44`; Progress frame button y=HeaderHeight+467, fill y=HeaderHeight+514; both inside a 768px viewport. This is **not** native pixel/input verification; no owner acceptance claim.\n- Native GUI screenshot and click-through not yet conducted. Keep existing owner-test Studio and projects untouched; disposable native QA needed, especially image chooser, import, frame stacking and four fill directions.\n- Loading Screen fill binding still lacks a verified real loading-progress producer; no default loading binding claim.\n

## 19 October continuation: native artwork import crash and desktop blocker

Current owner-test readiness remains OPEN; earlier final-candidate claims below are historical.

- Worked only in the isolated UI worktree at baseline 60634cec26feb7ffc2d0f941d3381865fa3b41c7. Preserved prior changes; no commit/push/merge or writes to original checkout/owner project.
- Disposable fixture: `BUILD/ux-acceptance-20261019/Project`, copied from the prior disposable OwnerProjectCopy; private standalone Studio support copied under the same evidence directory. Preferences/AppData isolated.
- Fresh native run at 1366x768 outer window: Options is visible in Journey and opens through OPEN EDITOR. Progress Bar is discoverable in BASIC, creates selected in visible canvas, autosaves, and survives cold reopen. Both FRAME IMAGE and FILL IMAGE buttons fit; FRAME IMAGE opens the existing chooser without enabling Advanced. FILL IMAGE chooser interaction remains pending. Evidence: `storyflow.png`, `options-selected.png`, `options-editor.png`, `progress-created.png`, `frame-picker.png`, `debug-picker.png` in that directory. This does not certify missing-screen recovery from scratch, all 22 controls, true loading progress, or final artwork rendering.
- Native FRAME computer import reproducibly crashed with access violation c0000005, RVA 0x555dea. Windows Application Error record 19 October 13:10:07 names the private QA executable. Debugger stack + map identifies SetImportedProductRecords -> ResourceAssetService::ImportResourceAsset -> CreatorTextureWorkflowService::ImportTexture -> Inspector safe-point callback. Root cause: validation permits retained provenance referencing missing-asset tombstones, but mutation dereferenced end iterators for those historical records.
- Fixed `EngineBridge/src/AssetRegistryService.cpp`: preserve unchanged historical provenance, validate current source/product hashes for new or changed records, explicitly reject missing active records before dereference. No history deletion or fabricated recovery. `Tests/AssetRegistryTests.cpp` now tests adding a fresh import with both tombstoned historical products and changed historical sources, and rejection/no mutation for newly changed tombstoned provenance. Backups stored with evidence; scoped diff check passes.
- Studio and Runtime Release rebuilds pass (`import-fix-build.log`, `runtime-fix-build.log`, exit files 0). Rebuilt AssetRegistry regression passes; focused registry/resource tests 7/7 pass (`import-fix-tests.log`). A complete rebuild/regression is recorded separately after completion; do not confuse earlier executables with rebuilt ones.
- Native fix recheck blocked: repeated new QA processes receive normal title-bar close input during startup and exit 0. `startup-exit-debug.log` breakpoint on user32!PostQuitMessage captures WM_NCLBUTTONDOWN -> OnDwpNcLButtonDown -> system-command handling -> DestroyWindow -> PostQuitMessage, not an application startup AV. Do not blame decoder/renderer or patch startup based on this. No more QA windows launched after this diagnosis; uninterrupted desktop access is needed to continue.
- A temporary rename of the reveal movie in the PRIVATE QA package was restored. Owner package was never replaced. No validated new owner-test candidate produced.
- Next: rebuild-completion result, then uninterrupted private Studio run; reimport QA frame/fill images and verify import no longer crashes, project-contained persistence/Undo/cold reopen, real rendered 0/25/50/100 clips in four directions, runtime parity; finish the 22-entry contextual Basic Inspector matrix and outstanding StoryFlow/create/rename/recovery/loading lifecycle tests before packaging.


### Final verification for registry import crash repair
- Complete Release build: `cmake --build BUILD/ui-complete --config Release -- /m:1`, exit 0; `BUILD/ux-acceptance-20261019/all-build.log` and exit file. Studio and Runtime both rebuilt.
- Full regression: `ctest --test-dir BUILD/ui-complete -C Release --output-on-failure`, **230/231 PASS**, exit 8, 212.26 seconds. The only failure is `RenegadeProjectileWorldTests`, which hung during CRT exit joining a worker thread and was deliberately terminated after stack capture so remaining tests could run. This is NOT a nominal assertion pass or a fully green suite. Native debugger sample: `BUILD/ux-acceptance-20261019/stalled-test.log`.
- All other rebuilt regressions, including registry/resource/Screen/StoryFlow suites, passed. New registry regression covers the missing historical provenance crash. The post-fix native import remains UNVERIFIED because QA windows receive title-bar close input during startup; uninterrupted desktop access is the immediate validation blocker.
- No new owner-test release packaged. Outstanding full Basic Inspector audit, artwork clipping/runtime/loading lifecycle, StoryFlow recovery/rename placement acceptance and clean native recheck remain open. Do not deliver the private QA package as a completed candidate.
