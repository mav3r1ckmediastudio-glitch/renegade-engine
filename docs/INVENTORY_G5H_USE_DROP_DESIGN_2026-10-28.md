# Inventory G5H — Use, Take and World Drop

Checkpoint: 28 October 2026
Branch: feature/inventory-core-g1-20261026 (isolated). Do not stage, commit, push or merge without owner approval.

## Implemented for owner testing

The existing native gameplay inventory now supports a compact, transient right-click item menu. Backpack: Use, Drop One, Drop Stack. Chest: Take One, Take Stack. Existing E/left-click one-item transfers, drag, Shift+drag, arrow/Tab focus and I open/close remain intact. Esc closes an open context menu before normal pause. Menu buttons are added to an in-memory Screen copy, never the developer's saved Screen, and they use responsive geometry for outside-click hit testing.

Use invokes the existing authenticated health-potion gameplay handler, only when the item definition explicitly supports RestoreHealth. Other effects remain unsupported; menu does not invent equipment or scripted effects. KeyItem world drops are blocked by the item policy. A Drop action validates the current player stack, checks for a usable ground ray near the player's feet and creates a visible, stationary neutral-green sphere pickup in the native scene. The world controller registers its stable ID, item ID and exact quantity before removing the item through InventoryGameplayGateway; denied withdrawal removes the new entity and registry entry. E collects the object using the existing pickup controller, and a full backpack leaves it in the world. No silent item deletion or recreation of contents.

The world pickup state is now version 2, recording retired authored world IDs and dynamic drop IDs, quantities and positions; version 1 remains readable. Quick-save's existing outer single-file checksum and project/scene checks continue to wrap the data. On Quick Load, dynamic worlds are validated, then restored to a freshly loaded scene before the saved inventory snapshot is applied. Quick-save remains DIRECT-SCENE only. Story Flow multi-level resume is not implemented.

## Test evidence

Release Runtime built successfully.
Dedicated WorldPickupControllerTests (native graphics initialized) cover drop quantities, denied withdrawal rollback, save/restore with preflight, v1 backward compatibility, corrupted input rejection, re-collection exactly once and refusal to re-collect into a full backpack.
RuntimeScreenTests cover menu buttons hidden/unfocusable, focus and keyboard dispatch when enabled, and disabled focus after dismissal.
InventoryItemContextActionsTests cover player/chest action eligibility, nonmutating denied drop/unsupported use and KeyItem protection.
RuntimeGameSaveSlotTests use a real v2 dynamic world-drop record to verify atomic checksum-preserved round-trip.
Production Windows Build Game stage G5H V3: 18 dependencies, 37 governed files, manifest/identity/integrity PASS and staged executable matching rebuilt Release Runtime. Path:
BUILD/G5PackagePlay/Builds/Windows/.renegade-staging/Renegade G5 Packaged Inventory Proof Windows Build.g5h-use-drop-20261028-v3/G5InventoryProof.exe
Focus on BUILD/g5h-post-fullbackpack-regressions.log for latest full targeted CTest.

## Acceptance / limitations

**DO NOT MARK OWNER-SIGNED-OFF YET.** Live DX12 packaged application launched and rendered at 75 FPS, but manual right-click/menu/drop/collect was not visually confirmed because DESIGN's keyboard/mouse were actively being used in Discord/Edge. A targeted background WM_KEYDOWN/WM_RBUTTONDOWN injection did not affect Wicked physical input and is not evidence of gameplay acceptance. Latest clean V3 DX12 test PID 28304 may remain running in background, from unrelated TEMP working directory, with isolated user-data directory.

For the final live acceptance: bring the V3 packaged window forward, press E to collect three potions, I to open inventory, right-click backpack slot to inspect menu, use Drop One and Drop Stack separately, press I to close and E to collect the green world item, then right-click chest slot and test Take One/Stack, test full-health Use refusal, and in direct-scene Test Game use F5/F9 around a dropped item. Verify old mouse E/drag behavior remains unchanged. Report screenshots/counts and any failure.

The current default visual is a stationary green spherical world pickup; creator-selected 3D prefab/model assignment, physics impulse/rigidbody and fully general Use effects (equipment, materials, scripted custom actions) are not implemented. Direct-scene Save Game supports dynamic drops; Story Flow multi-level save/resume remains separate. Four-config CI and final merge are pending owner approval.

The follow-on full UI Designer audit should start after this G5H acceptance or user direction; do not merge without permission.
