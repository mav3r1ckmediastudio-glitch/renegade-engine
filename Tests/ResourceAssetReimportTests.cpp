#include "renegade/bridge/CreatorAssetActionPolicy.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ResourceAssetService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "91111111-1111-4111-8111-111111111111";
    int failures = 0;

    void Require(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL // " << message << '\n';
        }
    }

    void WriteBytes(
        const fs::path& path,
        const std::vector<std::uint8_t>& bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!bytes.empty())
        {
            stream.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
    }

    void WriteText(const fs::path& path, const std::string& text)
    {
        WriteBytes(path, {text.begin(), text.end()});
    }

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return {
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
    }

    std::string ReadText(const fs::path& path)
    {
        const auto bytes = ReadBytes(path);
        return {bytes.begin(), bytes.end()};
    }

    bool WriteEmptyRegistry(const fs::path& root)
    {
        AssetRegistry registry;
        registry.projectId = ProjectId;
        registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
        std::string json;
        std::string error;
        if (!SerializeAssetRegistry(registry, json, error))
            return false;
        WriteText(root / AssetRegistryDocumentName, json);
        return true;
    }

    struct Fixture
    {
        ResourceClass resourceClass = ResourceClass::Unknown;
        ResourceSourceFormat format = ResourceSourceFormat::Unknown;
        std::string sourcePath;
        std::string productPath;
        std::vector<std::uint8_t> first;
        std::vector<std::uint8_t> second;
    };

    ResourceAssetImportResult ImportFixture(
        const fs::path& root,
        const Fixture& fixture)
    {
        ResourceAssetImportRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = ProjectId;
        request.sourceProjectRelativePath = fixture.sourcePath;
        request.assetProjectRelativePath = fixture.productPath;
        request.expectedFormat = fixture.format;
        return ResourceAssetService().ImportResourceAsset(request);
    }

    ResourceAssetReimportResult Reimport(
        const fs::path& root,
        const StableId& assetId,
        ResourceAssetReimportOptions options = {})
    {
        ResourceAssetReimportRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = ProjectId;
        request.assetId = assetId;
        return ResourceAssetService().ReimportResourceAsset(
            request, std::move(options));
    }

    bool RefreshCatalogue(
        const fs::path& root,
        AssetCatalogue& catalogue,
        std::string& error)
    {
        return CreatorAssetWorkflowService().BuildCatalogue(
            root.generic_u8string(), ProjectId, catalogue, error);
    }

    const AssetCatalogueEntry* FindEntry(
        const AssetCatalogue& catalogue,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            catalogue.entries.begin(), catalogue.entries.end(),
            [&assetId](const AssetCatalogueEntry& entry)
            { return entry.assetId == assetId; });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    const AssetRecord* FindRecord(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.records.begin(), registry.records.end(),
            [&assetId](const AssetRecord& record)
            { return record.assetId == assetId; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const ImportedProductRecord* FindProvenance(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            registry.importedProducts.begin(), registry.importedProducts.end(),
            [&assetId](const ImportedProductRecord& record)
            { return record.productAssetId == assetId; });
        return found == registry.importedProducts.end() ? nullptr : &*found;
    }

    const ResourceAssetMetadataRecord* FindMetadata(
        const ResourceAssetMetadataDocument& metadata,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            metadata.records.begin(), metadata.records.end(),
            [&assetId](const ResourceAssetMetadataRecord& record)
            { return record.assetId == assetId; });
        return found == metadata.records.end() ? nullptr : &*found;
    }

    wi::Resource FakeLoaderCapture(
        const PreparedMaterialTextureAsset& prepared,
        std::string& error,
        std::vector<std::uint8_t>& captured)
    {
        captured = prepared.payload;
        wi::vector<std::uint8_t> bytes;
        bytes.assign(prepared.payload.begin(), prepared.payload.end());
        wi::Resource resource;
        resource.SetFileData(std::move(bytes));
        if (!resource.IsValid())
        {
            error = "fake loader could not retain governed bytes";
            return {};
        }
        error.clear();
        return resource;
    }
}

