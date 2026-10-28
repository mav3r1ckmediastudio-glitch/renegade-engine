# Inventory G4-B3B1 — Original icon artwork and editable grid

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026` (isolated worktree `renegade-inventory-core-20261026`)
**Status:** production Release build and DX12 StoryFlow smoke PASS; native Screen authoring/slot tests PASS; **owner visual and pointer acceptance pending**.

## Owner goals and boundaries

G4-B3A2 is owner accepted: standard click, E/Interact and unmodified drag move exactly one item, while Shift+drag moves a whole stack. The next slice replaces text-heavy, narrow vertical lists with editable, compact **4 columns x 2 rows per container** (16 slots in total), a distinct original icon set and an item hover-details strip. This is still a functional sample level, not the final project-facing Inspector screen selector. It must not create a new Inventory Designer or duplicate equipment or item authority.

## New art and provenance

`Tools/generate_inventory_icons.py` is the committed source generator for four original 128x128 transparent RGBA PNG assets: potion (faceted glass with red liquid), key (brass), coin (gold), and unknown (muted fallback). It uses Pillow shapes and gradients and imports no published MAX-community icon or third-party art. The source icon assets are in `assets/InventoryStarter/Icons/` (Windows treats `Assets` as the same working directory). The disposable authoring utility copies them into `Content/UI/InventoryIcons` in the BUILD Inventory-Proof fixture; it refuses to overwrite pre-existing art. The icons can be replaced by project authors in the normal Screen resource locations.

`InventorySlotView` carries the existing inventory definition ID **as read-only presentation data**. The starter art mapping resolves only explicitly known IDs (`potion`, `key`, `coin`), falling back to `unknown.png` for all other IDs. It never concatenates untrusted item-definition IDs into filesystem paths. The gameplay item definition and serialized inventory snapshot formats remain unchanged.

## Authorable Screen

`MakeInventoryIconGridScreen` returns an ordinary, disabled-by-default Screen v3 overlay with the same 16 validated Button slot actions, stable keyboard focus and accessible interactivity as G4-B2C. The buttons are rearranged into two 4x2 grids (103x122 design pixels). Sixteen extra non-interactive Image widgets display icon resources only for occupied slots. Empty slot buttons remain disabled as before but still serve as drag targets. A Text widget at the bottom shows full item name, current count and maximum stack on hover; the separate status/help strip preserves the proven transfer feedback. The editor saves the same normal `.renegade-screen` schema, not a separate inventory document.

`ProjectInventoryIntoScreen` produces a transient copy: slot captions and enabled state still come from authoritative `InventoryService`; icon resource paths and visibility come from the same read-only view. The saved editable Screen retains its own layout, colors, image widget IDs and controls. `RuntimeScreenPresenter` exposes `SetVisualImageResource` for a loaded normal Image widget. `ScreenRenderer::SetWidgetImageResource` uses `ResolveScreenResourcePath` and the existing Wicked resource manager, refuses non-Image widgets or state-owned images and only accepts project-contained resources. Icon resource updates do not overwrite editor-authored Screen assets or gameplay state. The existing per-frame inventory revision invalidation refreshes both text and images as quantities change.

`RuntimeApplication` changes the fixed BUILD-only preview marker/file to `Content/UI/InventoryG4B3BGrid.{renegade-screen,rinvui}`, presents hover details using the existing slot rectangle hit test, and refreshes tooltip text only when it changes. The accepted G4-B3A2 edit/click/Shift-drag/E logic is untouched. The test marker still isn't a shipping Inspector/StoryFlow binding contract.

## Verification on DESIGN

- Original four icon PNGs generated and copied to guarded disposable BUILD project: PASS, verified file presence and byte sizes.
- Production `RenegadeInventoryScreenAdapterTests`: PASS. Covers old screen compatibility, new 38-widget standard Screen schema with 16 actions/icons, graphical slot geometry, live icon projection, definition-ID whitelist fallback, hiding all empty slot art and editor document round trip.
- Windows VS2022 x64 linked Release `RenegadeRuntime-G4B3B.exe`: PASS (Wicked/Bridge/ScreenRenderer/RuntimeBootstrap linked; no changes to pinned Wicked itself).
- Inventory-Proof project with new Screen art + governed Lua companions, DX12 auto-Play StoryFlow smoke: PASS, exit 0, backend DX12, Complete Game.
- Existing 12 isolated Release inventory tests: 12/12 PASS.
- Linked Screen/Runtime/input/drag Release regression tests: 4/4 PASS.
- **Not automatically verified:** actual PNG appearance, hover pointer details, visual grid spacing and E/click/drag parity within the new icon Screen. DX12 smoke does not inspect pixels or drive pointer events. Require user screenshot and functional approval.
- **Not part of G4-B3B1:** final icons for all possible user-defined items, draggable sprite ghost, fully responsive narrow layouts, general Inspector selection by Screen ID, game save slots, Build Game packaging and standalone acceptance.

## Owner check

Launch `BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B3B1-Icon-Grid.cmd` against the disposable Inventory-Proof project. Press Play then I. Expect Player and Chest columns each showing eight graphical slots (4x2), with the newly authored Potion icon on any occupied potion slot and empty icon surfaces hidden. Hover the potion to see its full name and count. Click, drag and Shift+drag as in accepted G4-B3A2; both icon visibility and slot labels must follow Player 3/Chest 2 -> 2/3 -> 0/5 and back. Check that the icon overlay does not steal pointer clicks or make empty slots incorrectly interactive. Report screenshots and regressions before this gate is accepted.

No merge/push. Isolated Wicked overlay remains unstaged; active main, river, equipment and UI Designer branches untouched.
