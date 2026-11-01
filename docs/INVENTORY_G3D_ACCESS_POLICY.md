# Inventory G3-D — One authority for script access policies

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Status:** Shared-policy integration verified with isolated Windows Debug/Release tests; full-engine build, packaged launch, scene authoring and owner acceptance are not claimed.

## Why this gate

Earlier G3-A/G3-C code separately parsed `.rinventory-access` in:
- `RuntimeInventorySession::OpenForScene` (authorising creator Lua);
- `InventoryDependencyProvider::Discover` (Build Game dependency closure);
- `SnapshotInventoryCompanions` (Test Level projection initially checked only file size).

That could allow Build Game to accept a permission policy that Runtime interpreted differently, or Test Level to stage permissions Runtime could not load. A malformed or ambiguous document should fail before gameplay starts and should behave identically in every path.

## Implemented

- Added `EngineBridge/include/renegade/bridge/InventoryAccessPolicy.h` and `EngineBridge/src/InventoryAccessPolicy.cpp`: one, UI-independent versioned decoder with a bounded file reader.
- `ParseInventoryAccessPolicy(bytes, inventory, result, error)` produces an **atomic** vector of `InventoryAccessGrant` records; on failure it clears output and explains the rejection.
- Version 1 grants include `CONTAINER` (read, deposit, withdraw), `DEFINE` and `CREATE`. Enforces existing container references, valid permission flags and positive read rights for writes, exact per-script owner consistency, safe identities, 1 MiB document/4096-row/1024-byte-row limits, and no unknown or trailing fields.
- Duplicate CONTAINER grants, repeated DEFINE or multiple CREATE namespaces for one script are explicitly rejected (instead of last-write-wins). Each script may receive separate grants for multiple different containers.
- Runtime consumes these parsed grants and instantiates the existing context-scoped `InventoryGameplayGateway` objects. It still checks the script instance/owner ID at runtime; parsing alone grants no permissions.
- Build Game and Test Level call the **same validated policy**. Test Level now rejects invalid access policies before creating disposable inventory copies, rather than allowing malformed permissions through to Runtime.
- Registered shared implementation in `RenegadeEngineBridge`, standard CMake tests and isolated standalone test harness. No new editor, inventory default, equipment controller or game-save manager.

## Verification (Windows DESIGN, MSVC 19.44 / Visual Studio 2022)

- Release standalone build + CTest: **9/9 PASS**.
- Debug policy, Test Level projection, dependency provider and Runtime session CTest: **4/4 PASS**.
- `RenegadeInventoryAccessPolicyTests`: valid multiple-scoped grants, malformed version, missing containers, illegal write-only grants, conflicting duplicates, conflicting owners, unknown/trailing fields, and cleared outputs after failure.
- `RenegadeInventoryDependencyProviderTests`, `RenegadeInventoryTestLevelProjectionTests`, `RenegadeRuntimeInventorySessionTests`: all explicitly reject the *same* duplicate-grant policy.
- Prior G1/G2/G3-C behavior retained. Modified production translation units `RuntimeInventorySession.cpp`, `InventoryDependencyProvider.cpp`, `InventoryTestLevelSnapshot.cpp` compile as real standalone-test sources.
- The previous Debug harness initially stalled on a test-only out-of-bounds vector read; this was corrected and **both configurations re-run successfully**.

## Open work before user-visible acceptance

1. A complete linked EngineBridge/Runtime/Studio build, with the required pinned Wicked and imnodes dependencies available within the isolated worktree, rather than object-only or standalone tests.
2. Package-manifest and packaged-game launch checks with inventory-enabled and inventory-free scenes, actual script grants, and stable project/scene identity.
3. Proper save-slot ownership/identity, persistent transaction journalling and rollback; current `CaptureRuntimeState/RestoreRuntimeState` provides only a payload.
4. Editable inventory screen in **existing UI Designer**, Script/Inspector authoring, in-world interaction arbitration and pickup mesh presentation; no separate Inventory Designer.
5. Full gameplay/weapon regression and owner visual acceptance.

**Safety:** local isolated inventory branch only. No merge or push. Active editor, equipment, river and UI Designer worktrees are not edited.
