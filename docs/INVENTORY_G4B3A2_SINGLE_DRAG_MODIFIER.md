# Inventory G4-B3A2 — one-item drag by default, Shift+drag stack

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Status:** **OWNER ACCEPTED** after testing the G4-B3A2 preview on DESIGN (26 October 2026): "that all works". This acceptance covers the single-item drag default and Shift/full-stack transfer in the existing Inventory-Proof screen; original artwork, UI Designer integration, packaging and save games remain open.

## Owner feedback

Owner confirmed the G4-B3A drag **works** in real DX12 Runtime but pointed out that it moves all items in the stack rather than one at a time. That was the deliberately coded G4-B3A default, not a transaction failure. The new owner-requested control choice is:

- **Ordinary drag onto the other container:** transfer exactly **one** unit.
- **Shift held when beginning a drag:** transfer the **entire stack**.
- Existing click and E/Lua: **one** unit, unchanged.
- Drop outside or back into the same container: no transfer.

## Change

`InventoryDragTransfer::Begin` now takes a default-false `wholeStack` flag and captures the requested transfer count (1, unless Shift indicates entire stack) together with stack identity and authoritative revision. The modifier is sampled by `RuntimeApplication::OnInventoryScreenPointerDrag` at **mouse-down**, not on drop, so releasing Shift mid-gesture does not change the transfer amount. The controller's `Quantity()` allows the footer to truthfully say `MOVED ONE ITEM` or `MOVED STACK`. The operation still uses the existing scoped atomic gateway, rejects stale Lua mutations, and clears on drop/cancel. No new item system and no edit to the pinned Wicked submodule.

The ordinary editable Screen footer explains the mapping. Test-only G4-B3A2 Screen/marker `InventoryG4B3SingleDrag.*` in the disposable BUILD Inventory-Proof project are new files, leaving the previously owner-tested G4-B3A Screen and executable untouched.

## Regression evidence

`Tests/InventoryDragTransferTests.cpp` now asserts a first ordinary drag moves Player 3 / Chest 2 to **2 / 3**, a Shift drag moves the remaining Player stack **2 / 3 → 0 / 5**, an ordinary reverse drag moves Chest **5 → 4** and Player **0 → 1**, and a Shift drag thereafter moves the entire remaining Chest quantity. Also checks no-permission, same-container, empty-source, cancelled and duplicate events, and an intervening authoritative Lua transfer that invalidates the gesture. The modifier remains captured from the beginning of the gesture.

Full build, DX12 smoke and live owner interaction gate statuses are recorded in HANDOFF.md. Do not accept the live UI based on tests alone.

## Visual test

Run `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B3A2-Single-Drag.cmd`, enter Play, press I. Drag a potion Player → Chest **without Shift**; quantities should go from 3/2 to 2/3. **Hold Shift before mouse-down**, then drag Player → Chest; quantities should go from 2/3 to 0/5. Drag Chest → Player normally: expect 1/4. E should transfer one. Check no double transfers and accurate live labels.

Out of scope: original item icon artwork, animated drag ghost, grid redesign, split-stack quantity dialog, general UI Designer Inspector bindings, Save Game and packaged standalone.
