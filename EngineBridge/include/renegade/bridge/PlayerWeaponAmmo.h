#pragma once
#include <string>
namespace renegade::bridge
{
    // Read-only presentation of the active weapon. Reserve is deliberately absent
    // until the player ammunition owner models it. Slot zero means no loadout slot.
    struct PlayerWeaponAmmo
    {
        bool available=false, equipped=false, reloading=false;
        unsigned slot=0;
        std::string name;
        int loaded=0, capacity=0;
    };
}
