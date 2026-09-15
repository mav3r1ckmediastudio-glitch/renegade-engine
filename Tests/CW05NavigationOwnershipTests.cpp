#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/PatrolRouteService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/TestLevelSnapshotService.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int Fail(const std::string& message)
    {
        std::cerr
            << "CW-05 TestGame navigation ownership test failed: "
            << message << '\n';
        return 1;
    }

    bool LiveAuthoredGridIntact(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity authoredGrid,
        const StableId& authoredId,
        const wi::vector<std::uint64_t>& authoredVoxels,
        std::string& error)
    {
        if (scene.voxel_grids.GetCount() != 1 ||
            !IsRenegadeNavigationGrid(scene, authoredGrid) ||
            PersistentEntityId(scene, authoredGrid) != authoredId)
        {
            error =
                "TestGame navigation changed live authored grid ownership";
            return false;
        }

        const auto* component = scene.voxel_grids.GetComponent(authoredGrid);
        if (component == nullptr || component->voxels != authoredVoxels)
        {
            error =
                "TestGame navigation changed live authored VoxelGrid bytes";
            return false;
        }

        error.clear();
        return true;
    }

    bool InspectSnapshot(
        const TestLevelSnapshot& snapshot,
        const StableId& authoredId,
        std::string& error)
    {
        auto prepared = PrepareWickedSceneOpen(snapshot.scenePath);
        if (!prepared.IsReady() || prepared.ReadOnlyScene() == nullptr)
        {
            error = "snapshot could not be reopened: " + prepared.Error();
            return false;
        }
        const wi::scene::Scene& scene = *prepared.ReadOnlyScene();
        if (snapshot.navigationGridCount != 1)
        {
            error =
                "TestGame did not report exactly the one authored Runtime navigation grid";
            return false;
        }

        wi::ecs::Entity authored = wi::ecs::INVALID_ENTITY;
        std::size_t renegadeGridCount = 0;
        for (std::size_t index = 0; index < scene.voxel_grids.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.voxel_grids.GetEntity(index);
            if (!IsRenegadeNavigationGrid(scene, entity))
                continue;
            ++renegadeGridCount;
            if (PersistentEntityId(scene, entity) == authoredId)
                authored = entity;
        }
        if (renegadeGridCount != 1 || authored == wi::ecs::INVALID_ENTITY)
        {
            error =
                "detached TestGame did not preserve the authored navigation identity";
            return false;
        }

        const auto* grid = scene.voxel_grids.GetComponent(authored);
        if (grid == nullptr || !grid->IsValid())
        {
            error = "detached TestGame authored navigation grid is invalid";
            return false;
        }

        const wi::ecs::Entity runtimeDefault = FindDefaultNavigationGrid(scene);
        if (runtimeDefault != authored)
        {
            error =
                "Runtime default navigation did not select the prepared authored grid";
            return false;
        }

        const auto* metadata = scene.metadatas.GetComponent(authored);
        if (metadata == nullptr ||
            !metadata->string_values.has(NavigationDefaultGridMetadataKey) ||
            metadata->string_values.get(NavigationDefaultGridMetadataKey) !=
                NavigationDefaultGridMetadataVersion)
        {
            error = "prepared authored grid lost its Runtime-default marker";
            return false;
        }

        error.clear();
        return true;
    }
}

