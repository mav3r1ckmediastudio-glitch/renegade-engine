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
    inline constexpr const char* ReusableModelManagedProjectionFormat =
        "renegade-managed-asset-projection";

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

    struct ReusableModelManagedProjection
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;

        std::string formatIdentifier = ReusableModelManagedProjectionFormat;
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId projectId;
        StableId assetId;
        StableId sourceAssetId;
        std::string sourceProjectRelativePath;
        std::string assetProjectRelativePath;
        std::string sourceFormat;
        std::string importer;
        std::uint32_t importerVersion = 1;
        std::string settingsSchema = ReusableModelImportSettingsSchema;
        std::uint32_t settingsVersion = 1;
        std::string settingsJson = "{}";
        std::string payloadHash;
        ModelDerivedMetadata modelMetadata;
        std::string thumbnailProjectRelativePath;
    };

    [[nodiscard]] std::string ResolveReusableModelManagedProjectionPath(
        const std::string& assetProjectRelativePath);
    [[nodiscard]] std::string ResolveReusableModelThumbnailPath(
        const std::string& assetProjectRelativePath);
    [[nodiscard]] bool ValidateReusableModelManagedProjection(
        const ReusableModelManagedProjection& projection,
        std::string& error);
    [[nodiscard]] bool SerializeReusableModelManagedProjection(
        const ReusableModelManagedProjection& projection,
        std::string& json,
        std::string& error);

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
        std::string sourceProjectRelativePath;
        std::string assetProjectRelativePath;
        ModelSourceFormat expectedFormat = ModelSourceFormat::Unknown;
        std::string settingsJson = "{}";
        std::vector<std::uint8_t> thumbnailPngBytes;
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
        std::string managedProjectionProjectRelativePath;
        std::string thumbnailProjectRelativePath;
        ImportResult import;
        ModelDerivedMetadata modelMetadata;
        ProjectDocumentTransactionResult transaction;
        std::string error;
    };

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
        std::string managedProjectionProjectRelativePath;
        std::string thumbnailProjectRelativePath;
        ImportedProductStatus statusBefore;
        ImportResult import;
        ModelDerivedMetadata modelMetadata;
        std::string previousProductHash;
        std::string productHash;
        ProjectDocumentTransactionResult transaction;
        std::string error;
    };

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

    class ReusableAssetService
    {
    public:
        [[nodiscard]] ReusableModelImportResult ImportModelAsset(
            const ReusableModelImportRequest& request,
            ReusableModelImportOptions options = {},
            PreparedModelImport preparedModel = {},
            PreparedReusableModelPlacement* preparedPlacement = nullptr) const;

        [[nodiscard]] ReusableModelReimportResult ReimportModelAsset(
            const ReusableModelReimportRequest& request,
            ReusableModelReimportOptions options = {}) const;

        // Creator-facing placement accepts both ordinary reusable model/Character
        // products and CW-05 Character Prefabs. Prefabs resolve their stable base
        // Character Asset and return that prepared physical template with a
        // transient portable-prefab authoring marker.
        [[nodiscard]] PreparedReusableModelPlacement PrepareModelAssetPlacement(
            const ReusableModelPlacementRequest& request) const;

        // Internal accepted LP07/CW-04 backend. Character Prefab routing calls
        // this for ordinary .rasset products and for the prefab's stable base
        // Character dependency; callers outside the placement router should use
        // PrepareModelAssetPlacement().
        [[nodiscard]] PreparedReusableModelPlacement PrepareModelAssetPlacementLegacy(
            const ReusableModelPlacementRequest& request) const;
    };
}
