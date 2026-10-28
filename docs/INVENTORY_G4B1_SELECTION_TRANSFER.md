# G4-B1 — Selection and governed transfer in live inventory overview

**Date:** 26 October 2026
**Worktree:** `renegade-inventory-core-20261026`
**Branch:** `feature/inventory-core-g1-20261026`
**Scope:** Owner-reviewable keyboard interaction. Not a complete authorable UI Designer screen, inventory grid, icon art or drag-and-drop interface.

## What changed

G4-A established a working, owner-seen two-column inventory overview backed by `InventoryService`. G4-B1 adds `InventorySelectionController` in EngineBridge: a C++ class with **no Wicked/UI Designer dependencies** that stores only a selected container/stack identity (not item data). It provides controlled selection, Up/Down movement, Tab container switching, selection reconciliation after Lua mutations and `TransferSelected(view, scopedGateway, quantity)`. It checks that the selected identity still exists in the visible view, that the destination is another distinct container and that the requested count is valid. All real transfers go through the G2 capability-scoped `InventoryGameplayGateway`, which delegates to the one authoritative, atomic `InventoryService`. No duplicate item or equipment authority is introduced.

Runtime constructs a **trusted, per-Scene UI gateway** explicitly granting read/deposit/withdraw for the existing authored `player` and `chest` containers **only**, and releases that gateway before the inventory session is reset. Other containers are neither listed nor editable by this prototype.

With the inventory open, the temporarily native G4 overview highlights the current item row. Up/Down changes the highlighted stack, Tab switches to the other container when that container has rows, and Enter transfers exactly one item into the opposite container through the scoped gateway. After successful transfers the view and selection are immediately rebuilt from authoritative state. Failures produce a bounded status footer (destination full, access denied, unavailable item). The existing owner-accepted Lua **E** transfer remains active in this fixture. Press **I** to open/close. While the overview is visible, native movement and combat input are suppressed, preserving Esc/Pause and E/Interact from the existing fixture, so inventory navigation doesn't fire a gun or move the Player inadvertently. The older duplicate Lua debug prompt remains hidden while the panel is open, per G4-A fix.

The selection controller is deliberately presentation-layer neutral so it can be reused by **existing UI Designer/StoryFlow** authoring later, without introducing an Inventory Designer. G4-B2 needs real Screen widgets, item icon resource binding, mouse hit testing/focus ownership, clicks and drag/drop instead of expanding temporary hard-coded native UI into a permanent second framework. No gameplay screen pack is implicitly enabled for games lacking inventories.

## Verification (fill from checked builds, not assumptions)

- `Tests/InventorySelectionControllerTests.cpp` includes scoped permission denial, row movement/clamping, switch between containers, bidirectional atomic transfer, stable instance reconciliation after stack changes, and duplicate-source/target rejection.
- `Tests/InventoryCoreStandalone/CMakeLists.txt` and `Tests/CMakeLists.txt` register the new test; `EngineBridge/CMakeLists.txt` compiles its real source.
- Release controller test: PASS after correcting test fixture's Player deposit permission.
- Isolated Release CTest: **11/11 PASS** including all G1–G4B tests. Focused new selection controller MSVC Debug CTest **1/1 PASS**.
- Full pinned Wicked/EngineBridge/Runtime Windows x64 Release link: **PASS**; source and build target `RenegadeRuntime.exe`.
- Real linked DX12 Inventory-Proof project smoke: **status PASS, exit code 0, graphics_backend DX12**, reached StoryFlow Complete Game. This smoke does not press Enter or inspect the rendered selection.
- Source correction: player/combat input is gated while inventory is open; E/Interact and Esc/Pause retained to preserve the G3-F scripted proof. The test is keyboard-first and does not yet own a shared mouse/pointer focus route.
- Owner visual check: **pending**; launcher `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B1-Selection.cmd` was created after successful full build.

## Owner test sequence

Use the **new G4-B1 launcher** in `BUILD/inventory-full/Runtime/Release` (created after successful Runtime build). On loading the same Inventory-Proof Project, press Play, then I. The previous owner session left five potions in the chest; a fresh run begins with Player 3 / Chest 2 by default. One row should have a visibly highlighted left border. Up/Down changes selection when multiple stacks exist; Tab switches between the Player and chest item rows. Enter transfers one potion from the selected container to the opposite container. The opposite container's quantity updates immediately. Press I to close. E remains a scripted transfer used by this fixture. Esc/Pause is not consumed by the inventory.

Do **not** call this G4-B owner accepted until the layout, Enter transfer, selection and focus are confirmed visually. No push or merge. The isolated Wicked checkout's generated night-sky overlays remain separate from the actual parent engine checkout.
