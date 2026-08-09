#pragma once

#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* AssetRegistryDocumentName =
        "AssetRegistry.renegade-assets";

    struct AssetRecord
    {
        StableId assetId;
        std::string dependencyNodeId;
        std::string projectRelativePath;
        DependencyClass dependencyClass = DependencyClass::Data;
        DependencyRequirement requirement = DependencyRequirement::Required;
        std::string applicability = "windows-x64";
        std::string provider;
        std::uint32_t providerVersion = 1;
        std::string contentHash;
        bool root = false;
        bool sourceAvailable = true;
        std::vector<StableId> dependencyAssetIds;
    };

    struct AssetRegistry
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        std::string formatIdentifier = "renegade-asset-registry";
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId projectId;
        std::vector<AssetRecord> records;
    };

    struct AssetRegistryRefresh
    {
        AssetRegistry registry;
        std::vector<StableId> addedAssetIds;
        std::vector<StableId> changedAssetIds;
        std::vector<StableId> removedAssetIds;
    };

    using AssetIdGenerator = std::function<StableId()>;

    struct AssetRegistryPersistenceOptions
    {
        // Optional deterministic token and failure/interruption seam for
        // tests. Production callers normally leave both empty.
        std::string transactionId;
        ProjectDocumentTransactionHook operationHook;
    };

    // LC01 Gate 1 consumes LP05's accepted graph without touching project
    // content. Existing path-to-ID assignments are retained, content changes
    // are reported, and graph edges are projected onto stable asset IDs.
    // Runtime-support nodes are deliberately not project asset records.
    [[nodiscard]] bool RefreshAssetRegistry(
        const StableId& projectId,
        const DependencyGraph& graph,
        const AssetRegistry* existingRegistry,
        AssetRegistryRefresh& refresh,
        std::string& error,
        AssetIdGenerator generateId = GenerateStableId);

    [[nodiscard]] bool ValidateAssetRegistry(
        const AssetRegistry& registry,
        std::string& error);
    [[nodiscard]] bool SerializeAssetRegistry(
        const AssetRegistry& registry,
        std::string& json,
        std::string& error);
    [[nodiscard]] bool DeserializeAssetRegistry(
        const std::string& json,
        AssetRegistry& registry,
        std::string& error);

    // LC01 Gate 2: one authoritative registry document at the project root,
    // committed through the shared project journal/rollback boundary. A
    // later ProjectService::OpenProject() recovers interrupted writes from
    // Intermediate/Transactions before activating the project.
    [[nodiscard]] bool ResolveAssetRegistryDocumentPath(
        const std::string& projectRoot,
        std::string& documentPath,
        std::string& error);
    [[nodiscard]] ProjectDocumentTransactionResult WriteAssetRegistry(
        const std::string& projectRoot,
        const AssetRegistry& registry,
        AssetRegistryPersistenceOptions options = {});
    [[nodiscard]] bool ReadAssetRegistry(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        AssetRegistry& registry,
        std::string& error);
}
