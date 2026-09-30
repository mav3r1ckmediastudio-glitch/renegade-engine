#pragma once

#include "renegade/bridge/AssetCatalogueService.h"
#include "renegade/bridge/ReusableAssetService.h"

#include <string>
#include <vector>

namespace renegade::bridge
{
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
        // Normal browser presentation remains snapshot-only. If that strict
        // projection detects a real file at a missing-asset tombstone path,
        // Studio performs one authoritative LC01 recovery/persistence pass and
        // retries. Any still-ambiguous collision is quarantined as Invalid in
        // the in-memory catalogue so one stale asset cannot blank every healthy
        // browser entry or break post-import reveal.
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
