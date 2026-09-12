#pragma once

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ResourceAssetService.h"

#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr int VideoAssetBindingVersion = 1;
    inline constexpr const char* VideoAssetBindingVersionMetadataKey =
        "renegade.video_asset_binding_version";
    inline constexpr const char* VideoAssetIdMetadataKey =
        "renegade.video.asset_id";

    struct PreparedVideoAsset
    {
        StableId projectId;
        StableId assetId;
        std::string productProjectRelativePath;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        std::string payloadHash;
        std::string logicalResourceName;
        std::vector<std::uint8_t> payload;
    };

    struct VideoAssetBindingRecord
    {
        wi::ecs::Entity videoEntity = wi::ecs::INVALID_ENTITY;
        StableId videoAssetId;
    };

    struct VideoAssetRestoreResult
    {
        bool succeeded = false;
        std::size_t discovered = 0;
        std::size_t restored = 0;
        std::string error;
    };

    [[nodiscard]] bool PrepareVideoAsset(
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& videoAssetId,
        PreparedVideoAsset& prepared,
        std::string& error);

    [[nodiscard]] wi::Resource LoadPreparedVideoAsset(
        const PreparedVideoAsset& prepared,
        std::string& error);

    [[nodiscard]] bool ApplyPreparedVideoAsset(
        wi::scene::Scene& scene,
        wi::ecs::Entity videoEntity,
        const PreparedVideoAsset& prepared,
        std::string& error);

    [[nodiscard]] bool InspectVideoAssetBindings(
        const wi::scene::Scene& scene,
        std::vector<VideoAssetBindingRecord>& bindings,
        std::string& error);

    [[nodiscard]] VideoAssetRestoreResult RestoreVideoAssetBindings(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId);

    [[nodiscard]] VideoAssetRestoreResult RestorePackagedVideoAssetBindings(
        wi::scene::Scene& scene,
        const std::string& packageRoot,
        const StableId& projectId);

    class SetVideoAssetCommand final : public ICommand
    {
    public:
        SetVideoAssetCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity videoEntity,
            PreparedVideoAsset prepared);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] const StableId& AssetId() const noexcept { return prepared_.assetId; }
        [[nodiscard]] const std::string& Error() const noexcept { return error_; }

    private:
        void CaptureBefore();
        void RestoreBefore() noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity videoEntity_ = wi::ecs::INVALID_ENTITY;
        PreparedVideoAsset prepared_;
        std::string beforeFilename_;
        wi::Resource beforeResource_;
        bool beforeLooped_ = false;
        bool capturedBefore_ = false;
        bool hadMetadata_ = false;
        bool hadVersion_ = false;
        int beforeVersion_ = 0;
        bool hadAssetId_ = false;
        StableId beforeAssetId_;
        std::string error_;
    };
}
