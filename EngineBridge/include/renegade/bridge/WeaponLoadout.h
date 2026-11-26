#pragma once
#include "renegade/bridge/IdentityService.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace renegade::bridge {
struct WeaponSlot { StableId equipment; std::string setup; };
inline std::filesystem::path WeaponLoadoutPath(const StableId& initial) {
    return std::filesystem::path("Content/Player/Loadouts")/(initial+".slots");
}
// One ordered list per starting item. An absent list preserves single-item projects.
inline std::vector<WeaponSlot> ReadWeaponSlots(const std::filesystem::path& root,const StableId& initial) {
    std::vector<WeaponSlot> slots;
    if(!IsValidStableId(initial))return slots;
    std::ifstream file(root/WeaponLoadoutPath(initial));
    std::set<std::string> seen;
    for(std::string line;slots.size()<9 && std::getline(file,line);){
        auto split=line.find('\t');
        auto id=line.substr(0,split);
        if(!IsValidStableId(id)||!seen.insert(id).second)continue;
        std::string setup=split==line.npos?"":line.substr(split+1);
        auto path=std::filesystem::u8path(setup).lexically_normal();
        if(path.has_root_path() || path.extension()!=".weapon" || path.generic_u8string().rfind("Content/Player/Weapons/",0)!=0)setup.clear();
        else setup=path.generic_u8string();
        slots.push_back({id,setup});
    }
    if(!seen.count(initial))return {{initial,{}}};
    return slots;
}
}
