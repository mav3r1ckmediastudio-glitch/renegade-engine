# Inventory G4-B3A — scoped whole-stack drag transfer (owner preview)

**Date:** 2026-10-26
**Branch:** `feature/inventory-core-g1-20261026`
**Worktree:** isolated `renegade-inventory-core-20261026`
**Status:** implementation/build/automated tests/real DX12 startup PASS; live user pointer-drag acceptance PENDING.

## Background / safety

Owner accepted G4-B2C live UI on DESIGN after confirming that clicks, E and the authored Player/chest Screen showed accurate updated quantities (acceptance commit `5ce48a5`). This G4-B3A adds an **optional drag gesture**, preserving that exact one-item click and Lua/E path, same authorised gameplay inventory and editor Screen renderer. Nothing changed in parent Renegade/main, UI Designer, river or equipment worktrees. The isolated pinned Wicked worktree remains dirty from generated CMake shader overlays, which are unstaged.

## Implementation

- New `InventoryDragTransfer` (`EngineBridge/include/renegade/bridge/InventoryDragTransfer.h` and `src/InventoryDragTransfer.cpp`): UI/Wicked-independent gesture holder. Mouse-down validates a visible, populated `inventory.slot.<panel>.<row>`, recording the stable stack ID, source container, entire stack quantity and inventory revision, but **does not** mutate gameplay.
- The drop validates a distinct target container, exact previously captured item instance, sufficient available quantity and **unchanged revision**, then transfers the full captured stack by the existing `InventoryGameplayGateway`. Failure/cancel clears the gesture, leaving inventory unchanged. Same-container, empty-source, malformed action, permissions denial, stale intervening Lua mutation and duplicate drop are rejected.
- Generic `ScreenRenderer` now offers an **optional** per-button drag start/end callback; other Screen, Studio and menu clients are unaffected by default. The inventory buttons use existing Wicked Button `OnDragStart` and `OnDragEnd`; no new input subsystem.
- `RuntimeScreenPresenter::InventorySlotAt` hit-tests the shared ScreenRenderer's **resolved logical output rectangles**, not hard-coded design coordinates. Disabled empty destination slots remain legitimate drop targets; no drag on an empty source. `RuntimeApplication` bridges pointer drag start/end into the scoped controller.
- Drag must exceed a 12-screen-unit threshold, preventing a normal click from being mistaken for a drop. Existing click **still moves exactly one item**, and **E** still invokes the governed Lua proof transfer. Drag from Player to a slot in Chest (or reverse) attempts the whole source stack atomically. Drop outside cancels; stale inventory cancels.
- The existing transient authored Screen view still refreshes from the authoritative revision, so successful drag updates both labels, while the footer displays `MOVED STACK` or a bounded failure.
- No new item authority, visual-scripting gatekeeper or standalone Inventory Designer. The production editor Screen schema stays unchanged.

## Tests and build

- New production-linked `Tests/InventoryDragTransferTests.cpp`: empty source, same-container rejection, no permissions, whole-stack transfer, Lua mutation during drag, opposite direction, double-drop and cancel. **PASS**.
- Linked Screen/adapter/input/drag Release CTests: **4/4 PASS**.
- Isolated existing inventory Release CTests: **12/12 PASS**.
- Full Windows x64 VS2022 Release Wicked/Bridge/ScreenRenderer/RuntimeBootstrap and linked `RenegadeRuntime-G4B3.exe`: **PASS**.
- Real linked G4-B3 Inventory-Proof StoryFlow DX12 smoke: **status PASS, exit 0, reached Complete Game**. This validates launch only, **not** the physical pointer geometry or the visual drag experience.
- Preview is confined to the existing disposable `BUILD/inventory-full/Runtime/Release/Content/Inventory-Proof` with a new authored `Content/UI/InventoryG4B3Drag.renegade-screen` and its BUILD-only opt-in `.rinvui` marker (21 widgets/16 slot buttons). Previous G4-B2C Screen assets, acceptance build and separate executable retained.

## Owner check before continuing

Open `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B3-Drag.cmd` (or the already launched candidate). Play → **I**. Initial Player Health Potion x3 and Chest x2. **Click** one Player potion: moves 1 (2/3). Then **hold left mouse on the remaining Player Potion slot and drag to a Chest slot**, release: moves entire remaining 2 (0/5), both labels update. Try a Chest-to-Player drag: whole stack moves back. Drop outside: cancelled. Press **E**: original scripted one-unit transfer still works. If drag produces no change, double-transfer, stuck pointer, or wrong slot drop, this gate is NOT accepted; inspect cursor hit coordinates and logs.

**Not delivered:** original icon artwork (must not reuse community-released MAX art), sprite-grid redesign, item tooltips, quantity splitting, a packaged standalone, game save slots or the proper editable Inspector screen-ID selection. Those are next gates and must not be conflated with a verified drag prototype.
