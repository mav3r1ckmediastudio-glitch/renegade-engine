# Equipped-weapon ammo counter

In the UI Designer, choose Elements > BASIC > Ammo counter, or Gameplay > COMBAT > Ammo counter - equipped weapon. Add it to the HUD assigned to the project or Level. One counter follows the possessed player's active weapon across the existing number-key slots 1–9. No per-weapon UI connection or scripts are required.

The stock counter has five ordinary editable layers: the transparent parent, weapon name, loaded rounds, magazine capacity and active slot. Select a layer in the hierarchy to change its font, size, colour, position or binding. The parent can be moved/resized with normal UI controls. Add artwork or a background if desired; the default has no background or border. In the editor without live gameplay, the labels are authored placeholders, not simulated ammunition.

The readout observes the existing active Player View firearm owner. Accepted shots reduce its loaded count, completed reloads refill it, and existing weapon switching retains the loaded count for each slot. The counter hides when there is no equipped firearm, including melee, spell and holstered presentations. An empty magazine remains visible as zero. Capacity is the weapon assembly's Loaded capacity setting. It is not reserve ammunition: the player weapon system does not yet model finite reserve ammo or ammunition pickups.

## Bindings and scripting

| Source | Meaning |
| --- | --- |
| ammo.available | Whether an equipped firearm provides ammunition |
| ammo.loaded | Current loaded rounds |
| ammo.capacity | Authored magazine capacity |
| ammo.reloading | Active native reload presentation |
| equipment.name | Active weapon name |
| equipment.slot | Active slot, numbered 1–9 |
| equipment.slots | Existing formatted loadout list |

The C++ bridge exposes PlayerWeaponAmmo and MakeAmmoCounterPrefab / InsertAmmoCounterPrefab. Runtime's CapturePlayerWeaponAmmo reads the same active weapon for both native HUD bindings and the host-authorised Lua callback. Creator scripts can read a detached value:

```lua
local ammo = renegade.player.weapon_ammo()
if ammo.available then
    -- ammo.name, ammo.slot, ammo.loaded, ammo.capacity, ammo.reloading
end
```

When unavailable, loaded/capacity/reloading are absent rather than fabricated. Changing the returned Lua table does not change ammunition. The query takes no arguments. There is no reserve field and no new ammunition owner.

Stock insertion explicitly selects the possessed-player binding and creates all layers in one history operation. Saving and reopening the Screen retains the links. Arbitrary portable UI package adoption keeps its existing ownership policy: gameplay bindings require reassignment in the destination. Stock creation does not relax that policy.

## Local verification

RenegadeScreenAmmoPrefabTests checks transparent defaults, ordinary-layer persistence and one-step Undo/Redo, all nine active-slot snapshots, empty magazines, melee/spell/holster hiding, read-only C++ and governed Lua queries. The existing PlayerViewRig, EquipmentActionState, RuntimeUiGameplaySnapshot, ScreenLibrary, ScreenHealthPrefab, ScreenRenderer and WorldLocation tests pass in the combined Release checkout. Native gameplay and Studio evidence is recorded separately after the visual checks. Debug, exported-package gameplay and hosted release checks are separate gates.
