#pragma once
#include <WickedEngine.h>
#include <cmath>
namespace renegade::bridge {
struct MeleeWeaponSettings {
    float reach=2.25f,damage=12.f,contact=.5f;
    bool Valid() const {
        return std::isfinite(reach)&&reach>=.1f&&reach<=20.f&&std::isfinite(damage)&&damage>=0&&damage<=100000&&
            std::isfinite(contact)&&contact>=0&&contact<=1;
    }
};
inline constexpr char MeleeWeaponKey[]="renegade.weapon.melee";
inline void SetMeleeWeapon(wi::scene::MetadataComponent& m,const MeleeWeaponSettings& s){
    m.bool_values.set(MeleeWeaponKey,true);m.float_values.set("melee.reach",s.reach);
    m.float_values.set("melee.damage",s.damage);m.float_values.set("melee.contact",s.contact);
}
inline MeleeWeaponSettings GetMeleeWeapon(const wi::scene::MetadataComponent& m){
    return {m.float_values.get("melee.reach"),m.float_values.get("melee.damage"),m.float_values.get("melee.contact")};
}
}
