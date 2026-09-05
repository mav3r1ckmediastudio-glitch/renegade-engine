#include "renegade/bridge/ResourceAssetService.h"
#include "renegade/bridge/ReusableAssetService.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId = "71111111-1111-4111-8111-111111111111";
    constexpr const char* ModelSourceId = "72222222-2222-4222-8222-222222222222";
    constexpr const char* ModelAssetId = "73333333-3333-4333-8333-333333333333";

    int failures = 0;

    void Require(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL // " << message << '\n';
        }
    }

    void WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
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
        WriteBytes(path, std::vector<std::uint8_t>(text.begin(), text.end()));
    }

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    std::string ReadText(const fs::path& path)
    {
        const auto bytes = ReadBytes(path);
        return std::string(bytes.begin(), bytes.end());
    }

    std::string HashPayload(const std::vector<std::uint8_t>& payload)
    {
        constexpr std::uint64_t Offset = 1469598103934665603ull;
        constexpr std::uint64_t Prime = 1099511628211ull;
        std::uint64_t hash = Offset;
        for (const auto value : payload)
        {
            hash ^= value;
            hash *= Prime;
        }
        constexpr char Hex[] = "0123456789abcdef";
        std::string text = "fnv1a64:";
        for (int shift = 60; shift >= 0; shift -= 4)
            text.push_back(Hex[(hash >> shift) & 0x0f]);
        return text;
    }

    ReusableModelAssetDocument MakeModelDocument()
    {
        ReusableModelAssetDocument document;
        document.manifest.projectId = ProjectId;
        document.manifest.assetId = ModelAssetId;
        document.manifest.sourceAssetId = ModelSourceId;
        document.manifest.sourceFormat = "fbx";
        document.manifest.importer = "wicked.ufbx";
        document.manifest.importerVersion = 1;
        document.manifest.settingsJson =
            "{\"options\":{},\"source_format\":\"fbx\"}";
        document.payload = {0x57, 0x49, 0x53, 0x43, 0x45, 0x4e, 0x45, 0x01};
        document.manifest.payloadHash = HashPayload(document.payload);
        return document;
    }

    DependencyClass ExpectedDependencyClass(const ResourceClass resourceClass)
    {
        switch (resourceClass)
        {
        case ResourceClass::Texture: return DependencyClass::Texture;
        case ResourceClass::Audio: return DependencyClass::Audio;
        case ResourceClass::Script: return DependencyClass::Script;
        case ResourceClass::Video: return DependencyClass::Video;
        case ResourceClass::Font: return DependencyClass::Font;
        default: return DependencyClass::Data;
        }
    }

    const AssetRecord* FindRecord(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(registry.records.begin(),
            registry.records.end(), [&assetId](const AssetRecord& record)
            { return record.assetId == assetId; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    const ImportedProductRecord* FindProvenance(
        const AssetRegistry& registry,
        const StableId& assetId)
    {
        const auto found = std::find_if(registry.importedProducts.begin(),
            registry.importedProducts.end(),
            [&assetId](const ImportedProductRecord& record)
            { return record.productAssetId == assetId; });
        return found == registry.importedProducts.end() ? nullptr : &*found;
    }

    const ResourceAssetMetadataRecord* FindMetadata(
        const ResourceAssetMetadataDocument& metadata,
        const StableId& assetId)
    {
        const auto found = std::find_if(metadata.records.begin(),
            metadata.records.end(),
            [&assetId](const ResourceAssetMetadataRecord& record)
            { return record.assetId == assetId; });
        return found == metadata.records.end() ? nullptr : &*found;
    }

    struct Fixture
    {
        ResourceClass resourceClass;
        ResourceSourceFormat format;
        std::string sourcePath;
        std::string productPath;
        std::vector<std::uint8_t> bytes;
    };

    ResourceAssetImportResult ImportFixture(
        const fs::path& root,
        const Fixture& fixture,
        ResourceAssetImportOptions options = {})
    {
        ResourceAssetImportRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = ProjectId;
        request.sourceProjectRelativePath = fixture.sourcePath;
        request.assetProjectRelativePath = fixture.productPath;
        request.expectedFormat = fixture.format;
        ResourceAssetService service;
        return service.ImportResourceAsset(request, std::move(options));
    }
}

int main()
{
    using namespace renegade::bridge;
    std::string error;

    const fs::path root = fs::temp_directory_path() /
        "renegade-lp08-gate2-resource-asset";
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    for (const char* folder : {
        "Content/Textures", "Content/Audio", "Content/Scripts",
        "Content/Video", "Content/Fonts",
        "SourceAssets/Textures", "SourceAssets/Audio",
        "SourceAssets/Scripts", "SourceAssets/Video", "SourceAssets/Fonts",
        "Intermediate/Transactions"})
    {
        fs::create_directories(root / fs::u8path(folder), cleanupError);
    }
    Require(!cleanupError, "could not create Gate 2 project fixture");

    AssetRegistry initialRegistry;
    initialRegistry.projectId = ProjectId;
    initialRegistry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
    std::string initialRegistryJson;
    Require(SerializeAssetRegistry(initialRegistry, initialRegistryJson, error),
        "could not serialize initial LC01 registry: " + error);
    WriteText(root / AssetRegistryDocumentName, initialRegistryJson);

    Fixture png{
        ResourceClass::Texture,
        ResourceSourceFormat::Png,
        "SourceAssets/Textures/proof.png",
        "Content/Textures/proof.rasset",
        {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
         0x00,0x00,0x00,0x0d,'I','H','D','R',
         0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x03}
    };
    Fixture wav{
        ResourceClass::Audio,
        ResourceSourceFormat::Wav,
        "SourceAssets/Audio/proof.wav",
        "Content/Audio/proof.rasset",
        {'R','I','F','F',0,0,0,0,'W','A','V','E'}
    };
    Fixture lua{
        ResourceClass::Script,
        ResourceSourceFormat::Lua,
        "SourceAssets/Scripts/proof.lua",
        "Content/Scripts/proof.rasset",
        {'r','e','t','u','r','n',' ','{','g','a','t','e','=','2','}','\n'}
    };
    Fixture mp4{
        ResourceClass::Video,
        ResourceSourceFormat::Mp4,
        "SourceAssets/Video/proof.mp4",
        "Content/Video/proof.rasset",
        {0,0,0,16,'f','t','y','p','i','s','o','m'}
    };
    Fixture ttf{
        ResourceClass::Font,
        ResourceSourceFormat::Ttf,
        "SourceAssets/Fonts/proof.ttf",
        "Content/Fonts/proof.rasset",
        {0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00}
    };

    for (const Fixture* fixture : {&png, &wav, &lua, &mp4, &ttf})
        WriteBytes(root / fs::u8path(fixture->sourcePath), fixture->bytes);

    // LP07 model .rasset bytes must remain an independent accepted payload kind.
    const ReusableModelAssetDocument model = MakeModelDocument();
    std::vector<std::uint8_t> modelBytes;
    Require(SerializeReusableModelAssetDocument(model, modelBytes, error),
        "LP07 model .rasset no longer serializes: " + error);
    ReusableModelAssetDocument modelReopened;
    Require(DeserializeReusableModelAssetDocument(
            modelBytes, modelReopened, error),
        "LP07 model .rasset no longer reopens: " + error);
    std::vector<std::uint8_t> modelBytesAgain;
    Require(SerializeReusableModelAssetDocument(
            modelReopened, modelBytesAgain, error) &&
            modelBytesAgain == modelBytes,
        "LP07 model .rasset bytes changed after Gate 2 round-trip");
    ResourceAssetDocument wrongResourceKind;
    Require(!DeserializeResourceAssetDocument(
            modelBytes, wrongResourceKind, error),
        "resource parser accepted an LP07 model .rasset as a resource asset");

    const auto pngResult = ImportFixture(root, png);
    Require(pngResult.succeeded,
        "PNG first import failed: " + pngResult.error);
    Require(pngResult.resourceClass == ResourceClass::Texture &&
            pngResult.sourceFormat == ResourceSourceFormat::Png,
        "PNG import returned wrong class/format");
    Require(pngResult.derived.known && pngResult.derived.dimensionsKnown &&
            pngResult.derived.width == 2 && pngResult.derived.height == 3 &&
            pngResult.derived.mipCount == 1 &&
            pngResult.derived.byteCount == png.bytes.size(),
        "PNG derived metadata did not preserve dimensions/size");

    ResourceAssetDocument pngProduct;
    Require(ReadResourceAssetDocument(
            (root / fs::u8path(png.productPath)).generic_u8string(),
            pngProduct, error),
        "PNG product did not reopen: " + error);
    Require(pngProduct.payload == png.bytes,
        "PNG governed payload is not byte-identical to the retained source");
    Require(pngProduct.manifest.assetId == pngResult.assetId &&
            pngProduct.manifest.sourceAssetId == pngResult.sourceAssetId &&
            pngProduct.manifest.settingsJson ==
                "{\"options\":{},\"resource_class\":\"texture\",\"source_format\":\"png\"}",
        "PNG manifest identity/recipe is not canonical");
    Require(ReadBytes(root / fs::u8path(png.sourcePath)) == png.bytes,
        "PNG source bytes were mutated by first import");
    ReusableModelAssetDocument wrongModelKind;
    Require(!DeserializeReusableModelAssetDocument(
            ReadBytes(root / fs::u8path(png.productPath)), wrongModelKind, error),
        "LP07 model parser accepted an LP08 resource .rasset");

    // Force a failure after the transaction has replaced one destination.
    // ProjectDocumentTransaction must restore the exact previous registry and
    // metadata and remove the new product instead of leaving a half-asset.
    const std::string registryBeforeFault =
        ReadText(root / AssetRegistryDocumentName);
    const std::string metadataBeforeFault =
        ReadText(root / ResourceAssetMetadataDocumentName);
    ResourceAssetImportOptions faultOptions;
    faultOptions.transactionId = "lp08-gate2-rollback";
    faultOptions.operationHook = [](
        const ProjectDocumentTransactionStage stage,
        const std::size_t documentIndex,
        const std::string&,
        std::string& hookError)
    {
        if (stage == ProjectDocumentTransactionStage::AfterReplace &&
            documentIndex == 0)
        {
            hookError = "intentional Gate 2 rollback proof";
            return ProjectDocumentTransactionHookAction::Fail;
        }
        return ProjectDocumentTransactionHookAction::Continue;
    };
    const auto failedWav = ImportFixture(root, wav, std::move(faultOptions));
    Require(!failedWav.succeeded && failedWav.transaction.rolledBack,
        "fault-injected resource import did not report rollback");
    Require(!fs::exists(root / fs::u8path(wav.productPath)),
        "fault-injected import left a half-created .rasset product");
    Require(ReadText(root / AssetRegistryDocumentName) == registryBeforeFault,
        "fault-injected import changed last-good LC01 registry bytes");
    Require(ReadText(root / ResourceAssetMetadataDocumentName) ==
            metadataBeforeFault,
        "fault-injected import changed last-good resource metadata bytes");
    Require(ReadBytes(root / fs::u8path(wav.sourcePath)) == wav.bytes,
        "fault-injected import modified retained WAV source bytes");

    std::vector<ResourceAssetImportResult> accepted;
    accepted.push_back(pngResult);
    for (const Fixture* fixture : {&wav, &lua, &mp4, &ttf})
    {
        const auto result = ImportFixture(root, *fixture);
        Require(result.succeeded,
            fixture->productPath + " generic import failed: " + result.error);
        Require(result.resourceClass == fixture->resourceClass &&
                result.sourceFormat == fixture->format,
            fixture->productPath + " returned wrong class/format");
        Require(result.derived.known &&
                result.derived.byteCount == fixture->bytes.size(),
            fixture->productPath + " did not persist byte-count metadata");
        Require(ReadBytes(root / fs::u8path(fixture->sourcePath)) == fixture->bytes,
            fixture->sourcePath + " source bytes changed during import");
        ResourceAssetDocument reopened;
        Require(ReadResourceAssetDocument(
                (root / fs::u8path(fixture->productPath)).generic_u8string(),
                reopened, error) && reopened.payload == fixture->bytes,
            fixture->productPath + " governed payload did not reopen exactly");
        accepted.push_back(result);
    }

    AssetRegistry reopenedRegistry;
    Require(ReadAssetRegistry(root.generic_u8string(), ProjectId,
            reopenedRegistry, error),
        "LC01 registry did not reopen after resource imports: " + error);
    ResourceAssetMetadataDocument reopenedMetadata;
    Require(ReadResourceAssetMetadata(root.generic_u8string(), ProjectId,
            reopenedMetadata, error),
        "resource catalogue metadata did not reopen: " + error);
    Require(reopenedRegistry.importedProducts.size() == 5,
        "LC01 registry did not retain five resource provenance records");
    Require(reopenedMetadata.records.size() == 5,
        "resource metadata did not retain five stable-ID records");

    std::set<StableId> productIds;
    std::set<StableId> sourceIds;
    for (std::size_t index = 0; index < accepted.size(); ++index)
    {
        const auto& result = accepted[index];
        const Fixture& fixture = *std::array<const Fixture*, 5>{
            &png, &wav, &lua, &mp4, &ttf}[index];
        Require(productIds.insert(result.assetId).second &&
                sourceIds.insert(result.sourceAssetId).second,
            "resource first imports did not receive distinct stable IDs");
        const AssetRecord* source = FindRecord(
            reopenedRegistry, result.sourceAssetId);
        const AssetRecord* productRecord = FindRecord(
            reopenedRegistry, result.assetId);
        const ImportedProductRecord* provenance = FindProvenance(
            reopenedRegistry, result.assetId);
        const ResourceAssetMetadataRecord* metadata = FindMetadata(
            reopenedMetadata, result.assetId);
        Require(source != nullptr &&
                source->requirement == DependencyRequirement::EditorOnly &&
                source->dependencyClass == DependencyClass::ImportedContent &&
                source->provider == "lp08.source_asset",
            fixture.sourcePath + " is not an editor-only retained source");
        Require(productRecord != nullptr &&
                productRecord->requirement == DependencyRequirement::Required &&
                productRecord->dependencyClass ==
                    ExpectedDependencyClass(fixture.resourceClass) &&
                productRecord->provider == "lp08.rasset",
            fixture.productPath + " has the wrong governed product class");
        Require(provenance != nullptr &&
                provenance->sourceAssetId == result.sourceAssetId &&
                provenance->importer == "wicked.resourcemanager" &&
                provenance->settingsSchema ==
                    ResourceAssetImportSettingsSchema &&
                provenance->sourceContentHashAtImport == result.sourceHash &&
                provenance->productContentHashAtImport == result.productHash,
            fixture.productPath + " LC01 provenance did not survive reopen");
        Require(metadata != nullptr &&
                metadata->resourceClass == fixture.resourceClass &&
                metadata->sourceFormat == fixture.format &&
                metadata->derived == result.derived,
            fixture.productPath + " resource metadata did not survive reopen");
    }

    // First-import validation failure must not create any product or metadata.
    WriteBytes(root / "SourceAssets/Textures/bad.png",
        {'R','I','F','F',0,0,0,0,'W','A','V','E'});
    ResourceAssetImportRequest malformedRequest;
    malformedRequest.projectRoot = root.generic_u8string();
    malformedRequest.projectId = ProjectId;
    malformedRequest.sourceProjectRelativePath =
        "SourceAssets/Textures/bad.png";
    malformedRequest.assetProjectRelativePath =
        "Content/Textures/bad.rasset";
    malformedRequest.expectedFormat = ResourceSourceFormat::Png;
    ResourceAssetService service;
    const std::string registryBeforeMalformed =
        ReadText(root / AssetRegistryDocumentName);
    const std::string metadataBeforeMalformed =
        ReadText(root / ResourceAssetMetadataDocumentName);
    const auto malformed = service.ImportResourceAsset(malformedRequest);
    Require(!malformed.succeeded,
        "malformed PNG was accepted by governed first import");
    Require(!fs::exists(root / "Content/Textures/bad.rasset") &&
            ReadText(root / AssetRegistryDocumentName) == registryBeforeMalformed &&
            ReadText(root / ResourceAssetMetadataDocumentName) ==
                metadataBeforeMalformed,
        "malformed first import left product/registry/metadata changes");

    // Version-1 settings are deliberately fixed until a later gate exposes
    // a real creator-facing option contract.
    ResourceAssetImportRequest optionsRequest;
    optionsRequest.projectRoot = root.generic_u8string();
    optionsRequest.projectId = ProjectId;
    optionsRequest.sourceProjectRelativePath = wav.sourcePath;
    optionsRequest.assetProjectRelativePath = "Content/Audio/options.rasset";
    optionsRequest.expectedFormat = ResourceSourceFormat::Wav;
    optionsRequest.settingsJson = "{\"normalize\":true}";
    const auto unsupportedOptions = service.ImportResourceAsset(optionsRequest);
    Require(!unsupportedOptions.succeeded &&
            !fs::exists(root / "Content/Audio/options.rasset"),
        "unsupported version-1 resource options were accepted");

    fs::remove_all(root, cleanupError);
    if (failures != 0)
    {
        std::cerr << "LP08 GATE 2 RESOURCE ASSET FAIL // "
                  << failures << " checks failed\n";
        return 1;
    }

    std::cout << "LP08 GATE 2 RESOURCE ASSET PASS // classes=5 "
              << "atomic_rollback=proved model_rasset=compatible\n";
    return 0;
}
