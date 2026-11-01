# Inventory G4-B3C2 — visible interactive scrollbar

Date: 2026-10-27. Branch: feature/inventory-core-g1-20261026 (isolated DESIGN worktree).

## Owner feedback
Owner visually tested G4-B3C1B: mouse-wheel scrolling works very well. Add a visible scrollbar to each independently scrollable 5x4 inventory grid. Preserve the accepted mouse wheel, row UP/DN arrows, repeated normal drag (one), Shift+LMB drag (whole stack), click (one), E, icon/quantity/hover bindings, and original G4-B3B3 executable/fixture. Do not merge/push.

## UX / authored Screen contract
* One narrow vertical scrollbar alongside each grid, between existing UP and DN buttons. A dark track with contrasting grey accent thumb, no bright toy-like colours. It should be clearly visible against the dark UI without covering any of the 5x4 slots or image/count overlays.
* Thumb proportional to visibleRows / totalRows, minimum practical height, at top for topRow=0 and at bottom for topRow=maximumTopRow. For 100 slots, 5 columns, 4 visible rows: 20 rows, 16 possible topRow increments; 4/20 thumb fraction.
* A left-button press and drag on the thumb changes topRow, clamped to [0, maximumTopRow], mapped to integer rows; click elsewhere in track pages predictably toward the click. Use the SAME inventoryPagedTopRows_/paged page rebuild path used by arrows/wheel.
* Author track/thumb as ordinary, named, editable Screen v3 widgets; do NOT introduce a separate inventory renderer, second item store, or replace author-controlled Screen styling. The authored Screen should persist track/thumb positions/style, while thumb placement/size is transient Runtime projection.
* Keep independent track positions for Player and Chest. Hide or disable interaction when maximumTopRow==0. Wheel remains one row at a time over slots and preferably over the associated track. Keep UP/DN and range display.
* At pointer-down on scrollbar, suppress inventory item capture; thumb drag must never call Begin/CommitInventoryPagedTransfer. Reset stale inventory gesture on any page change. On release scrollbar drag must not initiate click/slot transfer; preserve repeat drags and Shift semantics unaffected.
* Verify screen resolution/UI scaling: hit testing must use the same authored-Screen-to-window transform as inventory slots; no guessed raw window pixels.

## Implementation evidence / critical files
* EngineBridge/src/InventoryPagedScreenAdapter.cpp authors 132 standard Screen widgets, binds projected cells/UP-DN/range; add scrollbar widgets and project state here.
* Runtime/src/RuntimeApplication.cpp: PollInventoryPointerInput around lines ~2050; RefreshPagedInventoryScreen around ~2227 currently updates state, images and display text, but DOES NOT update widget rectangles. Expose/update a validated transient rect/layout path via ScreenController/Presenter if needed (avoid modifying the serialized authored Screen).
* Runtime/src/RuntimeScreen.cpp and ScreenRenderer/src/ScreenRenderer.cpp provide widget runtime APIs and scaling/hit-tests. Existing InventorySlotAt resolves authored slot hitboxes; scrollbar should be tested separately before slot gestures.
* Tests/InventoryPagedScreenAdapterTests.cpp, Tests/InventoryGridPageTests.cpp and runtime pointer tests cover projection and transfers.
* Existing G4-B3C1B opt-in marker and launcher: BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B3C1B-Paged-Grid.cmd; make a separate candidate executable/project fixture or regenerate only disposable Inventory-Paged-Proof safely.

## Acceptance
1. Initially thumb top and SLOTS 1-20/100. Drag Player thumb to middle/bottom: range and icons update correctly, reaching slots 81-100 at bottom. Wheel/UP/DN reposition thumb. No scrolling Chest while using Player.
2. Drag back to top, click a filled Player slot, drag 1 repeatedly, Shift-drag full stack; no one-time drag regression, stale item mapping, or unintended transfer from scrollbar.
3. Chest empty/short inventories and resized inventory capacity: thumb correct / disabled; creator edits to scrollbar track/thumb theme and coordinates survive save/reopen.
4. Release build, adapter + viewport + pointer regressions, exact-project DX12 smoke, then open Runtime for owner's visual acceptance. Do not claim acceptance based solely on automated tests.

Status: **candidate implemented, Release built, 18/18 linked regressions pass, manual DX12 interaction tested**. Owner acceptance still pending; standalone Story Flow smoke in current environment gives code 27 / flow_not_complete for BOTH this candidate and prior B3C1B baseline.

## Implementation as built
* EngineBridge paged Screen now contains 136 standard widgets (two independently themed track/thumb pairs); no new game-store or Screen type.
* Renderer uses runtime-only responsive thumb positioning and sizing, without modifying author-owned geometry. Pointer input uses actual renderer logical rects, clamps row, blocks stale inventory drags, and pages by visible row count on empty track click.
* Candidate binary: `BUILD/inventory-full/Runtime/Release/RenegadeRuntime-G4B3C2.exe`. Launcher: `Run-Inventory-G4B3C2-Scrollbar.cmd`. Separate candidate project: `Content/Inventory-Scrollbar-Proof` (does not overwrite accepted B3B3 or C1B projects).
* Regression: `ctest -C Release -R 'Inventory|ScreenRenderer|RuntimeScreen|PointerGesture|ScreenAdapter|GridViewport'` = **18/18 PASS**.
* Physical DX12 screenshot tests: 75 FPS, two scrollbars render; Player thumb to bottom shows slots 81-100 while Chest unchanged; thumb back to top shows slots 1-20; wheel advances row; repeated drag 165/18 -> 163/20; Shift full stack back 166/17. `BUILD/G4B3C2_*.png` evidence.
* Track click and track-wheel were implemented but remain **unverified manually** because the running window exited before testing them; no Renegade crash event was found in Windows Application log.
* Exact DX12 automated smoke: capability STARTED; terminal Story Flow `flow_not_complete` (exit 27); original C1B executable + original paged project reproduces the same issue.

Do not merge or push without owner approval. Next is owner visual acceptance; Inspector grid sizing and native creator styling controls are separate future scope.
