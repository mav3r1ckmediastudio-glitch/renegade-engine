#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ResourceAssetDependencyService.h"
#include "renegade/bridge/ResourceAssetRuntimeService.h"
#include "renegade/bridge/ResourceAssetService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "a1111111-1111-4111-8111-111111111111";

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP08 GATE 5 PACKAGE RUNTIME FAIL // " << message << '\n';
        return false;
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

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        constexpr std::uint64_t Offset = 1469598103934665603ull;
        constexpr std::uint64_t Prime = 1099511628211ull;
        std::uint64_t hash = Offset;
        for (const auto value : bytes)
        {
            hash ^= value;
            hash *= Prime;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
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

    bool SaveScene(
        wi::scene::Scene& scene,
        const fs::path& path,
        std::string& error)
    {
        try
        {
            wi::Archive archive(path.generic_u8string(), false, false);
            if (!archive.IsOpen())
            {
                error = "could not create WISCENE archive";
                return false;
            }
            archive.SetCompressionEnabled(true);
            scene.Serialize(archive);
            if (!archive.SaveFile(path.generic_u8string()))
            {
                error = "could not save WISCENE archive";
                return false;
            }
            archive = wi::Archive();
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        error.clear();
        return true;
    }

    ResourceAssetImportResult Import(
        const fs::path& root,
        const std::string& sourcePath,
        const std::string& productPath,
        const ResourceSourceFormat format,
        const std::vector<std::uint8_t>& bytes)
    {
        WriteBytes(root / fs::u8path(sourcePath), bytes);
        ResourceAssetImportRequest request;
        request.projectRoot = root.generic_u8string();
        request.projectId = ProjectId;
        request.sourceProjectRelativePath = sourcePath;
        request.assetProjectRelativePath = productPath;
        request.expectedFormat = format;
        return ResourceAssetService().ImportResourceAsset(request);
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
            error = "fake loader could not retain packaged bytes";
            return {};
        }
        error.clear();
        return resource;
    }
}

