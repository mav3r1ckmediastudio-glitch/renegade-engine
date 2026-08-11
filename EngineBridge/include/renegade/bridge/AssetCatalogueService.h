#pragma once

#include "renegade/bridge/AssetBrowserService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* AssetCatalogueMetadataDocumentName =
        "AssetMetadata.renegade-assetmeta";

    struct ModelDerivedMetadata
    {
        bool known = false;
        std::uint32_t meshCount = 0;
        std::uint32_t materialCount = 0;
        std::uint32_t armatureCount = 0;
        std::uint32_t boneCount = 0;
        std::uint32_t animationClipCount = 0;
        std::uint32_t animationChannelCount = 0;
        std::uint32_t morphTargetCount = 0;
        bool skinned = false;
        bool animated = false;

        [[nodiscard]] bool operator==(
            const ModelDerivedMetadata& other) const noexcept
        {
            return known == other.known &&
                meshCount == other.meshCount &&
                materialCount == other.materialCount &&
                armatureCount == other.armatureCount &&
                boneCount == other.boneCount &&
                animationClipCount == other.animationClipCount &&
                animationChannelCount == other.animationChannelCount &&
                morphTargetCount == other.morphTargetCount &&
                skinned == other.skinned &&
                animated == other.animated;
        }
    };

    // Gate 2 deliberately keeps creator tags and derived presentation/index
    // facts out of LC01's accepted identity/provenance schema. Records are
    // keyed only by LC01 stable asset IDs, so this document cannot become a
    // parallel identity authority.
    struct AssetCatalogueMetadataRecord
    {
        StableId assetId;
        ModelDerivedMetadata model;
        std::vector<std::string> creatorTags;
    };

    struct AssetCatalogueMetadataDocument
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        std::string formatIdentifier = "renegade-asset-metadata";
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId projectId;
        std::vector<AssetCatalogueMetadataRecord> records;
    };

    struct AssetCatalogueMetadataPersistenceOptions
    {
        std::string transactionId;
        ProjectDocumentTransactionHook operationHook;
    };

    [[nodiscard]] bool ValidateAssetCatalogueMetadata(
        const AssetCatalogueMetadataDocument& document,
        std::string& error);
    [[nodiscard]] bool SerializeAssetCatalogueMetadata(
        const AssetCatalogueMetadataDocument& document,
        std::string& json,
        std::string& error);
    [[nodiscard]] bool DeserializeAssetCatalogueMetadata(
        const std::string& json,
        AssetCatalogueMetadataDocument& document,
        std::string& error);

    // Creator tags are canonicalised to trimmed lower-case ASCII for stable
    // search semantics. Model metadata is supplied by a trusted importer or
    // later governed import transaction; Gate 2 never opens/converts a model.
    [[nodiscard]] bool SetAssetCreatorTags(
        AssetCatalogueMetadataDocument& document,
        const StableId& assetId,
        std::vector<std::string> tags,
        std::string& error);
    [[nodiscard]] bool SetAssetModelDerivedMetadata(
        AssetCatalogueMetadataDocument& document,
        const StableId& assetId,
        ModelDerivedMetadata metadata,
        std::string& error);

    [[nodiscard]] bool ResolveAssetCatalogueMetadataDocumentPath(
        const std::string& projectRoot,
        std::string& documentPath,
        std::string& error);
    [[nodiscard]] ProjectDocumentTransactionResult WriteAssetCatalogueMetadata(
        const std::string& projectRoot,
        const AssetCatalogueMetadataDocument& document,
        AssetCatalogueMetadataPersistenceOptions options = {});
    // A project with no metadata document yet is valid: this returns an empty
    // version-1 document owned by expectedProjectId. Existing malformed or
    // cross-project documents fail closed.
    [[nodiscard]] bool ReadAssetCatalogueMetadata(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        AssetCatalogueMetadataDocument& document,
        std::string& error);

    enum class AssetCatalogueState : std::uint8_t
    {
        Unregistered,
        Current,
        Stale,
        Missing,
        Moved,
        Invalid,
    };

    struct AssetCatalogueEntry
    {
        std::string name;
        std::string projectRelativePath;
        AssetType type = AssetType::Unknown;
        AssetCatalogueState state = AssetCatalogueState::Unregistered;

        bool registered = false;
        StableId assetId;
        DependencyClass dependencyClass = DependencyClass::Data;

        bool importedProduct = false;
        StableId sourceAssetId;
        std::string sourceFormat;
        std::string importer;
        std::uint32_t importerVersion = 0;
        bool sourceAvailable = true;
        bool productAvailable = true;

        ModelDerivedMetadata model;
        std::vector<std::string> creatorTags;
        std::vector<StableId> dependencyAssetIds;
        std::vector<StableId> referencedByAssetIds;
    };

    struct AssetCatalogue
    {
        StableId projectId;
        std::vector<AssetCatalogueEntry> entries;
    };

    struct AssetCatalogueBuildOptions
    {
        // LC01 recovery reports recovered IDs during refresh. Passing them here
        // lets the catalogue surface a bounded Moved transition; after reopen
        // the same stable asset naturally returns to Current.
        std::vector<StableId> movedAssetIds;
    };

    struct AssetCatalogueQuery
    {
        std::string text;
        std::optional<AssetType> type;
        std::optional<AssetCatalogueState> state;
        std::string sourceFormat;
        std::optional<bool> skinned;
        std::optional<bool> animated;
        bool staticModelsOnly = false;
        // AND semantics: every requested tag must be present.
        std::vector<std::string> tags;
    };

    // Builds a deterministic creator-facing projection of project Content.
    // AssetBrowserService remains the filesystem containment/discovery layer;
    // LC01 remains the only identity/provenance authority; metadata is joined
    // strictly by stable ID. No import, scene mutation, or Wicked UI occurs.
    [[nodiscard]] bool BuildAssetCatalogue(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        const AssetRegistry& registry,
        const AssetCatalogueMetadataDocument& metadata,
        AssetCatalogue& catalogue,
        std::string& error,
        AssetCatalogueBuildOptions options = {});

    [[nodiscard]] std::vector<AssetCatalogueEntry> QueryAssetCatalogue(
        const AssetCatalogue& catalogue,
        const AssetCatalogueQuery& query);

    [[nodiscard]] const char* AssetCatalogueStateLabel(
        AssetCatalogueState state) noexcept;
}
