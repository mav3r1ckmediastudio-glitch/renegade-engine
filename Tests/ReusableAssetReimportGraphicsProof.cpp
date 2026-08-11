#include "renegade/bridge/ReusableAssetService.h"

#include <WickedEngine.h>
#include <wiJobSystem.h>

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr wchar_t WindowClassName[] = L"RenegadeLP07Gate4ReimportProofWindow";
    constexpr const char* ProjectId = "66666666-6666-4666-8666-666666666666";
    constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    LRESULT CALLBACK WindowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        if (message == WM_CLOSE)
        {
            DestroyWindow(window);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 GATE 4 PROOF FAIL // " << message << '\n';
        return false;
    }

    bool ReadBytes(const fs::path& path, std::vector<std::uint8_t>& bytes)
    {
        bytes.clear();
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;
        input.seekg(0, std::ios::end);
        const auto size = input.tellg();
        if (size < 0)
            return false;
        input.seekg(0, std::ios::beg);
        bytes.resize(static_cast<std::size_t>(size));
        if (!bytes.empty())
            input.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        return input.good() || input.eof();
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        std::uint64_t hash = FnvOffset;
        for (const auto value : bytes)
        {
            hash ^= value;
            hash *= FnvPrime;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    bool HashFile(const fs::path& path, std::string& hash)
    {
        std::vector<std::uint8_t> bytes;
        if (!ReadBytes(path, bytes))
            return false;
        hash = HashBytes(bytes);
        return true;
    }

    const renegade::bridge::AssetRecord* FindRecord(
        const renegade::bridge::AssetRegistry& registry,
        const renegade::bridge::StableId& id)
    {
        const auto found = std::find_if(registry.records.begin(), registry.records.end(),
            [&id](const auto& record) { return record.assetId == id; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const renegade::bridge::ImportedProductRecord* FindProvenance(
        const renegade::bridge::AssetRegistry& registry,
        const renegade::bridge::StableId& productId)
    {
        const auto found = std::find_if(
            registry.importedProducts.begin(), registry.importedProducts.end(),
            [&productId](const auto& record)
            { return record.productAssetId == productId; });
        return found == registry.importedProducts.end() ? nullptr : &*found;
    }

    const renegade::bridge::AssetCatalogueEntry* FindCatalogueEntry(
        const renegade::bridge::AssetCatalogue& catalogue,
        const renegade::bridge::StableId& id)
    {
        const auto found = std::find_if(catalogue.entries.begin(), catalogue.entries.end(),
            [&id](const auto& entry) { return entry.registered && entry.assetId == id; });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    const renegade::bridge::AssetCatalogueMetadataRecord* FindMetadataRecord(
        const renegade::bridge::AssetCatalogueMetadataDocument& metadata,
        const renegade::bridge::StableId& id)
    {
        const auto found = std::find_if(metadata.records.begin(), metadata.records.end(),
            [&id](const auto& record) { return record.assetId == id; });
        return found == metadata.records.end() ? nullptr : &*found;
    }

    bool PrepareProject(const fs::path& root)
    {
        std::error_code ec;
        fs::remove_all(root, ec);
        ec.clear();
        fs::create_directories(root / "Content" / "Models", ec);
        fs::create_directories(root / "SourceAssets" / "Models", ec);
        fs::create_directories(root / "SourceAssets" / "Moved", ec);
        fs::create_directories(root / "Intermediate" / "Transactions", ec);
        return !ec;
    }

    bool PersistRefresh(
        const fs::path& projectRoot,
        const renegade::bridge::StableId& sourceId,
        const std::string& sourceRelative,
        const renegade::bridge::StableId& productId,
        renegade::bridge::AssetRegistryRefresh& refresh)
    {
        using namespace renegade::bridge;
        AssetRegistry existing;
        std::string error;
        if (!Require(ReadAssetRegistry(projectRoot.generic_u8string(), ProjectId,
                existing, error), "refresh: registry read failed: " + error))
            return false;

        const auto* previousSource = FindRecord(existing, sourceId);
        const auto* previousProduct = FindRecord(existing, productId);
        if (!Require(previousSource != nullptr && previousProduct != nullptr,
                "refresh: stable source/product records are not active"))
            return false;

        std::string sourceHash;
        std::string productHash;
        if (!Require(HashFile(projectRoot / fs::u8path(sourceRelative), sourceHash),
                "refresh: source hash failed") ||
            !Require(HashFile(projectRoot / fs::u8path(previousProduct->projectRelativePath),
                    productHash), "refresh: product hash failed"))
            return false;

        DependencyNode sourceNode;
        sourceNode.id = previousSource->dependencyNodeId;
        sourceNode.projectRelativePath = sourceRelative;
        sourceNode.dependencyClass = previousSource->dependencyClass;
        sourceNode.requirement = previousSource->requirement;
        sourceNode.applicability = previousSource->applicability;
        sourceNode.provider = previousSource->provider;
        sourceNode.providerVersion = previousSource->providerVersion;
        sourceNode.contentHash = sourceHash;

        DependencyNode productNode;
        productNode.id = previousProduct->dependencyNodeId;
        productNode.projectRelativePath = previousProduct->projectRelativePath;
        productNode.dependencyClass = previousProduct->dependencyClass;
        productNode.requirement = previousProduct->requirement;
        productNode.applicability = previousProduct->applicability;
        productNode.provider = previousProduct->provider;
        productNode.providerVersion = previousProduct->providerVersion;
        productNode.contentHash = productHash;

        DependencyGraph graph;
        graph.nodes = {sourceNode, productNode};
        std::size_t generated = 0;
        const auto generator = [&generated]()
        {
            ++generated;
            return std::string("77777777-7777-4777-8777-") +
                (generated == 1 ? "777777777771" : "777777777772");
        };
        if (!Require(RefreshAssetRegistry(ProjectId, graph, &existing,
                refresh, error, generator),
                "refresh: LC01 refresh failed: " + error) ||
            !Require(FindRecord(refresh.registry, sourceId) != nullptr &&
                    FindRecord(refresh.registry, productId) != nullptr,
                "refresh: stable source/product IDs were not preserved"))
            return false;

        static std::size_t refreshSequence = 0;
        AssetRegistryPersistenceOptions options;
        options.transactionId = "lp07-gate4-refresh-" +
            std::to_string(++refreshSequence);
        const auto persisted = WriteAssetRegistry(
            projectRoot.generic_u8string(), refresh.registry, std::move(options));
        return Require(persisted.success && persisted.committed,
            "refresh: registry write failed: " + persisted.message);
    }

    bool RequireCatalogueState(
        const fs::path& projectRoot,
        const renegade::bridge::StableId& productId,
        const renegade::bridge::AssetCatalogueState expected)
    {
        using namespace renegade::bridge;
        AssetRegistry registry;
        AssetCatalogueMetadataDocument metadata;
        AssetCatalogue catalogue;
        std::string error;
        if (!ReadAssetRegistry(projectRoot.generic_u8string(), ProjectId,
                registry, error) ||
            !ReadAssetCatalogueMetadata(projectRoot.generic_u8string(), ProjectId,
                metadata, error) ||
            !BuildAssetCatalogue(projectRoot.generic_u8string(), ProjectId,
                registry, metadata, catalogue, error))
        {
            return Require(false, "catalogue projection failed: " + error);
        }
        const auto* entry = FindCatalogueEntry(catalogue, productId);
        return Require(entry != nullptr && entry->state == expected,
            "catalogue did not expose expected product source-health state");
    }

    bool AddCreatorTag(
        const fs::path& projectRoot,
        const renegade::bridge::StableId& productId,
        const std::string& tag)
    {
        using namespace renegade::bridge;
        AssetCatalogueMetadataDocument metadata;
        std::string error;
        if (!Require(ReadAssetCatalogueMetadata(projectRoot.generic_u8string(), ProjectId,
                metadata, error), "tag: metadata read failed: " + error) ||
            !Require(SetAssetCreatorTags(metadata, productId, {tag}, error),
                "tag: assignment failed: " + error))
            return false;

        AssetCatalogueMetadataPersistenceOptions options;
        options.transactionId = "lp07-gate4-tag";
        const auto written = WriteAssetCatalogueMetadata(
            projectRoot.generic_u8string(), metadata, std::move(options));
        return Require(written.success && written.committed,
            "tag: metadata write failed: " + written.message);
    }

    bool RequireCurrent(
        const fs::path& projectRoot,
        const renegade::bridge::StableId& productId,
        const renegade::bridge::StableId& sourceId,
        const std::string& expectedSourcePath,
        const std::string& expectedTag,
        const bool expectedSkinned,
        const bool expectedAnimated)
    {
        using namespace renegade::bridge;
        AssetRegistry registry;
        AssetCatalogueMetadataDocument metadata;
        std::string error;
        if (!Require(ReadAssetRegistry(projectRoot.generic_u8string(), ProjectId,
                registry, error), "verify: registry reopen failed: " + error) ||
            !Require(ReadAssetCatalogueMetadata(projectRoot.generic_u8string(), ProjectId,
                metadata, error), "verify: metadata reopen failed: " + error))
            return false;

        const auto* source = FindRecord(registry, sourceId);
        const auto* product = FindRecord(registry, productId);
        const auto* provenance = FindProvenance(registry, productId);
        if (!Require(source != nullptr && product != nullptr && provenance != nullptr,
                "verify: stable source/product/provenance relationship is missing") ||
            !Require(source->projectRelativePath == expectedSourcePath,
                "verify: source stable ID did not retain the expected current path"))
            return false;

        ImportedProductStatus status;
        if (!Require(GetImportedProductStatus(registry, *provenance, status, error),
                "verify: status failed: " + error) ||
            !Require(status.sourceAvailable && status.productAvailable &&
                    !status.sourceChanged && !status.productChanged,
                "verify: provenance is not current after successful reimport"))
            return false;

        const auto* metadataRecord = FindMetadataRecord(metadata, productId);
        if (!Require(metadataRecord != nullptr &&
                    metadataRecord->model.known &&
                    metadataRecord->model.skinned == expectedSkinned &&
                    metadataRecord->model.animated == expectedAnimated,
                "verify: derived model metadata did not update") ||
            !Require(std::find(metadataRecord->creatorTags.begin(),
                    metadataRecord->creatorTags.end(), expectedTag) !=
                    metadataRecord->creatorTags.end(),
                "verify: creator tag was not preserved"))
            return false;

        ReusableModelAssetDocument rasset;
        if (!Require(ReadReusableModelAssetDocument(
                (projectRoot / fs::u8path(product->projectRelativePath)).generic_u8string(),
                rasset, error), "verify: RAsset reopen failed: " + error) ||
            !Require(rasset.manifest.projectId == ProjectId &&
                    rasset.manifest.assetId == productId &&
                    rasset.manifest.sourceAssetId == sourceId &&
                    rasset.manifest.importer == provenance->importer &&
                    rasset.manifest.importerVersion == provenance->importerVersion &&
                    rasset.manifest.settingsJson == provenance->settingsJson,
                "verify: RAsset identity/recipe changed after reimport"))
            return false;

        return RequireCatalogueState(projectRoot, productId, AssetCatalogueState::Current);
    }

    bool CaptureAuthoritativeBytes(
        const fs::path& projectRoot,
        const std::string& productRelative,
        std::vector<std::uint8_t>& product,
        std::vector<std::uint8_t>& registry,
        std::vector<std::uint8_t>& metadata)
    {
        return ReadBytes(projectRoot / fs::u8path(productRelative), product) &&
            ReadBytes(projectRoot / renegade::bridge::AssetRegistryDocumentName, registry) &&
            ReadBytes(projectRoot / renegade::bridge::AssetCatalogueMetadataDocumentName,
                metadata);
    }

    bool RunFbxLifecycle(
        const fs::path& projectRoot,
        const fs::path& staticFixture,
        const fs::path& animatedFixture)
    {
        using namespace renegade::bridge;
        if (!Require(PrepareProject(projectRoot), "fbx: project setup failed"))
            return false;

        const std::string sourceRelative = "SourceAssets/Models/knight.fbx";
        const std::string productRelative = "Content/Models/knight.rasset";
        const fs::path sourcePath = projectRoot / fs::u8path(sourceRelative);
        std::error_code ec;
        fs::copy_file(staticFixture, sourcePath, fs::copy_options::overwrite_existing, ec);
        if (!Require(!ec, "fbx: initial source staging failed"))
            return false;

        ReusableModelImportRequest importRequest;
        importRequest.projectRoot = projectRoot.generic_u8string();
        importRequest.projectId = ProjectId;
        importRequest.sourceProjectRelativePath = sourceRelative;
        importRequest.assetProjectRelativePath = productRelative;
        importRequest.expectedFormat = ModelSourceFormat::Fbx;
        ReusableAssetService service;
        const auto imported = service.ImportModelAsset(importRequest);
        if (!Require(imported.succeeded && imported.transaction.committed,
                "fbx: initial Gate 3 import failed: " + imported.error) ||
            !Require(!imported.modelMetadata.skinned && !imported.modelMetadata.animated,
                "fbx: initial static fixture did not produce static metadata"))
            return false;

        const StableId sourceId = imported.sourceAssetId;
        const StableId productId = imported.assetId;
        if (!AddCreatorTag(projectRoot, productId, "medieval"))
            return false;

        std::vector<std::uint8_t> initialProduct;
        if (!Require(ReadBytes(projectRoot / fs::u8path(productRelative), initialProduct),
                "fbx: could not capture initial RAsset"))
            return false;

        // Real source update -> LC01 refresh -> visible Stale -> explicit
        // product-ID-only reimport -> same IDs + updated model facts.
        fs::copy_file(animatedFixture, sourcePath, fs::copy_options::overwrite_existing, ec);
        if (!Require(!ec, "fbx: animated source update failed"))
            return false;
        AssetRegistryRefresh staleRefresh;
        if (!PersistRefresh(projectRoot, sourceId, sourceRelative, productId, staleRefresh) ||
            !Require(std::find(staleRefresh.changedAssetIds.begin(),
                    staleRefresh.changedAssetIds.end(), sourceId) !=
                    staleRefresh.changedAssetIds.end(),
                "fbx: LC01 did not report the changed source stable ID") ||
            !RequireCatalogueState(projectRoot, productId, AssetCatalogueState::Stale))
            return false;

        ReusableModelReimportRequest reimportRequest;
        reimportRequest.projectRoot = projectRoot.generic_u8string();
        reimportRequest.projectId = ProjectId;
        reimportRequest.assetId = productId;
        const auto reimported = service.ReimportModelAsset(reimportRequest);
        if (!Require(reimported.succeeded && reimported.transaction.committed,
                "fbx: explicit reimport failed: " + reimported.error) ||
            !Require(reimported.assetId == productId &&
                    reimported.sourceAssetId == sourceId,
                "fbx: stable IDs changed during reimport") ||
            !Require(reimported.statusBefore.sourceChanged &&
                    !reimported.statusBefore.productChanged,
                "fbx: reimport did not observe the accepted stale state") ||
            !Require(reimported.modelMetadata.skinned && reimported.modelMetadata.animated,
                "fbx: reimport did not refresh skinned/animated metadata"))
            return false;

        std::vector<std::uint8_t> animatedProduct;
        if (!Require(ReadBytes(projectRoot / fs::u8path(productRelative), animatedProduct) &&
                    animatedProduct != initialProduct,
                "fbx: successful reimport did not replace product bytes") ||
            !RequireCurrent(projectRoot, productId, sourceId, sourceRelative,
                "medieval", true, true))
            return false;

        // Malformed conversion must perform no persistent write. The stale
        // registry produced by refresh remains authoritative and the prior
        // product/metadata bytes stay byte-identical.
        if (!Require(WriteText(sourcePath, "not-an-fbx"),
                "fbx: malformed source staging failed"))
            return false;
        AssetRegistryRefresh malformedRefresh;
        if (!PersistRefresh(projectRoot, sourceId, sourceRelative,
                productId, malformedRefresh) ||
            !RequireCatalogueState(projectRoot, productId, AssetCatalogueState::Stale))
            return false;

        std::vector<std::uint8_t> malformedProductBefore;
        std::vector<std::uint8_t> malformedRegistryBefore;
        std::vector<std::uint8_t> malformedMetadataBefore;
        if (!Require(CaptureAuthoritativeBytes(projectRoot, productRelative,
                malformedProductBefore, malformedRegistryBefore, malformedMetadataBefore),
                "fbx: could not capture last-good state before malformed reimport"))
            return false;

        const auto malformed = service.ReimportModelAsset(reimportRequest);
        std::vector<std::uint8_t> malformedProductAfter;
        std::vector<std::uint8_t> malformedRegistryAfter;
        std::vector<std::uint8_t> malformedMetadataAfter;
        if (!Require(!malformed.succeeded,
                "fbx: malformed source unexpectedly reimported") ||
            !Require(CaptureAuthoritativeBytes(projectRoot, productRelative,
                malformedProductAfter, malformedRegistryAfter, malformedMetadataAfter),
                "fbx: could not reopen state after malformed reimport") ||
            !Require(malformedProductAfter == malformedProductBefore &&
                    malformedRegistryAfter == malformedRegistryBefore &&
                    malformedMetadataAfter == malformedMetadataBefore,
                "fbx: malformed reimport mutated authoritative state"))
            return false;

        // A valid retry succeeds without identity repair.
        fs::copy_file(staticFixture, sourcePath, fs::copy_options::overwrite_existing, ec);
        if (!Require(!ec, "fbx: retry source restoration failed"))
            return false;
        AssetRegistryRefresh retryRefresh;
        if (!PersistRefresh(projectRoot, sourceId, sourceRelative, productId, retryRefresh))
            return false;
        const auto retry = service.ReimportModelAsset(reimportRequest);
        if (!Require(retry.succeeded && retry.assetId == productId &&
                    retry.sourceAssetId == sourceId,
                "fbx: valid retry failed or required identity repair: " + retry.error) ||
            !RequireCurrent(projectRoot, productId, sourceId, sourceRelative,
                "medieval", false, false))
            return false;

        // A failure after replacement has begun must roll .rasset + registry +
        // metadata back together, then permit a clean retry.
        fs::copy_file(animatedFixture, sourcePath, fs::copy_options::overwrite_existing, ec);
        if (!Require(!ec, "fbx: rollback source update failed"))
            return false;
        AssetRegistryRefresh rollbackRefresh;
        if (!PersistRefresh(projectRoot, sourceId, sourceRelative,
                productId, rollbackRefresh))
            return false;

        std::vector<std::uint8_t> rollbackProductBefore;
        std::vector<std::uint8_t> rollbackRegistryBefore;
        std::vector<std::uint8_t> rollbackMetadataBefore;
        if (!Require(CaptureAuthoritativeBytes(projectRoot, productRelative,
                rollbackProductBefore, rollbackRegistryBefore, rollbackMetadataBefore),
                "fbx: could not capture pre-rollback state"))
            return false;

        ReusableModelReimportOptions failedOptions;
        failedOptions.transactionId = "lp07-gate4-forced-rollback";
        failedOptions.operationHook = [](const ProjectDocumentTransactionStage stage,
            std::size_t,
            const std::string& path,
            std::string& error)
        {
            if (stage == ProjectDocumentTransactionStage::AfterReplace &&
                path.find("knight.rasset") != std::string::npos)
            {
                error = "intentional Gate 4 replacement rollback proof";
                return ProjectDocumentTransactionHookAction::Fail;
            }
            return ProjectDocumentTransactionHookAction::Continue;
        };
        const auto forced = service.ReimportModelAsset(
            reimportRequest, std::move(failedOptions));

        std::vector<std::uint8_t> rollbackProductAfter;
        std::vector<std::uint8_t> rollbackRegistryAfter;
        std::vector<std::uint8_t> rollbackMetadataAfter;
        if (!Require(!forced.succeeded && forced.transaction.rolledBack,
                "fbx: forced replacement failure did not roll back") ||
            !Require(CaptureAuthoritativeBytes(projectRoot, productRelative,
                rollbackProductAfter, rollbackRegistryAfter, rollbackMetadataAfter),
                "fbx: could not reopen rolled-back state") ||
            !Require(rollbackProductAfter == rollbackProductBefore &&
                    rollbackRegistryAfter == rollbackRegistryBefore &&
                    rollbackMetadataAfter == rollbackMetadataBefore,
                "fbx: transaction rollback did not restore all authoritative bytes"))
            return false;

        const auto rollbackRetry = service.ReimportModelAsset(reimportRequest);
        if (!Require(rollbackRetry.succeeded && rollbackRetry.assetId == productId &&
                    rollbackRetry.sourceAssetId == sourceId,
                "fbx: retry after rollback failed: " + rollbackRetry.error) ||
            !RequireCurrent(projectRoot, productId, sourceId, sourceRelative,
                "medieval", true, true))
            return false;

        // Moved-source recovery belongs to the SOURCE identity. The unchanged
        // RAsset therefore remains Current; the product itself is not falsely
        // labelled Moved. After changing bytes at the recovered path, the
        // product becomes Stale and reimport follows that new source path.
        const std::string movedRelative = "SourceAssets/Moved/knight-renamed.fbx";
        const fs::path movedPath = projectRoot / fs::u8path(movedRelative);
        fs::rename(sourcePath, movedPath, ec);
        if (!Require(!ec, "fbx: source move failed"))
            return false;
        AssetRegistryRefresh movedRefresh;
        if (!PersistRefresh(projectRoot, sourceId, movedRelative, productId, movedRefresh) ||
            !Require(std::find(movedRefresh.recoveredAssetIds.begin(),
                    movedRefresh.recoveredAssetIds.end(), sourceId) !=
                    movedRefresh.recoveredAssetIds.end(),
                "fbx: LC01 did not recover the moved source stable ID"))
            return false;
        const auto* movedSource = FindRecord(movedRefresh.registry, sourceId);
        if (!Require(movedSource != nullptr &&
                    movedSource->projectRelativePath == movedRelative,
                "fbx: recovered source ID did not adopt its new path") ||
            !RequireCatalogueState(projectRoot, productId, AssetCatalogueState::Current))
            return false;

        fs::copy_file(staticFixture, movedPath, fs::copy_options::overwrite_existing, ec);
        if (!Require(!ec, "fbx: moved-source content update failed"))
            return false;
        AssetRegistryRefresh movedStaleRefresh;
        if (!PersistRefresh(projectRoot, sourceId, movedRelative,
                productId, movedStaleRefresh) ||
            !RequireCatalogueState(projectRoot, productId, AssetCatalogueState::Stale))
            return false;

        const auto movedReimport = service.ReimportModelAsset(reimportRequest);
        if (!Require(movedReimport.succeeded &&
                    movedReimport.sourceAssetId == sourceId &&
                    movedReimport.assetId == productId &&
                    movedReimport.sourceProjectRelativePath == movedRelative,
                "fbx: reimport did not honour recovered source path/identity: " +
                    movedReimport.error) ||
            !RequireCurrent(projectRoot, productId, sourceId, movedRelative,
                "medieval", false, false))
            return false;

        std::cout << "LP07 GATE 4 FBX LIFECYCLE PASS // asset_id=" << productId
            << " // source_id=" << sourceId << '\n';
        return true;
    }

    bool RunGltfRegression(const fs::path& projectRoot)
    {
        using namespace renegade::bridge;
        if (!Require(PrepareProject(projectRoot), "gltf: project setup failed"))
            return false;

        const std::string gltf1 =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA\",\"byteLength\":42}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36,\"target\":34962},{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6,\"target\":34963}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
        const std::string gltf2 =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA\",\"byteLength\":42}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36,\"target\":34962},{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6,\"target\":34963}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
            "\"nodes\":[{\"mesh\":0,\"translation\":[1,0,0]}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";

        const std::string sourceRelative = "SourceAssets/Models/triangle.gltf";
        const std::string productRelative = "Content/Models/triangle.rasset";
        const fs::path sourcePath = projectRoot / fs::u8path(sourceRelative);
        if (!Require(WriteText(sourcePath, gltf1), "gltf: initial source write failed"))
            return false;

        ReusableModelImportRequest importRequest;
        importRequest.projectRoot = projectRoot.generic_u8string();
        importRequest.projectId = ProjectId;
        importRequest.sourceProjectRelativePath = sourceRelative;
        importRequest.assetProjectRelativePath = productRelative;
        importRequest.expectedFormat = ModelSourceFormat::Gltf;
        ReusableAssetService service;
        const auto imported = service.ImportModelAsset(importRequest);
        if (!Require(imported.succeeded, "gltf: initial import failed: " + imported.error))
            return false;

        const StableId sourceId = imported.sourceAssetId;
        const StableId productId = imported.assetId;
        std::vector<std::uint8_t> before;
        if (!Require(ReadBytes(projectRoot / fs::u8path(productRelative), before),
                "gltf: initial product read failed") ||
            !Require(WriteText(sourcePath, gltf2), "gltf: source update failed"))
            return false;

        AssetRegistryRefresh refresh;
        if (!PersistRefresh(projectRoot, sourceId, sourceRelative, productId, refresh) ||
            !RequireCatalogueState(projectRoot, productId, AssetCatalogueState::Stale))
            return false;

        ReusableModelReimportRequest request;
        request.projectRoot = projectRoot.generic_u8string();
        request.projectId = ProjectId;
        request.assetId = productId;
        const auto reimported = service.ReimportModelAsset(request);
        std::vector<std::uint8_t> after;
        if (!Require(reimported.succeeded && reimported.assetId == productId &&
                    reimported.sourceAssetId == sourceId,
                "gltf: reimport failed or changed identity: " + reimported.error) ||
            !Require(reimported.import.importerBackend == "wicked.gltf",
                "gltf: stored backend was not replayed") ||
            !Require(ReadBytes(projectRoot / fs::u8path(productRelative), after) &&
                    after != before,
                "gltf: successful update did not replace product bytes") ||
            !RequireCatalogueState(projectRoot, productId, AssetCatalogueState::Current))
            return false;

        std::cout << "LP07 GATE 4 GLTF REGRESSION PASS // asset_id=" << productId << '\n';
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: RenegadeReusableAssetReimportGraphicsProof "
            << "<static.fbx> <skinned-animated.fbx> <output-directory>\n";
        return 2;
    }

    const fs::path staticFixture = fs::u8path(argv[1]);
    const fs::path animatedFixture = fs::u8path(argv[2]);
    const fs::path outputRoot = fs::u8path(argv[3]);
    if (!Require(fs::is_regular_file(staticFixture), "static FBX fixture is missing") ||
        !Require(fs::is_regular_file(animatedFixture), "animated FBX fixture is missing"))
        return 3;

    std::error_code ec;
    fs::remove_all(outputRoot, ec);
    ec.clear();
    fs::create_directories(outputRoot, ec);
    if (!Require(!ec, "could not create Gate 4 proof output root"))
        return 4;

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    RegisterClassExW(&windowClass);
    const HWND window = CreateWindowExW(0, WindowClassName,
        L"Renegade LP07 Gate 4 Reimport Proof", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
    if (!Require(window != nullptr, "could not create Gate 4 graphics proof window"))
        return 5;

    wi::jobsystem::Initialize();
    int exitCode = 0;
    {
        wi::Application application;
        application.allow_hdr = false;
        application.SetWindow(window);
        if (!Require(wi::graphics::GetDevice() != nullptr,
                "Wicked graphics device was not initialized"))
        {
            exitCode = 6;
        }
        else
        {
            const bool fbxPassed = RunFbxLifecycle(
                outputRoot / "FBXProject", staticFixture, animatedFixture);
            const bool gltfPassed = fbxPassed &&
                RunGltfRegression(outputRoot / "GLTFProject");
            if (!fbxPassed || !gltfPassed)
                exitCode = 7;
        }
    }

    wi::jobsystem::ShutDown();
    DestroyWindow(window);
    fs::remove_all(outputRoot, ec);
    if (exitCode == 0)
        std::cout << "LP07 GATE 4 STABLE REIMPORT GRAPHICS PROOF PASS\n";
    return exitCode;
}
