# Renegade Inventory G5 — World Pickups and Shared Item Definitions
Date: 2026-10-27. Owner-approved direction; implementation contract, NOT a completed feature.
Worktree: C:\Users\paulw\source\repos\renegade-inventory-core-20261026
Branch: feature/inventory-core-g1-20261026. No merge/push to main without owner acceptance.

## Product outcome
An inexperienced creator imports/places any ordinary 3D object, selects it, enables **Can be collected** in its regular Level Editor Inspector, links an inventory item, sets quantity and collection method, and can collect it in Test Game. The very same logical item works in backpack, chest, store and other InventoryService-backed containers, with one inventory icon, one max stack size and one optional gameplay effect. No scripts required for the built-in basic path; governed Lua/C++ extensions remain possible. This is OPTIONAL: unrelated projects and ordinary props are unaffected.

## Current evidence and architectural boundaries
- The Level Editor already has a Wicked native Metadata Preset > PICKUP UI in Studio/src/StudioApplication.cpp (~1213), implemented by SetMetadataPresetCommand (~8793), with Undo/Redo. This is a semantic tag ONLY; there is no inventory transaction or usable pickup authoring workflow behind it.
- Native wi::scene::MetadataComponent has string_values/int_values/float_values and serializes with scenes (wiScene_Components.h). Avoid directly mutating scene data outside Renegade's command stack.
- InventoryService owns stable item definition ID, name, maximumStack, weightPerUnit, equipmentAssetId and authoritative Add/Remove/Transfer; it is not a Screen component. Inventory snapshot version 1 currently serializes definitions + container stacks.
- The test fixture Tests/InventoryPagedPreviewAuthor.cpp explicitly defines health-potion with maximumStack=3. It is NOT a global inventory limit. Other tests exercise 1, 5, 8, 10, 20. Do not change accepted fixtures merely to conceal the 3.
- World-to-item bindings, authored item definitions, pickup activation and health-consumable authority do not yet exist as a complete workflow. Do not claim PICKUP preset collects anything or health potion heals.
- Inventory Screen G4-B3C4 already supports standard Inspector configuration, Change Graphic and all-slot graphics, 4x3/5x5/6x6 layouts, paging, drag and Shift-drag. PRESERVE its untouched accepted logic and modifications.
- G3C packaged-inventory provider currently packages reachable .wiscene.rinventory and optional access policy; G5's definition/icon/pickup dependencies must be closed by the existing governed game-build pipeline, not assumed to come from loose developer files.
- The handoff at worktree root can contain unrelated older historical entries. This document is the authoritative scoped G5 contract.

## UX design contract — Item definition vs world instance
### Item definition (shared, reusable, project owned)
- Use a simple, discoverable Edit Item or Create Item panel reachable from the ordinary Gameplay > Pickup Inspector AND Inventory settings. Do not add another separate Inventory Designer application.
- Primary fields: **Item Name**, **Inventory Icon** with ADD/CHANGE graphic from local PNG/JPG via the existing governed import route, **Type** (e.g. Consumable/Equipment/Material/Key), and, if Consumable, **Effect** (None/Restore Health) and **Health Restored** numeric field.
- **Advanced > Inventory Properties** contains Maximum per Stack and optional Weight per Item. All items use individual maximumStack, not the inventory screen's columns or backpack slot count. Project default may initialise new definitions; existing definitions must not change when default changes.
- The definition's stable ID is independent of display name and prefab/scene identity. Changing a name/icon/effect updates every corresponding inventory representation. Do not stamp editable copies into all scene instances.
- A visual slot image is a template *background*, not the logical item icon. Item icons follow the definition in backpack/chest/store; UI templates may style their presentation independently.
- Preserve usable defaults: equipment and unique key items maximumStack=1; consumables/materials have a creator-editable positive limit. Reject 0/overflow/unsafe file paths, show actionable messages next to fields. Icon fallbacks are obvious and never make an item unusable.
- A health effect applies when item is USED, not when it is picked up. At full health do not waste/consume a potion by default. Successful use clamps to maximum health, removes exactly one item through authorised inventory transaction and publishes a change; failed use never consumes it.