int main()
{
    using namespace renegade::bridge;

    wi::jobsystem::Initialize();

    const fs::path root = fs::temp_directory_path() /
        fs::u8path(
            "renegade-cw05-nav-owner-" + GenerateStableId());
    const fs::path scenePath =
        root / "Content" / "Scenes" / "NavigationOwner.wiscene";
    std::error_code ec;
    fs::create_directories(scenePath.parent_path(), ec);
    if (ec)
        return Fail("could not create temporary project: " + ec.message());

    const auto cleanup = [&]()
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    };

    ProjectMetadata project;
    project.formatVersion = ProjectService::CurrentFormatVersion;
    project.projectId = "91919191-9191-4919-8919-919191919191";
    project.name = "CW-05 Navigation Owner Fixture";
    project.descriptorPath =
        (root / "NavigationOwner.renegade").generic_u8string();
    project.rootPath = root.generic_u8string();
    project.startupScene = "Content/Scenes/NavigationOwner.wiscene";

    StudioSession session;
    wi::scene::Scene& authoringScene = session.Scenes().GetScene();
    NavigationGridSettings authoredSettings;
    authoredSettings.resolutionX = 24;
    authoredSettings.resolutionY = 12;
    authoredSettings.resolutionZ = 24;
    authoredSettings.voxelSize = 0.5f;
    std::string error;
    const wi::ecs::Entity authoredGrid = CreateNavigationGrid(
        authoringScene,
        authoredSettings,
        error);
    if (authoredGrid == wi::ecs::INVALID_ENTITY)
    {
        cleanup();
        return Fail("could not create authored navigation grid: " + error);
    }
    const StableId authoredId = PersistentEntityId(authoringScene, authoredGrid);
    const auto* authoredComponent = authoringScene.voxel_grids.GetComponent(authoredGrid);
    if (!IsValidStableId(authoredId) || authoredComponent == nullptr ||
        !authoredComponent->IsValid())
    {
        cleanup();
        return Fail("authored navigation grid fixture lost identity/native state");
    }
    const wi::vector<std::uint64_t> authoredVoxels = authoredComponent->voxels;

    if (!session.SaveScene(scenePath.generic_u8string()))
    {
        cleanup();
        return Fail("could not save authored navigation fixture");
    }

    TestLevelSnapshotService snapshots(session.Scenes(), session.Commands());
    TestLevelSnapshot first;
    if (!snapshots.Create(project, first, error))
    {
        cleanup();
        return Fail("first TestGame snapshot failed: " + error);
    }
    if (!first.navigationCacheRebuilt || first.navigationCacheReused)
    {
        cleanup();
        return Fail("first TestGame did not build the navigation cache");
    }
    if (!InspectSnapshot(first, authoredId, error) ||
        !LiveAuthoredGridIntact(
            authoringScene,
            authoredGrid,
            authoredId,
            authoredVoxels,
            error))
    {
        cleanup();
        return Fail("first TestGame ownership check failed: " + error);
    }
    if (!snapshots.Cleanup(first, error))
    {
        cleanup();
        return Fail("first TestGame cleanup failed: " + error);
    }

    TestLevelSnapshot second;
    if (!snapshots.Create(project, second, error))
    {
        cleanup();
        return Fail("second TestGame snapshot failed: " + error);
    }
    if (!second.navigationCacheReused || second.navigationCacheRebuilt)
    {
        cleanup();
        return Fail("unchanged TestGame did not reuse the navigation cache");
    }
    if (!InspectSnapshot(second, authoredId, error) ||
        !LiveAuthoredGridIntact(
            authoringScene,
            authoredGrid,
            authoredId,
            authoredVoxels,
            error))
    {
        cleanup();
        return Fail("second TestGame ownership check failed: " + error);
    }
    if (!snapshots.Cleanup(second, error))
    {
        cleanup();
        return Fail("second TestGame cleanup failed: " + error);
    }

    NavigationGridSettings changedSettings = authoredSettings;
    changedSettings.center = XMFLOAT3(2.0f, 0.0f, 0.0f);
    if (!session.Commands().Execute(
            std::make_unique<RebuildNavigationGridCommand>(
                authoringScene,
                authoredGrid,
                changedSettings)))
    {
        cleanup();
        return Fail("could not create unsaved navigation-grid change");
    }
    const std::size_t undoBefore = session.Commands().UndoCount();

    TestLevelSnapshot changed;
    if (!snapshots.Create(project, changed, error))
    {
        cleanup();
        return Fail("changed-geometry TestGame snapshot failed: " + error);
    }
    if (!changed.navigationCacheRebuilt || changed.navigationCacheReused)
    {
        cleanup();
        return Fail("navigation-grid setting change did not rebuild the cache");
    }
    if (!InspectSnapshot(changed, authoredId, error) ||
        !LiveAuthoredGridIntact(
            authoringScene,
            authoredGrid,
            authoredId,
            authoredVoxels,
            error))
    {
        cleanup();
        return Fail("changed TestGame ownership check failed: " + error);
    }
    if (!session.Commands().IsDirty() ||
        session.Commands().UndoCount() != undoBefore)
    {
        cleanup();
        return Fail("TestGame navigation changed authoring dirty/Undo state");
    }

    if (!snapshots.Cleanup(changed, error))
    {
        cleanup();
        return Fail("changed TestGame cleanup failed: " + error);
    }

    cleanup();
    std::cout
        << "PASS: CW-05 TestGame navigation ownership, cache and stable identity\n";
    return 0;
}
