#pragma once

#include "renegade/bridge/AssetCatalogueService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ImportService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* ReusableAssetExtension = ".rasset";
    inline constexpr const char* ReusableModelAssetFormat = "renegade-rasset";
    inline constexpr const char* ReusableModelPayloadFormat = "wicked-wiscene";
    inline constexpr const char* ReusableModelImportSettingsSchema =
        "renegade-model-import-settings";

    struct ReusableModelAssetManifest
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        std::string formatIdentifier = ReusableModelAssetFormat;
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId projectId;
        StableId assetId;
        StableId sourceAssetId;
        std::string sourceFormat;
        std::string importer;
        std::uint32_t importerVersion = 1;
        std::string settingsSchema = ReusableModelImportSettingsSchema;
        std::uint32_t settingsVersion = 1;
        std::string settingsJson = "{}";
        std::string payloadFormat = ReusableModelPayloadFormat;
        std::string payloadHash;
    };

    struct ReusableModelAssetDocument
    {
        ReusableModelAssetManifest manifest;
        std::vector<std::uint8_t> payload;
    };

    [[nodiscard]] bool ValidateReusableModelAssetDocument(
        const ReusableModelAssetDocument& document,
        std::string& error);
    [[nodiscard]] bool SerializeReusableModelAssetDocument(
        const ReusableModelAssetDocument& document,
        std::vector<std::uint8_t>& bytes,
        std::string& error);
    [[nodiscard]] bool DeserializeReusableModelAssetDocument(
        const std::vector<std::uint8_t>& bytes,
        ReusableModelAssetDocument& document,
        std::string& error);
    [[nodiscard]] bool ReadReusableModelAssetDocument(
        const std::string& path,
        ReusableModelAssetDocument& document,
        std::string& error);

    struct ReusableModelImportRequest
    {
        std::string projectRoot;
        StableId projectId;

        // Gate 3 source-retention policy: the authoritative reimport source is
        // already project-owned below SourceAssets. Gate 5 can stage external
        // creator selections there before invoking this UI-free transaction.
        std::string sourceProjectRelativePath;

        // Permanent public reusable product below Content, for example
        // Content/Models/Knight.rasset.
        std::string assetProjectRelativePath;
        ModelSourceFormat expectedFormat = ModelSourceFormat::Unknown;

        // Version-1 conversion has no creator-tunable conversion settings yet.
        // The canonical object is nevertheless persisted now so Gate 4 can
        // replay the exact accepted recipe instead of guessing from paths.
        std::string settingsJson = "{}";
    };

    struct ReusableModelImportOptions
    {
        std::string transactionId;
        ProjectDocumentTransactionHook operationHook;
        AssetIdGenerator generateId = GenerateStableId;
    };

    struct ReusableModelImportResult
    {
        bool succeeded = false;
        StableId sourceAssetId;
        StableId assetId;
        std::string sourceProjectRelativePath;
        std::string assetProjectRelativePath;
        ImportResult import;
        ModelDerivedMetadata modelMetadata;
        ProjectDocumentTransactionResult transaction;
        std::string error;
    };

    // UI-independent Gate 3 project transaction. Conversion remains isolated
    // through ImportService. The final .rasset, LC01 registry and Gate 2
    // metadata documents are committed as one ProjectDocumentTransaction, so
    // failure cannot expose a half-import or a registry pointing at bytes that
    // were not accepted.
    class ReusableAssetService
    {
    public:
        [[nodiscard]] ReusableModelImportResult ImportModelAsset(
            const ReusableModelImportRequest& request,
            ReusableModelImportOptions options = {}) const;
    };
}
