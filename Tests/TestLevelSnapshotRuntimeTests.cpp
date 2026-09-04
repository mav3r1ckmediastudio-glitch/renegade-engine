#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "RuntimeBootstrap.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/TestLevelSnapshotService.h"

namespace
{
    namespace fs = std::filesystem;

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    struct TemporaryDirectory
    {
        fs::path path;

        ~TemporaryDirectory()
        {
            std::error_code error;
            fs::remove_all(path, error);
        }
    };

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            return {};
        }
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            return {};
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            if (!stream)
            {
                return {};
            }
        }
        return bytes;
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

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(stream);
    }

    bool WriteEmptyRegistry(
        const fs::path& root,
        const renegade::bridge::StableId& projectId)
    {
        renegade::bridge::AssetRegistry registry;
        registry.projectId = projectId;
        registry.schemaVersion =
            renegade::bridge::AssetRegistry::CurrentSchemaVersion;
        std::string json;
        std::string error;
        if (!renegade::bridge::SerializeAssetRegistry(registry, json, error))
            return false;
        std::ofstream stream(
            root / renegade::bridge::AssetRegistryDocumentName,
            std::ios::binary | std::ios::trunc);
        stream << json;
        return static_cast<bool>(stream);
    }

    wi::Resource FakeTextureLoader(
        const renegade::bridge::PreparedMaterialTextureAsset& prepared,
        std::string& error)
    {
        wi::vector<std::uint8_t> bytes;
        bytes.assign(prepared.payload.begin(), prepared.payload.end());
        wi::Resource resource;
        resource.SetFileData(std::move(bytes));
        if (!resource.IsValid())
        {
            error = "fake Test Level texture loader could not retain payload bytes";
            return {};
        }
        error.clear();
        return resource;
    }

    bool FindTranslationX(
        const renegade::bridge::SceneService& scenes,
        const std::string& name,
        float& value)
    {
        for (const auto& entity : scenes.ListEntities())
        {
            if (entity.name != name)
            {
                continue;
            }
            const auto* transform =
                scenes.GetScene().transforms.GetComponent(entity.entity);
            if (transform == nullptr)
            {
                return false;
            }
            value = transform->translation_local.x;
            return true;
        }
        return false;
    }
}

