# G3-A: Opt-in Runtime inventory session and script access

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Status:** bounded runtime-owner and host-wiring slice; standalone Windows MSVC Release 5/5 pass and modified Runtime source object compile pass. Not a full Test Game or packaged release acceptance.

## Outcome

The existing `RuntimeApplication::SyncCreatorScriptsForScene()` now uses
`RuntimeInventorySession::OpenForScene()` before starting governed Lua scripts.
The Runtime session is constructed with no inventory. If the active startup
scene has no optional `.rinventory` companion, it remains disabled and
scripts start exactly as before without an inventory namespace or store.

If a companion is explicitly present, the session loads the same
`InventoryService` G1/G2 authority from a deterministic v1 snapshot
and optionally configures script-specific capabilities. The
`RuntimeScriptRuntime` per-script resolver then exposes
`renegade.inventory` only to those exact script instance IDs with matching
script-scope owner IDs. Only authorised calls reach the authoritative
item/stack/container store. The Runtime always stops creator callbacks
before disposing the session and clears the provider at level transitions
and process exit.

## Temporary authoring companion contract

For a validated project scene `Content/Scenes/Starter.wiscene`:

- `Starter.wiscene.rinventory`: bytes from
  `InventoryService::SerializeSnapshot()`. Items and containers must be
  explicitly registered; the engine never creates a default Player backpack.
- `Starter.wiscene.rinventory-access`: optional plain-text script access
  policy, below. Without this file scripts get no inventory capability even
  when an inventory snapshot exists.

```text
RENEGADE_INVENTORY_ACCESS 1 5
CONTAINER "level-pickup-script" "-" "player" 1 1 1
CONTAINER "level-pickup-script" "-" "chest" 1 1 1
CONTAINER "chest-observer-script" "-" "chest" 1 0 0
DEFINE "level-pickup-script" "-"
CREATE "level-pickup-script" "-" "script:level-pickup:"
```

`-` means a level-scoped script; an entity-scoped script must provide the
same owner entity ID used by the governed script attachment. CONTAINER flags
are read / deposit / withdraw (each 0 or 1). DEFINE explicitly authorises
new item types. CREATE authorises containers **only** under the named prefix;
created containers receive read, deposit and withdraw permissions for that
script. Different scripts can have read-only access to the same chest.
The document is rejected if references are unknown, flags are invalid,
ownership is inconsistent, content is oversized, or a companion resolves
outside the canonical project root. Script permissions are not implicit.

**Temporary contract warning:** the authored snapshots and grants are a
G3 developer proof format, not a final asset registry, editor authoring
format or save-game authority. Later phases must integrate them with stable
project/scene IDs, governed file transactions, Build Game dependency packaging
and owner-approved editor workflows. **Do not store live game saves over the
scene authoring companion.** Runtime mutations are deliberately memory-only
in this checkpoint.

## Files added/modified

- Added `Runtime/src/RuntimeInventorySession.h/.cpp`: optional level
  inventory owner, canonical path checks, bounded companion policy parser,
  permissions mapped to exact governed script instance and owner.
- Modified `Runtime/src/RuntimeApplication.h/.cpp`: inventory session member
  outlives scripting, opt-in scene start, safe stop/reset and explicit diagnostics
  for malformed configured inventory.
- Modified `Runtime/CMakeLists.txt`: source registration.
- Added `Tests/RuntimeInventorySessionTests.cpp`: no-inventory startup,
  Player/chest permissions, transfer, Lua-definition/container rights,
  failure/rollback of malformed companions and unchanged authored snapshot.
- Added `Tests/RuntimeInventoryLuaHostTests.cpp`: scene snapshot + policy
  -> scope-authorised Lua 5.4 -> 5 Player potions -> transfer 2 to chest
  -> 3 Player/2 chest -> read-only rejection -> optional scripted definition
  and scoped container creation. Memory snapshot round-trip asserted.
- Modified `Tests/CMakeLists.txt` and the isolated harness to register
  the new tests, compile `RuntimeApplication.cpp` against Wicked headers
  without requiring a full engine reconfiguration.

## Verification on DESIGN

```powershell
cmake -S Tests/InventoryCoreStandalone -B BUILD/inventory-g1 -G "Visual Studio 17 2022" -A x64 -DRENEGADE_INV_LUA_INCLUDE=<Wicked LUA include> -DRENEGADE_INV_LUA_LIBRARY=<existing Release LUA.lib> -DRENEGADE_INV_WICKED_INCLUDE=<Wicked include>
cmake --build BUILD/inventory-g1 --config Release --parallel 4
ctest --test-dir BUILD/inventory-g1 -C Release --output-on-failure
```

**Result:** 5/5 Release tests PASS. The real
`RuntimeApplication.cpp` and `RuntimeScriptRuntime.cpp` compiled as object
targets against Wicked headers without source errors. No full Editor/Runtime
link or Test Game run, no packaged Runtime proof, and no owner visual check.

## Next (G3-B)

Integrate authoritative project item assets and Runtime save-game storage
with stable project/scene/entity IDs and authored Package dependency
declarations. Provide actual reusable Pickup/Chest behaviours through the
governed input/event routes and attach inventory access via the editor's
existing scripting/property workflows. Run the real Windows standalone and
Test Game matrix (including non-inventory FPS levels), then connect shared
UI Designer components. Preserve the existing .requipment ownership.
