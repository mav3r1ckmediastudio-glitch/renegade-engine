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
    };

    struct ResourceAssetManifest
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        std::string formatIdentifier = ResourceAssetFormat;
        std::uint32_t schemaVersion = CurrentSchemaVersion;
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
    };

    struct ResourceAssetDocument
    {
        ResourceAssetManifest manifest;
        std::vector<std::uint8_t> payload;
    };

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
    [[nodiscard]] bool ReadResourceAssetDocument(
        const std::string& path,
        ResourceAssetDocument& document,
        std::string& error);

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

        // Version 1 intentionally has no creator-tunable resource processing
        // options. The canonical recipe is persisted now so Gate 4 reimport can
        // replay an accepted recipe rather than infer behavior from file paths.
        std::string settingsJson = "{}";
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

    // UI-independent first-import transaction for the non-model resource
    // classes proven in LP08 Gate 1. The retained source remains editor-only;
    // the governed .rasset product, LC01 provenance and derived catalogue
    // metadata are committed atomically. Reimport is deliberately Gate 4.
    class ResourceAssetService
    {
    public:
        [[nodiscard]] ResourceAssetImportResult ImportResourceAsset(
            const ResourceAssetImportRequest& request,
            ResourceAssetImportOptions options = {}) const;
    };
}
