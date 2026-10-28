# Inventory G4-B3C1B — editable paged-grid Runtime candidate

**Date:** 27 October 2026
**Branch/worktree:** `feature/inventory-core-g1-20261026` / isolated `renegade-inventory-core-20261026` on DESIGN.
**Status:** Release built and integrated smoke/regression verified; **owner visual / physical mouse acceptance PENDING**. Do not merge or push.
**Accepted baseline:** G4-B3B3 original inventory Screen / icon, click, E, repeated drag and Shift+drag accepted by owner.

## Delivered

1. `InventoryPagedScreenAdapter` builds an **ordinary creator-editable Screen v3** overlay with two independent **5 columns × 4 rows** of graphical square Buttons, 40 ordinary icon Image widgets, 40 quantity Text widgets, separate up/down paging Buttons and read-only range Text labels. This Screen has **132 normal widgets / 44 interactive actions**, retains native Screen style/rectangle/colour/font properties, and is disabled by default. It is not a new Inventory Designer or duplicate item store. A creator can restyle and move widgets in the normal Screen UI Designer. A persisted reusable Grid layout component and Inspector row/column control are *future work*, not claimed to exist.
2. `ProjectPagedInventoryIntoScreen` reads two permission-scoped G4-B3C1A `InventoryGridPage` windows, projects stable button item captions, icon resources and quantity badges, updates active scroll buttons and a visible `SLOTS first–last / capacity` label. Projection is transient and the authored Screen document retains its original empty-label text, theme and button identities. Style and action-name validation/saving/reopening is covered by a production-linked test.
3. New Runtime Preview path is **strictly gated** by the separate disposable-project marker `Content/UI/InventoryG4B3C1BGrid.rinvui`. If absent, Runtime continues to load the already owner-tested G4-B3B2 fixed graphical Screen and its B3B3 pointer handler. This avoids overwriting accepted UI. Paged Preview binds Button actions `inventory.paged.<panel>.slot.<visible index>` through the G4-B3C1A page snapshot and existing permission-scoped `InventoryGameplayGateway`.
4. The page arrows scroll independently by one row. Mouse wheel over a visible inventory cell also advances the current panel's top row (Wicked `GetPointer().z` delta). Each scroll marks the transient projection dirty even if inventory revision did not change. Scroll cancels stale pointer gestures. Ordinary click transfers one, ordinary drag transfers one and Shift held at pointer-down transfers a whole stack; all resolve the captured stable stack ID and revision through the existing atomic service. Empty slot drop selects the destination CONTAINER, not an exact numbered placement slot: InventoryService still holds a compact list of stacks.
5. A **separate BUILD-only cloned project**, `Inventory-Paged-Proof`, was created by copying the disposable `Inventory-Proof` test fixture. `Tests/InventoryPagedPreviewAuthor.cpp` refuses writes outside that exact BUILD destination or overwrite of an existing paged Screen/marker. It generates an editable, disabled Screen and a valid authoritative inventory snapshot with 100-slot Player and Chest containers, 55 Health Potion stacks of 3 in Player and 6 in Chest. Assets and original screenshots/scripts remain in the copied project; `Inventory-Proof` and its accepted G4-B3B3 executable are unchanged.
6. Compiled separately as `RenegadeRuntime-G4B3C1B.exe`, with a dedicated launcher `Run-Inventory-G4B3C1B-Paged-Grid.cmd`.

## Evidence on DESIGN

- `cmake --build BUILD/inventory-full --config Release --target RenegadeRuntimeBootstrap RenegadeInventoryPagedPreviewAuthor RenegadeInventoryPagedScreenAdapterTests ...`: **PASS**
- Full linked `RenegadeRuntime-G4B3C1B.exe` Windows x64 VS2022 Release: **PASS**.
- `RenegadeInventoryPagedScreenAdapterTests`: verifies standard Screen validation, authorable document round-trip with 132 widgets and 44 focus/action keys, both panels' icons/buttons/quantities, item 80 projection after scrolling to row16, scroll keys, invalid key rejection, and preservation of authored button styles/text: **PASS**.
- Production-linked Screen/PagedPage/GridViewport/old Screen/Drag/Pointer/InputPolicy tests **8/8 PASS**.
- Isolated Windows inventory tests **15/15 PASS**.
- Real paged **DX12 StoryFlow** smoke: initial 40-second run timed out at the startup menu, despite DX12 capability STARTED. A/B checks: same new exe with old Inventory-Proof project **PASS**, old accepted G4-B3B3 exe with paged project **PASS**, new exe with paged marker temporarily disabled **PASS** (marker restored). A later diagnostic paged run completed **PASS** before 8s, and two further paged exact-project DX12 smokes completed **PASS, exit 0, Complete Game**. Thus first timeout is **intermittent**; it is neither concealed nor proof of a reproducible failure. No mouse/wheel events were driven by smoke tests.
- Screenshot of a non-smoke startup showed an interactive 75 FPS menu in the 1264x681 Runtime window. Actual inventory grid was not visually inspected inside a gameplay session by tools.

## Owner visual acceptance (PENDING)

Run `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B3C1B-Paged-Grid.cmd`. In the menu click Play, then press I.

- Player has **55 potion stacks of 3**, Chest has 6 stacks of 3, in two **5×4** slot panels.
- The Player panel should show SLOTS 1–20 / 100 initially. Wheel over any Player slot or click its DN button to scroll one row to 6–25, then on to 11–30 and so on. UP reverses. Chest scrolls independently into empty capacity. Only these 20 visible tiles should bind item icons/counts; full inventory remains present.
- Click, normal drag and Shift+drag across panels should transfer 1/1/full stack as appropriate, repeatedly. Test after scrolling: item identity and count must stay correct, with no double-transfer and no transfer from a stale drag. E remains the existing governed Lua proof.
- Verify icon rendering, no button overlay interference, proper hover hints and UI scaling. If anything regresses, **do not mark accepted**, leave old G4-B3B3 executable and Inventory-Proof project in place.

## Explicitly not delivered

A draggable scrollbar thumb/Slider and UI Designer Inspector row/column controls; general saveable Grid component properties; creator-chosen item icon mappings; split stack UI; positional placement/sorting within the gameplay container; arbitrary named container binding; documented custom Lua/C++ behaviour-provider hooks; packaged game and Save Game acceptance. G4-B3C2 covers scrollbar/Inspector and further visual UX, not this candidate.

No merge/push. Other main, river, equipment and UI Designer worktrees unchanged. Generated Wicked shader overlay in isolated checkout remains unstaged.
