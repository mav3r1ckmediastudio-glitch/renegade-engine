#pragma once

#include "renegade/bridge/ResourceAssetService.h"

#include <cstdint>
#include <string>

namespace renegade::bridge
{
    struct CreatorVideoImportResult
    {
        bool succeeded = false;
        bool committed = false;
        StableId assetId;
        StableId sourceAssetId;
        std::string sourceProjectRelativePath;
        std::string assetProjectRelativePath;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        ResourceAssetImportResult asset;
        std::string error;
    };

    // Studio-facing external-video staging boundary. The selected creator file
    // is retained under SourceAssets/Video and imported into the authoritative
    // LP08 Content/Video .rasset product. Scene authoring persists only the
    // governed product StableId; arbitrary external paths are never scene truth.
    class CreatorVideoWorkflowService
    {
    public:
        static constexpr std::uint64_t MaximumCreatorVideoBytes =
            4ull * 1024ull * 1024ull * 1024ull;
        static constexpr std::uint32_t MaximumNameAttempts = 1024;

        [[nodiscard]] CreatorVideoImportResult ImportVideo(
            const std::string& projectRoot,
            const StableId& projectId,
            const std::string& externalSourcePath) const;
    };
}
