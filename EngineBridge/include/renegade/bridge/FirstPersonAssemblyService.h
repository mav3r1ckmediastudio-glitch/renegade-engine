#pragma once
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/PlayerViewGripService.h"
#include <memory>
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/FirearmSettings.h"
#include <array>
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
namespace renegade::bridge {
inline constexpr std::array<const char*,16> FirstPersonAssemblyActions = {
 "Idle","Reload","Walk","Run","Attack","Equip","Unequip","AimIn","AimOut",
 "AimAttack","JumpStart","JumpLoop","JumpLand","ReloadPartial","Charge","Release"};
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
    FirearmSettings firearm;
    // Schema-v2 static dual-hand presentation; v1 paired shotgun stays unchanged.
    StableId offHandWeaponAssetId;
    std::string offHandParentBonePath, primaryLayerRootPath, offHandLayerRootPath;
    XMFLOAT3 offHandWeaponPosition = {};
    XMFLOAT4 offHandWeaponRotation = {0,0,0,1};
    unsigned blockStartClip = 0, blockLoopClip = 0, blockEndClip = 0;
    // Optional authored presentation proxies, in the static attachment roots.
    bool avoidOffHand = false;
    XMFLOAT3 bladeBase = {}, bladeTip = {0,1,0};
    XMFLOAT3 shieldCenter = {}, shieldHalfExtents = {0.3f,0.3f,0.04f};
    float bladeRadius = 0.015f, maximumHandCorrection = 0.35f;
    bool IndependentHands() const noexcept { return !offHandWeaponAssetId.empty(); }
};
bool SerializeFirstPersonAssemblySettings(const FirstPersonAssemblySettings&, std::string&, std::string&);
bool ParseFirstPersonAssemblySettings(const std::string&, FirstPersonAssemblySettings&, std::string&);
struct FirstPersonPartChoice {
    StableId assetId;
    std::string label;
    bool arms = false, weapon = false;
};
// Folder-scoped choices plus explicit roles from existing assembly provenance.
std::vector<FirstPersonPartChoice> CollectFirstPersonPartChoices(const AssetRegistry&);
class SetFirstPersonAssemblySettingsCommand final : public ICommand {
public:
    SetFirstPersonAssemblySettingsCommand(FirstPersonAssemblySettings& target,
        FirstPersonAssemblySettings next);
    bool Execute() override;
    void Undo() override;
private:
    FirstPersonAssemblySettings& target_;
    FirstPersonAssemblySettings before_, after_;
};
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
    bool Update(const std::string& root, const StableId& project,
        const StableId& asset, const std::string& expectedProductHash,
        const FirstPersonAssemblySettings&, const std::vector<std::uint8_t>& thumbnail,
        std::string& error, ProjectDocumentTransactionHook hook = {}) const;
private:
    bool SaveImpl(const std::string& root, const StableId& project, const std::string& name,
        const FirstPersonAssemblySettings&, const std::vector<std::uint8_t>& thumbnail,
        StableId& asset, std::string& error, const StableId& existing,
        const std::string& expectedHash, ProjectDocumentTransactionHook hook) const;
};
}
