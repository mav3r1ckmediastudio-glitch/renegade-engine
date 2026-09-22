#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* ReusableAssetInstanceIdMetadataKey =
        "renegade.reusable_asset_id";
    inline constexpr const char* ReusableAssetInstanceVersionMetadataKey =
        "renegade.reusable_asset_instance_version";
    inline constexpr const char* ReusableAssetPayloadRootMetadataKey =
        "renegade.reusable_asset_payload_root";
    inline constexpr const char* ReusableAssetInstanceDisplayNameMetadataKey =
        "renegade.reusable_asset_display_name";
    inline constexpr int ReusableAssetInstanceVersion = 1;

    struct ReusableAssetInstanceRecord
    {
        StableId assetId;
        wi::ecs::Entity instanceRoot = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payloadRoot = wi::ecs::INVALID_ENTITY;
    };

    [[nodiscard]] bool InspectReusableAssetInstances(
        const wi::scene::Scene& scene,
        std::vector<ReusableAssetInstanceRecord>& instances,
        std::string& error);

    [[nodiscard]] std::size_t RepairReusableAssetInstanceNames(
        wi::scene::Scene& scene) noexcept;

    // Creator-owned companion state can live outside WISCENE (notably the
    // governed .rscripts document). Placement remains one Undo/Redo operation:
    // a registered Studio companion factory applies any prefab companion state
    // after fresh scene identity exists, then supplies symmetric Undo/Redo hooks.
    struct ReusablePlacementCompanionCallbacks
    {
        std::function<void()> undo;
        std::function<bool()> redo;
    };

    using ReusablePlacementCompanionFactory = std::function<bool(
        wi::scene::Scene& scene,
        wi::ecs::Entity instanceRoot,
        wi::ecs::Entity payloadRoot,
        ReusablePlacementCompanionCallbacks& callbacks,
        std::string& error)>;

    void SetReusablePlacementCompanionFactory(
        ReusablePlacementCompanionFactory factory);
    void ClearReusablePlacementCompanionFactory() noexcept;

    class PlaceReusableModelCommand final : public ICommand
    {
    public:
        PlaceReusableModelCommand(
            wi::scene::Scene& targetScene,
            wi::allocator::shared_ptr<wi::scene::Scene> preparedScene,
            StableId assetId,
            const XMFLOAT3& placementPosition,
            float scaleFactor,
            std::string displayName = {});

        PlaceReusableModelCommand(
            wi::scene::Scene& targetScene,
            StableId assetId,
            wi::ecs::Entity existingInstanceRoot,
            wi::ecs::Entity existingPayloadRoot,
            std::size_t firstMaterialIndex,
            std::string displayName = {},
            bool preparedCharacterAsset = false);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity PlacedEntity() const noexcept;
        [[nodiscard]] wi::ecs::Entity PayloadRootEntity() const noexcept;
        [[nodiscard]] const StableId& AssetId() const noexcept;

    private:
        struct CapturedMaterialResource
        {
            wi::ecs::Entity materialEntity = wi::ecs::INVALID_ENTITY;
            std::uint32_t slot = 0;
            wi::Resource resource;
        };

        void CaptureMaterialResources(std::size_t firstMaterialIndex);
        void RestoreCapturedMaterialResources();
        [[nodiscard]] bool PromotePreparedCharacter();
        [[nodiscard]] bool ApplyPlacementCompanion();

        wi::scene::Scene* scene_ = nullptr;
        wi::allocator::shared_ptr<wi::scene::Scene> preparedScene_;
        StableId assetId_;
        std::string displayName_;
        XMFLOAT3 placementPosition_ = {};
        float scaleFactor_ = 1.0f;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payloadRoot_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        std::vector<CapturedMaterialResource> materialResources_;
        std::size_t firstMaterialIndex_ = 0;
        ReusablePlacementCompanionCallbacks companion_;
        bool companionActive_ = false;
        bool adoptExisting_ = false;
        bool promoteCharacter_ = false;
        bool hasSnapshot_ = false;
    };
}
