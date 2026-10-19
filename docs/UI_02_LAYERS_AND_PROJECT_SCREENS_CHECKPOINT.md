# UI-02 Native Designer: Layers drag and live project Screen library

Date: 16 October 2026
Branch: feature/p3-original-impact-vfx; local HEAD observed 60634ce, remote ahead by 13 commits at start. Shared worktree contains unrelated changes. No checkout, reset, rebase, push, game asset edit or assistant modification.

## Why

Owner approved Concept 03 and requested the native interface be completed before hands-on acceptance. Its initial left panel displayed a static catalogue and flat Layers list, without drag order or real project Screens.

## Work implemented in source

- EngineBridge ScreenAuthoringSession::MoveWidgetAdjacentTo(source, target, placeAfter): all-or-nothing validated sibling reorder. Parent equality enforced; preserves unrelated parent ordering and button focus order, leaves the Screen document unchanged for no-op drops, enters existing undo history exactly once per gesture.
- Studio ScreenEditorWorkspace: captures Layers row press, distinguishes click from >6px drag, renders cyan before/after insertion guide for eligible same-parent siblings, and calls the new authority once on release. No reparenting by accidental drop, no per-frame document mutation.
- Studio Screens toolbox: displays actual authored Screen nodes provided by StoryFlow (name, current-screen indication), alongside the nonfunctional template catalogue marked as such. Search operates on real Screen names; clicking a different Screen reuses the existing stable-ID Screen Editor handoff. Blocks leaving an unsaved current Screen.
- Studio StoryFlowStudioIntegration: supplies screen list from loaded authoritative Flow nodes on semantic change/screen selection, not filesystem scans each frame. Callback queues the existing Screen activation path. No second graph or routing authority, and no game-level Scene/Screen mutation just by viewing the catalogue.
- Toolbox categories now expand/collapse independently per tab (compact defaults), with search revealing matching entries even in closed categories. The Basic Inspector help and action-picker rows have been repositioned to avoid overlapping the apply/visibility controls.
- Other UI-01 toolbox/Inspector controls remain as in the previous checkpoint.

## Verification

- Focused Release build: RenegadeScreenAuthoringSessionTests target exit 0.
- Rebuilt RenegadeScreenAuthoringSessionTests CTest: **PASS** (0.27 s), including new sibling drag, transaction count, Undo/Redo, cross-parent rejection and no-op cases.
- Scoped git diff --check: **PASS** after final changes.
- Final six Release Screen/Flow CTest suites: **PASS 6/6** (1.01s), including the rebuilt authoring regression target.
- Studio Release ClCompile: **PASS**, exit 0 (313.42 seconds), log BUILD/ui01-full-library-compile.log.
- Studio linked Release target: **PASS**, exit 0, log BUILD/ui02-studio-release-build.log. A short native launch responded; offscreen PrintWindow of the DX12 frame returned black. To avoid interrupting the user's foreground application, no GUI interaction was performed; only the Studio process launched by this task was closed cleanly.
- Subsequent **linked Studio Release build PASS** (exit 0, 315.34 seconds), log BUILD/ui02-category-collapse-build.log. The linked binary contains collapsible palette categories and Inspector spacing refinements; no owner visual acceptance is claimed.
- No visual/native mouse test, fresh linked Studio launch, Runtime or packaged game verification claimed yet.

## Remaining approved interface scope

- Collapsible searchable palette category groups, hierarchy presentation and keyboard focus/navigation; persistent panel widths/collapsed state, canvas fit/zoom/pan, aspect variants, editor animations/timeline.
- Inspector-first *destination* selection must create/update StoryFlow routes atomically with symbolic Screen actions. Current Basic Inspector chooses existing action IDs only; it does not create a route.
- Screen templates/preconfigured new-project flow scaffold, actual gameplay bindings/profiles/HUD contexts, additional widget kinds, Live Test and runtime composition, theme/reuse/accessibility cannot be represented as working just because entries exist in the toolbox.
- UX/performance and old-project compatibility regressions, owner visual acceptance, Debug/Release integrated builds and independent packaging remain open.

## Next engineering gate: UI-024 Inspector destination transaction

Implement one deliberate `On Click -> Open Screen -> Destination` command without creating a second routing authority. Use the real current FlowDocument Screen nodes for the destination picker, detect shared button action IDs and multiple conditional Flow routes rather than silently repointing them, prepare and validate both Screen and Flow candidate documents, and commit with all-or-nothing rollback (including disk/write failures and external edits). One user Undo/Redo must restore Screen action identity and Flow routes together, and the existing ImNodes Graph must refresh from that same Flow authority. Until that gate is fully tested, the Basic Inspector exposes only the current symbolic action, honestly labelled.
