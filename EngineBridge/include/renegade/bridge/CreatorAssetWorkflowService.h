#pragma once

#include "renegade/bridge/AssetCatalogueService.h"
#include "renegade/bridge/ReusableAssetService.h"

#include <string>
#include <vector>

namespace renegade::bridge
{
    struct CreatorModelImportResult
    {
        bool succeeded = false;
        std::string stagedSourceProjectRelativePath;
        std::string assetProjectRelativePath;
        ReusableModelImportResult asset;
        std::string error;
    };

    // Gate 5 creator-facing orchestration. This service does not own Studio UI
    // and does not bypass LC01/LP07 services: it stages an external source into
    // project-owned SourceAssets, projects current disk state through LC01,
    // delegates import/reimport/placement to ReusableAssetService and persists
    // creator-owned catalogue tags through AssetCatalogueService.
    class CreatorAssetWorkflowService
    {
    public:
        [[nodiscard]] bool RefreshRegistryFromDisk(
            const std::string& projectRoot,
            const StableId& projectId,
            AssetRegistry& registry,
            std::string& error) const;

        [[nodiscard]] bool BuildCatalogue(
            const std::string& projectRoot,
            const StableId& projectId,
            AssetCatalogue& catalogue,
            std::string& error) const;

        [[nodiscard]] CreatorModelImportResult ImportModel(
            const std::string& projectRoot,
            const StableId& projectId,
            const std::string& externalSourcePath) const;

        [[nodiscard]] ReusableModelReimportResult ReimportModel(
            const std::string& projectRoot,
            const StableId& projectId,
            const StableId& assetId) const;

        [[nodiscard]] PreparedReusableModelPlacement PrepareModelPlacement(
            const std::string& projectRoot,
            const StableId& projectId,
            const StableId& assetId) const;

        [[nodiscard]] bool SetCreatorTags(
            const std::string& projectRoot,
            const StableId& projectId,
            const StableId& assetId,
            std::vector<std::string> tags,
            std::string& error) const;
    };
}