int main()
{
    using namespace renegade::bridge;

    const fs::path root = fs::temp_directory_path() /
        "renegade-lp08-gate4-resource-reimport";
    std::error_code ec;
    fs::remove_all(root, ec);
    for (const char* folder : {
        "Content/Textures", "Content/Audio", "Content/Scripts",
        "Content/Video", "Content/Fonts",
        "SourceAssets/Textures", "SourceAssets/Audio",
        "SourceAssets/Scripts", "SourceAssets/Video", "SourceAssets/Fonts",
        "Intermediate/Transactions"})
    {
        fs::create_directories(root / fs::u8path(folder), ec);
    }
    Require(!ec, "could not create Gate 4 project fixture");
    Require(WriteEmptyRegistry(root), "could not create Gate 4 LC01 registry");

    Fixture png{
        ResourceClass::Texture, ResourceSourceFormat::Png,
        "SourceAssets/Textures/proof.png", "Content/Textures/proof.rasset",
        {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
         0,0,0,13,'I','H','D','R',0,0,0,2,0,0,0,3},
        {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
         0,0,0,13,'I','H','D','R',0,0,0,4,0,0,0,5}
    };
    WriteBytes(root / fs::u8path(png.sourcePath), png.first);
    const auto importedPng = ImportFixture(root, png);
    Require(importedPng.succeeded, "PNG first import failed: " + importedPng.error);
    Require(IsValidStableId(importedPng.assetId) &&
            IsValidStableId(importedPng.sourceAssetId),
        "PNG first import did not create stable identities");

    PreparedMaterialTextureAsset prepared;
    std::string error;
    Require(PrepareMaterialTextureAsset(
            root.generic_u8string(), ProjectId, importedPng.assetId,
            prepared, error),
        "PNG could not prepare for material binding: " + error);
    wi::scene::Scene scene;
    const wi::ecs::Entity materialEntity = wi::ecs::CreateEntity();
    scene.materials.Create(materialEntity);
    std::vector<std::uint8_t> initialLoaded;
    MaterialTextureResourceLoader initialLoader =
        [&initialLoaded](const PreparedMaterialTextureAsset& value, std::string& loadError)
        {
            return FakeLoaderCapture(value, loadError, initialLoaded);
        };
    SetMaterialBaseColorTextureAssetCommand assign(
        scene, materialEntity, prepared, initialLoader);
    Require(assign.Execute() && initialLoaded == png.first,
        "initial governed texture assignment did not load first payload");

    const fs::path pngProductPath = root / fs::u8path(png.productPath);
    const auto firstProductBytes = ReadBytes(pngProductPath);
    WriteBytes(root / fs::u8path(png.sourcePath), png.second);

    AssetCatalogue catalogue;
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh after PNG source edit failed: " + error);
    const auto* staleEntry = FindEntry(catalogue, importedPng.assetId);
    Require(staleEntry != nullptr && staleEntry->state == AssetCatalogueState::Stale,
        "changed PNG source was not reported STALE");
    Require(staleEntry != nullptr && CanReimportCreatorResourceAsset(*staleEntry),
        "STALE governed texture was not creator-reimportable");
    Require(ReadBytes(pngProductPath) == firstProductBytes,
        "stale detection overwrote the last-good PNG product");

    const auto reimportedPng = Reimport(root, importedPng.assetId);
    Require(reimportedPng.succeeded,
        "PNG explicit reimport failed: " + reimportedPng.error);
    Require(reimportedPng.assetId == importedPng.assetId &&
            reimportedPng.sourceAssetId == importedPng.sourceAssetId &&
            reimportedPng.statusBefore.sourceChanged &&
            !reimportedPng.statusBefore.productChanged,
        "PNG reimport did not retain IDs or report stale pre-state");
    Require(reimportedPng.derived.dimensionsKnown &&
            reimportedPng.derived.width == 4 &&
            reimportedPng.derived.height == 5,
        "PNG reimport did not update derived dimensions");

    ResourceAssetDocument reopenedPng;
    Require(ReadResourceAssetDocument(
            pngProductPath.generic_u8string(), reopenedPng, error) &&
            reopenedPng.payload == png.second,
        "PNG reimport product did not contain second payload: " + error);

    std::vector<std::uint8_t> refreshedLoaded;
    MaterialTextureResourceLoader refreshLoader =
        [&refreshedLoaded](const PreparedMaterialTextureAsset& value, std::string& loadError)
        {
            return FakeLoaderCapture(value, loadError, refreshedLoaded);
        };
    const auto liveRefresh = RefreshMaterialTextureBindingsForAsset(
        scene, root.generic_u8string(), ProjectId,
        importedPng.assetId, refreshLoader);
    Require(liveRefresh.succeeded && liveRefresh.discovered == 1 &&
            liveRefresh.restored == 1 && refreshedLoaded == png.second,
        "material did not force-refresh from new governed PNG payload: " +
            liveRefresh.error);
    const auto* boundMetadata = scene.metadatas.GetComponent(materialEntity);
    Require(boundMetadata != nullptr &&
            boundMetadata->string_values.get(
                MaterialBaseColorTextureAssetIdMetadataKey) == importedPng.assetId,
        "live texture refresh changed durable stable-ID material identity");

    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh after PNG reimport failed: " + error);
    const auto* currentEntry = FindEntry(catalogue, importedPng.assetId);
    Require(currentEntry != nullptr && currentEntry->state == AssetCatalogueState::Current,
        "PNG did not return CURRENT after successful reimport");

    // A malformed candidate must fail before commit and preserve the exact
    // accepted product/provenance/metadata bytes.
    const auto lastGoodProduct = ReadBytes(pngProductPath);
    const std::string lastGoodRegistry = ReadText(root / AssetRegistryDocumentName);
    const std::string lastGoodMetadata =
        ReadText(root / ResourceAssetMetadataDocumentName);
    WriteBytes(root / fs::u8path(png.sourcePath), {1,2,3,4});
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh before malformed PNG proof failed: " + error);
    const auto malformed = Reimport(root, importedPng.assetId);
    Require(!malformed.succeeded,
        "malformed PNG candidate unexpectedly replaced last-good product");
    Require(ReadBytes(pngProductPath) == lastGoodProduct &&
            ReadText(root / AssetRegistryDocumentName) == lastGoodRegistry &&
            ReadText(root / ResourceAssetMetadataDocumentName) == lastGoodMetadata,
        "malformed reimport changed last-good governed state");

    // Restore the accepted source, then prove a fault after physical product
    // replacement rolls every document back byte-for-byte.
    WriteBytes(root / fs::u8path(png.sourcePath), png.second);
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh after restoring PNG failed: " + error);
    auto thirdPng = png.second;
    thirdPng[19] = 6; // width 6, still a signature-valid Gate-1 PNG fixture.
    WriteBytes(root / fs::u8path(png.sourcePath), thirdPng);
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh before rollback proof failed: " + error);
    const auto rollbackProductBefore = ReadBytes(pngProductPath);
    const std::string rollbackRegistryBefore = ReadText(root / AssetRegistryDocumentName);
    const std::string rollbackMetadataBefore =
        ReadText(root / ResourceAssetMetadataDocumentName);
    ResourceAssetReimportOptions fault;
    fault.transactionId = "lp08-gate4-resource-reimport-rollback";
    fault.operationHook = [](
        const ProjectDocumentTransactionStage stage,
        const std::size_t documentIndex,
        const std::string&,
        std::string& hookError)
    {
        if (stage == ProjectDocumentTransactionStage::AfterReplace &&
            documentIndex == 0)
        {
            hookError = "intentional Gate 4 last-good rollback proof";
            return ProjectDocumentTransactionHookAction::Fail;
        }
        return ProjectDocumentTransactionHookAction::Continue;
    };
    const auto rolledBack = Reimport(
        root, importedPng.assetId, std::move(fault));
    Require(!rolledBack.succeeded && rolledBack.transaction.rolledBack,
        "fault-injected reimport did not report rollback");
    Require(ReadBytes(pngProductPath) == rollbackProductBefore &&
            ReadText(root / AssetRegistryDocumentName) == rollbackRegistryBefore &&
            ReadText(root / ResourceAssetMetadataDocumentName) == rollbackMetadataBefore,
        "fault-injected reimport did not restore exact last-good documents");

    // Return to the accepted source snapshot before moved/missing recovery.
    WriteBytes(root / fs::u8path(png.sourcePath), png.second);
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh before move recovery failed: " + error);

    const fs::path movedSource = root / "SourceAssets/Textures/proof_moved.png";
    const fs::path movedProduct = root / "Content/Textures/proof_moved.rasset";
    fs::rename(root / fs::u8path(png.sourcePath), movedSource, ec);
    fs::rename(pngProductPath, movedProduct, ec);
    Require(!ec, "could not move Gate 4 source/product fixtures");
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh after move failed: " + error);
    const auto* movedEntry = FindEntry(catalogue, importedPng.assetId);
    Require(movedEntry != nullptr && movedEntry->state == AssetCatalogueState::Moved &&
            movedEntry->projectRelativePath == "Content/Textures/proof_moved.rasset" &&
            CanReimportCreatorResourceAsset(*movedEntry),
        "LC01 did not recover moved product identity/reimportability");
    const auto movedReplay = Reimport(root, importedPng.assetId);
    Require(movedReplay.succeeded &&
            movedReplay.sourceAssetId == importedPng.sourceAssetId &&
            movedReplay.assetId == importedPng.assetId &&
            movedReplay.sourceProjectRelativePath ==
                "SourceAssets/Textures/proof_moved.png" &&
            movedReplay.assetProjectRelativePath ==
                "Content/Textures/proof_moved.rasset",
        "resource reimport did not follow recovered moved LC01 paths");

    const auto movedLastGood = ReadBytes(movedProduct);
    fs::remove(movedSource, ec);
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh after missing source failed: " + error);
    const auto* missingSourceEntry = FindEntry(catalogue, importedPng.assetId);
    Require(missingSourceEntry != nullptr &&
            missingSourceEntry->state == AssetCatalogueState::Missing &&
            !CanReimportCreatorResourceAsset(*missingSourceEntry),
        "missing governed source did not block resource reimport in creator policy");
    const auto missingSourceReplay = Reimport(root, importedPng.assetId);
    Require(!missingSourceReplay.succeeded && ReadBytes(movedProduct) == movedLastGood,
        "missing source reimport changed last-good product");

    WriteBytes(root / "SourceAssets/Textures/proof_recovered.png", png.second);
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh after source recovery failed: " + error);
    AssetRegistry recoveredRegistry;
    Require(ReadAssetRegistry(
            root.generic_u8string(), ProjectId, recoveredRegistry, error),
        "could not reopen registry after source recovery: " + error);
    const auto* recoveredSourceRecord =
        FindRecord(recoveredRegistry, importedPng.sourceAssetId);
    Require(recoveredSourceRecord != nullptr &&
            recoveredSourceRecord->projectRelativePath ==
                "SourceAssets/Textures/proof_recovered.png",
        "LC01 did not recover the original stable source ID at its new path");

    fs::remove(movedProduct, ec);
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh after missing product failed: " + error);
    const auto* missingProductEntry = FindEntry(catalogue, importedPng.assetId);
    Require(missingProductEntry != nullptr &&
            missingProductEntry->state == AssetCatalogueState::Missing &&
            !CanReimportCreatorResourceAsset(*missingProductEntry),
        "missing governed product did not block resource reimport");
    const auto missingProductReplay = Reimport(root, importedPng.assetId);
    Require(!missingProductReplay.succeeded,
        "missing last-good product unexpectedly reimported");

    WriteBytes(root / "Content/Textures/proof_recovered.rasset", movedLastGood);
    Require(RefreshCatalogue(root, catalogue, error),
        "catalogue refresh after product recovery failed: " + error);
    Require(ReadAssetRegistry(
            root.generic_u8string(), ProjectId, recoveredRegistry, error),
        "could not reopen registry after product recovery: " + error);
    const auto* recoveredProductRecord =
        FindRecord(recoveredRegistry, importedPng.assetId);
    Require(recoveredProductRecord != nullptr &&
            recoveredProductRecord->projectRelativePath ==
                "Content/Textures/proof_recovered.rasset",
        "LC01 did not recover original stable product ID at its new path");

    // Common non-model classes use the same stable-ID transaction. Include both
    // accepted audio formats and both accepted video formats, not just one class.
    std::vector<Fixture> generic = {
        {ResourceClass::Audio, ResourceSourceFormat::Wav,
         "SourceAssets/Audio/proof.wav", "Content/Audio/proof.rasset",
         {'R','I','F','F',0,0,0,0,'W','A','V','E',1},
         {'R','I','F','F',0,0,0,0,'W','A','V','E',2}},
        {ResourceClass::Audio, ResourceSourceFormat::Ogg,
         "SourceAssets/Audio/proof.ogg", "Content/Audio/proof_ogg.rasset",
         {'O','g','g','S',0,1,2,3}, {'O','g','g','S',4,5,6,7}},
        {ResourceClass::Script, ResourceSourceFormat::Lua,
         "SourceAssets/Scripts/proof.lua", "Content/Scripts/proof.rasset",
         {'r','e','t','u','r','n',' ','1','\n'},
         {'r','e','t','u','r','n',' ','2','\n'}},
        {ResourceClass::Video, ResourceSourceFormat::Mp4,
         "SourceAssets/Video/proof.mp4", "Content/Video/proof.rasset",
         {0,0,0,16,'f','t','y','p','i','s','o','m',1},
         {0,0,0,16,'f','t','y','p','i','s','o','m',2}},
        {ResourceClass::Video, ResourceSourceFormat::H264,
         "SourceAssets/Video/proof.h264", "Content/Video/proof_h264.rasset",
         {0,0,0,1,0x67,1,2,3}, {0,0,0,1,0x67,4,5,6}},
        {ResourceClass::Font, ResourceSourceFormat::Ttf,
         "SourceAssets/Fonts/proof.ttf", "Content/Fonts/proof.rasset",
         {0,1,0,0,0,1,0,0,1}, {0,1,0,0,0,1,0,0,2}},
    };

    struct AcceptedFixture
    {
        Fixture fixture;
        ResourceAssetImportResult imported;
    };
    std::vector<AcceptedFixture> accepted;
    for (const auto& fixture : generic)
    {
        WriteBytes(root / fs::u8path(fixture.sourcePath), fixture.first);
        const auto imported = ImportFixture(root, fixture);
        Require(imported.succeeded,
            fixture.sourcePath + " first import failed: " + imported.error);
        accepted.push_back({fixture, imported});
    }
    for (const auto& item : accepted)
        WriteBytes(root / fs::u8path(item.fixture.sourcePath), item.fixture.second);
    Require(RefreshCatalogue(root, catalogue, error),
        "generic resource stale refresh failed: " + error);

    for (const auto& item : accepted)
    {
        const auto* stale = FindEntry(catalogue, item.imported.assetId);
        Require(stale != nullptr && stale->state == AssetCatalogueState::Stale &&
                CanReimportCreatorResourceAsset(*stale),
            item.fixture.sourcePath + " did not enter generic STALE/reimportable state");
        const auto replayed = Reimport(root, item.imported.assetId);
        Require(replayed.succeeded &&
                replayed.assetId == item.imported.assetId &&
                replayed.sourceAssetId == item.imported.sourceAssetId &&
                replayed.resourceClass == item.fixture.resourceClass &&
                replayed.sourceFormat == item.fixture.format,
            item.fixture.sourcePath + " generic reimport failed: " + replayed.error);
        ResourceAssetDocument document;
        Require(ReadResourceAssetDocument(
                (root / fs::u8path(item.fixture.productPath)).generic_u8string(),
                document, error) && document.payload == item.fixture.second,
            item.fixture.productPath + " did not adopt second payload");
    }

    AssetRegistry finalRegistry;
    ResourceAssetMetadataDocument finalMetadata;
    Require(ReadAssetRegistry(
            root.generic_u8string(), ProjectId, finalRegistry, error),
        "final Gate 4 registry did not reopen: " + error);
    Require(ReadResourceAssetMetadata(
            root.generic_u8string(), ProjectId, finalMetadata, error),
        "final Gate 4 resource metadata did not reopen: " + error);
    for (const auto& item : accepted)
    {
        const auto* provenance = FindProvenance(finalRegistry, item.imported.assetId);
        const auto* metadata = FindMetadata(finalMetadata, item.imported.assetId);
        Require(provenance != nullptr && metadata != nullptr &&
                metadata->resourceClass == item.fixture.resourceClass &&
                metadata->sourceFormat == item.fixture.format &&
                metadata->derived.byteCount == item.fixture.second.size(),
            item.fixture.productPath + " did not update provenance/metadata generically");
    }

    fs::remove_all(root, ec);
    if (failures != 0)
    {
        std::cerr << "LP08 GATE 4 RESOURCE REIMPORT FAIL // "
                  << failures << " checks failed\n";
        return 1;
    }
    std::cout << "LP08 GATE 4 RESOURCE REIMPORT PASS // stale stable_id last_good moved_missing texture_refresh multi_resource\n";
    return 0;
}
