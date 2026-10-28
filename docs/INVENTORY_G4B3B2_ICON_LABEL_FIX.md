# Inventory G4-B3B2 — correctly mapped icons and non-clipped quantity badges

**Date:** 27 October 2026
**Branch/worktree:** `feature/inventory-core-g1-20261026` / `renegade-inventory-core-20261026` (isolated)
**Status:** OWNER ACCEPTED on 27 October 2026 ("It works"). G4-B3B2's corrected potion artwork and quantity-badge UI were confirmed in live use. No merge or push. Follow-up creator request: configurable traditional square inventory grid with vertical scrolling; NOT IMPLEMENTED in this gate.

## Owner screenshot / first failed gate

The owner provided a G4-B3B1 DX12 screenshot at 1920x1009, ~74 FPS showing:
- Player Health Potion x5 and Chest 0 matching the independent Lua footer. Gameplay inventory item counts were correct.
- The occupied Health Potion showed the **? unknown/fallback** icon rather than the newly generated original Potion asset.
- The long `Health Potion x5` and repetitive `Empty slot` text clipped/crowded the tile bottoms. The visual grid was not fit for visual acceptance.
- The screenshot alone did not prove hover/drag parity in the new graphical screen; do not claim such approval.

## Root causes

The live Lua fixture uses the **exact item definition ID `health-potion`** in `Content/Scripts/inventory_live_proof.lua`, whereas `InventoryStarterIconForDefinition` only recognized `potion`, `coin` and `key`. The fallback PNG therefore appeared even for a valid populated Health Potion. This was not a missing PNG or a lost InventoryService transaction.

The first graphical design placed a long, right-sized label in the native button's bottom-aligned text area, causing the glyph baseline to clip at the edge of its 103x122 slot and cluttering empty tiles.

## Corrected implementation

- Keep the explicitly allowlisted mapping (no untrusted path concatenation): `health-potion` and `potion` both map to project-contained `Content/UI/InventoryIcons/potion.png`; `coin` and `key` retain own assets; all other identifiers map to `unknown.png`.
- The author's original interactive slot Button retains its normal, **nonempty semantic text** (`Health Potion` when populated, `Empty slot` when empty) and existing action, layout, focus and drag semantics; its visible font foreground is transparent in the five widget interaction visual states. This is necessary because ScreenService validates a Button's text/action pairing; making it blank was caught by the test and rejected.
- Add 16 normal non-interactive standard Screen **Text** widgets named `Inventory Slot Count P:R`. Positioned at lower-right within the corresponding authored tile, show only `xN` when occupied and remain hidden when empty. A hidden `x0` authored placeholder is used because ScreenService likewise rejects an authored Text widget with an empty string. Their data is a read-only projection of the real inventory state and uses the same runtime transient label overrides that solved G4-B2C.
- `ProjectInventoryIntoScreen` now projects both occupied quantity badge visibility and text, alongside the already existing item icons, button enabled states, item names and hover details.
- Runtime's existing inventory Screen refresh adds handling for the dedicated count Text widgets (`SetWidgetState` + `SetWidgetDisplayText`), without changing item-authority or mouse action routing, Shift-drag or Lua/E.
- New guarded, disabled standard Screen preview `InventoryG4B3B2Grid.renegade-screen` with **54 widgets / 16 slot Buttons**, paired with BUILD-only preview marker. Previous G4-B3B1 editable Screen, icons and binaries were not overwritten. The utility reuses existing art assets without replacing custom edits.

## Verification

- `RenegadeInventoryScreenAdapterTests` PASS, including authored 54-widget screen, normal validation/round trip, item icon alias, occupied quantity badges, hidden empty captions, fallback-to-unknown for other IDs, preservation of button semantic labels and original old-screen contract.
- Full Windows x64 Release `RenegadeRuntime-G4B3B2.exe` linked PASS.
- Full DX12 `--flow-outcome=level.complete --renegade-smoke-autoplay --renegade-smoke-exit` against inventory-enabled fixture PASS, graphics backend DX12 started, StoryFlow Complete Game, exit 0.
- Isolated Release inventory tests 12/12 PASS; production-linked drag/Screen/adapter/input tests 4/4 PASS.
- Two intermediate validation failures were deliberately rejected and repaired: blank Button caption violates action reference; blank Text violates text fields. Never bypassed the normal authoring schema.
- **Visual pixels, actual occupied Potion sprite, quantity baseline and mouse-input parity remain owner-review gates.** Smoke does not simulate these.

## Owner acceptance

Launch `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B3B2-Icon-Layout-Fix.cmd`. The title is `Renegade Inventory Icon Grid Fix (G4-B3B2)`. Play → I. The previously unknown Potion must now use the original red potion graphic; quantity should appear separately as x5 at the lower-right; all empty slots should be blank; hovering must show full Health Potion name and quantity. Transfer via click, ordinary one-item drag, Shift/full-stack drag and E, checking slot icon and count synchrony and avoiding accidental input consumption. If the visual layout or hover does not meet expectations, record screenshot before marking accepted. The previous G4-B3B1 accepted-input build is available for comparison.

No user-facing new icon art pack required, no edits to the pinned Wicked submodule, no merge/push, and no changes to main/river/UI Designer/equipment worktrees. General project-Inspector authoring, content packaging, standalone parity, Save Game and original visuals beyond this small starter icon set remain open.