### World instance pickup (ordinary selected scene object)
- Regular Level Editor Inspector: Gameplay > Pickup: [ ] Can be collected; Item [Choose / Create / Edit]; Quantity (positive integer); Collection Method (Press E by default / On Touch); Remove when collected (default yes); optional distance only for E interaction.
- Users should NOT need to understand Wicked metadata tags, Lua script names, internal container IDs or Story Flow routes.
- Turning on Can be collected is separate from the generic metadata preset. Preserve all pre-existing object metadata and gameplay components; use existing editor command / undo history, valid scene serialization and prefab override semantics.
- An object without a valid linked item definition must show a clear setup error and must NOT disappear or award an undefined item.
- Placed instances referencing the same item share its icon, health effect and stack policy, but can have different pickup quantities. Per-instance overrides should be explicit.
- Any suitable mesh/model can be a pickup. Model graphics and inventory icon need not look the same.

## Runtime gameplay contract
1. Load/validate the project item catalogue only for inventory-enabled projects; resolve by stable ID and protect imported resource paths.
2. On proximity interaction, show an accurate prompt; Press E or On Touch only when gameplay input permits (no conflict with modal Inventory Screen). Pick the eligible nearest target deterministically.
3. Attempt InventoryService::Add(player container, definition ID, quantity) via the existing capability policy/gateway. If capacity/weight/permissions deny it, retain object with a readable outcome; never partially add or silently destroy object.
4. Only after SUCCESS mark world instance collected, hide/remove its scene object and record that identity in governed Save Game state. Collection must survive save/reload; preserve scene authorship and normal object selection inside Studio.
5. Use / consume applies effect through the existing health/player authority, clamps amount, then removes one stack unit transactionally. Avoid bypassing combat/damage/health services or inventing an alternative player-health variable.
6. Regular props and projects without inventory have zero new pickup overhead or unintended input capture. Explicit opt-in stays enforced.
7. Respect renamed/replaced scene assets, prefab clones, duplicate IDs, single click vs touch, inventory-open input, full inventory, deleted/missing definitions, stale revisions and scene transitions.
8. Ensure project item catalogue, icon resources, world scene bindings, inventory snapshots and used-gameplay dependencies are included by the existing Build Game/asset registry, with bounded paths and safe reads.

## Stack-limit behaviour — first implementation on branch
- EngineBridge/include/renegade/bridge/InventoryService.h now exposes UpdateMaximumStack(id, newLimit).
- EngineBridge/src/InventoryService.cpp rejects zero/unknown IDs and rejects reduction below ANY existing stack's quantity across ALL containers. No loss, no revision/event mutation on failure. Successful change updates the definition and emits DefinitionUpdated; accepted value remains in the existing snapshot format. Exact equality is a no-op.
- Tests/InventoryCoreTests.cpp adds rejection, unchanged revision/snapshot, increase, restore and safe decrease coverage.
- Windows x64 Release RenegadeInventoryCoreTests build and CTest PASS on 2026-10-27. This is infrastructure only; no editor stack-size textbox is implemented yet. A creator-friendly action should translate rejection to a message such as 'Some stacks hold 8 items; reduce or split them before setting max to 5.'
- Do not auto-split existing stacks until stable-instance identity/equipment links and capacity implications are designed and tested. Reject safely first.

## Implementation order / owner checkpoints
**G5A:** Versioned, project-owned item catalogue using existing native project authoring/transactions, editor Create/Edit/Select Item, icon import/preview, item type and maximum stack. Do not misuse inventory runtime snapshot as the editable authoring source. Round-trip, undo/redo and validation tests; no silent changes to existing save data.
**G5B:** Selected-scene-object Pickup Inspector using native scene command stack/metadata or a more appropriate existing component, correct serialization/prefab/save, opt-in runtime E/touch pickup into InventoryService with atomic failure handling. Owner live interaction review.
**G5C:** Consumable Restore Health effect and use action integrated with actual player-health authority; icon parity across backpack/chest/store, package and save/reload, fallback/missing-item cases. Full Release Studio/Runtime build and packaged-game end-to-end check.

