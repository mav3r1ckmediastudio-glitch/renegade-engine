# Comprehensive UI Designer owner test

Open RenegadeStudio/Run-RenegadeStudio-DX12.cmd, or RenegadeStudio/RenegadeStudio.exe. Use the included ExampleProject/UI Fresh Native.renegade or create a new project; new projects receive the editable starter UI, real Level, HUD and governed routes. The package has no QA identity/profile/recents. Keep its sibling Content, shaders, BuildInputs and Runtime folders intact. Windows x64 DX12 and the system VC++ 2015-2022 x64 runtime are required. The standalone game is StandaloneGame/UI Fresh Native.exe and needs no project argument.

The entire candidate is ready for comprehensive owner testing. Record observations against UI-001 through UI-029 in the requirement matrix; automated/native results are in Evidence. Work on a copy of projects when comparing destructive edit/history scenarios.

1. Create, save and cold-reopen a project. Exercise blank and starter templates, toolbox search/tabs, Layers, contextual Basic/Advanced Inspectors and optional disabled modules.
2. Ctrl-select elements; move/resize using aggregate handles and Inspector. Exercise group/order/clipboard/linked refresh/conflict/detach, Undo/Redo and autosave. Verify a layout-managed child explains its geometry authority.
3. Resize each side pane, collapse each independently, restore via its edge tab, fit/zoom/pan/minimap. Set grid/snap/spacing. Screen Settings saves project defaults or returns a Screen to inheritance; per-Screen overrides remain separate.
4. Author actions/conditions, animation/default and per-Button transitions, themes and overrides. Choose destinations from unique/shared Library contexts, including Level/terminal outcomes. Verify chronological Screen/Flow Undo and save/reopen. Standalone Screens require an explicit governed Flow context before route editing.
5. Exercise explicit gameplay attribute/profile bindings, inheritance and simulation. Live preview uses actual matching Runtime values and reports disconnect/foreign data; sample values remain separate.
6. Publish/adopt a portable UI package across projects; inspect fresh identities, editable copies, images/fonts/components and reopen without the source library.
7. Run Title/MainMenu/NewGame/Level/HUD. Escape opens Pause; settings and Back/Resume preserve the Level session. Check per-Level inherited/None/override HUD and event-context transitions, pause/input ownership and failure recovery.
8. Exercise Toggle/Slider/Selector/TextInput keyboard focus order and runtime input. Test text/UI scaling up to200%, contrast, focus, targets, reduced motion and restart persistence. Resize through1366x768,1920x1080 and1920x820. Physical-controller and alternate-monitor-DPI tests are still outstanding.
9. Globals UI warning budgets persist project CPU/widget/track thresholds and travel through Build Game. CPU percentiles/counts are advisory; GPU/memory/end-to-end latency are unmeasured.
10. Build Game, run the exported executable without project arguments, then cold-reopen it. Compare authoring and Runtime visuals/resources/routes. Capability pages must honestly identify unconfigured gameplay backends.

Independent release acceptance is separate from this owner test: AGENTS.md requires a different AI or human to verify the exact commit using docs/VERIFICATION_CHECKLIST.md. No commit/push/merge is authorized or performed; this candidate uses source-content and executable hashes.
