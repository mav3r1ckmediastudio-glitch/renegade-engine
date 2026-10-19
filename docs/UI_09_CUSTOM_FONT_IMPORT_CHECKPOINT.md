# UI-09 — Native custom font import checkpoint

Date: 16 October 2026 (Windows device local time). Local dirty shared branch feature/p3-original-impact-vfx (HEAD 60634ce). Do not merge/push, alter user project assets, or alter the accepted night-sky work.

## Purpose
Complete missing native Screen Editor font authoring path: choose imported project fonts, or use native Windows file selection to import a TrueType font, assign it to Text/Heading/Button and have autosave preserve the binding. The approved Concept 03 three-column UI remains visual direction.

## Implementation
- **New:** `EngineBridge/include/renegade/bridge/CreatorScreenFontImportService.h`. Native importer checks project ID/root, selected file, .ttf format, file existence and 32 MiB ceiling; retains a uniquely named font under `SourceAssets/Fonts`, imports governed LP08 `.rasset` and registry provenance under `Content/Fonts`, and journal-commits a raw Content mirror under `Content/UI/Fonts/<asset-id>.ttf`. It verifies that ScreenService can resolve the resulting font within the project Content tree. Partial import errors remain explicit; no absolute external source file path is serialized into a Screen.
- `EngineBridge/include/renegade/bridge/ScreenCreatorResourceService.h` font catalogue now enumerates only governed font assets with a real Screen/Runtime-readable `Content/UI/Fonts` mirror. This prevents the previous unsound `SourceAssets/Fonts` selection from failing in the Screen Renderer.
- `Studio/src/RenegadeScreenEditorAdvancedInspector.h` Font chooser includes **Import TrueType Font from This Computer**; selecting a font (builtin or project) now commits the style change through the existing ScreenAuthoringSession, which the existing autosave system persists. File dialog selection is checked again at the Wicked safe point: wrong project, changed Screen or deleted/changed selection cancels. `Style/Font` on Text/Heading or Button opens the Typography/Text page directly; other widgets open Style.
- `Tests/ScreenAuthoringSessionTests.cpp` adds isolated font-import identity, catalogue, runtime path, duplicate-name, invalid OTF format and Screen save/reopen assertions.

## Verification
- New UI-09 Release test target `RenegadeScreenAuthoringSessionTests`: **PASS**; isolated CTest **PASS**; log `BUILD/ui09-font-import-tests-build.log`.
- Thirty Screen, StoryFlow, resource asset and transaction Release CTest suites **PASS 30/30**; `BUILD/ui09-font-regression-30.log`.
- Final isolated Studio Release executable build running as this checkpoint is authored, `BUILD/ui09-studio-font-release-build-isolated.log`. Do not publish a launcher until the linked executable and actual file path are verified.

## Runtime/UX boundaries
- Only `.ttf` is currently supported by ScreenService and pinned Wicked resource format table. `.otf` is not offered as working, even though some fonts have an OTTO signature.
- This release does not add a central script to ordinary button actions. Optional per-widget Lua is a separate future engine feature.
- The build and tests do not yet constitute native GPU visual testing of the imported TrueType. A real font file should be checked for glyph rendering, layout/line height and reopen in a disposable project.
- UI-001..029 as a whole is still incomplete; owner-first integrated test should focus on real supported tool workflows.

## Verified final build (17 October 2026, UK local time)

- `RenegadeScreenAuthoringSessionTests` rebuilt Release target: PASS; focused CTest PASS. Includes guarded import, project font catalogue, duplicate source-name imports, OTF rejection and Screen document save/reopen.
- Full selected Screen/StoryFlow/ResourceAsset/ProjectDocumentTransaction CTest regression: **PASS 30/30** (10.72 seconds), `BUILD/ui09-font-regression-30.log`.
- Initial Studio build was intentionally stopped because `TargetName` was propagating to dependencies. The second build used `BuildProjectReferences=false`, compiled only Studio and linked **PASS**, exit 0, `BUILD/ui09-studio-font-release-build-isolated.log`.
- Exact new file: `BUILD/renegade/Studio/Release/RenegadeStudioUIDesignerFontTest.exe`, 28,874,240 bytes, modified 2026-10-17T00:28:48 +01:00. SHA-256 `C64883C8C76723157FFA36131F5BD37D40BB9D6DBE3CF3795D3165010A8FDBE2`.
- Smoke launch PASS: minimized UI-09 Studio process responded, had title `Renegade Studio - Phase 3 [DX12]`, and closed gracefully. This is **not** GPU screenshot/Inspector interaction verification.
- `git diff --check` PASS for edited tracked UI/Screen test files; source remains local and uncommitted.
- Downloads launcher `C:\Users\paulw\Downloads\Renegade_UI_Designer_TEST_LOCAL.cmd` now opens this exact UI-09 executable. It prevents running multiple known Studio variants simultaneously.
- No game/project assets were modified. The previous UI-08 test executable remains available for fallback.

## Owner real-font test

Use a disposable project. In StoryFlow, open a Screen, add Text, Heading or Button and select it. In the main Inspector click `STYLE / FONT` to open Typography. Click the Font field then `IMPORT TRUETYPE FONT FROM THIS COMPUTER`. Choose a real `.ttf` (e.g., a user-owned font; do not distribute restricted-license system fonts). Verify glyphs appear on canvas with expected height, spacing and colour. Wait for autosave; change Screens and reopen. Check persisted font still selected, that another Text element can choose it from the project font dropdown, and that Ctrl+Z/Redo behaves sensibly. `.otf` is intentionally unsupported for now.

The full UI/HUD framework UI-001 to UI-029 is not yet finished; this gate completes the missing custom-font asset-import slice and leaves UI scripting optional, not standard.
