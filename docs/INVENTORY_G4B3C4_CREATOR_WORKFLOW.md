# G4-B3C4 — Creator-friendly Inventory authoring
27 October 2026. Isolated inventory worktree and feature branch. No merge or push.
Base commit: b2da8466c0726ca94becb30445afd0149afce792. Candidate changes are uncommitted.

## Creator workflow
1. Open the Inventory Screen from its real Story Flow node. The normal Inspector opens Inventory.
2. Set Columns, Visible rows and Slot size, then Apply layout. Both panels update together; Undo restores the whole change.
3. Expand Appearance & Spacing for gaps, icon padding/fitting and scrollbar visibility. Edit native slot/icon/background artwork, then explicitly copy the selected style to matching elements if wanted. Save normally.

The dimensions describe visible slots, not carrying capacity. Backpack and Chest share the authored layout; scrolling offsets remain independent.

## Ownership and design
The Inventory card is a focused authoring facade over the existing native Screen widgets and ScreenAuthoringSession. It does not replace the Screen Designer, InventoryService, permissions, item stacks, inventory persistence or Screen history. No additional inventory data document is introduced. The paired template retains its geometric limits: up to 40 visible cells per panel within the native 256-widget budget, at most 128 interactive actions, slots 42–82 pixels and a 452 by 337 pixel grid area. A 5 by 5 layout fits at 62 pixel slots with six pixel gaps; 72 pixel slots are rejected without changing the document.

An explicit Inventory starter selected through the existing Story Flow Screen creation workflow produces the real editable grid rather than a placeholder. Automatic optional scaffold pages remain disabled placeholders. The current gameplay integration still requires the existing opt-in Inventory module; this gate does not claim new production module configuration, save-game or standalone packaging acceptance.

The normal Inventory card remains available for the whole inventory Screen, including when its background or an individual slot is selected. Edit selected element exposes the original native element Inspector. Inventory layout returns to the component controls. Generic Advanced layout keeps generic layout controls; inventory layout has one visible owner.

Apply uses the existing atomic Screen mutation. Retained widgets preserve IDs, native styling, artwork, theme/component links and interactions; unrelated widgets retain authored content and geometry. Added cells inherit their panel's first matching element appearance. Native document validation rejects orphan references or impossible layouts before commit. Explicit style propagation is one undoable transaction, never an automatic overwrite of individually authored art.

## Story Flow and Runtime
Strict parsing recognises only supported inventory slot and up/down action keys. Both legacy local_only=false and newer local_only=true inventory buttons are local gameplay actions, not navigation outcomes. A malformed lookalike or unrelated unconnected navigation button remains an error.

Only the separate inventory Runtime presenter opts into inventory-local dispatch. Regular presenters suppress those actions; unrelated local-only buttons still suppress Flow dispatch. The existing authorised inventory gateway performs actual transfers and scrolling. Pointer gesture handling, Shift capture, revision checks and independent viewport offsets remain in the accepted path.

In the opt-in preview, a unique inventory Screen referenced by the real startup Story Flow is resolved by stable document identity before considering the old preview template. The runtime does not copy edited widgets to a second Screen. Multiple candidate inventory Screens fail clearly rather than choosing arbitrarily. Project-containment and preview opt-in boundaries remain.

Native Screen v3 adds optional image_fit (Stretch/Contain/Cover). Old files default to their previous Stretch behaviour. The shared ScreenRenderer applies aspect-preserving geometry for Contain/Cover, including Runtime. Custom icon resources survive inventory binding; empty/default icons continue using the existing item-definition fallback.

Routing summaries show counts and one example while complete missing/invalid action lists remain in structured audit results and Output. Inspector message lines are bounded, clipped and constrained to their reserved lane; long messages cannot move validation over authoring controls.

## Verification record
Windows x64 Release Studio and Runtime LINK PASS. Both RENEGADE_EMBED_SHADERS and WICKED_EMBED_SHADERS are ON. The initial candidate aliases were BUILD/inventory-full/Studio/Release/RenegadeStudio-G4B3C4.exe and BUILD/inventory-full/Runtime/Release/RenegadeRuntime-G4B3C4.exe; current G4B3C4b aliases are listed below. Launchers with matching G4B3C4 names are beside them.

