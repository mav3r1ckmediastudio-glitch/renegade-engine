#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/IdentityService.h"

#include <cmath>
#include <iostream>
#include <memory>
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

    void SetTransformPosition(
        wi::scene::TransformComponent& transform,
        const XMFLOAT3& position)
    {
        transform.ClearTransform();
        transform.Translate(position);
        transform.UpdateTransform();
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
    CommandService commands;
    auto createCommand = std::make_unique<CreateNavigationGridCommand>(
        scene, gridSettings);
    auto* createCommandView = createCommand.get();
    if (!commands.Execute(std::move(createCommand)))
        return Fail("command-backed navigation grid creation failed");

    const wi::ecs::Entity gridEntity = createCommandView->CreatedEntity();
    if (gridEntity == wi::ecs::INVALID_ENTITY)
        return Fail("create navigation grid returned invalid entity");
    if (!IsRenegadeNavigationGrid(scene, gridEntity))
        return Fail("created entity did not retain Renegade navigation identity");
    if (!IsValidStableId(PersistentEntityId(scene, gridEntity)))
        return Fail("navigation grid did not receive persistent scene identity");

    // Creator creation must be a normal undoable scene mutation. Redo must
    // restore the exact persistent entity rather than remapping identity.
    if (!commands.Undo() || scene.voxel_grids.Contains(gridEntity))
        return Fail("navigation grid creation did not undo cleanly");
    if (!commands.Redo() || !IsRenegadeNavigationGrid(scene, gridEntity))
        return Fail("navigation grid creation did not redo with stable identity");

    auto* grid = scene.voxel_grids.GetComponent(gridEntity);
    if (grid == nullptr || !grid->IsValid())
        return Fail("created entity has no valid native Wicked VoxelGrid");

    // Rebuilds are also command-backed. Prove the native grid state can be
    // undone/redone before populating the deterministic pathfinding fixture.
    auto rebuildSettings = CaptureNavigationGridSettings(scene, gridEntity);
    rebuildSettings.voxelSize = 0.5f;
    if (!commands.Execute(std::make_unique<RebuildNavigationGridCommand>(
            scene, gridEntity, rebuildSettings)))
    {
        return Fail("command-backed navigation grid rebuild failed");
    }
    grid = scene.voxel_grids.GetComponent(gridEntity);
    if (grid == nullptr || !Near(grid->voxelSize.x, 0.5f))
        return Fail("navigation rebuild did not apply authored voxel size");
    if (!commands.Undo())
        return Fail("navigation rebuild undo failed");
    grid = scene.voxel_grids.GetComponent(gridEntity);
    if (grid == nullptr || !Near(grid->voxelSize.x, 1.0f))
        return Fail("navigation rebuild undo did not restore native grid state");
    if (!commands.Redo())
        return Fail("navigation rebuild redo failed");
    grid = scene.voxel_grids.GetComponent(gridEntity);
    if (grid == nullptr || !Near(grid->voxelSize.x, 0.5f))
        return Fail("navigation rebuild redo did not restore rebuilt grid state");

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

    // Build the same serialized authoring relationship Studio creates, but use
    // component-only entities so this test remains GPU-free. Runtime must
    // resolve persistent IDs, activate the native CharacterComponent, submit
    // its goal to Wicked, then drive Turn()/Move() toward the next waypoint.
    const wi::ecs::Entity authoredAgent = wi::ecs::CreateEntity();
    scene.names.Create(authoredAgent) = "Navigation Agent Test";
    auto& authoredAgentTransform = scene.transforms.Create(authoredAgent);
    SetTransformPosition(authoredAgentTransform, start);
    auto& authoredCharacter = scene.characters.Create(authoredAgent);
    authoredCharacter.SetActive(false);
    auto& authoredAgentMetadata = scene.metadatas.Create(authoredAgent);
    authoredAgentMetadata.string_values.set(
        NavigationAgentMetadataKey, NavigationAgentMetadataVersion);
    if (!AssignNewPersistentEntityId(scene, authoredAgent, error))
        return Fail("assign agent identity: " + error);

    const wi::ecs::Entity authoredDestination = wi::ecs::CreateEntity();
    scene.names.Create(authoredDestination) = "Navigation Destination Test";
    auto& authoredDestinationTransform = scene.transforms.Create(authoredDestination);
    SetTransformPosition(authoredDestinationTransform, goal);
    auto& authoredDestinationMetadata = scene.metadatas.Create(authoredDestination);
    authoredDestinationMetadata.string_values.set(
        NavigationDestinationMetadataKey,
        NavigationDestinationMetadataVersion);
    if (!AssignNewPersistentEntityId(scene, authoredDestination, error))
        return Fail("assign destination identity: " + error);

    const std::string gridId = PersistentEntityId(scene, gridEntity);
    const std::string destinationId =
        PersistentEntityId(scene, authoredDestination);
    authoredAgentMetadata.string_values.set(
        NavigationGridReferenceMetadataKey, gridId);
    authoredAgentMetadata.string_values.set(
        NavigationDestinationReferenceMetadataKey, destinationId);
    authoredAgentMetadata.float_values.set(NavigationMoveSpeedMetadataKey, 0.12f);
    authoredAgentMetadata.bool_values.set(NavigationFlyingMetadataKey, false);
    authoredDestinationMetadata.string_values.set(
        NavigationGridReferenceMetadataKey, gridId);

    NavigationAgentBinding binding;
    if (!ResolveNavigationAgentBinding(
            scene, authoredAgent, binding, error))
    {
        return Fail("resolve authored agent binding: " + error);
    }
    if (binding.grid != gridEntity ||
        binding.destination != authoredDestination)
    {
        return Fail("authored navigation references did not resolve by persistent ID");
    }

    NavigationPathResult authoredPath;
    if (!QueryNavigationAgentPath(
            scene, authoredAgent, authoredPath, error) ||
        !authoredPath.successful || authoredPath.waypoints.size() < 2)
    {
        return Fail("authored agent path preview did not resolve a native route: " + error);
    }

    NavigationRuntimeState runtimeState;
    if (!InitializeRuntimeNavigation(scene, runtimeState, error))
        return Fail("initialize Runtime navigation: " + error);
    if (runtimeState.agents.size() != 1)
        return Fail("Runtime did not discover exactly one authored navigation agent");
    if (!authoredCharacter.IsActive())
        return Fail("Runtime did not activate the authored native CharacterComponent");
    if (authoredCharacter.voxelgrid != grid)
        return Fail("Runtime agent was not handed the authored native VoxelGrid");

    // SetPathGoal is intentionally deferred to Wicked's normal Scene update.
    // For this GPU-free unit test only, run the same native PathQuery directly
    // so the next Runtime step can prove it consumes Wicked's next waypoint and
    // issues native CharacterComponent movement rather than moving a Transform.
    authoredCharacter.pathquery.flying = false;
    authoredCharacter.pathquery.agent_height = 1;
    authoredCharacter.pathquery.agent_width = 0;
    authoredCharacter.pathquery.process(start, goal, *grid);
    authoredCharacter.movement = XMFLOAT3(0.0f, 0.0f, 0.0f);
    UpdateRuntimeNavigation(scene, runtimeState, 1.0f / 60.0f);
    const float movementSquared =
        authoredCharacter.movement.x * authoredCharacter.movement.x +
        authoredCharacter.movement.y * authoredCharacter.movement.y +
        authoredCharacter.movement.z * authoredCharacter.movement.z;
    if (movementSquared <= 0.000001f)
        return Fail("Runtime path follower did not issue CharacterComponent::Move()");

    std::cout <<
        "Phase 6 native Wicked VoxelGrid + PathQuery + Character following passed.\n";
    return 0;
}