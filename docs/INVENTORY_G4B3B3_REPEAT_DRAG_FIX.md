# Inventory G4-B3B3 — repeated mouse drags / Shift stack input regression

**Date:** 27 October 2026
**Branch/worktree:** `feature/inventory-core-g1-20261026` / isolated `renegade-inventory-core-20261026`
**Status:** **OWNER ACCEPTED** on 27 October 2026 after testing the G4-B3B3 DX12 preview on DESIGN ("everything works now"). Covers repeat drag, Shift+LMB full-stack, and preserved icon/click/E interactions in the Inventory-Proof fixture. No merge or push.

## User report and scope

After accepting the G4-B3B2 icon/quantity screen, the owner reported: **only one mouse drag can be performed, not one item per repeated drag, and Shift+LMB no longer transfers a full stack**. This explicitly supersedes any claim that the icon-grid input path is accepted. Existing gameplay InventoryService/Gateway transactions and the separate G4-B3C0 viewport experiment were not the reported failure. Preserve all G4-B3B2 screen art/layout and prior preview builds while repairing mouse gesture capture.

## Diagnosis and correction

Prior Runtime depended on `wi::gui::Button::OnDragStart` / `OnDragEnd`, which were triggered only when the native widget received the pointer gesture. Graphical icon/quantity overlays, native button focus, sorting and state changes meant the UI had no independent guarantee of a fresh drag start or consistent Shift capture for subsequent operations. The pure InventoryDragTransfer model itself can already transfer one or whole stack and rejects stale revisions; its tests previously did **not** exercise repeated physical input. Do not claim the exact GUI failure sequence has been visually reproduced from code alone.

`Runtime/src/InventoryPointerGesture.h` is a small graphics-independent pointer lifecycle policy. Every physical left-button press gets its own **rearmed gesture**, start slot from the Screen's actual resolved rectangle (not the icon widget focus), and a Shift modifier snapshot at mouse-down. Mouse move beyond a 12-pixel threshold marks the operation as drag; release yields one well-defined drop event. Native double-click-on-release output for the same drag source is suppressed, but a new press re-arms the slot and normal click still moves one item. An outside press never turns into a captured drag. Mouse/overlay reset cancels pending gestures.

`RuntimeApplication::PollInventoryPointerInput` is invoked during the existing inventory Screen update while open and interactive. The polled event is routed through existing `OnInventoryScreenPointerDrag` and unchanged `InventoryDragTransfer` + scoped `InventoryGameplayGateway`; there is no second item authority or new UI renderer. The old native per-button drag callbacks are no longer connected in the G4-B3B2 runtime to avoid two transfer paths. The native Button Click remains the ordinary one-item command and is suppressed only on a detected mouse drag of the same source. No changes to Lua/Script, C++ inventory transactions, the fixed icons, original artwork or B3C0 grid configuration.

## Automated verification

- `Tests/InventoryPointerGestureTests.cpp`: 3 consecutive press/drag/release sequences from one source, Shift held only at mouse-down, late Shift must not upgrade, normal clicks aren't suppressed, movement out/back still counts as drag, outside press ignored and reset. PASS.
- `Tests/InventoryDragTransferTests.cpp` expanded with a **real InventoryService/Gateway integrated sequence**: three successive ordinary drops move Player 5→2 / Chest 0→3; a Shift drag sends the remaining Chest stack back, restoring 5/0. No duplicate native clicks permitted. PASS.
- Isolated Windows Release inventory CTest **14/14 PASS**; production-linked RuntimeScreen/InputPolicy/PointerGesture/Drag/ScreenAdapter **5/5 PASS**.
- Windows x64 VS2022 `RenegadeInventoryApplicationSyntax` compiled PASS; full linked `RenegadeRuntime-G4B3B3.exe` PASS.
- G4-B3B3 DX12 StoryFlow smoke PASS (DX12 STARTED, exit 0, Complete Game). This checks startup/integration only, not physical mouse behaviour. Live user verification of multiple repeated drag transfers, Shift+LMB, click, and E is required.

## Owner interaction gate

Open distinct `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B3B3-Repeat-Drag-Fix.cmd` against the unchanged guarded Inventory-Proof sample. Play then I. Fresh sample session begins with Player x3 / Chest x2. Drag Player potion → Chest once: 2/3; again: 1/4; again: 0/5. Then hold **Shift before pressing left mouse on the Chest potion icon** and drag to Player: expected Player 5 / Chest 0. Next ordinary click or E should transfer one. Confirm icon/quantity badges and hover remain correct, and dropping outside doesn't transfer. Never mark accepted merely because automated suite or smoke pass.

No new icon art, no saving/packaged game acceptance, no merge/push, no change in the parent main, river, equipment or UI Designer worktrees. Isolated generated Wicked shader overlay remains unstaged.