int main(int argc, char** argv)
{
    using namespace renegade::bridge;

    const fs::path root = argc > 1
        ? fs::absolute(fs::u8path(argv[1]))
        : fs::temp_directory_path() / "renegade-lp08-gate5-package-runtime";
    std::error_code ec;
    fs::remove_all(root, ec);
    for (const char* folder : {
        "Content/Scenes", "Content/Textures", "Content/Audio",
        "Content/Scripts", "Content/Video", "Content/Fonts",
        "SourceAssets/Textures", "SourceAssets/Audio",
        "SourceAssets/Scripts", "SourceAssets/Video", "SourceAssets/Fonts",
        "Intermediate/Transactions"})
    {
        fs::create_directories(root / folder, ec);
    }
    if (!Require(!ec && WriteEmptyRegistry(root),
            "could not prepare Gate 5 project fixture"))
        return 1;

    const std::vector<std::uint8_t> png = {
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
        0,0,0,13,'I','H','D','R',0,0,0,2,0,0,0,3};
    const auto texture = Import(
        root, "SourceAssets/Textures/runtime.png",
        "Content/Textures/runtime.rasset",
        ResourceSourceFormat::Png, png);
    if (!Require(texture.succeeded,
            "texture import failed: " + texture.error))
        return 1;

    wi::scene::Scene scene;
    const wi::ecs::Entity materialEntity = wi::ecs::CreateEntity();
    scene.materials.Create(materialEntity);
    auto& metadata = scene.metadatas.Create(materialEntity);
    metadata.int_values.set(
        MaterialTextureAssetBindingVersionMetadataKey,
        MaterialTextureAssetBindingVersion);
    metadata.string_values.set(
        MaterialBaseColorTextureAssetIdMetadataKey,
        texture.assetId);
    // Regression: Creator Recovery persists governed texture IDs independently
    // for every material slot. Package Runtime must never read the Gate 1-era
    // baseColorTextureAssetId compatibility mirror for a Normal/Surface/etc
    // binding, and must restore the resource to the authored Wicked slot.
    metadata.string_values.set(
        MaterialNormalTextureAssetIdMetadataKey,
        texture.assetId);

    const fs::path scenePath = root / "Content/Scenes/Gate5.wiscene";
    std::string error;
    if (!Require(SaveScene(scene, scenePath, error),
            "could not save dependency scene: " + error))
        return 1;

    ResourceAssetDependencyProvider provider(ProjectId);
    DependencyCollector collector(root.generic_u8string());
    DependencyRoot dependencyRoot;
    dependencyRoot.declaredPath = "Content/Scenes/Gate5.wiscene";
    dependencyRoot.dependencyClass = DependencyClass::Scene;
    dependencyRoot.requirement = DependencyRequirement::Required;
    dependencyRoot.provenance = "test.scene";
    if (!Require(collector.RegisterProvider(provider, error) &&
            collector.AddRoot(dependencyRoot, error) &&
            collector.DiscoverRootDependencies(error),
            "resource dependency discovery failed: " + error))
        return 1;

    const auto& graph = collector.Graph();
    const auto productNode = std::find_if(
        graph.nodes.begin(), graph.nodes.end(),
        [&texture](const DependencyNode& node)
        { return node.projectRelativePath == texture.assetProjectRelativePath; });
    const auto sourceNode = std::find_if(
        graph.nodes.begin(), graph.nodes.end(),
        [&texture](const DependencyNode& node)
        { return node.projectRelativePath == texture.sourceProjectRelativePath; });
    if (!Require(productNode != graph.nodes.end() &&
            productNode->dependencyClass == DependencyClass::Texture &&
            productNode->requirement == DependencyRequirement::Required &&
            productNode->provider == "lp08.rasset",
            "material stable ID did not resolve to authoritative governed texture product") ||
        !Require(sourceNode == graph.nodes.end(),
            "retained texture source leaked into Runtime dependency closure"))
        return 1;

    struct GenericFixture
    {
        std::string source;
        std::string product;
        ResourceSourceFormat format;
        ResourceClass resourceClass;
        std::vector<std::uint8_t> bytes;
    };
    const std::vector<GenericFixture> generic = {
        {"SourceAssets/Audio/runtime.wav", "Content/Audio/runtime.rasset",
         ResourceSourceFormat::Wav, ResourceClass::Audio,
         {'R','I','F','F',0,0,0,0,'W','A','V','E',1}},
        {"SourceAssets/Scripts/runtime.lua", "Content/Scripts/runtime.rasset",
         ResourceSourceFormat::Lua, ResourceClass::Script,
         {'r','e','t','u','r','n',' ','1','\n'}},
        {"SourceAssets/Video/runtime.mp4", "Content/Video/runtime.rasset",
         ResourceSourceFormat::Mp4, ResourceClass::Video,
         {0,0,0,16,'f','t','y','p','i','s','o','m',1}},
        {"SourceAssets/Fonts/runtime.ttf", "Content/Fonts/runtime.rasset",
         ResourceSourceFormat::Ttf, ResourceClass::Font,
         {0,1,0,0,0,1,0,0,1}},
    };

    struct Accepted
    {
        GenericFixture fixture;
        ResourceAssetImportResult imported;
    };
    std::vector<Accepted> accepted;
    for (const auto& fixture : generic)
    {
        auto imported = Import(
            root, fixture.source, fixture.product,
            fixture.format, fixture.bytes);
        if (!Require(imported.succeeded,
                fixture.source + " import failed: " + imported.error))
            return 1;
        accepted.push_back({fixture, std::move(imported)});
    }

    const fs::path packageRoot = root / "Package";
    fs::create_directories(packageRoot / "GameData", ec);
    std::vector<std::pair<StableId, std::string>> packageEntries;
    const auto stageProduct = [&](const ResourceAssetImportResult& imported)
    {
        const std::string packagedPath =
            "GameData/" + imported.assetProjectRelativePath;
        const auto bytes = ReadBytes(root / fs::u8path(imported.assetProjectRelativePath));
        WriteBytes(packageRoot / fs::u8path(packagedPath), bytes);
        packageEntries.push_back({imported.assetId, packagedPath});
    };
    stageProduct(texture);
    for (const auto& item : accepted)
        stageProduct(item.imported);

    std::ostringstream manifest;
    manifest << "{\"files\":[";
    for (std::size_t index = 0; index < packageEntries.size(); ++index)
    {
        if (index != 0) manifest << ',';
        const auto productBytes = ReadBytes(
            packageRoot / fs::u8path(packageEntries[index].second));
        manifest << "{\"asset_id\":\"" << packageEntries[index].first
                 << "\",\"path\":\"" << packageEntries[index].second
                 << "\",\"source_hash\":\"" << HashBytes(productBytes)
                 << "\"}";
    }
    manifest << "],\"format\":\"renegade-content-manifest\","
             << "\"project_id\":\"" << ProjectId
             << "\",\"schema_version\":1}";
    WriteText(packageRoot / "GameData/content-manifest.json", manifest.str());

    PackagedResourceAsset packagedTexture;
    if (!Require(PreparePackagedResourceAsset(
            packageRoot.generic_u8string(), ProjectId, texture.assetId,
            packagedTexture, error),
            "packaged texture resolution failed: " + error) ||
        !Require(packagedTexture.resourceClass == ResourceClass::Texture &&
                packagedTexture.payload == png &&
                packagedTexture.payloadHash == texture.sourceHash,
            "packaged texture did not retain current governed payload identity"))
        return 1;

    for (const auto& item : accepted)
    {
        PackagedResourceAsset packaged;
        if (!Require(PreparePackagedResourceAsset(
                packageRoot.generic_u8string(), ProjectId,
                item.imported.assetId, packaged, error),
                item.fixture.product + " packaged resolution failed: " + error) ||
            !Require(packaged.resourceClass == item.fixture.resourceClass &&
                    packaged.sourceFormat == item.fixture.format &&
                    packaged.payload == item.fixture.bytes,
                item.fixture.product +
                    " did not use the generic packaged resource contract"))
            return 1;
    }

    PackagedMaterialTextureRefreshResult refreshed;
    if (!Require(RefreshPackagedMaterialTextureAssets(
            scene, packageRoot.generic_u8string(), ProjectId,
            refreshed, error, FakeLoader),
            "packaged material texture refresh failed: " + error) ||
        !Require(refreshed.discoveredBindingCount == 2 &&
                refreshed.refreshedBindingCount == 2 &&
                refreshed.records.size() == 2 &&
                std::all_of(refreshed.records.begin(), refreshed.records.end(),
                    [&texture](const PackagedMaterialTextureRefreshRecord& record)
                    { return record.assetId == texture.assetId &&
                        record.payloadHash == texture.sourceHash; }),
            "Runtime refresh evidence did not identify both authored texture-slot bindings") ||
        !Require(scene.materials.GetComponent(materialEntity)->textures[
                wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid(),
            "packaged Runtime did not install the governed Base Color resource") ||
        !Require(scene.materials.GetComponent(materialEntity)->textures[
                wi::scene::MaterialComponent::NORMALMAP].resource.IsValid(),
            "packaged Runtime did not install the governed Normal resource"))
        return 1;

    fs::remove_all(root, ec);
    std::cout << "LP08 GATE 5 PACKAGE RUNTIME PASS // stable_texture_dependency no_source_leak packaged_texture generic_audio_script_video_font multi_slot_runtime_refresh\n";
    return 0;
}
