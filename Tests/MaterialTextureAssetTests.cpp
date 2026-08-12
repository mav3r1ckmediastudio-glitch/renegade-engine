#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"

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

    PreparedMaterialTextureAsset prepared;
    std::string error;
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
    auto* commandView = command.get();
    Require(commands.Execute(std::move(command)),
        "stable texture assignment command failed: " + commandView->Error());
    Require(baseColor.name.empty() && baseColor.resource.IsValid() &&
            baseColor.uvset == 2,
        "base-colour assignment did not use in-memory governed payload or preserve UV set");
    const auto* metadata = scene.metadatas.GetComponent(materialEntity);
    Require(metadata != nullptr &&
            metadata->int_values.get(
                MaterialTextureAssetBindingVersionMetadataKey) ==
                MaterialTextureAssetBindingVersion &&
            metadata->string_values.get(
                MaterialBaseColorTextureAssetIdMetadataKey) == imported.assetId,
        "material did not retain persistent stable texture identity metadata");
    Require(commands.IsDirty() && commands.UndoCount() == 1,
        "texture assignment did not enter normal creator command history");

    Require(commands.Undo(), "texture assignment Undo failed");
    Require(baseColor.name == "authored/before.png" && baseColor.uvset == 2 &&
            scene.metadatas.GetComponent(materialEntity) == nullptr,
        "texture assignment Undo did not restore exact prior material/metadata state");
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

    const auto restored = RestoreMaterialTextureBindings(
        reopened, root.generic_u8string(), ProjectId, FakeLoader);
    Require(restored.succeeded && restored.discovered == 1 && restored.restored == 1,
        "WISCENE governed texture did not rehydrate by stable ID: " + restored.error);
    reopenedMaterial = reopened.materials.GetComponent(bindings.front().materialEntity);
    Require(reopenedMaterial != nullptr &&
            reopenedMaterial->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid(),
        "rehydrated WISCENE material has no live governed texture resource");
    const auto secondRestore = RestoreMaterialTextureBindings(
        reopened, root.generic_u8string(), ProjectId, FakeLoader);
    Require(secondRestore.succeeded && secondRestore.discovered == 1 &&
            secondRestore.restored == 0,
        "material texture restore is not idempotent once the resource is live");

    fs::remove_all(root, ec);
    fs::remove_all(externalRoot, ec);
    if (failures != 0)
    {
        std::cerr << "LP08 GATE 3 MATERIAL TEXTURE FAIL // "
                  << failures << " checks failed\n";
        return 1;
    }
    std::cout << "LP08 GATE 3 MATERIAL TEXTURE PASS // stable_id save_open undo_redo\n";
    return 0;
}
