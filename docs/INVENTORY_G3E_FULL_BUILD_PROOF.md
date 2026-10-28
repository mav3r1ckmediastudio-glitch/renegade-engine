# Inventory G3-E — Linked Runtime and production package-plan proof

**Date:** 2026-10-26
**Branch:** `feature/inventory-core-g1-20261026`
**Scope:** Production build linkage, real StoryFlow package dependency planning, bounded DX12 Runtime smoke. No new inventory editing UI, save authority or gameplay interaction is claimed.

## Environment and isolation

Full Release configuration used pinned Wicked `3a800b7134aafe58461093c8abb2e274d4e64033` and imnodes `eb36902c892548ef94f88f51ad7e7c9c7058a71c`, checked out as separate detached linked Git worktrees **inside the inventory development worktree**. The parent engine checkout's two locally modified Wicked physics sources were not touched. The available C: space was checked before building (about 79 GB, about 72 GB after). Full source build folder is `BUILD/inventory-full`. No source changes outside the isolated inventory worktree.

The existing `cmake/RenegadeNightSkyOverlay.cmake` intentionally **edits files in this isolated Wicked checkout** during CMake configure and leaves generated `renegade*.hlsli` files: this makes the isolated Wicked worktree dirty, **not** the parent engine checkout. Do not stage or merge the overlay diff or treat the submodule as clean; account for it before any future branch/release integration.

## Production fix

The full Release build initially failed in `Runtime/src/InventoryLuaBridge.cpp` with MSVC C1083 `lua.h` missing. The Lua 5.4 sources in Wicked do not export their header directory in CMake. `Runtime/S3GovernedLuaRuntime.cmake` now adds `${PROJECT_SOURCE_DIR}/WickedEngine/WickedEngine/LUA` to `RenegadeRuntimeBootstrap`'s **PRIVATE** include paths. No Lua headers are vendored or replaced, and the existing Wicked target is unmodified for this fix.

## Full Windows x64 Release build

```powershell
cmake -S . -B BUILD/inventory-full -G "Visual Studio 17 2022" -A x64 -DRENEGADE_EMBED_SHADERS=OFF
$env:CL = '/MP8'
cmake --build BUILD/inventory-full --config Release --target RenegadeRuntime --parallel 4 -- /v:minimal
```

**PASS**, linked `BUILD/inventory-full/Runtime/Release/RenegadeRuntime.exe` (approximately 47 MB). Wicked/Jolt/LUA/Utility and full EngineBridge, ScreenRenderer, RuntimeBootstrap and Runtime all built. The first uncorrected build did not pass; only the second, fixed build did. /MP8 was used for lengthy existing EngineBridge compilation. `RENEGADE_EMBED_SHADERS=OFF` is a development proof setting, **not** a distributable-shader acceptance.

## Real Build Game plan regression

Extended the pre-existing `Tests/WindowsGameBuildProjectTests.cpp` (not a mock-only new provider test), using its genuine project/StoryFlow startup fixture and `PrepareWindowsGameBuildProjectState` + `CreateWindowsGameBuildPlan` production APIs to verify:

- An inventory-free StoryFlow journey emits **zero inventory companions**.
- A real reachable Level One with valid `.rinventory` and `.rinventory-access` discovers both as **required** typed Data dependencies.
- Package-plan destination includes `GameData/Content/Scenes/LevelOne.wiscene.rinventory` and `GameData/Content/Scenes/LevelOne.wiscene.rinventory-access`.
- Unrelated Level Two does not gain invented inventory dependencies.
- Invalid permissions naming a missing container make production Build Game preparation fail.

```powershell
cmake --build BUILD/inventory-full --config Release --target RenegadeGate4SceneFixture RenegadeWindowsGameBuildProjectTests --parallel 4
ctest --test-dir BUILD/inventory-full -C Release -R "^RenegadeWindowsGameBuildProjectTests$" --output-on-failure
```

**2/2 PASS** including the required Gate 4 Scene fixture setup. Separate isolated inventory core suite `ctest --test-dir BUILD/inventory-g1 -C Release --output-on-failure`: **9/9 PASS**.

This proves the **actual production dependency collector and package plan**, but does **not yet** prove the final package-stage file copy, signed manifest, integrity digest, detached export or packaged standalone launch.

## Real executable DX12 smoke

Smoke launched the newly linked Runtime with explicit `--project`, `dx12`, two `--flow-outcome=level.complete` outcomes, `--renegade-smoke-autoplay` and `--renegade-smoke-exit`. The built-in LP03 valid StoryFlow fixture reached **Complete Game**, `status=PASS`, `exit_code=0`, `graphics_backend=DX12`, `graphics_capability=STARTED`, `smoke_status=PASS`, with both Level One and Level Two traversed.

Created a **disposable copy in BUILD only** at `BUILD/inventory-full/Runtime/Release/Content/Inventory-Proof`, with valid hand-authored schema V1 Scene inventory and empty permissions companions on both reachable levels (Player contains three potions; chest contains two). The real linked DX12 executable launched this copied project and again returned `status=PASS`, `exit_code=0`, reached StoryFlow **Complete Game**. This checks runtime compatibility with inventory-enabled Scene companions; it **does not independently verify** live inventory quantities, Lua transfers, pickup/chest presentation or saves in the running game. Those remain gameplay acceptance tasks.

The development runtime's `Run-LP03-Screen-Proof.cmd` is provided in its Release folder for a correct project launch. A separate owner screenshot showed `PACKAGE_INTEGRITY_FAILED/MISSING_MANIFEST` after the developer `RenegadeRuntime.exe` was opened **without `--project`**. We inspected the actual `unknown-package/Logs/RuntimeBootstrap.log` and confirmed `package_relative_launch=true`, missing `package-manifest.json`, package root pointing to the raw developer Release folder. This is **expected for direct developer-executable launch**, not an inventory regression or failing valid package manifest. Do **not** place a bogus package manifest in that folder.

## Open acceptance gates

1. Real complete Build Game package staging, integrity hash/manifest and detached packaged launch with and without inventory, including a valid policy and script.
2. Playable Player -> pickup -> chest interaction, focus arbitration, exhausted pickup visual changes and actual in-game visible inventory UI in existing UI Designer.
3. Save Game integration with project/saveDataId/scene/player identity and real persistent disk-slot recovery. Current memory snapshot adapter is **not** an accepted save module.
4. Full editor Studio link, owner visual Test Game inspection and equipment/weapon regressions.
5. Independent exact-commit verification and approved merge; no push or merge in this checkpoint.

**Safety:** No modifications to active main, river, UI Designer or equipment worktrees. The generated Build folder contains only disposable proof projects, and no user's levels were overwritten.
