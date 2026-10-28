# G4-B3C3 — Inventory Screen Layout Customisation (implementation contract)
Date 2026-10-27. Isolated worktree: renegade-inventory-core-20261026.
Owner accepted paged scrolling and then confirmed visible scrollbar works well. Preserve both, along with owner-accepted B3B3 click, E, repeated drag-one, Shift-drag-whole-stack. Do not merge to main.

## Current verified code gaps
* InventoryGridLayout in EngineBridge/include/renegade/bridge/InventoryGridViewport.h supports columns, visibleRows, slot dimensions, gaps, padding, iconPadding, squareSlots, showScrollbar, iconFit; its validation and viewport mapping are unit-tested.
* MakeInventoryPagedGridScreen in EngineBridge/src/InventoryPagedScreenAdapter.cpp generates normal editable Screen v3 widget instances, but restricts columns <=6, visibleRows <=5, slot sizes <=82. Its placement assumes a fixed two-panel 1280x720 template: panel x=126+548*p, y=209, and hardcoded UP/DN, range and track geometry. These constraints must be reflected in Inspector validation; don't simply allow arbitrary 32x32 because it overlaps controls.
* RuntimeApplication::LoadInventoryAuthoredScreen around line 2174 sets columns=5 and visibleRows=4 regardless of authored document. This is the critical integration gap. A creator's saved grid must determine the Runtime viewport and per-visible-slot binding; do NOT infer from quantity of PNG widgets or let stale marker text set permissions.
* Current G4B3C2 scrollbar changes are uncommitted, plus the pre-existing generated WickedEngine git dirt. Preserve/check them first; don't reset, overwrite or stage Wicked. Separate previous accepted fixture and executable must remain intact.
* Current preview Screen is 132 widgets / 44 interactive actions (5x4x2 panels). General authoring must not depend on that count.

## Next implementation slice
1. Introduce an explicitly versioned creator-owned grid layout document/metadata associated with an ordinary Screen, with strict schema validation, safe project-relative paths and documented defaults. JSON or existing native project document mechanism, whichever the Screen Designer can persist. Layout data is UI-only and MUST NOT mutate authoritative inventory container capacity.
2. Add layout editing in the existing Screen Designer Inspector for a selected Inventory Grid: Columns, Visible Rows, Square Slots, Slot Width/Height, Horizontal/Vertical Gap, Icon Padding, Scrollbar enabled, with meaningful limits and immediate previews. Keep ordinary slot/button/image/count widgets author-editable; avoid a locked separate Inventory Designer. For full authorability eventually add track/thumb style, text and backgrounds using existing widget style controls.
3. Make rendering geometry derived from the saved layout and panel container bounds, not hardcoded 5x4. Respect screen boundaries, prevent overlapping panels, arrows, tracks, quantities, tooltip and range labels. Supply a clear layout validation message for an impossible arrangement. Maintain consistent hit-test geometry after scaling and scrollbar dragging.
4. Bind viewport dimensions from saved layout at Runtime load, verify the same generated widget action set and visible-cell count as the saved metadata, never silently truncate 100+ capacity. Keep each panel's independent scroll offset. On changed layout invalidate active gestures and refresh projected labels/icons/quantities.
5. Screens should remain standard saveable/reopenable Screen v3 documents; original art and individual slot style changes survive new layout authoring. When adjusting row/column count preserve existing stable widget IDs/overrides for cells in both the old and new range; create only added cells, retire removed cells intentionally (with undo).
6. Add ordinary editor Undo/Redo to layout changes and test persistence across close/reopen; avoid authoring Runtime fixture generators as the only way to change layout.

## Test/acceptance gate
* 5x4 legacy: 100 capacity, 20 visible, maxTopRow=16; B3C2 scrollbar, UP/DN, wheel intact.
* 4x3: 12 visible, 100 capacity, maxTopRow=22; wheel/track work, last row reaches slots 89..100 (subject to row alignment), icon/stable-ID transfer correct.
* 5x5: 25 visible, maxTopRow=15; range text and geometry correct without overlapping track/controls.
* Small chest <= visible capacity has stationary thumb and cannot page past zero; no phantom inventory slots.
* Save/reopen, changed per-slot artwork and styling, different panel positions, resolution scale, repeated drag + Shift drag and stale revision/permission denial.
* Build Windows x64 Release Runtime + relevant Studio targets, linked tests, launch isolated proof in DX12. Owner visually accepts before any merge.

This is a scope/implementation handoff, NOT proof that a creator Inspector is implemented. The owner’s acceptance of scrollbar is preserved.

## 2026-10-27 11:27 — Studio Inspector integration investigation

Actual Inspector entry point: `Studio/src/RenegadeScreenEditorAdvancedInspector.h` owns the LAYOUT tab and `RenegadeScreenLayoutInspector` child. `RenegadeScreenLayoutInspector.h` currently edits generic container padding, spacing, grid columns, safe area and responsive breakpoint. The selected-widget authoring API `ScreenAuthoringSession::UpdateWidget` edits just one widget; `TransformGeometry` mutates selected widget rects as one Undo gesture but is not a generator/remover of grid child widgets. Neither directly implements changing the number of authored slot+icon+quantity triplets. The correct editor integration must expose one atomic ScreenAuthoringSession operation to regenerate/reshape the entire paired grid, preserve matching existing widget IDs and style, validate both panels and scrolling controls, and make one Undo/Redo history entry. Only then enable an inventory-specific Inspector page. A single-slot width/height control masquerading as a layout editor would desynchronise the UI from Runtime hit testing and is intentionally not delivered.

Current proof evidence: 4x3 and 5x5 adapter generation/projection test passes. The owner-accepted G4B3C2 scrollbar Runtime is still unaffected. Inspector UI functionality NOT implemented yet. No Studio build or UI acceptance for this gate. Continue on isolated branch without merging.

## 2026-10-27 — Implementation and verification update

Implemented `ScreenAuthoringSession::ResizePagedInventoryGrid` in `EngineBridge/src/ScreenInventoryGridAuthoring.cpp`. A single validated Screen transaction regenerates the paired slot/icon/count widgets, retains stable IDs/style/resource for matching names and preserves unrelated authored widgets/actions. The existing native Undo/Redo and Save handle the mutation; no second inventory store is introduced. Inventory-specific Layout Inspector page is wired into the existing `RenegadeScreenLayoutInspector.h`, offering columns, visible rows, slot width/height, gaps, icon padding and scrollbar visibility. Runtime infers row/column geometry from saved slot widgets and reads scrollbar visibility from the authored track.

`RenegadeInventoryGridAuthoringTests`: PASS for 5x4 → 4x3 → 5x5 (62 px), Undo, Redo, save/reopen, stable widget identity and invalid oversized layout rejection. All five focused inventory test suites PASS. EngineBridge Release and full RenegadeStudio Release build PASS. Runtime rebuild / owner visual Inspector verification still pending at time of this log. Do not merge into main. Current template is a paired Screen proof, not arbitrary independent named-container layout; custom icon mappings and deep layout repositioning remain future work.

Final build gate: Windows x64 VS2022 Release `RenegadeRuntime.exe` and `RenegadeStudio.exe` both linked successfully, exit 0. Separate G4B3C3 Runtime copy and launch command exist under BUILD/inventory-full/Runtime/Release. Isolated Studio was started on DESIGN on 27 October 2026; its interactive Inspector workflow and owner visual acceptance remain unverified. No merge/push.
