# Inventory G3-F — Script-backed live Runtime proof (developer candidate)

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Status:** Owner verbally accepted the live G3-F proof after testing the dedicated launcher ("yeaH that works"). Automated authoring/launch evidence was already green; no packaged build, inventory grid, save/load or finished gameplay screen is implied.

## Why this candidate exists

The owner opened `Run-LP03-Screen-Proof.cmd` and saw a black scene with a grey cube at 75 FPS. This is the minimal LP03 graphics/StoryFlow diagnostic fixture; it never contained inventory, a chest, a game HUD, or a pickup. The inventory Core G1–G3-E tests and project packaging gates were not a substitute for a visible Player/chest test.

## Work completed

Added `Tests/InventoryRuntimeProofAuthor.cpp` and the opt-in build target `RenegadeInventoryRuntimeProofAuthor`. It uses production `ReadDocumentEnvelope`, `CreateScriptDocument`, `CreateScriptAttachment`, `AddScriptAttachment`, and `WriteScriptDocument` to attach a **Global Script** to each LP03 fixture Scene. It strictly refuses to write outside a project named `Inventory-Proof` located inside this branch's disposable `BUILD/inventory-full/Runtime/Release/Content` folder. Level-scoped scripts must be Global Scripts; the first attempt with the ordinary Script presentation was rejected by the governed validation and corrected.

The authored `Content/Scripts/inventory_live_proof.lua` queries `renegade.inventory.list("player")` and `list("chest")` under the same trusted per-script G3-A access gateway. Each Scene's `.rscripts` companion contains a generated stable ScriptInstanceId. Each `.rinventory-access` companion explicitly grants that exact instance read/deposit/withdraw to only `player` and `chest`. The snapshot already contains three Player potions, two chest potions. No new item authority is introduced.

In an active Level the script requests a bottom-screen prompt through the existing `renegade.ui.show_prompt` API showing Player/chest potion quantities. `renegade.input.was_pressed("interact")` (default **E**) requests an atomic `transfer("player","chest",stackId,1)`. When the Player stack is empty, subsequent presses transfer one potion back from chest to Player. The old geometry remains a grey cube—this is an interaction proof, not a finished inventory screen. No visual chest model/pickups and no save slots are introduced.

The disposable LP03 copy is renamed `Renegade Inventory Live Proof (G3-F)`. Added a convenience script `BUILD/inventory-full/Runtime/Release/Run-Inventory-Live-Proof.cmd` which invokes the linked DX12 Runtime **with the required --project argument**, rather than double-clicking `RenegadeRuntime.exe` and triggering the expected missing-package-manifest error.

## Verification and limits

- `cmake --build BUILD/inventory-full --config Release --target RenegadeInventoryRuntimeProofAuthor` PASS (real production EngineBridge linked).
- Running the author against `BUILD/inventory-full/Runtime/Release/Content/Inventory-Proof`: PASS; both validated Scene `.rscripts` companions and per-script access files created.
- DX12 smoke with the authored companion on both Scenes and explicit `--project`: **exit 0 / status PASS / Complete Game**.
- Existing linked DX12 smoke without Lua companions also PASS. Existing core CTest 9/9 PASS.
- **Owner verification:** User reports the G3-F live launcher works. Treat the Player/chest prompt and E-key interaction proof as owner-accepted based on their test, separate from the automated smoke, which only validates startup. This does not prove persistent saves, a packaged executable, or a complete inventory UI.
- The fixture is **not** an inventory-grid implementation, user-customisable UI screen or model/pickup placement workflow. A proper gameplay level, existing UI Designer bindings, save-slot authority, package staging and owner acceptance remain open.

## Owner interaction (completed; retained for regression testing)

Close the older LP03 Runtime window first. In the inventory branch's
`BUILD/inventory-full/Runtime/Release` folder double-click
`Run-Inventory-Live-Proof.cmd`. Use its Play button to enter Level One.
Look for the bottom of screen text:

`INVENTORY PROOF | Player: 3 potions | Chest: 2 potions | Interact: transfer 1`

Press **E** repeatedly. Expected state change: `3/2 → 2/3 → 1/4 → 0/5 → 1/4`.
Close game and inspect Runtime diagnostics if text does not appear or quantities do not change. This is a scripted contract proof, and the shared UI Designer screen comes next.

## Safety

The generator and output are confined to BUILD and the isolated inventory branch; it does not touch the owner project, active Renegade, river, equipment or UI Designer worktrees. Do not push or merge this unfinished feature based on the smoke result alone.
