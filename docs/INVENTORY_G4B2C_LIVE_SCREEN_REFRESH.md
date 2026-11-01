# Inventory G4-B2C — Live Screen projection preserves item labels

**Date:** 2026-10-26
**Worktree/branch:** `renegade-inventory-core-20261026` / `feature/inventory-core-g1-20261026`
**Status:** G4-B2C **OWNER ACCEPTED** after live testing on DESIGN (owner: "its all working now"). E/Interact and clickable item transfers update the authored Player/chest slot quantities in Runtime, resolving the stale-text defect. No merge or push; broader drag-and-drop, artwork, UI Designer authoring and packaged-game gates remain open.

## Owner-reported defect

G4-B2B correctly handled the E/Lua transfer and received Screen slot mouse activation. Owner screenshot shows authored inventory panel still displaying **Player Health Potion x3 / Chest Health Potion x2** while the independent Lua debug counter says **Player 0 / Chest 5**. The panel also displayed `MOVED ONE ITEM`, proving a successful click feedback did not imply correct visual projection. The InventoryService was changing; displayed slots were stale, misleading, and could leave no-longer-populated slots apparently clickable. This is a G4-B2 visual acceptance blocker.

## Root cause (verified in source and regression)

`RuntimeApplication::RefreshInventoryOverlay` projects current authoritative InventoryService rows, then loops over all slot widgets and calls, for each:
1. `RuntimeScreenController::SetWidgetState`, which calls `BuildScreenBindingFrame` and `ApplyResolvedFrame` **for every widget**, resetting all displayed text from the authored initial Screen.
2. `RuntimeScreenController::SetWidgetDisplayText`, updating text for only that one slot.

Thus the next slot's state refresh overwrites labels already updated for earlier slots; the last empty-slot state update effectively restores the authored original values x3/x2. The Lua debug counter reads InventoryService directly and stays correct.

## Implementation

Added `displayTextOverrides_` to `RuntimeScreenController` as a **per-Screen, transient, UI-only** text override map. `SetWidgetDisplayText` continues rejecting wrong widget types, oversized strings and text fields already governed by explicit author bindings; accepted text is recorded in this instance-scoped map and immediately displayed. `ApplyResolvedFrame` re-applies only those accepted UI text overrides *after* resolving authoritative bound state, so a later `SetWidgetState` on any other slot cannot restore old item labels. The map is cleared in `Initialize`, preventing a previous Scene's labels from leaking into another Screen. No Screen file is rewritten and the gameplay/Script/InventoryService state is untouched. Existing authored style, layout, input route and user-created artwork remain intact.

This resolves the sequential Screen-update issue generically for other read-only runtime presentation text, rather than adding an inventory-specific item store or duplicating data.

## Targeted regression

Extended the production-linked `RenegadeRuntimeScreenTests` to reproduce the *actual* runtime loop: a two-container regular authorable Screen is projected, and every slot gets `SetWidgetState` then `SetWidgetDisplayText`. The test verifies:
- Real scoped click transfers Player 3→2 / Chest 0→1 with visible rows reflecting 2/1.
- External direct transfer (the same InventoryService path called by Lua) consumes remaining Player stock, giving **Player Empty slot disabled / Chest Potion x3**.
- A subsequent chest-to-Player transfer returns displayed labels to **Player x1 / Chest x2**.
- A later unrelated state/binding resolution preserves all the updated item labels **and the `MOVED ONE ITEM` status**.
- Reinitialising the authored Screen clears overrides and restores its original initial values, preventing Scene leakage.

This regression failed by construction before the override fix and passes after it.

## DESIGN verification

- `cmake --build BUILD/inventory-full --config Release --target RenegadeRuntimeScreenTests`: PASS.
- `ctest --test-dir BUILD/inventory-full -C Release -R "^RenegadeRuntimeScreenTests$"`: PASS with the new stale-label and transient-status checks.
- Full production Release Runtime linked into **`BUILD/inventory-full/Runtime/Release/RenegadeRuntime-G4B2Live.exe`**, a separate TargetName because the owner's prior `RenegadeRuntime-G4B2Fix.exe` process was open. **PASS**; no owner process was terminated.
- Linked RuntimeScreen / InventoryScreenAdapter / InventoryUiInputPolicy Release tests **3/3 PASS**.
- Isolated inventory core tests **12/12 PASS**.
- Real linked G4B2Live DX12 smoke with the Inventory-Proof Screen and creator Lua companions: **status PASS, exit code 0, Complete Game**.
- DX12 StoryFlow smoke does **not** press E, click any item, or capture/check UI pixels. **Owner must still verify live displayed slot updates and disabled empty slots.**

## Owner candidate

Use `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B2C-Live-Refresh.cmd` with the existing disposable Inventory-Proof project. It launches the alternate linked `RenegadeRuntime-G4B2Live.exe` with an explicit `--project`, so the old G4B2Fix window stays untouched.

In game press **Play → I**, then transfer with E and a click. Expected: Player 3/Chest 2 → 2/3 → 1/4 → 0/5, displayed directly in slot buttons as well as the independent Lua counter. At Player 0, its slot should read **Empty slot** and be disabled, while the Chest item reads **Health Potion x5**. Clicking a populated Chest slot should transfer one back and update both labels. The Screen status `MOVED ONE ITEM` must not revert when another slot state changes. Screen click activation/performance still require owner confirmation.

This is **not** packaged standalone validation, drag/drop, original artwork, editable Inspector selection by stable Screen ID, or persistent Save Game acceptance. The BUILD-only `.rinvui` marker remains provisional, documented under G4-B2A/B. Pinned Wicked shader overlay changes stay unstaged; active main/river/UI Designer/equipment worktrees remain untouched.