Automated verification: **10/10 PASS**, 2.37 seconds, BUILD/g4b3c4-verified-tests.log:
- InventoryGridAuthoring, InventoryPagedScreenAdapter, InventoryGridPage, InventoryGridViewport, InventoryPointerGesture.
- RuntimeScreen, ScreenRenderer (real native renderer instance with dynamic disabled-to-enabled state and source preservation), RuntimeScreenBinding.
- StoryFlowGate8EOutcomeParity (legacy/new local flags and genuine unconnected navigation).
- StoryFlowJourneyUiLayout (wrapped messages stay below authoring controls).

Exact command shapes, from the worktree root:
- cmake -S . -B BUILD/inventory-full -DRENEGADE_EMBED_SHADERS=ON
- cmake --build BUILD/inventory-full --config Release --target RenegadeStudio RenegadeRuntime --parallel 4
- Focused targets above were built in Release, then ctest --test-dir BUILD/inventory-full -C Release -R 'Renegade(Inventory(GridAuthoring|PagedScreenAdapter|GridPage|GridViewport|PointerGesture)|RuntimeScreen|ScreenRenderer|RuntimeScreenBinding|StoryFlowGate8EOutcomeParity|StoryFlowJourneyUiLayout)Tests' --output-on-failure

Logs: g4b3c4-configure.log, g4b3c4-final-release-build.log, g4b3c4-final-label-build.log, g4b3c4-paging-build.log, g4b3c4-renderer-final-build.log, g4b3c4-verified-tests.log, all under BUILD. Review rebuilds used process-local CL=/MP4. Existing MSVC nodiscard warnings remain; no link failures in the final candidate.

Actual interactive checks on DESIGN:
- Opened the real Inventory Grid Designer Test project from Project Hub, double-clicked its actual Story Flow Screen node and inspected the new normal Inventory card. No Advanced navigation required.
- 5 x 4 / 72 px loaded. Applied 4 x 3 to both panels. One Undo restored 5 x 4; Redo restored 4 x 3.
- Attempted 5 x 5 / 72 px: friendly rejection, original 4 x 3 remained intact. Applied 5 x 5 / 62 px successfully.
- Saved, returned to Story Flow, reopened the same Screen: 5 x 5 / 62 px persisted. Stable document ID dd27ed6b-0209-4bf4-ae35-5594162e9e86 retained.
- Opened final Studio candidate and directly typed replacement numeric values (without manual selection/backspacing): rows 4 and slot size 72. Applied and saved the familiar 5 x 4. Preview range captions now correctly say 20 VISIBLE SLOTS.
- Native runtime loaded that same Story Flow Screen as 5 x 5 before the final reset: 25 visible slots, each container still 100 capacity. Actual 1264 x 681 client-space hit tests exercised.
- Repeated ordinary drags moved one item each (165/18 -> 163/20); reverse Shift-drag moved a three-item stack (166/17). A controlled Player click returned 165/18; E also changed counts by one. Captured quantities/icons and status labels, not only service logs.
- Thumb drag reached Player SLOTS 76-100 / 100 while Chest stayed 1-25 / 100. Track click returned to 51-75; wheel advanced one row. Item totals were unchanged by scrolling.
- The live pass exposed an initially-disabled UP button losing its click state on every layout. Fixed ScreenRenderer::SetWidgetState to update the renderer instance's own transient layout document, and made input eligibility reflect native enabled state. Retested the rebuilt runtime: DN 1-25 -> 6-30; UP -> 1-25. The native renderer regression now covers this lifecycle.
- Studio logs Embedded shaders found: 800 and Wicked Engine Initialized. Final Studio and runtime reported Responding=True. Observed runtime frame rate was about 75 FPS; this is a smoke observation, not a performance benchmark.
- Story Flow no longer reports 44/54 spurious destination errors. The proof's genuinely unreachable optional Screen warning remains; it was not hidden or rewired.

