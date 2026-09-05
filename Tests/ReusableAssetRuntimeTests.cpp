#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/ReusableAssetRuntimeService.h"
#include "renegade/bridge/ReusableAssetService.h"

#include <cstdint>
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
    constexpr const char* ProjectId = "77777777-7777-4777-8777-777777777777";
    constexpr const char* ProductId = "88888888-8888-4888-8888-888888888888";
    constexpr const char* SourceId = "99999999-9999-4999-8999-999999999999";
    constexpr const char* MissingProductId = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 GATE 6 RUNTIME TEST FAIL // " << message << '\n';
        return false;
    }

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        std::uint64_t hash = FnvOffset;
        for (const std::uint8_t value : bytes)
        {
            hash ^= value;
            hash *= FnvPrime;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        if (!bytes.empty())
        {
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        return static_cast<bool>(output);
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(output);
    }

    std::vector<std::uint8_t> SerializeScenePayload(const std::string& rootName)
    {
        wi::scene::Scene scene;
        scene.Entity_CreateTransform(rootName);
        wi::Archive archive;
        scene.Serialize(archive);
        std::vector<std::uint8_t> bytes;
        archive.WriteData(bytes);
        return bytes;
    }

    wi::allocator::shared_ptr<wi::scene::Scene> MakePreparedScene(
        const std::string& rootName)
    {
        auto scene = wi::allocator::make_shared_single<wi::scene::Scene>();
        scene->Entity_CreateTransform(rootName);
        return scene;
    }

    bool PlaceOldPayload(
        wi::scene::Scene& scene,
        const renegade::bridge::StableId& assetId,
        const float x,
        wi::ecs::Entity& wrapper)
    {
        using namespace renegade::bridge;
        PlaceReusableModelCommand command(
            scene,
            MakePreparedScene("old-payload-root"),
            assetId,
            XMFLOAT3(x, 0.0f, 0.0f),
            1.0f);
        if (!command.Execute())
            return false;
        wrapper = command.PlacedEntity();
        return wrapper != wi::ecs::INVALID_ENTITY;
    }

    bool HasTranslationX(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const float expected)
    {
        const auto* transform = scene.transforms.GetComponent(entity);
        return transform != nullptr && transform->translation_local.x == expected;
    }
}

