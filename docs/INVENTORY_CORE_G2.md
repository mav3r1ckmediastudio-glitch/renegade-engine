# Optional Inventory Core — G2 Scoped C++ and Lua Gateway

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Base:** G1 commits `6fc68f7`, `ba9582f`
**Status:** C++ permissions and event journal + real Lua 5.4 binding validated in isolated MSVC Release tests. Modified `RuntimeScriptRuntime.cpp` compiles as an object against Wicked headers. No active-game Runtime host binding or package proof yet.

## Contract and boundaries

- `InventoryService` is still optional and the only authoritative store. It now writes bounded revision-stamped change journal records after successful register/create/add/remove/transfer/restore operations. No event is published on failure.
- `ChangesSince(cursor, out)` lets many consumers observe updates without consuming each other's history. There is a maximum 1024-change retention; an overrun returns false and callers must reread authoritative state. A restore emits `Restored` to invalidate cached screens.
- `InventoryGameplayGateway` is a scoped C++ access facade that owns no inventory. Trusted Runtime code explicitly grants read, deposit, withdraw permissions per container. Reads are copies, not mutable backing references.
- Scripts can only create item definitions with `AllowItemDefinitions(true)`. Scripted container creation requires a trusted namespace prefix granted using `AllowContainerCreation(prefix)`; successful creation gives the creating gateway read/deposit/withdraw access to its new container. These permissions are disabled by default.
- Failed permissions never execute an item transaction. Cross-container transfers require source withdrawal and destination deposit permission. Journal reads redact unseen endpoints and suppress events from entirely private containers.
- `InventoryLuaBridge` binds `renegade.inventory` to a single `InventoryLuaContext` token. It uses Lua 5.4's existing native VM; does not open a second VM or expose arbitrary Wicked globals. All script writes go through the gateway.
- The bounded Lua contract supports `define(id,name,maxStack,weight[,equipmentId])`, `create(id[,slots,weight])`, `list(containerId[,offset,limit])`, `add(containerId,definitionId,count)`, `remove(containerId,instanceId,count)` and `transfer(from,to,instance,count)`. `list` pages up to 256 stacks and includes `total_stacks` and `next_offset`. Invalid writes return `false,reason`; invalid reads return `nil,reason` rather than raising an unhandled exception.
- `RuntimeScriptRuntime::SetInventoryGatewayProvider()` provides a trusted per-script resolver before `StartScene()`. Only successfully resolved contexts gain `renegade.inventory`. The `Instance` owns its `InventoryLuaContext`, which is invalidated on release; the runtime cannot swap the resolver while active. **This is a host wiring seam, not an already-connected Player inventory feature.**
- Existing equipment assets and input remain authoritative and unchanged. No mandatory screens, HUD or StoryFlow edits.

## Test evidence

The portable test harness compiles the **real production sources** with Windows MSVC 19.44, VS2022 x64:

```powershell
cmake -S Tests/InventoryCoreStandalone -B BUILD/inventory-g1 -G "Visual Studio 17 2022" -A x64 -DRENEGADE_INV_LUA_INCLUDE=<existing Wicked LUA include> -DRENEGADE_INV_LUA_LIBRARY=<existing LUA.lib> -DRENEGADE_INV_WICKED_INCLUDE=<existing WickedEngine include>
cmake --build BUILD/inventory-g1 --config Release --parallel 4
ctest --test-dir BUILD/inventory-g1 -C Release --output-on-failure
```

**Observed result:** 3/3 PASS: `RenegadeInventoryCoreTests`, `RenegadeInventoryGatewayTests`, and `RenegadeInventoryLuaTests`. A separate `RenegadeInventoryRuntimeSyntax` object compilation of the actual `RuntimeScriptRuntime.cpp` passes with the existing Wicked headers. The Lua test links the real Wicked LUA.lib and checks capability denial, namespace creation, definition registration, weight enforcement, reads/pagination, transfers, removals and lifecycle invalidation. Core + gateway Debug tests passed 2/2 earlier in the task.

**Not verified:** Whole-engine CMake generation/link, packaged Runtime, owner in-game test, full Lua creator-script lifecycle, equipment hooks or game-save authority. No upstream Wicked or other worktree changes.

## Next required vertical slice (G3)

1. Create an opt-in Runtime inventory owner that resolves stable Player/scene entity and project identities. Confirm persistence with the existing game-save lifecycle and project asset registry before introducing any new save authority.
2. On explicit project opt-in, construct `InventoryService` and per-script `InventoryGameplayGateway` instances, then install `SetInventoryGatewayProvider` before `StartScene`. Provide an explicit capability manifest granting script access only to owned Player bags / selected containers; never grant every container.
3. Connect Pickup and Chest gameplay actions through the governed event and input path, with one authoritative item transaction per request. Audit existing `.requipment` and do not invent a second weapon system.
4. Verify TEST GAME and standalone package data/behaviour; a simple Player + chest + 5 potions end-to-end scenario. Then the UI Designer/StoryFlow inventory screen can subscribe to the change journal.
5. Capture exact build, tests, dependency closure, save/reopen, visual evidence and independent verification before asking to merge.

G2 stays on the isolated inventory branch. No merge or GitHub push is authorised.