Evidence images: BUILD/g4b3c4-probe-inspector.png, -4x3.png, -undo.png, -redo.png, -invalid5x5.png, -5x5.png, -reopened.png; BUILD/g4b3c4-runtime-drag2.png, -shift.png, -click-confirm.png, -thumb.png, -track.png, -wheel.png; BUILD/g4b3c4-fixed-down.png, -fixed-up.png; BUILD/g4b3c4-final-saved.png. The probe screenshots precede final numeric editing/message/renderer fixes; the fixed/final screenshots identify the rebuilt candidate.

Initial failures are retained in logs: the new audit fixture initially omitted its new button's focus entry; the native renderer fixture initially had an empty caption. These fixture defects were corrected. More significantly, the first live UP-button check failed and drove the production transient-layout repair; passing earlier tests did not close that defect.
Owner visual and usability acceptance is OPEN. Do not merge on automated results alone.

## Local evidence and rollback
Build/test logs and before-edit source snapshots are under BUILD/g4b3c4-*.
The original accepted preview executables and launchers retain their separate filenames.
BUILD/g4b3c4-before contains the initial contents of files changed in the first patch, including the pre-existing dirty edits. Do not reset or copy this directory over later work indiscriminately; review a scoped diff for rollback. WickedEngine remains separately dirty and is not part of this change.

## G4-B3C4b owner follow-up — 2026-10-27
Current owner build: BUILD/inventory-full/Studio/Release/RenegadeStudio-G4B3C4b.exe and matching Runtime alias. New Run-Inventory-G4B3C4b launchers sit beside each. Earlier builds/launchers are preserved.

Errors now appear immediately below Apply Layout in red (255,145,125), at 14 px with extra weight and wrapping, in a reserved clipped 66 px region. The explicit Cannot apply prefix identifies failure without relying on colour. Applied summary describes the actual document separately from pending numeric values. Feedback cannot overflow over Appearance controls.

The old 64-action limit was a whole-Screen guard: two 36-slot panels plus four scroll actions require 76. ScreenDocument::MaximumActions is now 128, shared by parsing, validation, authoring and duplication. Existing schema and legacy files remain supported. The native 256-widget limit still caps this paired grid at 40 visible cells per panel. Layouts must also fit the existing 452 x 337 area with slots 42-82 px. No capacity/data-model changes.

When an otherwise valid layout needs smaller slots, a contextual FIT SLOTS & APPLY button computes a fit and applies one Undo transaction. 6 x 6 with 72 px slots reports the size error; fitting yields 51 px at the default 6 px gaps. It never silently changes requested row or column counts.

Automated: Studio and Runtime Release LINK PASS with both embedded-shader options ON (BUILD/g4b3c4b-build.log). 11/11 focused tests PASS in 7.23 s (BUILD/g4b3c4b-tests.log). Added 6 x 6 fit/rejection/76-action save-reopen/Undo and >40-cell atomic rejection coverage; updated authoring limit regression to 128. Legacy inventory tests remain passing.

Interactive on DESIGN: inspected red wrapped error in the normal Inspector (BUILD/g4b3c4b-error-red.png); clicked Fit and visually confirmed 6 x 6 / 51 px in both panels (-fit6x6.png); Undo returned 4 x 4 / 72 and Redo restored 6 x 6; saved, returned to the actual Story Flow node, reopened and confirmed 6 x 6 persisted (-reopened.png). Runtime loaded the same saved 76-action Screen and showed 36 slots with capacity still 100. UP returned 7-42 to 1-36; ordinary drag moved one item, totals 165/18 to 164/19 (-runtime-drag.png). Earlier full interaction checks above remain applicable; this follow-up repeated loading/paging/drag at the new larger grid.

Workflow: (1) Select inventory in Screen Designer. (2) Set columns, visible rows and slot size; Apply, or use Fit Slots & Apply if offered. (3) Expand Appearance & Spacing for styling; save normally.

Owner visual/usability acceptance remains OPEN. No commit, push or merge. Source and binary hashes: BUILD/g4b3c4b-candidate-manifest.json.