## Minimum acceptance matrix
- A normal rock remains a prop unless Can be collected is explicitly enabled.
- Place potion model; assign shared health potion definition with PNG icon, +50 healing, stack 10, quantity 3; press E, verify one world object retires and 3 potions appear in backpack with correct icon; transfer to chest and back; displayed name/icon match.
- A second potion world object shares the definition without duplicating icon/effect settings; changing item icon updates both representations.
- From 70/100 HP, use potion once -> 100/100 and quantity decreases by 1 (not +50 beyond cap); at 100/100 default use fails without decrement.
- Full inventory and weight/permission denial leave object untouched and player inventory unchanged.
- Item maximum stack 3 -> 10 succeeds and survives Save/Reopen; lowering to 2 with a stack holding 3 fails with clear message, no mutation.
- E and touch honour chosen method and do not cause duplicate collection in the same frame; inventory open consumes E as designed.
- Undo/Redo of pickup properties and item catalogue edits, renamed item, reused prefab instance, archived save, missing icon, missing item ID, malformed catalogue, fresh project without inventory.
- Standalone Build Game bundles all required item definitions, icon assets and scene references; launch and collect after restarting Runtime.
- Measure input interaction frame/latency and ensure no new UI Designer slowdowns. Owner visual checks before merge.

## Source safety and delivery
- Work only in inventory isolated worktree. Preserve dirty G4 work, owner Screens, standalone candidate exes and independently dirty WickedEngine submodule. No reset/clean.
- Prefer existing bridge services, native scene command history, governed asset/resource import and Build Game providers; no independent second inventory authority.
- Build and test exact targets; include CTest commands and actual evidence. Run Studio and Runtime on DESIGN for visual validation, but describe what has and has not been interactively tested.
- Stop at review checkpoint. No commits to main, merges or pushes without owner approval. In final report explain creator workflow in 3 simple steps and list unresolved acceptance risks.

## 2026-10-27 G5A/G5B backend implementation checkpoint — VERIFIED, UI NOT DELIVERED
- Added native project-owned versioned item catalogue: EngineBridge/include/renegade/bridge/InventoryItemCatalogueService.h and src/InventoryItemCatalogueService.cpp. Existing DocumentEnvelope plus WriteTransactionalDocument protect atomic writes; schema 1 stores ID, name, kind (Consumable / Equipment / Material / Key Item), icon Content path, stack maximum, weight, equipment association, effect (None / Restore Health) and health restored, as well as catalogue default stack maximum. Existing Runtime InventoryService still owns stacks.
- UpsertInventoryAuthoringItem edits or creates by stable item ID without changing prior item data on rejected edits. Strict duplicate IDs, project ownership, safe icon paths, malformed file, future schema, unsupported effects, zero stack size, type/effect mismatch, file-size/record limits are rejected.
- Added explicit pickup authoring contract InventoryPickupAuthoringService.h/.cpp. It is stored in native wi::scene::MetadataComponent typed properties, on the scene object or the imported model's stable creator root, using an Undo/Redo SetInventoryPickupConfigCommand. Captures enabled, item ID, quantity, Press E/On Touch, remove after collection and interaction radius. Existing unrelated typed metadata survives. The old generic Wicked Metadata > PICKUP semantic preset alone remains *not collectable*.
- New RenegadeInventoryItemCatalogueTests and RenegadeInventoryPickupAuthoringTests, included in Tests/CMakeLists.txt and linked EngineBridge. Item catalogue test proves write/read, replace without changed identity, resource path preservation, invalid edit atomicity, corrupt/missing/wrong-project rejection. Pickup test proves entity-root targeting, semantic preset separation, native Undo/Redo, invalid input rejection, unrelated metadata preservation and actual WISCENE archive serialization round-trip.
- Existing UpdateMaximumStack() (G5 first slice) safely rejects attempts to reduce below currently occupied stack quantities and uses existing version-1 InventoryService serialization for changes.
- Windows x64 Release RenegadeInventoryItemCatalogueTests and RenegadeInventoryPickupAuthoringTests builds LINK PASS. Combined 5/5 focused tests PASS: InventoryCore, InventoryItemCatalogue, InventoryPickupAuthoring, InventoryGateway, RuntimeInventorySession. No unrelated Dirty WickedEngine mutations, no merge/push/commit.

