# Inventory G3-C — Optional Build Game dependency closure + save adapter

**Date:** 26 October 2026
**Branch:** `feature/inventory-core-g1-20261026`
**Status:** Local code and isolated automated tests passing; **full Build Game stage, packaged Runtime launch, Studio Test Game and owner acceptance remain OPEN**.

## What was implemented

`InventoryDependencyProvider` is a new typed, opt-in `IDependencyProvider` in EngineBridge. The production `WindowsGameBuildProjectService::CollectCurrentDependencies` now registers it alongside existing Project, StoryFlow, Scene, scene-identity, resource and reusable-asset providers. The collector only invokes it for **reachable Scene nodes**. If a Scene has no `.rinventory` or access policy, it emits no dependencies and the project remains inventory-free. If a Scene has a snapshot, it emits an explicitly required `DependencyClass::Data` edge for `<scene>.rinventory`, and a second required Data edge for `<scene>.rinventory-access` when that policy exists. The existing collector and Asset Registry remain authoritative for project-relative path validation, source digest, stable asset record and package staging.

The provider validates root containment, bounded regular files, the snapshot's versioned `InventoryService` schema, and the G3-A access-policy grammar (declared container IDs, read/deposit/withdraw flags, script owner consistency, DEFINE and CREATE permissions). It fails closed on a policy without snapshot, corrupt snapshot, invalid access policy, or paths outside the project root. No mandatory inventory file is introduced in ordinary projects.

`RuntimeInventorySession` gains `CaptureRuntimeState(bytes,error)` and `RestoreRuntimeState(bytes,error)`. This is a **payload-only handoff** to a future governed Save Game service. Capturing never writes a file; restoring validates the complete snapshot before mutating the live service, preserves existing gateway references, and emits the pre-existing `Restored` event for subscribers to refresh. Neither operation creates a new save-slot manager, invents a user profile, writes to `<scene>.rinventory`, or attempts to infer a project/saveDataId/scene identity from an unsafe path.

## Focused tests and build checks

- New `Tests/InventoryDependencyProviderTests.cpp` exercises absent companions, missing snapshots with permissions, required typed Data candidates, valid/invalid permissions, unreachable Scene isolation and corrupt snapshots.
- Updated `Tests/RuntimeInventorySessionTests.cpp` performs a Player -> chest transfer, captures a live save payload, validates failed restore atomicity, resets Runtime, opens a fresh authoring Scene state and restores the captured Player/chest quantities **without changing the authored snapshot**.
- MSVC VS2022 x64 Release: **8/8 isolated CTest PASS**. Debug: **2/2** focused provider and session tests PASS.
- Object-only compilations of modified `WindowsGameBuildProjectService.cpp`, `RuntimeApplication.cpp`, `RuntimeScriptRuntime.cpp` and `TestLevelSnapshotService.cpp` against existing parent-worktree Wicked headers: PASS.
- Tests do **not** prove a complete EngineBridge/Studio/Runtime link, executable launch, managed game-save file transaction, packaged resource loading, original weapon parity, or a visually accepted Player inventory.

## Remaining gates

1. Run a fully linked Windows Build Game and check **actual** `GameData/Content/Scenes/<name>.wiscene.rinventory` and optional policy closure, hash/registry parity, published content manifest and packaged Runtime execution for inventory-enabled and inventory-free game builds.
2. Promote prototype snapshot/policy companions to stable-ID project-owned inventory assets and edit them through existing Inspector and UI Designer/StoryFlow contracts (no separate Inventory Designer). Remove duplicate G3-A temporary-parser semantics by consolidating the policy validation in one shared service.
3. Introduce a governed Save Game authority with project/saveDataId, scene ID, player identity, slot selection, rollback and recovery; call the payload-only Runtime inventory hook from that service. Do not overwrite authored companions.
4. Add real gameplay interaction arbitration and exhausted-world-object presentation, then complete owner Test Game with Player pickup, chest, save/reload and equipment regression matrix.

**Source safety:** all changes isolated to this branch. Never merge or push without exact-head review and runtime/package proof.
