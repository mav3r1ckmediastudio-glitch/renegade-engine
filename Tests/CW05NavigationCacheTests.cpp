#include "renegade/bridge/TestLevelNavigationCacheService.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/NavigationService.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int Fail(const std::string& message)
    {
        std::cerr
            << "CW-05 TestGame navigation cache test failed: "
            << message << '\n';
        return 1;
    }

    void AddNavigationCube(
        wi::scene::Scene& scene,
        const XMFLOAT3& position)
    {
        const wi::ecs::Entity objectEntity =
            wi::ecs::CreateEntity();
        const wi::ecs::Entity meshEntity =
            wi::ecs::CreateEntity();

        auto& transform =
            scene.transforms.Create(objectEntity);
        transform.ClearTransform();
        transform.Translate(position);
        transform.UpdateTransform();

        auto& object =
            scene.objects.Create(objectEntity);
        object.meshID = meshEntity;
        object.filterMask |=
            wi::enums::FILTER_NAVIGATION_MESH;

        auto& mesh =
            scene.meshes.Create(meshEntity);
        mesh.vertex_positions = {
            XMFLOAT3(-1, -1, -1), XMFLOAT3(1, -1, -1),
            XMFLOAT3(-1,  1, -1), XMFLOAT3(1,  1, -1),
            XMFLOAT3(-1, -1,  1), XMFLOAT3(1, -1,  1),
            XMFLOAT3(-1,  1,  1), XMFLOAT3(1,  1,  1),
        };
        mesh.indices = {
            0,2,1, 1,2,3, 4,5,6, 5,7,6,
            0,1,4, 1,5,4, 2,6,3, 3,6,7,
            0,4,2, 2,4,6, 1,3,5, 3,7,5,
        };
        mesh.aabb = wi::primitive::AABB(
            XMFLOAT3(-1.0f, -1.0f, -1.0f),
            XMFLOAT3(1.0f, 1.0f, 1.0f));
        auto& subset = mesh.subsets.emplace_back();
        subset.indexOffset = 0;
        subset.indexCount =
            static_cast<std::uint32_t>(
                mesh.indices.size());
    }

    void AddIrrelevantLight(
        wi::scene::Scene& scene,
        const XMFLOAT3& position)
    {
        const wi::ecs::Entity entity =
            wi::ecs::CreateEntity();
        auto& transform =
            scene.transforms.Create(entity);
        transform.ClearTransform();
        transform.Translate(position);
        transform.UpdateTransform();
        scene.lights.Create(entity);
    }

    wi::ecs::Entity FirstNavigationGrid(
        const wi::scene::Scene& scene)
    {
        for (std::size_t index = 0;
            index < scene.voxel_grids.GetCount(); ++index)
        {
            const wi::ecs::Entity entity =
                scene.voxel_grids.GetEntity(index);
            if (IsRenegadeNavigationGrid(scene, entity))
                return entity;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    bool HasOccupiedVoxel(const wi::VoxelGrid& grid)
    {
        for (const auto word : grid.voxels)
        {
            if (word != 0)
                return true;
        }
        return false;
    }
}

int main()
{
    using namespace renegade::bridge;

    wi::jobsystem::Initialize();

    const fs::path root =
        fs::temp_directory_path() /
        fs::u8path(
            "renegade-cw05-nav-cache-" +
            GenerateStableId());
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec)
        return Fail(
            "could not create temporary project: " +
            ec.message());

    const auto cleanup = [&]()
    {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    };

    const std::string sceneIdentity =
        "Content/Scenes/CW05NavigationCacheTest.wiscene";
    std::string error;

    wi::scene::Scene first;
    AddNavigationCube(first, XMFLOAT3(0, 0, 0));

    TestLevelNavigationPreparation firstResult;
    if (!PrepareTestLevelNavigationCache(
            root.generic_u8string(),
            sceneIdentity,
            first,
            firstResult,
            error))
    {
        cleanup();
        return Fail("first bake failed: " + error);
    }
    if (!firstResult.rebuiltCache ||
        firstResult.reusedCache ||
        firstResult.gridCount != 1 ||
        firstResult.signature.empty() ||
        firstResult.cacheDirectory.empty())
    {
        cleanup();
        return Fail(
            "first TestGame did not report one rebuilt navigation cache");
    }

    const wi::ecs::Entity firstGridEntity =
        FirstNavigationGrid(first);
    const auto* firstGrid =
        first.voxel_grids.GetComponent(
            firstGridEntity);
    if (firstGridEntity == wi::ecs::INVALID_ENTITY ||
        firstGrid == nullptr ||
        !firstGrid->IsValid() ||
        !HasOccupiedVoxel(*firstGrid))
    {
        cleanup();
        return Fail(
            "first TestGame did not create an occupied native Wicked grid");
    }

    wi::scene::Scene second;
    AddNavigationCube(second, XMFLOAT3(0, 0, 0));

    TestLevelNavigationPreparation secondResult;
    if (!PrepareTestLevelNavigationCache(
            root.generic_u8string(),
            sceneIdentity,
            second,
            secondResult,
            error))
    {
        cleanup();
        return Fail("second bake/reuse failed: " + error);
    }
    if (!secondResult.reusedCache ||
        secondResult.rebuiltCache ||
        secondResult.gridCount != 1 ||
        secondResult.signature != firstResult.signature)
    {
        cleanup();
        return Fail(
            "unchanged second TestGame did not reuse the previous bake");
    }

    wi::scene::Scene moved;
    AddNavigationCube(moved, XMFLOAT3(4, 0, 0));

    TestLevelNavigationPreparation movedResult;
    if (!PrepareTestLevelNavigationCache(
            root.generic_u8string(),
            sceneIdentity,
            moved,
            movedResult,
            error))
    {
        cleanup();
        return Fail("changed-geometry bake failed: " + error);
    }
    if (!movedResult.rebuiltCache ||
        movedResult.reusedCache ||
        movedResult.signature == firstResult.signature)
    {
        cleanup();
        return Fail(
            "navigation-relevant transform change did not invalidate the bake");
    }

    wi::scene::Scene lightOnlyChange;
    AddNavigationCube(
        lightOnlyChange,
        XMFLOAT3(4, 0, 0));
    AddIrrelevantLight(
        lightOnlyChange,
        XMFLOAT3(100, 200, 300));

    TestLevelNavigationPreparation lightResult;
    if (!PrepareTestLevelNavigationCache(
            root.generic_u8string(),
            sceneIdentity,
            lightOnlyChange,
            lightResult,
            error))
    {
        cleanup();
        return Fail(
            "non-navigation change reuse failed: " +
            error);
    }
    if (!lightResult.reusedCache ||
        lightResult.rebuiltCache ||
        lightResult.signature != movedResult.signature)
    {
        cleanup();
        return Fail(
            "a light-only change incorrectly invalidated navigation");
    }

    wi::scene::Scene authored;
    AddNavigationCube(authored, XMFLOAT3(0, 0, 0));
    // Supply the transient object streams expected by the existing creator
    // creation command. The automatic cache service will rebuild them itself.
    authored.aabb_objects = {
        wi::primitive::AABB(
            XMFLOAT3(-1, -1, -1),
            XMFLOAT3(1, 1, 1))
    };
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    authored.matrix_objects = { identity };

    NavigationGridSettings authoredSettings;
    authoredSettings.resolutionX = 24;
    authoredSettings.resolutionY = 12;
    authoredSettings.resolutionZ = 24;
    authoredSettings.voxelSize = 0.5f;
    const wi::ecs::Entity authoredGrid =
        CreateNavigationGrid(
            authored,
            authoredSettings,
            error);
    if (authoredGrid == wi::ecs::INVALID_ENTITY)
    {
        cleanup();
        return Fail(
            "could not create authored navigation grid fixture: " +
            error);
    }
    const std::string authoredId =
        PersistentEntityId(authored, authoredGrid);

    TestLevelNavigationPreparation authoredResult;
    if (!PrepareTestLevelNavigationCache(
            root.generic_u8string(),
            sceneIdentity + ".authored",
            authored,
            authoredResult,
            error))
    {
        cleanup();
        return Fail(
            "authored-grid preparation failed: " +
            error);
    }
    if (PersistentEntityId(authored, authoredGrid) !=
            authoredId ||
        !IsRenegadeNavigationGrid(
            authored, authoredGrid))
    {
        cleanup();
        return Fail(
            "automatic TestGame preparation replaced authored grid identity");
    }

    cleanup();
    std::cout
        << "PASS: CW-05 automatic TestGame navigation bake/cache\n";
    return 0;
}
