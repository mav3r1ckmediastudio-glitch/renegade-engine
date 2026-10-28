# Inventory G4-B3C1A — authorised paging and stable Screen action bindings

**Date:** 27 October 2026
**Branch/worktree:** `feature/inventory-core-g1-20261026` /
`renegade-inventory-core-20261026` (isolated worktree).
**Scope:** Implement the governed paginated **data binding** underpinning a creator-controlled scrolling inventory. This is not yet the visual scrollbar or Inspector.
**Owner baseline:** G4-B3B3 has been visually and functionally accepted (repeated single-item drag, Shift+drag whole stacks, E/click, original icons, live quantity badges).

## Why this is a separate layer

G4-B3B2/B3 used `InventoryOverlayView`, a hard-coded **two-panel, maximum-24-stack** presentation snapshot. `G4-B3C0 InventoryGridViewport` computed scroll rows for arbitrary sizes, but did not fetch or bind the actual items beyond that limit. Reusing the old view for scrolling would silently hide items and risk mapping a recycled button index to the wrong inventory instance.

The new `InventoryGridPage` (`EngineBridge/include/renegade/bridge/InventoryGridPage.h`, `src/InventoryGridPage.cpp`) reads **one visible window** of a creator-configured grid from the existing `InventoryGameplayGateway`. It requires an authorised `ReadContainer` permission; denied results clear prior page data rather than expose stale items. It presents absolute packed-stack indices, stable item instance IDs, definition IDs and quantities in each visible cell, together with authoritative revision, occupancy and the G4-B3C0 viewport metadata. The full container snapshot is copied by today's gateway and then projected to the visible window; improving query granularity is an optimisation for later, not a claim of O(page-size) backend reads.

With a 100-slot container and 5x5 screen, topRow=0 maps absolute 0..24; topRow=4 maps 20..44; topRow=15 maps 75..99. An empty cell carries no item identity and never begins a drag. The number of cells is bounded by the validated layout (maximum 1024 visible cells). For an unlimited-capacity container (maximumSlots=0), a finite display window includes one spare row beyond its populated stacks, **without setting a gameplay inventory cap**.

## Authorable screen action contract

`inventory.grid.cell.N` is a deterministic visible-cell action ID. `ParseInventoryGridCellActionId` accepts only exact unsigned decimal N from 0..1023 with no signs, leading zeros or trailing content. The on-screen action is NOT a container identifier or item authority. `ExecuteInventoryGridCellAction` resolves it through a specific read-only `InventoryGridPage` and requests an ordinary one-item or Shift/full-stack transaction through the same scoped gateway. The same model will bind to regular Screen Button events later.

A drag uses `BeginInventoryPagedTransfer`: capture source container, **absolute position**, stable instance ID, whole-stack-or-single quantity and revision at pointer down. `CommitInventoryPagedTransfer` requires:
- unchanged revision on source page, destination page and live gateway;
- correct source page/container and matching instance/quantity at captured absolute index;
- valid currently visible target cell in a different container;
- a fresh scoped read of the live source container verifying that absolute position still holds that exact instance/quantity;
- existing permission-scoped atomic `InventoryGameplayGateway::Transfer`, which continues to enforce withdraw/deposit, slot capacity, weight and maximum-stack rules.

The destination *visible cell* currently means **the destination container**, not guaranteed placement at that particular cell. InventoryService uses a packed stack vector, not fixed physical slots. Implementing positional placement, sorting or drag-reordering must be a separate authoritative inventory-service feature; never falsely imply this API already does it.

No additional item store, new inventory Screen renderer, pointer manager, Lua API or globally enabled StoryFlow Screen is introduced.

## Verification

`Tests/InventoryGridPageTests.cpp` exercises, against real InventoryService:
- 100 uniquely identified stacks, accessible by page even at offset 75 (past old 24-row limit).
- denied reads cannot leak data; denied transfer does not mutate source or target.
- action-ID parsing rejects signs, overflow, leading zeros, out-of-range and unrelated strings.
- stale source when a different page is passed is rejected despite the same authoritative revision.
- legitimate scoped page transfer from index 75 and then 76, item identity unchanged, and a double-use of the previous revision rejected.
- reverse transfer requiring the inverse permission grants.
- the authoritative packed vector shifts after removal, so later pages rebind to the correct instance.
- capacity-exceeded full-stack transfer is wholly rejected, not partially applied.
- unlimited empty containers display a bounded blank grid, not a newly created inventory.
- invalid layout leaves no stale page data.

Windows x64 standalone inventory suite `ctest --test-dir BUILD/inventory-g1 -C Release`: **15/15 PASS**. Normal production EngineBridge Release link **PASS**, and `RenegadeInventoryGridPageTests` production-linked Release inventory/Screen regression **7/7 PASS**. No Runtime UI/scrollbar was modified or DX12 visual acceptance claimed in G4-B3C1A.

## What remains (not built, not accepted)

**G4-B3C1B:** normal editable Screen v3 Grid template that binds stable Button/Icon/Quantity widgets to this page model and updates on a row offset change while preserving existing drag and E behaviour.

**G4-B3C2:** proper interactive scroll wheel/scrollbar and clipping, Inspector controls for row/column count and appearance, external item icon assignment and real mouse/controller focus. Must test scrolling during a drag and cancel/reconcile stale interactions.

**Later:** authorable gameplay action policies/hook providers for Lua/C++, save/load and packaged standalone parity. Existing fixed B3B3 icon-grid Runtime remains the owner-accepted baseline.

No merge or push; the parent main, river, UI Designer and equipment worktrees are untouched. The pinned isolated Wicked shader overlay is left unstaged.
