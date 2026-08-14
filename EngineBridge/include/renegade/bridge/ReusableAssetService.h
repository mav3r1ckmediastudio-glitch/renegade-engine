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

    // Gate 5 placement is stable-ID driven for the same reason reimport is.
    // The creator selects a registered product; its path is resolved from
    // LC01 and its embedded WISCENE payload is loaded without consulting or
    // converting the original FBX/GLTF source.
    struct ReusableModelPlacementRequest
    {
        std::string projectRoot;
        StableId projectId;
        StableId assetId;
    };

    struct ReusableModelPlacementResult
    {
        bool succeeded = false;
        StableId sourceAssetId;
        StableId assetId;
        std::string assetProjectRelativePath;
        ImportedSceneSummary sceneSummary;
        ImportedModelEvidence modelEvidence;
        std::string error;
    };

    class PreparedReusableModelPlacement
    {
    public:
        PreparedReusableModelPlacement() = default;
        PreparedReusableModelPlacement(PreparedReusableModelPlacement&&) noexcept = default;
        PreparedReusableModelPlacement& operator=(PreparedReusableModelPlacement&&) noexcept = default;
        PreparedReusableModelPlacement(const PreparedReusableModelPlacement&) = delete;
        PreparedReusableModelPlacement& operator=(const PreparedReusableModelPlacement&) = delete;

        [[nodiscard]] bool IsReady() const noexcept
        {
            return scene_.IsValid() && result_.succeeded && result_.error.empty();
        }

        [[nodiscard]] const ReusableModelPlacementResult& Result() const noexcept
        {
            return result_;
        }

        [[nodiscard]] const wi::scene::Scene* PeekScene() const noexcept
        {
            return scene_.IsValid() ? scene_.get() : nullptr;
        }

        [[nodiscard]] wi::scene::Scene* PeekMutableScene() noexcept
        {
            return scene_.IsValid() ? scene_.get() : nullptr;
        }

        [[nodiscard]] wi::allocator::shared_ptr<wi::scene::Scene>
        ReleaseScene() noexcept
        {
            return std::move(scene_);
        }

    private:
        friend class ReusableAssetService;

        wi::allocator::shared_ptr<wi::scene::Scene> scene_;
        ReusableModelPlacementResult result_;
    };

    // UI-independent reusable-asset lifecycle boundary. First import creates
    // stable identity; Gate 4 reimport resolves and replays only the stored
    // accepted recipe, retaining those IDs and transactionally replacing the
    // last-good product only after conversion and validation succeed. Gate 5
    // placement resolves that same stable product and deserializes its accepted
    // payload without invoking a format converter.
    class ReusableAssetService
    {
    public:
        [[nodiscard]] ReusableModelImportResult ImportModelAsset(
            const ReusableModelImportRequest& request,
            ReusableModelImportOptions options = {},
            PreparedModelImport preparedModel = {}) const;

        [[nodiscard]] ReusableModelReimportResult ReimportModelAsset(
            const ReusableModelReimportRequest& request,
            ReusableModelReimportOptions options = {}) const;

        [[nodiscard]] PreparedReusableModelPlacement PrepareModelAssetPlacement(
            const ReusableModelPlacementRequest& request) const;
    };
}
