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

    // Creator-facing orchestration. This service does not own Studio UI and
    // does not bypass LC01/LP07/LP08 services: it stages external sources into
    // project-owned SourceAssets, delegates governed import/reimport/placement,
    // and persists creator-owned catalogue state.
    class CreatorAssetWorkflowService
    {
    public:
        [[nodiscard]] bool RefreshRegistryFromDisk(
            const std::string& projectRoot,
            const StableId& projectId,
            AssetRegistry& registry,
            std::string& error) const;

        // Fast creator-facing projection of the last committed LC01 state.
        // Unlike BuildCatalogue(), this never rescans or hashes Content/SourceAssets;
        // Studio uses it for normal browser presentation and immediate post-import
        // reveal so a committed stable ID cannot be replaced by a recovery pass.
        [[nodiscard]] bool BuildCatalogueSnapshot(
            const std::string& projectRoot,
            const StableId& projectId,
            AssetCatalogue& catalogue,
            std::string& error) const;

        // Explicit disk-recovery projection. This retains the existing LC01 moved /
        // missing / stale refresh semantics for lifecycle checks and recovery flows.
        [[nodiscard]] bool BuildCatalogue(
            const std::string& projectRoot,
            const StableId& projectId,
            AssetCatalogue& catalogue,
            std::string& error) const;

        [[nodiscard]] CreatorModelImportResult ImportModel(
        const std::string& projectRoot,
        const StableId& projectId,
        const std::string& externalSourcePath,
        const std::string& settingsJson = "{}",
        const std::string& assetName = {},
        const std::string& destinationFolder = "Content/Models",
        PreparedModelImport preparedModel = {},
        const std::string& thumbnailSourcePath = {},
        PreparedReusableModelPlacement* preparedPlacement = nullptr) const;

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
