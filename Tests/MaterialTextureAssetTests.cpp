#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/CreatorAssetActionPolicy.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "81111111-1111-4111-8111-111111111111";

    int failures = 0;
    int fakeLoaderCalls = 0;

    void Require(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL // " << message << '\n';
        }
    }

    const std::vector<std::uint8_t>& PngBytes()
    {
        static const std::vector<std::uint8_t> bytes = {
            137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
            0,0,0,1,0,0,0,1,8,4,0,0,0,181,28,12,2,
            0,0,0,11,73,68,65,84,120,218,99,100,248,15,0,1,
            5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
        return bytes;
    }

    void WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
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
        std::ofstream stream(root / AssetRegistryDocumentName,
            std::ios::binary | std::ios::trunc);
        stream << json;
        return static_cast<bool>(stream);
    }

    wi::Resource FakeLoader(
        const PreparedMaterialTextureAsset& prepared,
        std::string& error)
    {
        ++fakeLoaderCalls;
        wi::vector<std::uint8_t> bytes;
        bytes.assign(prepared.payload.begin(), prepared.payload.end());
        wi::Resource resource;
        resource.SetFileData(std::move(bytes));
        if (!resource.IsValid())
        {
            error = "fake resource loader could not retain payload bytes";
            return {};
        }
        error.clear();
        return resource;
    }
}

