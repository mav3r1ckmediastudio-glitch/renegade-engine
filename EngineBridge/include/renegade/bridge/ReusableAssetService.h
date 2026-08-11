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

    // Gate 4 is deliberately stable-ID driven. Callers identify the registered
    // reusable product; the source path, product path, importer/backend,
    // versions, format and settings are resolved from LC01 + the accepted
    // .rasset manifest. No caller-supplied path or format can redirect reimport.
    struct ReusableModelReimportRequest
    {
        std::string projectRoot;
        StableId projectId;
        StableId assetId;
    };

    struct ReusableModelReimportOptions
    {
        std::string transactionId;
        ProjectDocumentTransactionHook operationHook;
    };

    struct ReusableModelReimportResult
    {
        bool succeeded = false;
        StableId sourceAssetId;
        StableId assetId;
        std::string sourceProjectRelativePath;
        std::string assetProjectRelativePath;
        ImportedProductStatus statusBefore;
        ImportResult import;
        ModelDerivedMetadata modelMetadata;
        std::string previousProductHash;
        std::string productHash;
        ProjectDocumentTransactionResult transaction;
        std::string error;
    };

    // UI-independent reusable-asset lifecycle boundary. First import creates
    // stable identity; Gate 4 reimport resolves and replays only the stored
    // accepted recipe, retaining those IDs and transactionally replacing the
    // last-good product only after conversion and validation succeed.
    class ReusableAssetService
    {
    public:
        [[nodiscard]] ReusableModelImportResult ImportModelAsset(
            const ReusableModelImportRequest& request,
            ReusableModelImportOptions options = {}) const;

        [[nodiscard]] ReusableModelReimportResult ReimportModelAsset(
            const ReusableModelReimportRequest& request,
            ReusableModelReimportOptions options = {}) const;
    };
}
