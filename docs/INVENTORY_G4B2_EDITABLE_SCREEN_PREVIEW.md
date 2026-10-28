# Inventory G4-B2A — Editable Screen adapter and governed Runtime preview

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Worktree:** `renegade-inventory-core-20261026`
**Status:** Linked DX12 startup / production Screen document validation PASS. Owner visual/click/keyboard acceptance, original icon art, general authoring integration and packaged-game acceptance remain OPEN.

## Owner goal

Replace the hard-coded G4-A/G4-B1 temporary native overview with an **ordinary, editable Renegade Screen**. Use the existing UI Designer/ScreenRenderer conventions, independent of StoryFlow's navigation, and keep inventory optional. Developers may still use their own Lua/C++ interface and existing equipment actions. The UI must never hold or duplicate authoritative item state.

## Built

- `InventoryScreenAdapter` is a UI-independent adapter in EngineBridge. `MakeInventoryStarterScreen` creates a schema-v3 `ScreenUsage::Overlay` document with editable text, background panel and 16 slot **Button** widgets, eight for each of two explicitly chosen containers. It is disabled by default; the project must opt in to runtime usage.
- Slot actions use stable identifiers `inventory.slot.<panel>.<row>` and standard Button activation. No private widget format or MAX artwork. Unlike local-only button interactions, these actions must reach the slot request sink, which validates the currently visible row and a scoped gateway.
- `ProjectInventoryIntoScreen` makes a **transient copy** of the authored screen, setting slot text/enabled states according to `InventoryOverlayView`. It preserves the creator's widget IDs, layout, styling, focus order and artwork paths. Absent or empty slots are disabled. No authored screen file changes during gameplay.
- `ExecuteInventoryScreenSlotAction` dispatches one item transfer after validating an exact currently visible slot and the G4-B1 selection controller. G2 `InventoryGameplayGateway` still enforces source/target permissions; neither the Screen nor its text can grant them.
- `RuntimeScreenController::SetWidgetDisplayText` is a general Runtime Screen API for transient Text/Button labels; it rejects bindings-owned text, wrong widgets and oversized strings. This allows normal ScreenPresenter to update the GPU display without repeatedly reloading a Screen or writing project files.
- `RuntimeApplication` conditionally loads a disabled authored Screen as a **transient enabled view** through the existing `RuntimeScreenPresenter` when the level is inventory-enabled and the test has an explicit module opt-in. The pre-existing G4-B1 native overview remains the fallback for projects without an inventory Screen. The new Screen uses existing mouse/keyboard activation, input focus and RenderPath drawing. It doesn't introduce a separate RenderPath, a new UI Designer or a second inventory database.
- When the authored Screen is open, movement/weapon input stays gated as in G4-B1; the mouse cursor becomes visible, and primary/HUD input is suppressed while this overlay owns focus. On close, usual UI input ownership resumes.

## Why the preview uses a separate marker

A first disposable candidate with `InventoryG4B2Preview.renegade-screen` globally enabled made the game's **startup smoke time out at the menu**. The same G4-B2 executable passed the LP03 project and passed Inventory-Proof after temporarily moving that enabled Screen aside; this isolated a startup/discovery interference from the globally enabled overlay asset.

Do not bypass StoryFlow or mark every optional Overlay globally enabled. The authored starter now remains `enabledInGame=false` by default. For this disposable test fixture only, the module opt-in is represented by the bounded marker:
`Content/UI/InventoryG4B2Preview.rinvui`, contents `RENEGADE_INVENTORY_UI_PREVIEW 1`.
The Runtime recognises it only for an unpackaged developer project and loads a transient enabled copy **after gameplay begins**. Later production integration must replace this prototype marker and fixed filename with an editor-authored, project/scene-owned stable Screen ID selection; this marker is **NOT** a public asset-format contract and MUST NOT be packaged automatically.

The first enabled preview document was retained as a `.initial-enabled-backup` within BUILD; it was not silently overwritten. The disposable fixture was regenerated with an editable but normally disabled Screen plus a separate marker. Project's title now distinguishes the test as `Renegade Inventory UI Designer Preview (G4-B2)`.

## Verification

- `RenegadeInventoryScreenAdapterTests` uses the **real ScreenService writer, reader, validator and production EngineBridge**. It verifies schema-v3 authoring round trip, 16/6 slot actions as authored, retaining creator-specific style changes, live quantities, disabled empty slots, permission-denied atomic transfers, bidirectional transfers, and rejecting malformed action IDs and absent slots. **PASS**.
- `RenegadeInventoryScreenTemplateAuthor` is an intentionally guarded developer-only tool: it refuses any project except the disposable `BUILD/inventory-full/Runtime/Release/Content/Inventory-Proof` root, validates project Scene metadata, refuses overwrites and writes a standard editable Screen. It now writes a separate local-only preview marker to opt the development runtime into the transient screen view.
- Full Windows VS2022 x64 Release production `RenegadeRuntime-G4B2.exe` linked successfully, using pinned Wicked + EngineBridge + ScreenRenderer + Runtime. The normal `RenegadeRuntime.exe` was locked by the owner's previous game window; no attempt was made to terminate it.
- Initial DX12 smoke with the globally enabled Overlay **TIMED OUT** and was rejected. Its successful LP03 control and A/B inventory test isolated Screen discovery interference. After changing the preview to a disabled authored asset with a separate explicit module opt-in, the same inventory-enabled project reached **StoryFlow Complete Game, status PASS, exit_code=0**, with DX12 graphics started. **This proves startup and compatibility, not user-visible click transfers or rendered layout.**
- Isolated G4-B1 inventory core CTests remain an independent known-green gate; G4-B2's linked Screen adapter regression is separate.

## Owner visual test (pending)

Close prior preview windows when convenient; do not double-click the raw developer Runtime EXE. Launch:
`BUILD/inventory-full/Runtime/Release/Run-Inventory-G4B2-Editable-Screen.cmd`.

Enter Play, press **I**. Expected result is an editable-slot overlay (different from the native G4-B1 panel) with Player Backpack / Storage Chest and live Health Potion quantities. A mouse click on an occupied button should move one item to the other container. Keyboard Screen focus (Tab/Enter) should also activate a visible slot; **E** remains the independent scripted fallback. Quantities should update without duplication. Close with **I**. Capture a screenshot and report any failure; until that visual check, do not mark G4-B2 UI accepted.

## Remaining work

1. Replace the developer-only preview marker/fixed filename with an approved stable screen-ID binding in the existing Inspector/StoryFlow UI Designer workflow, including package dependency discovery and Test Game parity.
2. Item icon metadata and **entirely new artwork** (never reuse previously published MAX radial wheel art), a responsive slot-grid model, tooltips, clicking/drag-drop quantities, focus and input interaction arbitration.
3. Actual in-game chest model and loot pickup presentation, save-slot authority, full standalone packaging and owner acceptance.
4. Clean isolated source review and eventually merge approval. Nothing is pushed or merged by this gate; parent Wicked/river/UI/equipment worktrees untouched.
