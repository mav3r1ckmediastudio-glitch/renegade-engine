# G4-A — Optional live Inventory Overview (first visual milestone)

**Date:** 2026-10-26
**Branch:** `feature/inventory-core-g1-20261026`
**Scope:** Minimal 2-column, read-only Player/chest overview in the existing native Runtime presentation, backed by one authoritative `InventoryService` and a tested immutable view model.
**Status:** Full VS2022 Release Runtime link PASS; DX12 Inventory-Proof smoke PASS; automated tests PASS; **owner I-key visual acceptance pending**.

## Design/authority

A fully customisable inventory remains an optional gameplay component, not required for FPS, racing or arcade projects. This slice adds no independent item store, no Inventory Designer, no second equipment system, and no changes to Wicked's source or the UI Designer/StoryFlow branch.

`InventoryOverlayView` in EngineBridge is a pure, bounded projection of the existing gameplay inventory. The caller chooses container IDs/titles; missing containers are omitted, and no containers are created implicitly. Each panel carries occupied/maximum slots, live total/maximum weight, up to eight item name/quantity rows, truncation state and data revision. Item names are bounded before presentation. Transferring items via the accepted G3-F Lua path changes the one authoritative inventory revision and invalidates the cached view.

The Runtime application caches the view model and updates it only when the inventory revision changes. It never reads source files per frame. A small hint at the upper-right of the gameplay viewport displays `I INVENTORY` only when a level explicitly has inventory containers. Pressing **I** toggles a dark, two-column `PLAYER BACKPACK / STORAGE CHEST` panel with occupied slots, item names, quantities and weights. Press **I** again to close. Lua's existing **E/Interact** transfer remains the gameplay command used by this proof, and visible counts follow that command's authoritative result.

The first panel is **read-only**; it does not yet select a slot, transfer by click/drag, equip a weapon, author screens, or pause gameplay. This keeps the first visual gate constrained and avoids competing Interact/focus consumers. This is a temporary presentation inside the **existing** `RuntimeRenderPath::Compose`, not an editor UI template or a substitute for the existing UI Designer. The follow-on gate should bring slot list/grid and binding into shared UI Designer/RuntimeScreen semantics with governed pointer focus and transaction dispatch.

## Production changes

- `EngineBridge/include/renegade/bridge/InventoryOverlayView.h` and `src/InventoryOverlayView.cpp`: tested UI-independent read model.
- `Runtime/src/RuntimeApplication.h/.cpp`: optional view cache and native RenderPath overlay; toggled by `I` in unblocked gameplay; reset on scene transition; no panel for inventory-free projects.
- `Tests/InventoryOverlayViewTests.cpp`: empty project, two containers, name/quantity, weight, maximum slots, revision, transfer refresh, truncation and missing containers.
- `EngineBridge/CMakeLists.txt`, `Tests/CMakeLists.txt`, `Tests/InventoryCoreStandalone/CMakeLists.txt`: production and test build wiring.

## Verification on DESIGN

- `cmake -S Tests/InventoryCoreStandalone -B BUILD/inventory-g1 -G "Visual Studio 17 2022" -A x64`: PASS.
- Isolated MSVC Release CTest: **10/10 PASS**, including new `RenegadeInventoryOverlayViewTests`; Debug focused model test: **1/1 PASS**.
- Full Runtime `cmake --build BUILD/inventory-full --config Release --target RenegadeRuntime --parallel 4`: **PASS**. Pinned Wicked remains confined to the isolated checkout; existing shader overlay modifications are not staged.
- Launched real linked DX12 Runtime with inventory-enabled `Content/Inventory-Proof/ScreenProject.renegade` and two StoryFlow completions using existing smoke flags: **status=PASS, exit_code=0, graphics_backend=DX12, smoke_status=PASS**.
- The smoke harness does not press `I` or render-inspect the overlay; visual layout, input toggle and live count text **require owner inspection**, and no visual/interaction acceptance is asserted from a 0 exit code.
- **No full packaged standalone, persistent game save, editable UI Designer inventory screen, icons, drag/drop, slot selection or finished world pickup/chest model is included.**

## Owner visual test

Open `BUILD/inventory-full/Runtime/Release/Run-Inventory-Live-Proof.cmd` (not the bare `.exe`). Click Play, press **I**, verify two visible panels with **PLAYER BACKPACK**, **STORAGE CHEST**, the existing Healing Potion item, initial counts **3 and 2**, occupied slot/weight data. Press **E** with the panel visible and verify **2 and 3**; repeat. Press **I** to close. If it does not draw, remains stale, blocks Interact, clips, or affects performance, do not mark G4-A owner-accepted; inspect screenshots and log the specific failure.

## Next

G4-B: integrate genuinely interactive item slots through the existing UI Designer/Screen runtime with original icon assets, selection and validated item transfers, responsive layout and keyboard/mouse focus. Do not treat the temporary Compose panel as a permanent divergent UI system. Require ordinary non-inventory FPS regression, Test Game and packaged-game proof, then owner review.

## Owner screenshot and overlap refinement — 2026-10-26

The owner supplied a visual screenshot of the real DX12 `Renegade Inventory Live Proof (G3-F)` session at **1264×681**, displaying **75 FPS**. The inventory panel was open, with **0/12 occupied Player slots**, **1/12 chest slots**, **Health Potion ×5** in the chest, and **0.0/5.0 Player vs 1.0/5.0 chest weight**. That is consistent with five actual G3-F Lua transfers; the screen demonstrably reads the authoritative inventory and renders it.

The screenshot exposed a **presentation defect**: the older Lua `show_prompt` overlay was still painted *after* the inventory panel, hiding part of the bottom footer. `RuntimeRenderPath::Compose()` now suppresses only the generic native in-world interaction prompt **while the inventory overview is visible**, without clearing the prompt or altering the E/Interact Lua logic. Closing the inventory restores normal prompts. The built-in footer remains visible.

The user was still running the previous developer `RenegadeRuntime.exe` (process 2104), which Windows locked against replacement. Do **not** terminate the owner's session. The corrected source `RuntimeApplication.cpp` compiled; MSVC's ordinary final link failed with **LNK1104** for the locked target (not a C++ failure). An isolated **alternate executable** was linked from the already-built targets using `/p:BuildProjectReferences=false /p:TargetName=RenegadeRuntime-G4A`, producing `BUILD/inventory-full/Runtime/Release/RenegadeRuntime-G4A.exe` without touching the active executable. Its real DX12 Inventory-Proof StoryFlow smoke **PASS, exit 0**.

Use `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4A-Fixed-Preview.cmd` to test this fixed preview, not the older launcher, while the original window remains open. **Owner visual confirmation of the corrected prompt/layout remains pending**. After the old Runtime is closed, ordinary build can relink the canonical `RenegadeRuntime.exe`. Do not merge/push yet.