**OPEN / DO NOT CLAIM COMPLETE:**
- No Item Definition UI is wired to the 3D Level Editor or Inventory Inspector yet. The new item catalogue is native data + service only; there is no owner-usable Change Icon or Max Stack creator textbox yet.
- No Gameplay > Pickup Inspector controls are wired to SetInventoryPickupConfigCommand yet; even though the command is valid and persists, ordinary developers cannot configure world pickups through the UI.
- There is no Runtime E/touch collection, no health effect use/consumption, no shared icon lookup in gameplay, no packaged catalogue/icon dependency closure or managed world pickup save-state yet.
- No final Studio/Runtime rebuild, no live visual check or packaged Test Game acceptance for G5. Never equate engine/unit tests with these missing proofs.

**NEXT MANDATORY INTEGRATION STEPS:**
1. Wire intuitive G5A item catalogue editing in an existing native Inspector, using existing governed image import. Associate a selected world entity with a shared item ID using a picker; don't force typing IDs.
2. Wire G5B Can be collected, quantity, Press E/On Touch, Remove after collection to the new undoable scene command in the ordinary 3D Inspector; ensure widgets don't collide with existing metadata/material/objects panels.
3. Bind Runtime E/touch interaction to existing authorised InventoryGameplayGateway/InventoryService only. At full inventory retain object; no duplicate successful pickup.
4. Wire successful item USE to native health authority and exact-one inventory Remove; no consume if full health by default.
5. Resolve catalogues/icons/pickup scene and world-collected state via governed save and Build Game. Full owner tests and merge approval separately.

## 2026-10-27 final verification of this backend checkpoint
- Runtime x64 Release full RenegadeRuntime.exe linked at BUILD/inventory-full/Runtime/Release/RenegadeRuntime.exe; build exit 0. No new live pickup behaviour: new services are not yet called by Runtime.
- 21/21 focused inventory, Runtime screen, renderer and Story Flow audit tests passed with ctest --test-dir BUILD/inventory-full -C Release -R 'Renegade(Inventory.*|RuntimeInventorySession|RuntimeScreen|ScreenRenderer|StoryFlowGate8EOutcomeParity)Tests$' --output-on-failure.
- New G5B fixture additionally passed a real native WISCENE in-memory archive save/reopen, in addition to Undo/Redo, reusing metadata on the correct imported asset root and preserving unrelated keys.
- Scoped git diff --check passed (line-ending normalisation warnings only). Untracked new G5 sources/docs exist in the inventory feature worktree; no staging, commit, push, merge or WickedEngine edit.
- The current inventory Studio executable is running and was intentionally not closed/relinked. DO NOT claim G5 visual Inspector testing or acceptance.

## 2026-10-27 G5 Inspector candidate — Studio Release LINKED
- Added Studio/src/RenegadePickupEditor.cpp, wired in Studio/CMakeLists.txt and StudioApplication.h/.cpp. On ordinary mesh object selection, the normal right-hand 3D Inspector now includes a **PICKUP / ITEM** action beside Object Rendering. It opens a focused 560x740 native UI window without burying gameplay settings under Metadata or the Screen Designer's Advanced area.
- The new editor loads the optional project-owned Content/Gameplay/Inventory.renegade-items document or offers a new catalogue if missing. It supports choose existing shared item, new item, item name, kind (Consumable/Equipment/Material/Key), effect (None/Restore Health), health restored, individual max stack, weight, and **ADD / CHANGE ICON** via local PNG/JPG file picker. Icon bytes are checked, copied through ProjectDocumentTransaction into Content/UI/Icons and persist when SAVE ITEM writes the versioned shared catalogue; the icon is not yet bound into the runtime inventory renderer.
- World instance configuration shows Can be collected, Pickup quantity, Press E / Touch, Remove when collected and Interaction Distance. APPLY PICKUP executes the existing SetInventoryPickupConfigCommand on the selected object's stable imported root. It refuses changed selection/project, invalid quantities and unsaved item definition. Scene undo/redo is via normal command stack.
- Editable inputs retain readable persistent descriptions rather than unlabeled numbers, and validation failures show red status text. The editor has separate explicit SAVE ITEM and APPLY PICKUP actions so editing a shared potion never silently changes each world instance's pickup quantity.
- Release Studio compiled cleanly except pre-existing StudioApplication.cpp C4834 warning. Initial link failed LNK1104 solely because original RenegadeStudio.exe remains open. To preserve its unsaved work, branch Studio/CMakeLists.txt temporarily names the standalone candidate **RenegadeStudioG5PickupCandidate.exe**, at BUILD/inventory-full/Studio/Release. Candidate Release linked successfully (30,637,056 bytes) after recompiling the isolated Pickup editor source.
- Final 3/3 InventoryCore, InventoryItemCatalogue and InventoryPickupAuthoring tests PASS after candidate build; prior broad 21/21 inventory/Runtime/StoryFlow/ScreenRenderer regression PASS. No GUI click-through or file picker visual acceptance done because existing inventory Studio and two independent waterfall review Studios are running on DESIGN; do not launch a fourth GPU editor automatically and worsen UI sluggishness.
- Owner visual test steps: close unrelated/older Studio sessions after saving, launch candidate from full path; open a project and ordinary imported mesh, select it, click PICKUP / ITEM beside Object Rendering; create or select Health Potion; set Consumable / Restore Health / 50 / max stack 10, add PNG icon, click SAVE ITEM, tick Can be collected, quantity 3, Press E, Apply Pickup, Undo/Redo, save level, reopen and check. **Do not attempt collection in Test Game yet: Runtime E/touch pickup and potion use remain unimplemented.**
- OPEN: proper icon thumbnail preview and import resource registry linkage; new authoring item catalogue Build Game packaging and runtime definition/icon resolution; live E/touch pickup input arbitration, world persistence and Health Restored effect; world sample and end-to-end owner acceptance. No merge/push to main.

