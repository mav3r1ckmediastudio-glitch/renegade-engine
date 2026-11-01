# Inventory G5G — packaged Runtime final check, 28 October 2026

Status: G5G reported inventory input regression **owner accepted** ("that works now"). Branch: feature/inventory-core-g1-20261026. No staging, committing, pushing or merging into main without further owner approval.

## Fixes included
- Runtime owns inventory-specific key dispatch while the authored inventory is open: E, Enter or Space activates the focused occupied slot; arrows/Tab navigate, while world movement/combat and world E interactions are suppressed. Standard pause remains available.
- Native pointer hit-test handles LMB click, distinct from drag/Shift-drag. Clicks on icon overlays resolve to the actual slot. Duplicate Wicked native inventory-button callbacks are suppressed.
- Status label now restores the **authored instruction text** when the inventory is closed and reopened, rather than leaving stale health/transfer messages from the previous opening. The item inventory state does not change on toggle.

## Release checks performed
- Rebuilt Release Runtime and package-proof executable.
- Focused Release CTest: 27/27 PASS, 3.99 seconds. Log: BUILD/g5g-v2-final-regressions.log.
- Production Windows package discovery/staging/identity/integrity test: PASS, 18 dependencies, 37 governed files including inventory assignment, disabled editable Screen, potion PNG, catalogue and LevelOne inventory companion. Log: BUILD/g5g-v2-final-stage.log.
- New isolated package: BUILD/G5PackagePlay/Builds/Windows/.renegade-staging/Renegade G5 Packaged Inventory Proof Windows Build.g5g-input-proof-20261028-v2/G5InventoryProof.exe. Prior V1/V3 packages untouched. Launched from unrelated TEMP working directory with isolated LOCALAPPDATA, DX12, responsive at 75 FPS.
- Earlier V1 standalone on the same fixed input code: Story Flow Play -> Level One -> collected 3 health potions -> graphical inventory showed backpack x3 and chest x2; pressed E on focused chest potion -> backpack x4, chest x1, exactly one transfer and MOVED ONE ITEM. Screenshot: BUILD/G5PackagePlay/g5g-packaged-after-e.png.
- V1 live right-click on backpack potion at full health: status HEALTH FULL - POTION KEPT; count x4 unchanged. Screenshot: BUILD/G5PackagePlay/g5g-packaged-fullhealth-potion.png.
- V1 live I-close/I-open showed status remained stale; fixed in RuntimeApplication::RefreshInventoryOverlay.
- Rebuilt and restaged clean V2; screenshot after I-close/I-open shows normal authored instruction line CLICK/DRAG: 1 SHIFT+DRAG: STACK E: TRANSFER I: CLOSE, while backpack x3, chest x2 remain stable. Screenshot: BUILD/G5PackagePlay/g5g-v2-reopened.png.
- G5E native production combat-damage-to-healing integration remains green: 100 -> 27 health from actual ApplyCombatDamage, two potion uses -> 77 -> 100, with quantities 2 -> 1 -> 0. This validates gameplay logic, **not** a live wounded-character gameplay demonstration.
- No user-owned scenes or master worktree modified by staging; only isolated BUILD sandboxes.

## Remaining before branch merge
1. Owner-facing visual test of Studio V4 Project Inventory Screen selector (unit-tested, built, not yet visually owner-confirmed).
2. Manual drag/Shift+drag and consumption after taking *actual live gameplay damage*, if not already accepted; live full-health refusal is confirmed.
3. Four-configuration Studio/Runtime CI (after a commit/push is explicitly authorized), final branch review and explicit merge approval.
4. Story Flow multi-level quick-save/resume remains a separately scoped feature, not a blocker for direct-scene G5D.
