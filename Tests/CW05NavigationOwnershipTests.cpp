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

    void Trace(const char* stage)
    {
        // This test has historically terminated with an unreported Windows
        // access violation. Keep the checkpoints on stderr and flush each one
        // so CTest preserves the last completed native ownership boundary.
        std::cerr << "CW05 ownership checkpoint: " << stage << std::endl;
    }

    wi::ecs::Entity AddNavigationCube(wi::scene::Scene& scene)
    {
        const wi::ecs::Entity objectEntity = wi::ecs::CreateEntity();
        const wi::ecs::Entity meshEntity = wi::ecs::CreateEntity();

        scene.names.Create(objectEntity) = "Navigation Floor";
        auto& transform = scene.transforms.Create(objectEntity);
        transform.ClearTransform();
        transform.UpdateTransform();

        auto& object = scene.objects.Create(objectEntity);
        object.meshID = meshEntity;
        object.filterMask |= wi::enums::FILTER_NAVIGATION_MESH;

        auto& mesh = scene.meshes.Create(meshEntity);
        mesh.vertex_positions = {
            XMFLOAT3(-3, -0.5f, -3), XMFLOAT3(3, -0.5f, -3),
            XMFLOAT3(-3,  0.5f, -3), XMFLOAT3(3,  0.5f, -3),
            XMFLOAT3(-3, -0.5f,  3), XMFLOAT3(3, -0.5f,  3),
            XMFLOAT3(-3,  0.5f,  3), XMFLOAT3(3,  0.5f,  3),
        };
        mesh.indices = {
            0,2,1, 1,2,3, 4,5,6, 5,7,6,
            0,1,4, 1,5,4, 2,6,3, 3,6,7,
            0,4,2, 2,4,6, 1,3,5, 3,7,5,
        };
        mesh.aabb = wi::primitive::AABB(
            XMFLOAT3(-3.0f, -0.5f, -3.0f),
            XMFLOAT3(3.0f, 0.5f, 3.0f));
        auto& subset = mesh.subsets.emplace_back();
        subset.indexOffset = 0;
        subset.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
        return objectEntity;
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
    Trace("job system initialized");

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
    Trace("StudioSession created");
    wi::scene::Scene& authoringScene = session.Scenes().GetScene();
    const wi::ecs::Entity floor = AddNavigationCube(authoringScene);

    // Phase-6 manual creation expects the native object streams that a normal
    // frame produces. Seed them only for the authored-grid fixture.
    const auto* floorTransform = authoringScene.transforms.GetComponent(floor);
    const auto* floorObject = authoringScene.objects.GetComponent(floor);
    const auto* floorMesh = floorObject == nullptr
        ? nullptr
        : authoringScene.meshes.GetComponent(floorObject->meshID);
    if (floorTransform == nullptr || floorMesh == nullptr)
    {
        cleanup();
        return Fail("navigation floor fixture was incomplete");
    }
    authoringScene.matrix_objects = { floorTransform->world };
    authoringScene.aabb_objects = {
        floorMesh->aabb.transform(floorTransform->world)
    };

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
    if (!IsValidStableId(authoredId) || authoredComponent == nullptr)
    {
        cleanup();
        return Fail("authored navigation grid fixture lost identity/native state");
    }
    const wi::vector<std::uint64_t> authoredVoxels = authoredComponent->voxels;
    Trace("authored grid created");

    if (!session.SaveScene(scenePath.generic_u8string()))
    {
        cleanup();
        return Fail("could not save authored navigation fixture");
    }
    Trace("authored scene saved");

    TestLevelSnapshotService snapshots(session.Scenes(), session.Commands());
    TestLevelSnapshot first;
    Trace("first snapshot create begin");
    if (!snapshots.Create(project, first, error))
    {
        cleanup();
        return Fail("first TestGame snapshot failed: " + error);
    }
    Trace("first snapshot create complete");
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
    Trace("first snapshot inspected");
    if (!snapshots.Cleanup(first, error))
    {
        cleanup();
        return Fail("first TestGame cleanup failed: " + error);
    }
    Trace("first snapshot cleaned");

    TestLevelSnapshot second;
    Trace("second snapshot create begin");
    if (!snapshots.Create(project, second, error))
    {
        cleanup();
        return Fail("second TestGame snapshot failed: " + error);
    }
    Trace("second snapshot create complete");
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
    Trace("second snapshot inspected");
    if (!snapshots.Cleanup(second, error))
    {
        cleanup();
        return Fail("second TestGame cleanup failed: " + error);
    }
    Trace("second snapshot cleaned");

    if (!session.Commands().Execute(
            std::make_unique<SetTranslationCommand>(
                authoringScene,
                floor,
                XMFLOAT3(2.0f, 0.0f, 0.0f))))
    {
        cleanup();
        return Fail("could not create unsaved navigation-geometry change");
    }
    const std::size_t undoBefore = session.Commands().UndoCount();
    Trace("authoring geometry changed");

    TestLevelSnapshot changed;
    Trace("changed snapshot create begin");
    if (!snapshots.Create(project, changed, error))
    {
        cleanup();
        return Fail("changed-geometry TestGame snapshot failed: " + error);
    }
    Trace("changed snapshot create complete");
    if (!changed.navigationCacheRebuilt || changed.navigationCacheReused)
    {
        cleanup();
        return Fail("navigation geometry change did not rebuild the cache");
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
    Trace("changed snapshot inspected");
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
    Trace("changed snapshot cleaned");

    cleanup();
    Trace("test complete before destruction");
    std::cout
        << "PASS: CW-05 TestGame navigation ownership, cache and stable identity\n";
    return 0;
}
