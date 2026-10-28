# Optional Inventory Core — G0/G1 checkpoint

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Base:** `main` at `11bbb90e4214776d5e51c8bc4cba2e7f1ccde296`
**Status:** bounded C++ core implementation with focused standalone MSVC Release tests; no Runtime/Studio/Lua/HUD integration or full-engine build acceptance.

## Decisions (G0)

- An inventory is **never mandatory**. New or existing projects without inventory containers do not construct `InventoryService`, change their equipment paths, show inventory UI, or create gameplay assets.
- No separate Inventory Designer. Screen packs remain optional StoryFlow assets authored in the existing UI Designer. Independently scripted Lua/C++ overlays can work without a StoryFlow screen.
- One UI-agnostic gameplay service owns item definitions, stack instances, containers and transactions. Items can reference existing `.requipment` assets, but equipment animation/actions remain governed by the existing equipment services.
- Data ownership is distinct from UI. UI and scripted hotbars will observe inventories/equipment through explicit read-only adapters and issue validated gameplay commands. A hotbar is never an extra item container.
- Backpacks and chests are ordinary containers; trading adds economy policies; crafting/alchemy add recipes; skills, quests and map do **not** depend on inventory.
- Public Lua and C++ extension points must not create competing inventory authority. Developers remain free to implement an entirely custom provider, including a Renegade-equivalent of the owner's MAXLua radial wheel with new, original art.

## Implemented G1 surface

Files: `EngineBridge/include/renegade/bridge/InventoryService.h`,
`EngineBridge/src/InventoryService.cpp` and `Tests/InventoryCoreTests.cpp`.

- Definition: stable string ID, name, per-stack maximum, optional nonnegative per-unit weight, optional existing equipment asset ID.
- Container: explicit string ID, optional maximum number of occupied stacks, optional total-weight limit.
- Stack: durable instance ID, item-definition ID and positive count.
- Commands: `RegisterItem`, `CreateContainer`, `Add`, `Remove`, `Transfer`. All transfers and additions apply to staging copies first: failure leaves both containers and the revision unchanged.
- Full-stack transfers preserve stack identity if a new target stack is needed; splits generate a fresh identity. Compatible target stacks merge first. Deleting the last item removes its stack.
- Snapshot string format `RENEGADE_INVENTORY 1` with deterministic ordering, classic numeric locale, explicit format version, bounds and internal integrity validation. Load/restore validates before mutation; unsupported future schemas and malformed data fail closed.
- Explicit file Save/Load creates a temporary candidate, preserves a previous `.bak` on successful replacement and attempts rollback on promotion failure. File integration with Renegade's governed game-save lifecycle is **future work**.
- Container weight units are deliberately unspecified until project item semantics are authored; zero maximum is unlimited. This is not a spatial item-packing system.

## Non-goals of this checkpoint

- No auto-created inventory, item library UI, pickup/drop entities, saved prefabs, Game Build packaging, editor commands, HUD, hotbars, character equipment transitions, currency/trading/recipe systems, Lua or plugin ABI.
- No `WickedEngine` or `ThirdParty` modifications.
- No claims about Test Game, packaged Runtime, StoryFlow visual acceptance, player equip parity, or unmeasured frame time.
- No push or merge to `main`.

## Release test

Isolated portability harness: `Tests/InventoryCoreStandalone/CMakeLists.txt`.

```powershell
cmake -S Tests/InventoryCoreStandalone -B BUILD/inventory-g1 -G "Visual Studio 17 2022" -A x64
cmake --build BUILD/inventory-g1 --config Release --target RenegadeInventoryCoreTests --parallel 4
ctest --test-dir BUILD/inventory-g1 -C Release --output-on-failure
```

The standalone harness compiles the exact production `InventoryService.cpp` without Wicked or Studio. Tests verify optional-empty startup, malformed definitions, duplicate IDs, stack merge, partial/complete transfers, capacity and weight, atomic failed transfers, stable IDs, snapshot round-trip, unsupported versions, tampering, file save/reload and backup.

**Result:** MSVC Release configure PASS; build PASS; CTest 1/1 PASS after correcting a test fixture to actually fill the destination slots. `CHECK` tests execute in Release (not disabled `assert`). Full engine build/CTest cannot be claimed from this evidence: a new Git worktree has uninitialised Wicked and imnodes submodules; do not pull/copy gigabytes into it without a planned integration task.

## Next work (G2)

1. Decide how project item definitions and runtime inventory snapshots connect to the existing asset registry, game-save authority, Player/NPC identity and Build Game dependency graph. Reuse Renegade's governed document transactions rather than shipping parallel save authority.
2. Expose capability-checked C++ and governed Lua read/write surfaces, observable `InventoryChanged` events, and a custom-provider contract that does not force this implementation.
3. Exercise a Game Runtime test fixture (Player inventory + chest), then optional world pickups and equipment adapter. Verify no regression in existing shotgun/sword/shield/other accepted loadouts.
4. Only after live authority and events are proven: StoryFlow screen packs through shared UI Designer and independently scriptable hotbar/weapon wheel.

**Safety:** This branch is a separately isolated G1 implementation. Preserve all existing dirty and owner project worktrees. Do not merge or push until explicitly requested and independently reviewed.