int main(int argc, char** argv)
{
    using namespace renegade::bridge;

    if (argc != 2)
    {
        std::cerr << "Usage: RenegadeReusableAssetRuntimeTests <output-directory>\n";
        return 2;
    }

    const fs::path packageRoot = fs::absolute(fs::u8path(argv[1]));
    const fs::path productPath =
        packageRoot / "GameData" / "Content" / "Models" / "runtime-test.rasset";
    const fs::path contentManifest =
        packageRoot / "GameData" / "content-manifest.json";

    std::error_code ec;
    fs::remove_all(packageRoot, ec);
    ec.clear();
    fs::create_directories(productPath.parent_path(), ec);
    if (!Require(!ec, "could not create package fixture"))
        return 1;

    ReusableModelAssetDocument asset;
    asset.manifest.projectId = ProjectId;
    asset.manifest.assetId = ProductId;
    asset.manifest.sourceAssetId = SourceId;
    asset.manifest.sourceFormat = "fbx";
    asset.manifest.importer = "wicked.ufbx";
    asset.manifest.importerVersion = 1;
    asset.manifest.settingsSchema = ReusableModelImportSettingsSchema;
    asset.manifest.settingsVersion = 1;
    asset.manifest.settingsJson =
        "{\"options\":{},\"source_format\":\"fbx\"}";
    asset.payload = SerializeScenePayload("new-payload-root");
    asset.manifest.payloadHash = HashBytes(asset.payload);

    std::string error;
    std::vector<std::uint8_t> productBytes;
    if (!Require(!asset.payload.empty(), "could not serialize current payload scene") ||
        !Require(SerializeReusableModelAssetDocument(
                asset, productBytes, error),
            "could not serialize packaged RAsset: " + error) ||
        !Require(WriteBytes(productPath, productBytes),
            "could not write packaged RAsset"))
    {
        return 1;
    }

    const std::string manifestJson =
        std::string("{\"files\":[{\"asset_id\":\"") + ProductId +
        "\",\"path\":\"GameData/Content/Models/runtime-test.rasset\","
        "\"source_hash\":\"" + HashBytes(productBytes) +
        "\"}],\"format\":\"renegade-content-manifest\","
        "\"project_id\":\"" + ProjectId + "\",\"schema_version\":1}";
    if (!Require(WriteText(contentManifest, manifestJson),
            "could not write content manifest"))
        return 1;

    wi::scene::Scene scene;
    wi::ecs::Entity firstWrapper = wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity secondWrapper = wi::ecs::INVALID_ENTITY;
    if (!Require(PlaceOldPayload(scene, ProductId, 3.0f, firstWrapper),
            "could not create first last-good instance") ||
        !Require(PlaceOldPayload(scene, ProductId, 9.0f, secondWrapper),
            "could not create second last-good instance"))
    {
        return 1;
    }

    PackagedReusableAssetRefreshResult refreshed;
    if (!Require(RefreshPackagedReusableAssetInstances(
            scene,
            packageRoot.generic_u8string(),
            ProjectId,
            refreshed,
            error),
            "packaged Runtime refresh failed: " + error) ||
        !Require(refreshed.discoveredInstanceCount == 2 &&
                refreshed.refreshedInstanceCount == 2 &&
                refreshed.records.size() == 2,
            "packaged Runtime did not refresh both repeated instances") ||
        !Require(HasTranslationX(scene, firstWrapper, 3.0f) &&
                HasTranslationX(scene, secondWrapper, 9.0f),
            "packaged Runtime refresh changed creator-authored wrapper transforms") ||
        !Require(scene.Entity_FindByName("old-payload-root", firstWrapper) ==
                    wi::ecs::INVALID_ENTITY &&
                scene.Entity_FindByName("old-payload-root", secondWrapper) ==
                    wi::ecs::INVALID_ENTITY,
            "old last-good payload survived after successful replacement") ||
        !Require(scene.Entity_FindByName("new-payload-root", firstWrapper) !=
                    wi::ecs::INVALID_ENTITY &&
                scene.Entity_FindByName("new-payload-root", secondWrapper) !=
                    wi::ecs::INVALID_ENTITY,
            "current packaged payload was not installed under both wrappers") ||
        !Require(refreshed.records[0].payloadHash == asset.manifest.payloadHash &&
                refreshed.records[1].payloadHash == asset.manifest.payloadHash,
            "Runtime refresh evidence does not identify the current packaged payload hash"))
    {
        return 1;
    }

    // Failure preservation: an unresolved stable ID must be rejected before
    // any old payload is removed or wrapper state is changed.
    wi::scene::Scene failureScene;
    wi::ecs::Entity failureWrapper = wi::ecs::INVALID_ENTITY;
    if (!Require(PlaceOldPayload(
            failureScene, MissingProductId, 15.0f, failureWrapper),
            "could not create failure-preservation instance"))
        return 1;
    std::vector<ReusableAssetInstanceRecord> before;
    if (!Require(InspectReusableAssetInstances(failureScene, before, error) &&
            before.size() == 1,
            "could not inspect pre-failure instance: " + error))
        return 1;
    const wi::ecs::Entity oldPayload = before.front().payloadRoot;

    PackagedReusableAssetRefreshResult rejected;
    const bool rejectedResult = RefreshPackagedReusableAssetInstances(
        failureScene,
        packageRoot.generic_u8string(),
        ProjectId,
        rejected,
        error);
    std::vector<ReusableAssetInstanceRecord> after;
    if (!Require(!rejectedResult &&
            error.find("absent from the packaged content manifest") !=
                std::string::npos,
            "missing packaged stable ID did not fail closed: " + error) ||
        !Require(InspectReusableAssetInstances(failureScene, after, error) &&
                after.size() == 1 &&
                after.front().instanceRoot == failureWrapper &&
                after.front().payloadRoot == oldPayload,
            "failed Runtime refresh changed last-good instance/payload identity") ||
        !Require(HasTranslationX(failureScene, failureWrapper, 15.0f) &&
                failureScene.Entity_FindByName(
                    "old-payload-root", failureWrapper) != wi::ecs::INVALID_ENTITY,
            "failed Runtime refresh changed wrapper state or removed last-good payload"))
    {
        return 1;
    }

    fs::remove_all(packageRoot, ec);
    std::cout << "LP07 GATE 6 PACKAGED RUNTIME REFRESH PASS\n";
    return 0;
}
