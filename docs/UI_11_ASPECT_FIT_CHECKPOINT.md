# UI-11 Native UI canvas sizing checkpoint
Date: 17 October 2026.

The existing StoryFlow Screen lifecycle creates 1280x720 Screens (already 16:9). ScreenService defaults likewise. We intentionally retain those defaults and do not migrate or mutate saved Screens.

Changed Studio/src/RenegadeScreenEditorWorkspace.cpp PreviewBounds to aspect-fit authored design dimensions inside the available panel-delimited area, centered in both axes. Uses 16:9 fallback when no Screen is loaded and respects existing custom aspect ratios. All render viewport consumers, selection hit tests, grid and guides use this shared PreviewBounds.

Verification: VS18 BuildTools standalone Studio source ClCompile SelectedFiles=RenegadeScreenEditorWorkspace.cpp exit 0; log BUILD/ui11-preview-compile.log. Existing 30 screen/flow/resource/transaction Release CTests passed 30/30 in 10.73s. git diff --check scoped source passed.

No freshly linked Studio binary or owner visual acceptance yet; do not redirect Downloads launcher to this source-only build. Graph Editor's cursor-centred wheel zoom was located in RenegadeStoryFlowGraphEditor.cpp, using ImNodes panning. It is not a drop-in UI Designer renderer, which uses ScreenRenderer fixed document geometry. Camera helper and standalone passing MSVC test remain unintegrated, and must not be represented as working Studio zoom/pan.

Optional future: resolution preview selector. Priority remains functional native designer acceptance and safe navigation integration. Do not alter user's running night-sky preview or unrelated dirty changes.
