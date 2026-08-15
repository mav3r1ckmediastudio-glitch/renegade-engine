#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // Gate 6 keeps creator-authored instance state separate from the replaceable
    // .rasset payload. These keys live on ordinary Wicked MetadataComponent
    // values, so the identity survives WISCENE Save/Open without changing the
    // Wicked file format or the pinned upstream source.
    inline constexpr const char* ReusableAssetInstanceIdMetadataKey =
        "renegade.reusable_asset_id";
    inline constexpr const char* ReusableAssetInstanceVersionMetadataKey =
        "renegade.reusable_asset_instance_version";
    inline constexpr const char* ReusableAssetPayloadRootMetadataKey =
        "renegade.reusable_asset_payload_root";
    inline constexpr int ReusableAssetInstanceVersion = 1;

    struct ReusableAssetInstanceRecord
    {
        StableId assetId;
        wi::ecs::Entity instanceRoot = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payloadRoot = wi::ecs::INVALID_ENTITY;
    };

    // Reads only Renegade-owned metadata/hierarchy markers from an already
    // loaded Wicked scene. No registry access, file I/O or renderer mutation.
    [[nodiscard]] bool InspectReusableAssetInstances(
        const wi::scene::Scene& scene,
        std::vector<ReusableAssetInstanceRecord>& instances,
        std::string& error);

    // Reusable-asset placement command. Unlike direct format import, the
    // creator-facing reusable workflow needs a durable stable-ID wrapper so a
    // future packaged Runtime can refresh only the payload from the current
    // governed .rasset while preserving authored instance transform/state.
    class PlaceReusableModelCommand final : public ICommand
    {
    public:
        PlaceReusableModelCommand(
            wi::scene::Scene& targetScene,
            wi::allocator::shared_ptr<wi::scene::Scene> preparedScene,
            StableId assetId,
            const XMFLOAT3& placementPosition,
            float scaleFactor);

        // Adopt a live cursor instance without cloning, reparsing or moving it.
        // The first Execute() only stamps stable Renegade metadata and captures
        // the Undo/Redo snapshot; the visible entity is already in the scene.
        PlaceReusableModelCommand(
            wi::scene::Scene& targetScene,
            StableId assetId,
            wi::ecs::Entity existingInstanceRoot,
            wi::ecs::Entity existingPayloadRoot,
            std::size_t firstMaterialIndex);

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

        wi::scene::Scene* scene_ = nullptr;
        wi::allocator::shared_ptr<wi::scene::Scene> preparedScene_;
        StableId assetId_;
        XMFLOAT3 placementPosition_ = {};
        float scaleFactor_ = 1.0f;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payloadRoot_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        std::vector<CapturedMaterialResource> materialResources_;
        std::size_t firstMaterialIndex_ = 0;
        bool adoptExisting_ = false;
        bool hasSnapshot_ = false;
    };
}
