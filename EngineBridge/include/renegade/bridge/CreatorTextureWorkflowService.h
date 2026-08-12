#pragma once

#include "renegade/bridge/ResourceAssetService.h"

#include <cstdint>
#include <string>

namespace renegade::bridge
{
    struct CreatorTextureImportResult
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

    // Studio-facing staging boundary for the first creator-consumed LP08
    // resource class. The external image is copied byte-for-byte into the
    // canonical SourceAssets/Textures folder, then Gate 2 remains the sole
    // authority that creates the governed .rasset, LC01 provenance and
    // resource-derived metadata transaction.
    class CreatorTextureWorkflowService
    {
    public:
        static constexpr std::uint64_t MaximumCreatorTextureBytes =
            512ull * 1024ull * 1024ull;

        [[nodiscard]] CreatorTextureImportResult ImportTexture(
            const std::string& projectRoot,
            const StableId& projectId,
            const std::string& externalSourcePath) const;
    };
}