## G5 Runtime E / touch collection implementation — verified checkpoint
- Added EngineBridge WorldPickupController.{h,cpp}; an opt-in, non-script native picker that indexes typed scene metadata on reachable entities with valid InventoryService item definitions. It uses deterministic nearest selection, a 2.5 m per-instance E radius, bounded touch radius, and touch latch until the player exits contact.
- Renderer prompts show display names from current InventoryService definitions. On E/touch, controller uses only an explicitly granted native InventoryGameplayGateway -> Add("player", item ID, authored quantity), which is atomic in existing InventoryService. On failure the world prop and inventory are unchanged; no partial quantity. Successful remove-after-collect retires the entity only after Add success; a retained prop with On Touch cannot replenish inventory every frame while player stands inside its trigger.
- RuntimeApplication now loads/validates Content/Gameplay/Inventory.renegade-items after the scene's existing opt-in .rinventory companion and before script binding. Changes to an existing item's name, stack max, equipment ref or weight are applied with atomic UpdateItemDefinition after pre-validating a detached inventory snapshot; new definitions are registered safely. A rejected catalogue logs an error and does not register G5 pickups.
- WorldPickupController is opened at new scene/script session, reset on scene transition, and routed through trusted native UI's already granted player container only. Pickup E input is consumed for native pickup only when a world pickup is eligible and Gameplay has keyboard focus; Inventory Screen/paused flow blocks collection. UI result status persists for roughly two seconds. Entity removal occurs BEFORE Wicked scene physics/render upload to avoid corruption.
- Added RenegadeWorldPickupControllerTests, including metadata-only not collectible, E prompt, paused/focus block, atomic failure in a full inventory, successful removal, no duplicates and On Touch latching until exit/re-entry. Expanded InventoryCoreTests to check atomic full definition edits and unchanged original snapshot on conflict.
- Release RenegadeRuntime.exe rebuilt successfully; subsequent combined tests 22/22 PASS prior to Build Game provider integration. Source passes git diff --check (line-end warnings only).
- **Important current boundary:** this is a compiled, unit-tested Runtime integration, not yet live owner-verified in Test Game. Current Runtime expects the optional scene .rinventory companion with a "player" container, by original G3 design. Projects without the companion do not silently create inventory or collect anything.
- For shipping, extended InventoryDependencyProvider.cpp to emit a reachable scene's optional G5 item catalogue plus unique referenced icon textures, with root containment and file-size checks. It refuses Build Game if icon missing, authored stack conflicts with existing snapshot, catalogue corrupt or unbounded references; unrelated inventory-free scenes unchanged. Regression InventoryDependencyProviderTests covers catalogue and icon inclusion plus failure recovery; Release provider test PASS.
- No game-save persistence of retired world entities yet; current Runtime player inventory snapshot API is an unintegrated Save Game seam. No health consumption/use gameplay or definition icon projection to UI widgets yet. No full packaged Build Game e2e proof yet. No merge/push to main.
- Test candidate Studio: BUILD/inventory-full/Studio/Release/RenegadeStudioG5PickupCandidate.exe; Runtime: BUILD/inventory-full/Runtime/Release/RenegadeRuntime.exe. All under isolated inventory feature worktree. Do not replace an open Studio without owner saving.

