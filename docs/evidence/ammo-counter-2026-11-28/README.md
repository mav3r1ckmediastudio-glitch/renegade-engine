# Equipped ammo counter native evidence, 28 November 2026

Windows x64 Release, DX12 on DESIGN. A disposable copy of the existing nine-weapon Kit 1 review project received the stock counter through InsertAmmoCounterPrefab. Original weapon assets and owner projects were preserved. The default parent background and border are transparent.

The controlled native sequence showed M416 30/30, one accepted mouse shot to 29/30, switching through AKS74U / MicroUzi / Benelli / Beretta_M9 / R90 / Strike / SVD / TT in slots 2–9, and returning to M416 at 29/30. Those eight review weapons currently author capacity 6; this is their actual source value, not a UI preset. Native snapshots of the HUD area are included. The earlier return capture belongs to the completed first switch sequence; the other captures were refreshed in a subsequent run. A later six-second reload capture was interrupted by a focus change and is not counted as independent end-to-end reload evidence. Existing native PlayerViewRig reload tests and the ammo adapter tests pass.

Locally rebuilt Release targets: EngineBridge, TextureCook, RuntimeBootstrap, ScreenRenderer, Runtime and normal Studio. Focused checks passed: ScreenAmmoPrefab, RuntimeUiGameplaySnapshot, PlayerViewRig, EquipmentActionState, ScreenLibrary, ScreenHealthPrefab, ScreenRenderer and WorldLocation. Source contracts: Phase5Gate3, CharacterWorkflow, EditorPointLookup and StudioUiPolicy pass. New counter tests cover all nine active readouts, transparent default, insert history and persistence, empty/melee/spell/holster states, and read-only governed Lua access.

This records local review, not Debug, packaged gameplay, hosted CI or release approval.
