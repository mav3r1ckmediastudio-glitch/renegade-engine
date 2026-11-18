#pragma once

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ResourceImportService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* ResourceAssetExtension = ".rasset";
    inline constexpr const char* ResourceAssetFormat =
        "renegade-resource-rasset";
    inline constexpr const char* ResourceAssetPayloadFormat =
        "wicked-resource-filedata";
    inline constexpr const char* ResourceAssetImportSettingsSchema =
        "renegade-resource-import-settings";
    inline constexpr const char* ResourceAssetMetadataDocumentName =
        "ResourceAssetMetadata.renegade-resourcemeta";

    struct ResourceAssetDerivedMetadata
    {
        bool known = false;
        std::uint64_t byteCount = 0;
        bool dimensionsKnown = false;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t mipCount = 0;

        [[nodiscard]] bool operator==(
            const ResourceAssetDerivedMetadata& other) const noexcept
        {
            return known == other.known &&
                byteCount == other.byteCount &&
                dimensionsKnown == other.dimensionsKnown &&
                width == other.width &&
                height == other.height &&
                mipCount == other.mipCount;
        }

        [[nodiscard]] bool operator!=(
            const ResourceAssetDerivedMetadata& other) const noexcept
        {
            return !(*this == other);
        }
    };

    struct ResourceTextureVariant
    {
        std::string semantic;
        std::string hash;
        std::uint64_t length = 0;
    };

    struct ResourceAssetManifest
    {
        static constexpr std::uint32_t ResidentSchemaVersion = 1;
        static constexpr std::uint32_t CurrentSchemaVersion = 2;

        std::string formatIdentifier = ResourceAssetFormat;
        std::uint32_t schemaVersion = ResidentSchemaVersion;
        StableId projectId;
        StableId assetId;
        StableId sourceAssetId;
        ResourceClass resourceClass = ResourceClass::Unknown;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        std::string importer = "wicked.resourcemanager";
        std::uint32_t importerVersion = 1;
        std::string settingsSchema = ResourceAssetImportSettingsSchema;
        std::uint32_t settingsVersion = 1;
        std::string settingsJson = "{}";
        std::string payloadFormat = ResourceAssetPayloadFormat;
        std::string payloadHash;
        ResourceAssetDerivedMetadata derived;
        std::vector<ResourceTextureVariant> textureVariants;
    };

    struct ResourceAssetDocument
    {
        ResourceAssetManifest manifest;
        std::vector<std::uint8_t> payload;
        std::vector<std::vector<std::uint8_t>> cookedTextures;
    };

    bool ReadTextureCookSemantic(const std::string& recipeJson, std::string& semantic,
        std::string& error);

    bool CookResourceTextureVariants(ResourceAssetDocument& document, std::string& error);

    struct ResourceAssetMetadataRecord
    {
        StableId assetId;
        ResourceClass resourceClass = ResourceClass::Unknown;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        ResourceAssetDerivedMetadata derived;
    };

    struct ResourceAssetMetadataDocument
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        std::string formatIdentifier = "renegade-resource-asset-metadata";
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId projectId;
        std::vector<ResourceAssetMetadataRecord> records;
    };

    [[nodiscard]] bool ValidateResourceAssetDocument(
        const ResourceAssetDocument& document,
        std::string& error);
    [[nodiscard]] bool SerializeResourceAssetDocument(
        const ResourceAssetDocument& document,
        std::vector<std::uint8_t>& bytes,
        std::string& error);
    [[nodiscard]] bool DeserializeResourceAssetDocument(
        const std::vector<std::uint8_t>& bytes,
        ResourceAssetDocument& document,
        std::string& error);
    struct ResourceAssetPayloadSpan
    {
        std::uint64_t offset = 0;
        std::uint64_t length = 0;
    };

    [[nodiscard]] bool ReadResourceAssetDocument(
        const std::string& path,
        ResourceAssetDocument& document,
        std::string& error,
        ResourceAssetPayloadSpan* span = nullptr);

    [[nodiscard]] bool ValidateResourceAssetMetadata(
        const ResourceAssetMetadataDocument& document,
        std::string& error);
    [[nodiscard]] bool SerializeResourceAssetMetadata(
        const ResourceAssetMetadataDocument& document,
        std::string& json,
        std::string& error);
    [[nodiscard]] bool DeserializeResourceAssetMetadata(
        const std::string& json,
        ResourceAssetMetadataDocument& document,
        std::string& error);
    [[nodiscard]] bool ReadResourceAssetMetadata(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        ResourceAssetMetadataDocument& document,
        std::string& error);

    struct ResourceAssetImportRequest
    {
        std::string projectRoot;
        StableId projectId;
        std::string sourceProjectRelativePath;
        std::string assetProjectRelativePath;
        ResourceSourceFormat expectedFormat = ResourceSourceFormat::Unknown;

        // Import persists the canonical settings so reimport can replay the accepted recipe.
        std::string settingsJson = "{}";
        // Empty for resident-only consumers. Material import supplies the selected slot.
        std::string materialTextureSemantic;
    };

    struct ResourceAssetImportOptions
    {
        std::string transactionId;
        ProjectDocumentTransactionHook operationHook;
        AssetIdGenerator generateId = GenerateStableId;
    };

    struct ResourceAssetImportResult
    {
        bool succeeded = false;
        StableId sourceAssetId;
        StableId assetId;
        ResourceClass resourceClass = ResourceClass::Unknown;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        std::string sourceProjectRelativePath;
        std::string assetProjectRelativePath;
        std::string sourceHash;
        std::string productHash;
        ResourceAssetDerivedMetadata derived;
        ProjectDocumentTransactionResult transaction;
        std::string error;
    };

    // Gate 4 is stable-ID driven. Callers identify the governed product only;
    // LC01 provenance and the accepted registry state resolve the retained
    // source, product path, class, format, importer and version-1 recipe.
    //
    // IMPORTANT: callers must refresh LC01 from disk before invoking reimport.
    // The source AssetRecord content hash is deliberately required to describe
    // the bytes being replayed; ReimportResourceAsset will not silently absorb
    // an edit that LC01 has not observed. A missing product is recoverable when
    // its retained source is active and LC01 still owns the product tombstone.
    struct ResourceAssetReimportRequest
    {
        std::string projectRoot;
        StableId projectId;
        StableId assetId;
    };

    struct ResourceAssetReimportOptions
    {
        std::string transactionId;
        ProjectDocumentTransactionHook operationHook;
    };

    struct ResourceAssetReimportResult
    {
        bool succeeded = false;
        bool recoveredMissingProduct = false;
        StableId sourceAssetId;
        StableId assetId;
        ResourceClass resourceClass = ResourceClass::Unknown;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        std::string sourceProjectRelativePath;
        std::string assetProjectRelativePath;
        ImportedProductStatus statusBefore;
        std::string previousProductHash;
        std::string sourceHash;
        std::string productHash;
        ResourceAssetDerivedMetadata derived;
        ProjectDocumentTransactionResult transaction;
        std::string error;
    };

    // UI-independent governed-resource lifecycle boundary. First import creates
    // stable identity; Gate 4 reimport resolves and replays only the stored
    // accepted recipe, retaining source/product IDs. An active product is
    // replaced only after last-good validation; a deleted product can be
    // recreated from its LC01 tombstone plus intact retained source. Product,
    // registry and resource metadata always commit through one transaction.
    class ResourceAssetService
    {
    public:
        [[nodiscard]] ResourceAssetImportResult ImportResourceAsset(
            const ResourceAssetImportRequest& request,
            ResourceAssetImportOptions options = {}) const;

        [[nodiscard]] ResourceAssetReimportResult ReimportResourceAsset(
            const ResourceAssetReimportRequest& request,
            ResourceAssetReimportOptions options = {}) const;
    };
}
