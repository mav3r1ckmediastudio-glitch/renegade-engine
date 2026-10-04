#pragma once
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/PlayerViewGripService.h"
#include <memory>
namespace renegade::bridge {
struct FirstPersonAssemblyPair {
    std::string action = "Idle";
    unsigned armsClip = 0, weaponClip = 0;
};
struct FirstPersonAssemblySettings {
    StableId armsAssetId, weaponAssetId;
    std::string parentBonePath;
    XMFLOAT3 weaponPosition = {};
    XMFLOAT4 weaponRotation = {0,0,0,1};
    XMFLOAT3 cameraPosition = {};
    XMFLOAT4 cameraRotation = {0,0,0,1};
    std::vector<FirstPersonAssemblyPair> pairs;
};
bool SerializeFirstPersonAssemblySettings(const FirstPersonAssemblySettings&, std::string&, std::string&);
bool ParseFirstPersonAssemblySettings(const std::string&, FirstPersonAssemblySettings&, std::string&);
class FirstPersonAssemblyService {
public:
    // Private native scenes only. No part product or editor world is mutated.
    bool Prepare(const std::string& root, const StableId& project,
        const FirstPersonAssemblySettings&, wi::scene::Scene& result, std::string& error) const;
    ModelDerivedMetadata Describe(const wi::scene::Scene&) const;
    bool Pose(wi::scene::Scene&, const std::string& action, float seconds, std::string& error) const;
    bool ReadSettings(const std::string& root, const StableId& project,
        const StableId& assembly, FirstPersonAssemblySettings&, std::string& error) const;
    bool Save(const std::string& root, const StableId& project, const std::string& name,
        const FirstPersonAssemblySettings&, const std::vector<std::uint8_t>& thumbnail,
        StableId& asset, std::string& error) const;
};
}
