#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/IdentityService.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
    using namespace renegade::bridge;

    int Fail(const std::string& message)
    {
        std::cerr << "Phase 6 native navigation test failed: "
                  << message << '\n';
        return 1;
    }

    bool Near(const float a, const float b) noexcept
    {
        return std::fabs(a - b) < 0.001f;
    }
}

int main()
{
    using namespace renegade::bridge;

    wi::scene::Scene scene;
    NavigationGridSettings gridSettings;
    gridSettings.resolutionX = 24;
    gridSettings.resolutionY = 8;
    gridSettings.resolutionZ = 24;
    gridSettings.center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    gridSettings.voxelSize = 1.0f;
    gridSettings.fitToSceneBounds = false;

    std::string error;
    const wi::ecs::Entity gridEntity =
        CreateNavigationGrid(scene, gridSettings, error);
    if (gridEntity == wi::ecs::INVALID_ENTITY)
        return Fail("create navigation grid: " + error);
    if (!IsRenegadeNavigationGrid(scene, gridEntity))
        return Fail("created entity did not retain Renegade navigation identity");
    if (!IsValidStableId(PersistentEntityId(scene, gridEntity)))
        return Fail("navigation grid did not receive persistent scene identity");

    auto* grid = scene.voxel_grids.GetComponent(gridEntity);
    if (grid == nullptr || !grid->IsValid())
        return Fail("created entity has no valid native Wicked VoxelGrid");

    // Build a deterministic synthetic walkable surface directly in Wicked's
    // native grid. Ground is occupied; obstacles are occupied voxels above it.
    constexpr std::uint32_t groundY = 4;
    for (std::uint32_t x = 1; x < 23; ++x)
    {
        for (std::uint32_t z = 1; z < 23; ++z)
            grid->set_voxel(XMUINT3(x, groundY, z), true);
    }

    // A wall above the walkable surface forces the path around either end.
    // The ground voxel itself stays occupied so the route remains valid there
    // once a free horizontal route is found.
    for (std::uint32_t z = 3; z <= 20; ++z)
        grid->set_voxel(XMUINT3(12, groundY - 1, z), true);

    const XMFLOAT3 start = grid->coord_to_world(XMUINT3(3, groundY, 12));
    const XMFLOAT3 goal = grid->coord_to_world(XMUINT3(20, groundY, 12));

    NavigationPathResult path;
    NavigationQuerySettings querySettings;
    querySettings.flying = false;
    querySettings.agentHeight = 1;
    querySettings.agentWidth = 0;

    if (!QueryNavigationPath(
            scene,
            gridEntity,
            start,
            goal,
            querySettings,
            path,
            error))
    {
        return Fail("query path: " + error);
    }
    if (!path.successful)
        return Fail("Wicked PathQuery did not find a grounded route around the wall");
    if (path.waypoints.size() < 2)
        return Fail("native path result did not expose start-to-goal waypoints");

    const XMFLOAT3 finalWaypoint = path.waypoints.back();
    if (!Near(finalWaypoint.x, path.resolvedGoal.x) ||
        !Near(finalWaypoint.y, path.resolvedGoal.y) ||
        !Near(finalWaypoint.z, path.resolvedGoal.z))
    {
        return Fail("Renegade waypoint ordering is not start-to-goal");
    }

    // Prove the same grid can be handed directly to Wicked's CharacterComponent
    // rather than copied into a Renegade-owned navigation simulation.
    const wi::ecs::Entity characterEntity = wi::ecs::CreateEntity();
    scene.characters.Create(characterEntity);
    if (!SetCharacterNavigationGoal(
            scene,
            characterEntity,
            gridEntity,
            goal,
            querySettings,
            error))
    {
        return Fail("assign native character path goal: " + error);
    }

    const auto* character = scene.characters.GetComponent(characterEntity);
    if (character == nullptr || character->voxelgrid != grid)
        return Fail("Wicked CharacterComponent did not retain the native VoxelGrid pointer");
    if (!Near(character->goal.x, goal.x) ||
        !Near(character->goal.y, goal.y) ||
        !Near(character->goal.z, goal.z))
    {
        return Fail("Wicked CharacterComponent did not receive the authored goal");
    }

    NavigationQuerySettings flyingSettings;
    flyingSettings.flying = true;
    flyingSettings.agentHeight = 1;
    flyingSettings.agentWidth = 0;

    // Flying mode traverses empty voxels in the same native grid. Use a height
    // above the floor and below the grid edge to prove the 3D path mode exists.
    const XMFLOAT3 flyingStart =
        grid->coord_to_world(XMUINT3(3, groundY - 2, 2));
    const XMFLOAT3 flyingGoal =
        grid->coord_to_world(XMUINT3(20, groundY - 2, 21));
    NavigationPathResult flyingPath;
    if (!QueryNavigationPath(
            scene,
            gridEntity,
            flyingStart,
            flyingGoal,
            flyingSettings,
            flyingPath,
            error))
    {
        return Fail("query flying path: " + error);
    }
    if (!flyingPath.successful)
        return Fail("Wicked PathQuery flying mode did not find a 3D route");

    std::cout <<
        "Phase 6 native Wicked VoxelGrid + PathQuery foundation passed.\n";
    return 0;
}