int main()
{
    using namespace renegade::bridge;

    AssetCatalogueEntry modelPolicy;
    modelPolicy.registered = true;
    modelPolicy.assetId = "82222222-2222-4222-8222-222222222222";
    modelPolicy.importedProduct = true;
    modelPolicy.productAvailable = true;
    modelPolicy.sourceFormat = "FBX";
    Require(CanPlaceCreatorModelAsset(modelPolicy) &&
            CanReimportCreatorModelAsset(modelPolicy),
        "available LP07 model should remain placeable and reimportable");
    modelPolicy.productAvailable = false;
    Require(!CanPlaceCreatorModelAsset(modelPolicy) &&
            CanReimportCreatorModelAsset(modelPolicy),
        "missing LP07 product must remain reimportable for recovery");
    AssetCatalogueEntry texturePolicy = modelPolicy;
    texturePolicy.sourceFormat = "png";
    texturePolicy.productAvailable = true;
    Require(!CanPlaceCreatorModelAsset(texturePolicy) &&
            !CanReimportCreatorModelAsset(texturePolicy),
        "LP08 texture must never enter the LP07 model place/reimport policy");
    texturePolicy.productAvailable = false;
    Require(!CanReimportCreatorModelAsset(texturePolicy),
        "missing LP08 texture must remain excluded from Gate 3 model reimport");

    const fs::path root = fs::temp_directory_path() /
        "renegade-lp08-gate3-material-texture";
    const fs::path externalRoot = fs::temp_directory_path() /
        "renegade-lp08-gate3-external-texture";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::remove_all(externalRoot, ec);
    fs::create_directories(root / "Content", ec);
    fs::create_directories(root / "SourceAssets", ec);
    fs::create_directories(root / "Intermediate" / "Transactions", ec);
    fs::create_directories(externalRoot, ec);
    Require(!ec, "could not create Gate 3 test folders");
    Require(WriteEmptyRegistry(root), "could not create Gate 3 LC01 registry");

    const fs::path external = externalRoot / "brick.png";
    WriteBytes(external, PngBytes());

    CreatorTextureWorkflowService creator;
    const auto imported = creator.ImportTexture(
        root.generic_u8string(), ProjectId, external.generic_u8string());
    Require(imported.succeeded && imported.committed,
        "creator texture import failed: " + imported.error);
    Require(imported.sourceFormat == ResourceSourceFormat::Png,
        "creator texture import returned wrong source format");
    Require(IsValidStableId(imported.assetId) &&
            IsValidStableId(imported.sourceAssetId),
        "creator texture import did not return stable source/product IDs");
    Require(ReadBytes(root / fs::u8path(imported.sourceProjectRelativePath)) ==
            PngBytes(),
        "creator texture import did not retain exact source bytes");
    Require(ReadBytes(external) == PngBytes(),
        "creator texture import modified the external source");
    Require(fs::is_regular_file(
            root / fs::u8path(imported.assetProjectRelativePath)),
        "creator texture import did not create the governed .rasset");

    CreatorAssetWorkflowService catalogueWorkflow;
    AssetCatalogue catalogue;
    std::string error;
    Require(catalogueWorkflow.BuildCatalogue(
            root.generic_u8string(), ProjectId, catalogue, error),
        "creator catalogue failed after texture import: " + error);
    Require(creator.EnrichTextureCatalogue(
            root.generic_u8string(), ProjectId, catalogue, error),
        "texture catalogue metadata enrichment failed: " + error);
    AssetCatalogueQuery pngQuery;
    pngQuery.sourceFormat = "png";
    const auto pngMatches = QueryAssetCatalogue(catalogue, pngQuery);
    Require(std::any_of(pngMatches.begin(), pngMatches.end(),
            [&imported](const AssetCatalogueEntry& entry)
            {
                return entry.assetId == imported.assetId;
            }),
        "Studio PNG format filter cannot find the governed texture product");

    ResourceAssetMetadataDocument degradedMetadata;
    Require(ReadResourceAssetMetadata(
            root.generic_u8string(), ProjectId, degradedMetadata, error),
        "could not read Gate 2 resource metadata for degradation proof: " + error);
    degradedMetadata.records.erase(
        std::remove_if(degradedMetadata.records.begin(), degradedMetadata.records.end(),
            [&imported](const ResourceAssetMetadataRecord& record)
            {
                return record.assetId == imported.assetId;
            }),
        degradedMetadata.records.end());
    std::string degradedJson;
    Require(SerializeResourceAssetMetadata(
            degradedMetadata, degradedJson, error),
        "could not serialize degraded resource metadata: " + error);
    {
        std::ofstream stream(root / ResourceAssetMetadataDocumentName,
            std::ios::binary | std::ios::trunc);
        stream << degradedJson;
        Require(static_cast<bool>(stream),
            "could not persist degraded resource metadata proof fixture");
    }
    AssetCatalogue degradedCatalogue;
    Require(catalogueWorkflow.BuildCatalogue(
            root.generic_u8string(), ProjectId, degradedCatalogue, error),
        "creator catalogue failed before degraded enrichment: " + error);
    const std::size_t catalogueEntriesBeforeDegradedEnrichment =
        degradedCatalogue.entries.size();
    std::string enrichmentWarning;
    Require(creator.EnrichTextureCatalogue(
            root.generic_u8string(), ProjectId,
            degradedCatalogue, enrichmentWarning),
        "degraded texture metadata must not make catalogue unusable");
    const auto degradedTexture = std::find_if(
        degradedCatalogue.entries.begin(), degradedCatalogue.entries.end(),
        [&imported](const AssetCatalogueEntry& entry)
        {
            return entry.assetId == imported.assetId;
        });
    Require(degradedCatalogue.entries.size() ==
            catalogueEntriesBeforeDegradedEnrichment &&
            degradedTexture != degradedCatalogue.entries.end() &&
            degradedTexture->state == AssetCatalogueState::Invalid &&
            degradedTexture->sourceFormat.empty() &&
            !enrichmentWarning.empty(),
        "missing texture metadata blanked or hid the catalogue instead of flagging the texture INVALID");

    PreparedMaterialTextureAsset prepared;
    Require(PrepareMaterialTextureAsset(
            root.generic_u8string(), ProjectId, imported.assetId,
            prepared, error),
        "stable texture product did not prepare: " + error);
    Require(prepared.assetId == imported.assetId &&
            prepared.projectId == ProjectId &&
            prepared.sourceFormat == ResourceSourceFormat::Png &&
            prepared.payload == PngBytes(),
        "prepared material texture did not preserve stable identity/payload");
    Require(fs::u8path(prepared.logicalResourceName).extension() == ".png",
        "prepared Wicked logical resource name lost the source extension");

    wi::scene::Scene scene;
    const wi::ecs::Entity materialEntity = wi::ecs::CreateEntity();
    auto& material = scene.materials.Create(materialEntity);
    auto& baseColor = material.textures[
        wi::scene::MaterialComponent::BASECOLORMAP];
    baseColor.name = "authored/before.png";
    baseColor.uvset = 2;

    CommandService commands;
    auto command = std::make_unique<SetMaterialBaseColorTextureAssetCommand>(
        scene, materialEntity, prepared, FakeLoader);
    Require(commands.Execute(std::move(command)),
        "stable texture assignment command failed");
    Require(baseColor.name.empty() && baseColor.resource.IsValid() &&
            baseColor.uvset == 2,
        "base-colour assignment did not use in-memory governed payload or preserve UV set");
    auto* metadata = scene.metadatas.GetComponent(materialEntity);
    Require(metadata != nullptr &&
            metadata->int_values.get(
                MaterialTextureAssetBindingVersionMetadataKey) ==
                MaterialTextureAssetBindingVersion &&
            metadata->string_values.get(
                MaterialBaseColorTextureAssetIdMetadataKey) == imported.assetId,
        "material did not retain persistent stable texture identity metadata");
    Require(commands.IsDirty() && commands.UndoCount() == 1,
        "texture assignment did not enter normal creator command history");

    metadata->string_values.set("audit.unrelated", "preserve-me");
    Require(commands.Undo(), "texture assignment Undo failed");
    metadata = scene.metadatas.GetComponent(materialEntity);
    Require(baseColor.name == "authored/before.png" && baseColor.uvset == 2 &&
            metadata != nullptr &&
            metadata->string_values.has("audit.unrelated") &&
            metadata->string_values.get("audit.unrelated") == "preserve-me" &&
            !metadata->string_values.has(
                MaterialBaseColorTextureAssetIdMetadataKey) &&
            !metadata->int_values.has(
                MaterialTextureAssetBindingVersionMetadataKey),
        "texture assignment Undo did not preserve unrelated metadata");
    Require(commands.Redo(), "texture assignment Redo failed");
    Require(baseColor.name.empty() && baseColor.resource.IsValid(),
        "texture assignment Redo did not restore governed texture state");

    std::vector<MaterialTextureBindingRecord> bindings;
    Require(InspectMaterialTextureBindings(scene, bindings, error) &&
            bindings.size() == 1 &&
            bindings.front().materialEntity == materialEntity &&
            bindings.front().baseColorTextureAssetId == imported.assetId,
        "stable texture binding inspection did not recover the assigned material");

    const fs::path scenePath = root / "Content" / "Gate3Texture.wiscene";
    {
        wi::Archive archive(scenePath.generic_u8string(), false, false);
        Require(archive.IsOpen(), "could not create Gate 3 WISCENE archive");
        archive.SetCompressionEnabled(true);
        scene.Serialize(archive);
        Require(archive.SaveFile(scenePath.generic_u8string()),
            "could not save Gate 3 WISCENE archive");
        archive = wi::Archive();
    }

    wi::scene::Scene reopened;
    {
        wi::Archive archive(scenePath.generic_u8string(), true, false);
        Require(archive.IsOpen(), "could not reopen Gate 3 WISCENE archive");
        reopened.Serialize(archive);
        Require(archive.GetPos() == archive.GetSize(),
            "Gate 3 WISCENE did not deserialize completely");
    }
    bindings.clear();
    Require(InspectMaterialTextureBindings(reopened, bindings, error) &&
            bindings.size() == 1 &&
            bindings.front().baseColorTextureAssetId == imported.assetId,
        "WISCENE Save/Open lost the stable governed texture ID");
    auto* reopenedMaterial = reopened.materials.GetComponent(
        bindings.empty() ? wi::ecs::INVALID_ENTITY : bindings.front().materialEntity);
    Require(reopenedMaterial != nullptr,
        "WISCENE Save/Open lost the bound material entity");
    if (reopenedMaterial != nullptr)
    {
        Require(reopenedMaterial->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].name.empty(),
            "WISCENE serialized a governed texture as a fake external filename");
    }

    fakeLoaderCalls = 0;
    const auto restored = RestoreMaterialTextureBindings(
        reopened, root.generic_u8string(), ProjectId, FakeLoader);
    Require(restored.succeeded && restored.discovered == 1 && restored.restored == 1 &&
            restored.uniqueAssetIds == 1 && restored.preparedUnique == 1 &&
            restored.loadedUnique == 1 && fakeLoaderCalls == 1,
        "WISCENE governed texture did not rehydrate once by stable ID: " + restored.error);
    reopenedMaterial = reopened.materials.GetComponent(bindings.front().materialEntity);
    Require(reopenedMaterial != nullptr &&
            reopenedMaterial->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid(),
        "rehydrated WISCENE material has no live governed texture resource");
    const auto secondRestore = RestoreMaterialTextureBindings(
        reopened, root.generic_u8string(), ProjectId, FakeLoader);
    Require(secondRestore.succeeded && secondRestore.discovered == 1 &&
            secondRestore.restored == 0 && secondRestore.alreadyLive == 1 &&
            fakeLoaderCalls == 1,
        "material texture restore is not idempotent once the resource is live");

    wi::scene::Scene deduplicated;
    const wi::ecs::Entity dedupA = wi::ecs::CreateEntity();
    const wi::ecs::Entity dedupB = wi::ecs::CreateEntity();
    deduplicated.materials.Create(dedupA).textures[
        wi::scene::MaterialComponent::BASECOLORMAP].name =
        "legacy/original/source-a.png";
    deduplicated.materials.Create(dedupB).textures[
        wi::scene::MaterialComponent::BASECOLORMAP].name =
        "legacy/original/source-b.png";
    for (const auto entity : {dedupA, dedupB})
    {
        auto& dedupMetadata = deduplicated.metadatas.Create(entity);
        dedupMetadata.int_values.set(
            MaterialTextureAssetBindingVersionMetadataKey,
            MaterialTextureAssetBindingVersion);
        dedupMetadata.string_values.set(
            MaterialBaseColorTextureAssetIdMetadataKey,
            imported.assetId);
    }
    fakeLoaderCalls = 0;
    const auto dedupRestore = RestoreMaterialTextureBindings(
        deduplicated, root.generic_u8string(), ProjectId, FakeLoader);
    const auto* dedupMaterialA = deduplicated.materials.GetComponent(dedupA);
    const auto* dedupMaterialB = deduplicated.materials.GetComponent(dedupB);
    Require(dedupRestore.succeeded && dedupRestore.discovered == 2 &&
            dedupRestore.restored == 2 && dedupRestore.uniqueAssetIds == 1 &&
            dedupRestore.preparedUnique == 1 && dedupRestore.loadedUnique == 1 &&
            fakeLoaderCalls == 1 && dedupMaterialA != nullptr &&
            dedupMaterialB != nullptr &&
            dedupMaterialA->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].name.empty() &&
            dedupMaterialB->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].name.empty() &&
            dedupMaterialA->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid() &&
            dedupMaterialB->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid(),
        "duplicate StableId bindings were not prepared/loaded once and fanned out to both materials");

    wi::scene::Scene partial;
    const wi::ecs::Entity badMaterialEntity = wi::ecs::CreateEntity();
    partial.materials.Create(badMaterialEntity);
    auto& badMetadata = partial.metadatas.Create(badMaterialEntity);
    badMetadata.int_values.set(
        MaterialTextureAssetBindingVersionMetadataKey,
        MaterialTextureAssetBindingVersion);
    badMetadata.string_values.set(
        MaterialBaseColorTextureAssetIdMetadataKey,
        "83333333-3333-4333-8333-333333333333");
    const wi::ecs::Entity goodMaterialEntity = wi::ecs::CreateEntity();
    partial.materials.Create(goodMaterialEntity);
    auto& goodMetadata = partial.metadatas.Create(goodMaterialEntity);
    goodMetadata.int_values.set(
        MaterialTextureAssetBindingVersionMetadataKey,
        MaterialTextureAssetBindingVersion);
    goodMetadata.string_values.set(
        MaterialBaseColorTextureAssetIdMetadataKey,
        imported.assetId);
    fakeLoaderCalls = 0;
    const auto partialRestore = RestoreMaterialTextureBindings(
        partial, root.generic_u8string(), ProjectId, FakeLoader);
    Require(!partialRestore.succeeded && partialRestore.discovered == 2 &&
            partialRestore.restored == 1 && !partialRestore.error.empty() &&
            partial.materials.GetComponent(goodMaterialEntity)->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid(),
        "one corrupt texture binding prevented a later valid binding from restoring");

    fs::remove_all(root, ec);
    fs::remove_all(externalRoot, ec);
    if (failures != 0)
    {
        std::cerr << "LP08 GATE 3 MATERIAL TEXTURE FAIL // "
                  << failures << " checks failed\n";
        return 1;
    }
    std::cout << "LP08 GATE 3 MATERIAL TEXTURE PASS // stable_id save_open undo_redo dedup_restore audit_regressions\n";
    return 0;
}