## Final combined verification after packaging integration
- 2026-10-27 Release RenegadeRuntime and RenegadeStudioG5PickupCandidate both LINK PASS after updated EngineBridge.
- Full focused CTest selection (Inventory.*, WorldPickupController, RuntimeInventorySession, RuntimeScreen, ScreenRenderer, StoryFlowGate8EOutcomeParity) **22/22 PASS**, elapsed 3.61 s, command in previous checkpoint. Git diff --check PASS; LF->CRLF warnings only.
- Candidate binary timestamps: Studio 2026-10-27 21:54:54; Runtime 2026-10-27 21:48:30, both from BUILD/inventory-full.
- Original RenegadeStudio.exe and other review editor processes were neither terminated nor replaced. No merge/push/main changes.
- **Not owner-accepted:** no real scene click-through/collectable pickup demonstration, no packaged standalone e2e build, no player-health consumption wiring, no Runtime icon presentation, no governed collected-world save-state or creator-friendly auto setup of inventory companion for new projects.

## G5C native health potion + inventory-icon wiring — 2026-10-27
- Runtime/src/RuntimeInventoryItemUse.h: Authoritative native RuntimeCombatState player health; consumable RestoreHealth only. Max-health clamp, no use while dead or at full health, and one unit removed via permission-scoped inventory gateway BEFORE healing. Failed removal never heals.
- Runtime/src/RuntimeApplication.cpp: right-click an occupied PLAYER backpack slot to use one health potion in standard or paged inventory mode. Other transfer/Shift+drag interactions unchanged. Hover detail RIGHT-CLICK TO USE; success or failure status displayed. Shared authored item PNG/JPG icon paths override only the dynamic item image, not template backgrounds, using existing governed ScreenRenderer resource loading.
- Tests/RuntimeInventoryItemUseTests.cpp: checks 70->100 healing, consumed quantity, full health unchanged, denied gateway, chest item denied, equipment denied, dead/no-health guard, cap and exhausted stack. Release PASS.
- Release RenegadeRuntime.exe built and linked 2026-10-27 22:07. Targeted 9/9 and broad focused 23/23 CTest suites PASS, git diff --check PASS (LF/CRLF notices only).
- Studio candidate launched successfully on DESIGN, responsive at Project Hub. Existing Inventory Grid Designer Test project opened into Story Flow without editing/saving. No in-game pickup/use, icon visual or Inspector click-through acceptance yet.
- Remaining G5: governed save-game persistence for retired world entities, packaged standalone gameplay proof, creator-friendly initial inventory companion, full owner visual acceptance. No merge/push to main; inventory feature worktree preserved.

## 2026-10-27 reconnect / owner-facing gameplay test preparation
- Reconnected to DESIGN. The headless InventoryG5GameplayProofAuthor crashed (Windows access violation 0xC0000005) without saving new scene data. Do not rerun unchanged.
- Long proof clone path failed Project Loader registry journal staging. Short BUILD/G5Play/ScreenProject.renegade opened correctly in Studio, and LevelOne displayed the original Cube model. This is the safe disposable project; existing inventory designer project remains untouched.
- Selected Cube in Studio; new PICKUP / ITEM modal opens and shows creator-facing authoring fields. Confirmed overlap regression: PICKUP / ITEM shortcut overlaps Make Character. Review Studio/src/StudioApplication.cpp layout; do not accept current Inspector positioning.
- New purely-data setup target Tests/InventoryG5ItemSetup.cpp compiles and succeeds. It writes and reopens governed item catalogue in BUILD/G5Play: health-potion Consumable, RestoreHealth 50, stack 10, weight 0.2, icon Content/UI/InventoryIcons/potion.png.
- LevelOne and LevelTwo companions updated through InventoryService: player starts with zero potions, chest has two. Original companions backed up as .before-G5. Because Studio loaded project before these external writes, reload before testing.
- Status / outstanding work recorded in BUILD/G5Play/G5-TEST-STATUS.txt. Physical world pickups and control objects were NOT successfully authored; do not claim in-game pickup test done.
- Remaining: working safe authoring of world props, scene saving, full Test Game E/touch/health/icon visual checks, world collection persistence, packaging e2e. No push/merge into main.