int main()
{
    TemporaryDirectory fixture;
    fixture.path = fs::temp_directory_path() /
        ("renegade-lp04-runtime-snapshot-" +
            std::to_string(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()));

    const fs::path projectRoot = fixture.path / "Project";
    const fs::path scenePath =
        projectRoot / "Content" / "Scenes" / "Authoritative.wiscene";
    fs::create_directories(scenePath.parent_path());

    renegade::bridge::ProjectMetadata project;
    project.formatVersion = renegade::bridge::ProjectService::CurrentFormatVersion;
    project.projectId = "44444444-4444-4444-8444-444444444444";
    project.name = "LP04 Test Level Fixture";
    project.descriptorPath =
        (projectRoot / "LP04Fixture.renegade").generic_u8string();
    project.rootPath = projectRoot.generic_u8string();
    project.startupScene = "Content/Scenes/Authoritative.wiscene";
    // A real project may have Game Start roots. Test Level must deliberately
    // bypass them and boot the captured scene directly.
    project.startupFlowId = "55555555-5555-4555-8555-555555555555";
    project.startupFlow = "Content/Flow/Main.renegade-flow";
    project.startupScreenId = "66666666-6666-4666-8666-666666666666";
    project.startupScreen = "Content/UI/Main.renegade-screen";

    fs::create_directories(projectRoot / "SourceAssets");
    fs::create_directories(projectRoot / "Intermediate" / "Transactions");
    if (!WriteEmptyRegistry(projectRoot, project.projectId))
    {
        return Fail("LP04 material parity fixture could not create the asset registry");
    }

    const fs::path externalTexture = fixture.path / "test-level-base.png";
    if (!WriteBytes(externalTexture, PngBytes()))
        return Fail("LP04 material parity fixture could not write its texture source");

    renegade::bridge::CreatorTextureWorkflowService textureWorkflow;
    const auto importedTexture = textureWorkflow.ImportTexture(
        projectRoot.generic_u8string(),
        project.projectId,
        externalTexture.generic_u8string());
    if (!importedTexture.succeeded || !importedTexture.committed)
    {
        std::cerr << importedTexture.error << '\n';
        return Fail("LP04 material parity fixture could not import a governed texture");
    }

    renegade::bridge::PreparedMaterialTextureAsset preparedTexture;
    std::string materialError;
    if (!renegade::bridge::PrepareMaterialTextureAsset(
            projectRoot.generic_u8string(),
            project.projectId,
            importedTexture.assetId,
            preparedTexture,
            materialError))
    {
        std::cerr << materialError << '\n';
        return Fail("LP04 material parity fixture could not prepare its governed texture");
    }

    renegade::bridge::StudioSession session;
    const auto landmark = wi::ecs::CreateEntity();
    session.Scenes().GetScene().names.Create(landmark) = "Runtime Landmark";
    auto& transform = session.Scenes().GetScene().transforms.Create(landmark);
    transform.translation_local = XMFLOAT3(1.0f, 2.0f, 3.0f);
    transform.SetDirty();
    transform.UpdateTransform();

    const wi::ecs::Entity governedMaterial = wi::ecs::CreateEntity();
    session.Scenes().GetScene().materials.Create(governedMaterial);
    if (!renegade::bridge::ApplyPreparedMaterialTextureAsset(
            session.Scenes().GetScene(),
            governedMaterial,
            renegade::bridge::MaterialTextureSlot::BaseColor,
            preparedTexture,
            FakeTextureLoader,
            materialError))
    {
        std::cerr << materialError << '\n';
        return Fail("LP04 material parity fixture could not bind its governed texture");
    }

    if (!session.SaveScene(scenePath.generic_u8string()))
    {
        return Fail("authoritative LP04 Runtime fixture scene could not be saved");
    }
    const auto authoritativeBytes = ReadBytes(scenePath);
    if (authoritativeBytes.empty())
    {
        return Fail("authoritative LP04 Runtime fixture bytes could not be read");
    }

    if (!session.Commands().Execute(
            std::make_unique<renegade::bridge::SetTranslationCommand>(
                session.Scenes().GetScene(),
                landmark,
                XMFLOAT3(9.0f, 2.0f, 3.0f))))
    {
        return Fail("unsaved LP04 Runtime edit did not execute");
    }

    const std::size_t undoBeforeSnapshot = session.Commands().UndoCount();
    const std::size_t redoBeforeSnapshot = session.Commands().RedoCount();
    const std::string pathBeforeSnapshot = session.Scenes().CurrentPath();

    renegade::bridge::TestLevelSnapshotService snapshots(
        session.Scenes(),
        session.Commands());
    renegade::bridge::TestLevelSnapshot snapshot;
    std::string snapshotError;
    if (!snapshots.Create(project, snapshot, snapshotError))
    {
        std::cerr << snapshotError << '\n';
        return Fail("LP04 Runtime-ready snapshot creation failed");
    }
    if (!snapshot.IsRuntimeReady() ||
        !fs::is_regular_file(snapshot.scenePath) ||
        !fs::is_regular_file(snapshot.descriptorPath))
    {
        return Fail("LP04 snapshot did not produce a Runtime-ready shadow project");
    }

    const fs::path snapshotRoot = fs::u8path(snapshot.sessionDirectory);
    if (!fs::is_regular_file(
            snapshotRoot / renegade::bridge::AssetRegistryDocumentName) ||
        !fs::is_regular_file(
            snapshotRoot / fs::u8path(importedTexture.assetProjectRelativePath)))
    {
        return Fail("LP04 Test Level snapshot omitted governed material Runtime inputs");
    }

    renegade::bridge::ProjectService inspector;
    renegade::bridge::ProjectMetadata inspected;
    std::string inspectError;
    if (!inspector.InspectProject(snapshot.descriptorPath, inspected, inspectError))
    {
        std::cerr << inspectError << '\n';
        return Fail("LP04 shadow project was rejected by ProjectService");
    }
    if (inspected.projectId != project.projectId ||
        inspected.name != project.name ||
        fs::u8path(inspected.rootPath).lexically_normal() !=
            fs::u8path(snapshot.sessionDirectory).lexically_normal() ||
        inspected.startupScene != "Content/Scenes/TestLevel.wiscene" ||
        !inspected.startupFlowId.empty() ||
        !inspected.startupFlow.empty() ||
        !inspected.startupScreenId.empty() ||
        !inspected.startupScreen.empty())
    {
        return Fail("LP04 shadow project metadata was not isolated for Test Level");
    }

    // Use the existing project-aware Runtime contract unchanged. Graphics flags
    // remain Wicked-owned; Runtime bootstrap only consumes --project.
    auto launch = renegade::runtime::ParseRuntimeLaunchArguments(
        {"dx12", "--project", snapshot.descriptorPath});
    if (!launch.succeeded)
    {
        std::cerr << launch.message << '\n';
        return Fail("LP04 shadow project launch arguments were rejected");
    }
    auto resolved = renegade::runtime::ResolveRuntimeProject(launch);
    if (!resolved.succeeded)
    {
        std::cerr << resolved.message << '\n';
        return Fail("existing Runtime bootstrap rejected the LP04 shadow project");
    }
    if (fs::u8path(resolved.startupScenePath).lexically_normal() !=
            fs::u8path(snapshot.scenePath).lexically_normal() ||
        !resolved.startupFlowPath.empty() ||
        !resolved.startupScreenPath.empty())
    {
        return Fail("Runtime did not resolve the LP04 snapshot as direct startup");
    }

    renegade::bridge::SceneService runtimeScenes;
    auto loaded = renegade::runtime::LoadRuntimeProjectScene(
        runtimeScenes,
        resolved,
        FakeTextureLoader);
    if (!loaded.succeeded)
    {
        std::cerr << loaded.message << '\n';
        return Fail("existing Runtime scene loader rejected the LP04 snapshot");
    }
    std::vector<renegade::bridge::MaterialTextureBindingRecord> runtimeBindings;
    std::string runtimeMaterialError;
    if (!renegade::bridge::InspectMaterialTextureBindings(
            runtimeScenes.GetScene(), runtimeBindings, runtimeMaterialError) ||
        runtimeBindings.size() != 1)
    {
        std::cerr << runtimeMaterialError << '\n';
        return Fail("Runtime did not preserve the Test Level governed material binding");
    }
    const auto* runtimeMaterial = runtimeScenes.GetScene().materials.GetComponent(
        runtimeBindings.front().materialEntity);
    if (runtimeMaterial == nullptr ||
        !runtimeMaterial->textures[
            wi::scene::MaterialComponent::BASECOLORMAP].resource.IsValid())
    {
        return Fail("Runtime did not rehydrate the Test Level governed material resource");
    }

    float runtimeX = 0.0f;
    if (!FindTranslationX(runtimeScenes, "Runtime Landmark", runtimeX) ||
        !NearlyEqual(runtimeX, 9.0f))
    {
        return Fail("Runtime did not load the unsaved LP04 scene state");
    }

    if (ReadBytes(scenePath) != authoritativeBytes ||
        session.Scenes().CurrentPath() != pathBeforeSnapshot ||
        !session.Commands().IsDirty() ||
        session.Commands().UndoCount() != undoBeforeSnapshot ||
        session.Commands().RedoCount() != redoBeforeSnapshot)
    {
        return Fail("LP04 Runtime snapshot changed authoritative editor state");
    }

    const fs::path successfulSession = fs::u8path(snapshot.sessionDirectory);
    if (!snapshots.Cleanup(snapshot, snapshotError))
    {
        std::cerr << snapshotError << '\n';
        return Fail("LP04 Runtime snapshot cleanup failed");
    }
    if (fs::exists(successfulSession) ||
        fs::exists(projectRoot / "Intermediate" / "TestLevelSnapshots"))
    {
        return Fail("LP04 Runtime snapshot cleanup left temporary data behind");
    }

    // Prove a failure after the descriptor physically exists still cleans the
    // whole session and leaves the unsaved authoring document untouched.
    if (!session.Commands().Execute(
            std::make_unique<renegade::bridge::SetTranslationCommand>(
                session.Scenes().GetScene(),
                landmark,
                XMFLOAT3(12.0f, 2.0f, 3.0f))))
    {
        return Fail("LP04 descriptor-failure edit did not execute");
    }
    const std::size_t undoBeforeFailure = session.Commands().UndoCount();
    const std::size_t redoBeforeFailure = session.Commands().RedoCount();
    renegade::bridge::TestLevelSnapshot failedSnapshot;
    if (snapshots.Create(
            project,
            failedSnapshot,
            snapshotError,
            renegade::bridge::TestLevelSnapshotFailureInjection::
                AfterDescriptorWrite))
    {
        return Fail("LP04 forced descriptor failure unexpectedly succeeded");
    }
    if (failedSnapshot.IsValid() || failedSnapshot.IsRuntimeReady())
    {
        return Fail("LP04 descriptor failure returned a live snapshot handle");
    }
    if (fs::exists(projectRoot / "Intermediate" / "TestLevelSnapshots"))
    {
        return Fail("LP04 descriptor failure left temporary snapshot data behind");
    }
    if (ReadBytes(scenePath) != authoritativeBytes ||
        session.Scenes().CurrentPath() != pathBeforeSnapshot ||
        !session.Commands().IsDirty() ||
        session.Commands().UndoCount() != undoBeforeFailure ||
        session.Commands().RedoCount() != redoBeforeFailure)
    {
        return Fail("LP04 descriptor failure changed authoritative editor state");
    }

    std::cout << "PASS: LP04 Runtime-ready unsaved Test Level snapshot project\n";
    return 0;
}
