#pragma once

#include "renegade/bridge/AssetCatalogueService.h"
#include "renegade/bridge/ResourceAssetService.h"

#include <cstdint>
#include <string>

namespace renegade::bridge
{
    struct CreatorTextureImportResult
    {
        // succeeded means the complete import including post-commit verification
        // succeeded. committed is deliberately separate: when committed is true
        // but succeeded is false, persistent project state may already exist and
        // callers must refresh/inspect it rather than blindly retrying.
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
        static constexpr std::uint32_t MaximumNameAttempts = 1024;

        [[nodiscard]] CreatorTextureImportResult ImportTexture(
            const std::string& projectRoot,
            const StableId& projectId,
            const std::string& externalSourcePath) const;

        // Gate 3 keeps the generic LP07 catalogue contract unchanged, but the
        // creator Assets drawer needs resource-derived format truth for texture
        // filtering. Overlay accepted Gate 2 resource metadata by stable product
        // ID. Enrichment is deliberately fail-open: unreadable/missing texture
        // metadata marks affected texture entries Invalid and reports a warning,
        // but never withholds otherwise-usable models or healthy resources from
        // the creator catalogue. A true return means the catalogue remains safe
        // to present; warning may still be non-empty.
        [[nodiscard]] bool EnrichTextureCatalogue(
            const std::string& projectRoot,
            const StableId& projectId,
            AssetCatalogue& catalogue,
            std::string& warning) const;
    };
}
