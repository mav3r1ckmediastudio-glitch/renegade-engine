# Inventory G4-B2B — Repair E and clickable slot transfers

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Worktree:** `renegade-inventory-core-20261026`
**Acceptance:** The owner has **rejected G4-B2A input functionality**: clicking item slots flashes without changing inventory, and pressing E no longer transfers. Keep the G4-B2A visual smoke result distinct from interactive acceptance. **G4-B2B owner interaction acceptance remains pending.**

## Diagnosed integration faults and changes

1. In `RuntimeApplication::Update`, the G4-B2 Screen owns UI focus and ordinary gameplay input can be blocked. Creator Lua consumes `gameplayInput_.interactPressed` for its established E action. The previous code captured **an empty input frame** whenever UI composition blocked world gameplay, even though the later inventory input filter tried to preserve Interact. The new policy `InventoryInteractHasFocus` explicitly permits action-map capture while the inventory Screen owns input, no other text field is typing, the gameplay Scene is active and the game is not paused. `GateWorldInputForInventory` then discards movement/aim/fire/equipment input while preserving only authored Interact and Pause. The same focus decision enables the inventory Screen's `UpdateInput` and UI state while open. This does not introduce a separate key binding or bypass privileged item APIs.

2. Wicked's native `Button::Update` fires its click event on release. Renegade's `ScreenRenderer` had an additional `MouseButtonDown` guard in the button's callback, potentially discarding a release activation on the same input frame. The guard is exempted **only for explicitly named `inventory.slot.*` actions**, leaving all other Screen/StoryFlow buttons unchanged. Callback routing still passes through the ordinary `RuntimeScreenPresenter` `MakeActionRequest`, `ExecuteInteractions`, local-only/visibility/input checks and scoped `InventoryGameplayGateway`. Extra bounded rejection diagnostics were added to those callback gates to distinguish input ownership, a disabled widget, and a rejected action.

3. The generated ordinary editable Inventory Screen gains a status Text widget that displays command success/failure and remains visible through authoritative revision refreshes. `OnInventoryScreenAction` now reports the action/result to the Wicked backlog, preserves the authoritative inventory revision refresh and publishes a visible status message instead of showing buttons that appear to do nothing.

4. A fresh, guarded **ActionFix** editable Screen and developer marker are created in the disposable Inventory-Proof BUILD project under `Content/UI/InventoryG4B2ActionFix.*` rather than modifying the previous preview or owner source assets. The corrected Runtime looks for this specific marker and Screen. This remains a **test-only fixed path/marker**, not a production authoring format; final project-level stable-ID binding stays outstanding.

## Verification boundaries

- `Tests/InventoryUiInputPolicyTests.cpp` exercises the focus matrix and world-input suppression while preserving E/Pause. Header-only production policy stays graphics independent, so isolated standalone CTest can run without linking Wicked.
- Extended `Tests/RuntimeScreenTests.cpp` runs real Screen-controller projected button activation and a scoped gateway transfer, not only the adapter's direct result.
- The previous G4-B2 visual appearance was accepted as provisional; **owner reports that E and clicks fail**, overriding any prior implications of interaction acceptance.
- Standalone Release inventory CTests: **12/12 PASS** (Core/G1–G4B, including new input policy).
- Runtime Screen + Inventory Screen adapter linked Release regression: **PASS** before final GUI build.
- Full pinned Wicked + ScreenRenderer + EngineBridge + Runtime **Release link PASS**: `RenegadeRuntime-G4B2Fix.exe`, approximately 49.8 MB. Existing earlier GUI executables untouched.
- Linked Screen adapter, RuntimeScreen controller-action, and inventory input policy Release CTests: **3/3 PASS**. The input test is intentionally graphics-independent.
- Exact Inventory-Proof candidate with the new `InventoryG4B2ActionFix.renegade-screen` and preview marker initially had one **35-second DX12 StoryFlow smoke timeout** while previous Runtime windows existed. Isolated A/B tests showed the same executable LP03 and Inventory-Proof **without the ActionFix marker** both completed. After those other windows were no longer present, the complete ActionFix project passed **three consecutive DX12 StoryFlow smoke runs** (exit 0, status PASS). The reason for the initial timing variability has not been proven; retain it as a test-environment concern.
- Actual E and mouse-click transfers **cannot be proven by StoryFlow smoke alone**. The new ActionFix candidate requires live owner confirmation; do not mark it accepted prematurely.

## Test and safety requirements

Use the new distinct `Run-Inventory-G4B2-Transfer-Fix.cmd` after it has actually been built. Close neither the owner's running original `RenegadeRuntime.exe` nor old `RenegadeRuntime-G4B2.exe`; the fixed candidate links as a separate target name. Start gameplay, press I, observe Player 3 / Chest 2, then press E to move one and click an occupied slot to move one back. The status line must show actual transfers and the quantities must change immediately. Press I to close, verify normal movement and rendering. Reject candidate if the slots flash without moving quantities, or input is still lost.

No merge/push until E and click acceptance, clean Build Game staging/packaged launch and subsequent UI Designer authoring scope are reviewed. Active river, UI Designer and parent Wicked physics worktrees are unchanged. Isolated Wicked checkout contains generated overlay changes and is intentionally unstaged.