## 2026-10-28 G5 fixes verified on DESIGN
- Fixed Studio PICKUP/ITEM shortcut overlap by co-locating with native Metadata Preset on the same inspector row; Release RenegadeStudioG5PickupCandidateV2.exe LINK PASS. Old owner Studio preserved.
- Fixed Windows access violation in disposable scene author utility by giving Wicked a tiny native graphics window/Application and fully initialising its components before loading a WISCENE and creating primitives. Utility now accepts ONLY BUILD/G5Play, checks marker, validates existing separate shared item catalogue and inventory companion, backs up previous LevelOne.wiscene and creates a 5-prop walkable gameplay test scene. 2026-10-28 utility exit 0; output LevelOne.wiscene 18,642 bytes and backup 12,887 bytes.
- New non-destructive Tests/InventoryG5SavedSceneProof.cpp target loads the actual authored WISCENE and catalogue/companion, verifies Player Start, floor and ordinary props, and uses WorldPickupController to E collect 3 and touch collect 1 into the real InventoryService, each removing its world object while ordinary prop/chest remain. Release build/actual saved scene proof PASS.
- Shared item catalogue G5Play already created and round-trip validated: health 50, stack 10, PNG icon; player initially empty; chest 2 potions. Status/readme at BUILD/G5Play/G5-TEST-STATUS.txt.
- No foreground desktop interaction during the owner's separate Windows-game build. Actual game mouse/keyboard Test Game, V2 Inspector visual and standalone shipping package still need owner-visible verification. Don't claim visual acceptance; no main merge/push.

## 2026-10-28 continuation: owner live pickup acceptance, saved-world state and G5 Build Game closure

- On DESIGN, V2 review Studio opened BUILD/G5Play through its Hub, loaded LevelOne, and launched native DX12 Test Game at approximately 75 FPS. Runtime showed the correct "Press E to collect Health Potion" prompt; the owner subsequently confirmed "ok, that worked". This is live owner confirmation for the tested pickup path, NOT blanket acceptance of every inventory/use/packaging gate.
- New G5 saved-world foundation reuses the existing IdentityService persistent entity IDs. SetInventoryPickupConfigCommand now assigns an ID when enabling a new pickup or re-applying unchanged settings to a legacy scene without one. It preserves that ID through command Undo/Redo and real WISCENE serialization. Malformed pre-existing IDs fail closed.
- WorldPickupController now captures a bounded, versioned world-retirement payload identified by project ID and scene ID, with retired stable entity IDs; restore rejects corrupt, duplicate, unknown, mismatched and in-place resurrection cases *before* mutating the scene. Older pickups lacking stable identities still work in the live game but cannot be silently declared save-compatible until re-authored. No authored .wiscene/.rinventory is overwritten.
- WorldPickupControllerTests now exercises capture, project/scene mismatch rejection, corruption rejection, and restoration/removal in a freshly opened scene. PickupAuthoringTests exercises stable IDs, undo/redo, serialization and legacy reapply. Targeted Release 2/2 PASS.
- Extended production WindowsGameBuildProjectTests: the actual reachable level dependency graph/Windows package plan must contain the shared G5 catalogue and PNG icon, and missing referenced icons must stop the build. Initial test used the outer disposable fixture root rather than the projectRoot and failed; corrected fixture path, then both Gate4SceneFixture and WindowsGameBuildProject tests PASS 2/2. Production collector code was not changed: its G5 icon/catalogue path was already correct.
- The WORLD snapshot API is presently a *save integration seam*. It does NOT yet write/load persistent disk Save Game slots or atomically pair with InventorySession CaptureRuntimeState. Those are still open. No claims of actual save/reload, packaged standalone G5 launch, potion recovery from real damage, icon presentation, or finalized Inspector visual acceptance.
- The original V2 candidate was preserved. A V3 candidate output filename was set for the subsequent full Studio build so stable-ID authoring can be owner reviewed without overwriting V2 or existing Studio processes.
- Keep all changes isolated on branch feature/inventory-core-g1-20261026. No staging, commit, push or merge to main; independently dirty Wicked source overlay remains untouched.
