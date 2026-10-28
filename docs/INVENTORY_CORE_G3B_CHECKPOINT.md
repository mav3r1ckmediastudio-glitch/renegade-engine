# G3-B Inventory: Test Level projection and reusable world interaction

**Date:** 26 October 2026
**Worktree:** `C:\Users\paulw\source\repos\renegade-inventory-core-20261026`
**Branch:** `feature/inventory-core-g1-20261026`
**Status:** Partial G3-B checkpoint. The inventory **Test Level projection** and **reference world-interaction Lua** are implemented and tested. Full playable in-engine acceptance, save system, asset authoring and packaged build remain OPEN.

## Delivered

- `InventoryTestLevelSnapshot` is a UI-independent EngineBridge helper that validates and copies an optional scene's `.rinventory` and `.rinventory-access` companions into the disposable Test Level scene. It does nothing to scenes without companions.
- `TestLevelSnapshotService::Create()` invokes the inventory projection **after** copying scripts and **before** completing the Test Level snapshot. Invalid inventory data aborts and cleans up the staging session rather than loading into Runtime with silent missing items.
- Projection enforces project-root containment, regular-file + size limits (16 MiB inventory, 1 MiB policy), versioned source inventory validation, and no clobbering of already-staged companion files.
- An optional example `Examples/Inventory/InteractTransfer.lua` uses the existing governed `renegade.player.get_position`, `renegade.transform.get_world_position(self.entity)`, `renegade.ui.show_prompt`, `renegade.input.was_pressed("interact")` and `renegade.inventory.transfer` APIs. It performs a proximity-gated Player-to-container or world-to-Player transaction without a dedicated Inventory Designer or new equipment service. It requires explicitly authored source and target container names, script properties and grants. The example does not hide/disable consumed world meshes or solve multi-target input focus; these remain later UI/interaction gates.
- A real Lua 5.4 host test loads the example file, mocks existing gameplay queries, runs the lifecycle function and verifies out-of-range rejection, near-player prompt, single successful transfer, and atomic failure on a full destination. It never duplicates item ownership.

## Reference Action setup

Copy `Examples/Inventory/InteractTransfer.lua` to the project's `Content/Scripts` and attach it using the existing Action scripting workflow to a World entity. Author its script properties:

```
source_container: String = "world:herb-pickup"
target_container: String = "player"
interaction_radius: Float = 2.5
quantity: Integer = 1
prompt: String = "Press Interact to pick up"
```

Author `world:herb-pickup` and `player` containers and their items in an explicit G3-A scene inventory companion. Grant the exact script attachment ID read/withdraw to `world:herb-pickup` and read/deposit to `player`. The example remains inactive if the inventory namespace is not granted. A chest withdraw action uses `source_container="chest"` and the same target. For depositing, reverse source/target and grants. Source container remains authoritative; the Lua example never owns or duplicates its content.

The sample is a **reference**, not a shipped, one-click item authoring tool. Project setup is not automated in UI yet. Actions interact with physical entity location but a full owner Test Game has **not** been executed; do not represent this as owner-accepted pickup gameplay.

## Packaged Runtime limitation

Build Game uses a typed dependency graph, Asset Registry and an explicit `alwaysInclude` list. In the current prototype, `.rinventory` and `.rinventory-access` are **not automatically discovered** as dependencies. For controlled packaging experiments the project can use the existing `data:` always-include declarations for each applicable scene companion; the packaged copy must preserve the expected relative path under `GameData/Content/Scenes`. Production work must add a typed, tested dependency-provider contract, verify the registry and source digest, prevent missing scenes or policy files, and run an actual packaged launch. These conditions are not yet proven.

## Persistence limitation

There is no existing general Renegade Runtime save-slot authority: the default Save/Load UI says a save-game module must be configured. G1 snapshot save/load is an isolated low-level serialization tool; G3-A Runtime mutates **memory**, not authored scene snapshots. No checkpoint here asserts surviving a real game restart. Save/load must ultimately use a governed game-save module with project/saveDataId/scene identity, atomic slot commits and rollback, not overwrite authoring companions.

## Test evidence (DESIGN, MSVC VS2022 x64)

```powershell
cmake -S Tests/InventoryCoreStandalone -B BUILD/inventory-g1 -G "Visual Studio 17 2022" -A x64
cmake --build BUILD/inventory-g1 --config Release --parallel 4
ctest --test-dir BUILD/inventory-g1 -C Release --output-on-failure
```

The existing CMake cache points at the active parent worktree's pinned Wicked LUA headers and built Lua library for isolated compile/test only. **7/7 Release CTest PASS** (G1 core, G2 gateway, Lua, G3-A Runtime session, Lua host, G3-B Test Level projection, reference interaction). The actual `TestLevelSnapshotService.cpp`, `RuntimeApplication.cpp`, and `RuntimeScriptRuntime.cpp` compile against Wicked headers using isolated object-build harness targets. This is not a full Renegade Studio/Runtime link or render check.

## Remaining G3-B gates

1. Replace temporary authoring snapshots and grants with stable-ID, versioned project documents using existing editor transactions; connect authoring through Studio's current scripting/Inspector, not a new editor.
2. Add explicit Script/Scene inventory companion dependency discovery to existing Build Game graph and project asset-registry validation. Package smoke with and without inventory; verify no silent omission.
3. Add governed Runtime save-slot authority or integrate inventory into whichever approved Save Game service becomes available; prove persistence across actual process restarts.
4. Integrate pickup target selection, one-interaction-per-press ownership, exhausted-world-item visibility/removal and container focus, using existing input/UI and existing equipment.
5. Run a real owner Test Level, standalone build, existing FPS weapon parity, save/reload, and ordinary non-inventory project regression.

**Safety:** no push, no merge, no Wicked/third-party modification, no changes to active river/UI worktrees. Independent exact-commit review required before integration.
