#include "renegade/bridge/NavigationService.h"

#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace
{
    constexpr std::uint32_t MinimumResolution = 4;
    constexpr std::uint32_t MaximumResolution = 1024;
    constexpr std::uint64_t MaximumVoxelCount =
        128ull * 1024ull * 1024ull;
    constexpr int MaximumAgentExtentVoxels = 64;
    constexpr float DefaultNavigationMoveSpeed = 0.12f;
    constexpr float DefaultArrivalDistance = 0.55f;
    constexpr float RepathIntervalSeconds = 0.75f;

    bool ValidateQuerySettings(
        const renegade::bridge::NavigationQuerySettings& settings,
        std::string& error) noexcept
    {
        if (settings.agentHeight < 1 ||
            settings.agentHeight > MaximumAgentExtentVoxels)
        {
            error = "Navigation agent height must be between 1 and 64 voxels.";
            return false;
        }
        if (settings.agentWidth < 0 ||
            settings.agentWidth > MaximumAgentExtentVoxels)
        {
            error = "Navigation agent width must be between 0 and 64 voxels.";
            return false;
        }
        error.clear();
        return true;
    }

    bool EntityExists(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        if (entity == wi::ecs::INVALID_ENTITY)
            return false;
        wi::unordered_set<wi::ecs::Entity> entities;
        scene.FindAllEntities(entities);
        return entities.count(entity) != 0;
    }

    wi::ecs::Entity ResolvePersistentEntity(
        const wi::scene::Scene& scene,
        const std::string& stableId) noexcept
    {
        if (stableId.empty())
            return wi::ecs::INVALID_ENTITY;
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const auto& metadata = scene.metadatas[index];
            if (metadata.string_values.has(
                    renegade::bridge::PersistentEntityIdMetadataKey) &&
                metadata.string_values.get(
                    renegade::bridge::PersistentEntityIdMetadataKey) == stableId)
            {
                return scene.metadatas.GetEntity(index);
            }
        }
        return wi::ecs::INVALID_ENTITY;
    }

    bool Finite3(const XMFLOAT3& value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    float DistanceSquared(const XMFLOAT3& a, const XMFLOAT3& b) noexcept
    {
        const float x = a.x - b.x;
        const float y = a.y - b.y;
        const float z = a.z - b.z;
        return x * x + y * y + z * z;
    }

    void SyncGridTransform(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const wi::VoxelGrid& grid) noexcept
    {
        auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return;

        transform->translation_local = grid.center;
        transform->scale_local = grid.voxelSize;
        transform->SetDirty();
        transform->UpdateTransform();
    }

    bool ConfigureMarkerTransform(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& position,
        const XMFLOAT3& scale) noexcept
    {
        auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return false;
        transform->ClearTransform();
        transform->Scale(scale);
        transform->Translate(position);
        transform->UpdateTransform();
        return true;
    }
}

namespace renegade::bridge
{
    std::size_t PrepareRigidBodyNavigationGeometry(
        wi::scene::Scene& scene) noexcept
    {
        std::size_t changed = 0;
        for (std::size_t bodyIndex = 0;
            bodyIndex < scene.rigidbodies.GetCount(); ++bodyIndex)
        {
            const wi::ecs::Entity bodyEntity =
                scene.rigidbodies.GetEntity(bodyIndex);
            for (std::size_t objectIndex = 0;
                objectIndex < scene.objects.GetCount(); ++objectIndex)
            {
                const wi::ecs::Entity objectEntity =
                    scene.objects.GetEntity(objectIndex);
                if (objectEntity != bodyEntity &&
                    !scene.Entity_IsDescendant(objectEntity, bodyEntity))
                {
                    continue;
                }

                auto& object = scene.objects[objectIndex];
                if ((object.filterMask & wi::enums::FILTER_NAVIGATION_MESH) == 0u)
                {
                    object.filterMask |= wi::enums::FILTER_NAVIGATION_MESH;
                    ++changed;
                }

                if (object.meshID != wi::ecs::INVALID_ENTITY)
                {
                    if (auto* mesh = scene.meshes.GetComponent(object.meshID);
                        mesh != nullptr && !mesh->bvh.IsValid() &&
                        !mesh->vertex_positions.empty())
                    {
                        mesh->BuildBVH();
                    }
                }
            }
        }
        return changed;
    }

    bool ValidateNavigationGridSettings(
        const NavigationGridSettings& settings,
        std::string& error) noexcept
    {
        const auto validAxis = [](const std::uint32_t value)
        {
            return value >= MinimumResolution && value <= MaximumResolution;
        };
        if (!validAxis(settings.resolutionX) ||
            !validAxis(settings.resolutionY) ||
            !validAxis(settings.resolutionZ))
        {
            error =
                "Navigation grid resolution must be between 4 and 1024 voxels per axis.";
            return false;
        }

        const std::uint64_t voxelCount =
            static_cast<std::uint64_t>(settings.resolutionX) *
            static_cast<std::uint64_t>(settings.resolutionY) *
            static_cast<std::uint64_t>(settings.resolutionZ);
        if (voxelCount > MaximumVoxelCount)
        {
            error =
                "Navigation grid exceeds Renegade's bounded staging voxel budget.";
            return false;
        }

        if (!std::isfinite(settings.voxelSize) || settings.voxelSize <= 0.0f)
        {
            error = "Navigation voxel size must be a finite positive value.";
            return false;
        }
        if (!Finite3(settings.center))
        {
            error = "Navigation grid center must contain finite values.";
            return false;
        }
        if (settings.filterMask == 0u)
        {
            error = "Navigation generation requires at least one Wicked geometry filter.";
            return false;
        }

        error.clear();
        return true;
    }

    bool IsRenegadeNavigationGrid(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY ||
            !scene.voxel_grids.Contains(entity))
        {
            return false;
        }

        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->string_values.has(NavigationGridMetadataKey) &&
            metadata->string_values.get(NavigationGridMetadataKey) ==
                NavigationGridMetadataVersion;
    }

    bool IsRenegadeNavigationAgent(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(entity))
        {
            return false;
        }
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->string_values.has(NavigationAgentMetadataKey) &&
            metadata->string_values.get(NavigationAgentMetadataKey) ==
                NavigationAgentMetadataVersion;
    }

    bool IsRenegadeNavigationDestination(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY ||
            !scene.transforms.Contains(entity))
        {
            return false;
        }
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->string_values.has(NavigationDestinationMetadataKey) &&
            metadata->string_values.get(NavigationDestinationMetadataKey) ==
                NavigationDestinationMetadataVersion;
    }

    bool IsRenegadeNavigationEntity(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        return IsRenegadeNavigationGrid(scene, entity) ||
            IsRenegadeNavigationAgent(scene, entity) ||
            IsRenegadeNavigationDestination(scene, entity);
    }

    wi::ecs::Entity CreateNavigationGrid(
        wi::scene::Scene& scene,
        const NavigationGridSettings& settings,
        std::string& error)
    {
        if (!ValidateNavigationGridSettings(settings, error))
            return wi::ecs::INVALID_ENTITY;

        const wi::ecs::Entity entity = wi::ecs::CreateEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            error = "Wicked could not allocate a navigation-grid entity.";
            return wi::ecs::INVALID_ENTITY;
        }

        scene.names.Create(entity) = "Navigation Grid";
        scene.transforms.Create(entity);
        scene.voxel_grids.Create(entity);
        auto& metadata = scene.metadatas.Create(entity);
        metadata.string_values.set(
            NavigationGridMetadataKey,
            NavigationGridMetadataVersion);

        if (!AssignPersistentEntityId(scene, entity, GenerateStableId(), error))
        {
            scene.Entity_Remove(entity);
            return wi::ecs::INVALID_ENTITY;
        }

        if (!RebuildNavigationGrid(scene, entity, settings, error))
        {
            scene.Entity_Remove(entity);
            return wi::ecs::INVALID_ENTITY;
        }

        error.clear();
        return entity;
    }

    bool RebuildNavigationGrid(
        wi::scene::Scene& scene,
        const wi::ecs::Entity navigationGridEntity,
        const NavigationGridSettings& settings,
        std::string& error)
    {
        if (!IsRenegadeNavigationGrid(scene, navigationGridEntity))
        {
            error = "Selected entity is not a Renegade navigation grid.";
            return false;
        }
        if (!ValidateNavigationGridSettings(settings, error))
            return false;

        auto* grid = scene.voxel_grids.GetComponent(navigationGridEntity);
        if (grid == nullptr)
        {
            error = "Navigation grid lost its native Wicked VoxelGrid component.";
            return false;
        }

        grid->init(
            settings.resolutionX,
            settings.resolutionY,
            settings.resolutionZ);

        if (settings.fitToSceneBounds)
        {
            grid->from_aabb(scene.bounds);
        }
        else
        {
            grid->center = settings.center;
            grid->set_voxelsize(settings.voxelSize);
        }

        // A normal Renegade Rigid Body is Jolt physics, while Wicked's
        // FILTER_COLLIDER means its separate lightweight ColliderComponent.
        // Admit rigid-body-backed render geometry to FILTER_NAVIGATION_MESH
        // before baking so ordinary creator obstacles block both PathQuery and
        // Wicked CharacterComponent surface collision.
        (void)PrepareRigidBodyNavigationGeometry(scene);

        grid->cleardata();
        scene.VoxelizeScene(
            *grid,
            false,
            settings.filterMask,
            settings.layerMask,
            settings.lod);
        SyncGridTransform(scene, navigationGridEntity, *grid);

        if (!grid->IsValid())
        {
            error = "Wicked produced an invalid navigation VoxelGrid.";
            return false;
        }

        error.clear();
        return true;
    }

    NavigationGridSettings CaptureNavigationGridSettings(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity navigationGridEntity) noexcept
    {
        NavigationGridSettings state;
        const auto* grid = scene.voxel_grids.GetComponent(navigationGridEntity);
        if (grid == nullptr)
            return state;

        state.resolutionX = grid->resolution.x;
        state.resolutionY = grid->resolution.y;
        state.resolutionZ = grid->resolution.z;
        state.center = grid->center;
        state.voxelSize = grid->voxelSize.x;
        state.fitToSceneBounds = false;
        return state;
    }

    bool QueryNavigationPath(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity navigationGridEntity,
        const XMFLOAT3& start,
        const XMFLOAT3& goal,
        const NavigationQuerySettings& settings,
        NavigationPathResult& result,
        std::string& error)
    {
        result = {};
        result.requestedStart = start;
        result.requestedGoal = goal;

        if (!IsRenegadeNavigationGrid(scene, navigationGridEntity))
        {
            error = "Path query requires a Renegade navigation grid.";
            return false;
        }
        if (!ValidateQuerySettings(settings, error))
            return false;

        const auto* grid = scene.voxel_grids.GetComponent(navigationGridEntity);
        if (grid == nullptr || !grid->IsValid())
        {
            error = "Path query requires a generated Wicked VoxelGrid.";
            return false;
        }

        wi::PathQuery query;
        query.flying = settings.flying;
        query.agent_height = settings.agentHeight;
        query.agent_width = settings.agentWidth;
        query.process(start, goal, *grid);

        result.successful = query.is_succesful();
        result.resolvedGoal = query.get_goal();
        if (result.successful)
        {
            const std::size_t count = query.get_waypoint_count();
            result.waypoints.reserve(count);
            for (std::size_t index = 0; index < count; ++index)
                result.waypoints.push_back(query.get_waypoint(index));
        }

        error.clear();
        return true;
    }

    bool SetCharacterNavigationGoal(
        wi::scene::Scene& scene,
        const wi::ecs::Entity characterEntity,
        const wi::ecs::Entity navigationGridEntity,
        const XMFLOAT3& goal,
        const NavigationQuerySettings& settings,
        std::string& error)
    {
        if (!IsRenegadeNavigationGrid(scene, navigationGridEntity))
        {
            error = "Character path goal requires a Renegade navigation grid.";
            return false;
        }
        if (!ValidateQuerySettings(settings, error))
            return false;

        auto* character = scene.characters.GetComponent(characterEntity);
        const auto* grid = scene.voxel_grids.GetComponent(navigationGridEntity);
        if (character == nullptr)
        {
            error = "Navigation target entity has no native Wicked CharacterComponent.";
            return false;
        }
        if (grid == nullptr || !grid->IsValid())
        {
            error = "Character path goal requires a generated Wicked VoxelGrid.";
            return false;
        }

        character->pathquery.flying = settings.flying;
        character->pathquery.agent_height = settings.agentHeight;
        character->pathquery.agent_width = settings.agentWidth;
        character->SetPathGoal(goal, grid);

        error.clear();
        return true;
    }

    bool CreateNavigationAgentPair(
        wi::scene::Scene& scene,
        const wi::ecs::Entity navigationGridEntity,
        const XMFLOAT3& agentPosition,
        const XMFLOAT3& destinationPosition,
        wi::ecs::Entity& agentEntity,
        wi::ecs::Entity& destinationEntity,
        std::string& error)
    {
        agentEntity = wi::ecs::INVALID_ENTITY;
        destinationEntity = wi::ecs::INVALID_ENTITY;
        if (!IsRenegadeNavigationGrid(scene, navigationGridEntity))
        {
            error = "Navigation agent creation requires a Renegade navigation grid.";
            return false;
        }
        if (!Finite3(agentPosition) || !Finite3(destinationPosition))
        {
            error = "Navigation agent and destination positions must be finite.";
            return false;
        }

        const std::string gridId = PersistentEntityId(scene, navigationGridEntity);
        if (!IsValidStableId(gridId))
        {
            error = "Navigation grid has no valid persistent identity.";
            return false;
        }

        agentEntity = scene.Entity_CreateCube("Navigation Agent");
        destinationEntity = scene.Entity_CreateCube("Navigation Destination");
        if (agentEntity == wi::ecs::INVALID_ENTITY ||
            destinationEntity == wi::ecs::INVALID_ENTITY ||
            !ConfigureMarkerTransform(
                scene, agentEntity, agentPosition,
                XMFLOAT3(0.35f, 0.9f, 0.35f)) ||
            !ConfigureMarkerTransform(
                scene, destinationEntity, destinationPosition,
                XMFLOAT3(0.28f, 0.28f, 0.28f)))
        {
            if (destinationEntity != wi::ecs::INVALID_ENTITY)
                scene.Entity_Remove(destinationEntity);
            if (agentEntity != wi::ecs::INVALID_ENTITY)
                scene.Entity_Remove(agentEntity);
            agentEntity = wi::ecs::INVALID_ENTITY;
            destinationEntity = wi::ecs::INVALID_ENTITY;
            error = "Wicked could not create visible navigation authoring markers.";
            return false;
        }

        std::string identityError;
        if (!AssignNewPersistentEntityId(scene, agentEntity, identityError) ||
            !AssignNewPersistentEntityId(scene, destinationEntity, identityError))
        {
            scene.Entity_Remove(destinationEntity);
            scene.Entity_Remove(agentEntity);
            agentEntity = wi::ecs::INVALID_ENTITY;
            destinationEntity = wi::ecs::INVALID_ENTITY;
            error = "Navigation markers could not receive persistent identity: " +
                identityError;
            return false;
        }

        const std::string destinationId =
            PersistentEntityId(scene, destinationEntity);
        if (!IsValidStableId(destinationId))
        {
            scene.Entity_Remove(destinationEntity);
            scene.Entity_Remove(agentEntity);
            agentEntity = wi::ecs::INVALID_ENTITY;
            destinationEntity = wi::ecs::INVALID_ENTITY;
            error = "Navigation destination identity could not be resolved.";
            return false;
        }

        auto* agentMetadata = scene.metadatas.GetComponent(agentEntity);
        auto* destinationMetadata = scene.metadatas.GetComponent(destinationEntity);
        if (agentMetadata == nullptr || destinationMetadata == nullptr)
        {
            scene.Entity_Remove(destinationEntity);
            scene.Entity_Remove(agentEntity);
            agentEntity = wi::ecs::INVALID_ENTITY;
            destinationEntity = wi::ecs::INVALID_ENTITY;
            error = "Navigation marker metadata could not be resolved after identity assignment.";
            return false;
        }

        agentMetadata->string_values.set(
            NavigationAgentMetadataKey, NavigationAgentMetadataVersion);
        agentMetadata->string_values.set(
            NavigationGridReferenceMetadataKey, gridId);
        agentMetadata->string_values.set(
            NavigationDestinationReferenceMetadataKey, destinationId);
        agentMetadata->float_values.set(
            NavigationMoveSpeedMetadataKey, DefaultNavigationMoveSpeed);
        agentMetadata->bool_values.set(NavigationFlyingMetadataKey, false);

        destinationMetadata->string_values.set(
            NavigationDestinationMetadataKey,
            NavigationDestinationMetadataVersion);
        destinationMetadata->string_values.set(
            NavigationGridReferenceMetadataKey, gridId);

        // Keep Studio authoring markers as ordinary transform-owned entities.
        // Wicked's CharacterComponent writes its internal position back to the
        // Transform every Scene::Update(), even while inactive, which would
        // undo creator gizmo edits. Runtime materializes the native Character
        // component from this authored Transform when Test Level starts.
        error.clear();
        return true;
    }

    bool ResolveNavigationAgentBinding(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity agentEntity,
        NavigationAgentBinding& binding,
        std::string& error)
    {
        binding = {};
        if (!IsRenegadeNavigationAgent(scene, agentEntity))
        {
            error = "Selected entity is not a Renegade navigation agent.";
            return false;
        }

        const auto* metadata = scene.metadatas.GetComponent(agentEntity);
        if (metadata == nullptr ||
            !metadata->string_values.has(NavigationGridReferenceMetadataKey) ||
            !metadata->string_values.has(
                NavigationDestinationReferenceMetadataKey))
        {
            error = "Navigation agent is missing its grid or destination reference.";
            return false;
        }

        const auto grid = ResolvePersistentEntity(
            scene,
            metadata->string_values.get(NavigationGridReferenceMetadataKey));
        const auto destination = ResolvePersistentEntity(
            scene,
            metadata->string_values.get(
                NavigationDestinationReferenceMetadataKey));
        if (!IsRenegadeNavigationGrid(scene, grid))
        {
            error = "Navigation agent references a missing navigation grid.";
            return false;
        }
        if (!IsRenegadeNavigationDestination(scene, destination))
        {
            error = "Navigation agent references a missing destination.";
            return false;
        }

        binding.agent = agentEntity;
        binding.grid = grid;
        binding.destination = destination;
        binding.settings.moveSpeed = DefaultNavigationMoveSpeed;
        binding.settings.arrivalDistance = DefaultArrivalDistance;
        if (metadata->float_values.has(NavigationMoveSpeedMetadataKey))
        {
            const float authored =
                metadata->float_values.get(NavigationMoveSpeedMetadataKey);
            if (std::isfinite(authored))
                binding.settings.moveSpeed = std::clamp(authored, 0.01f, 1.0f);
        }
        if (metadata->bool_values.has(NavigationFlyingMetadataKey))
        {
            binding.settings.query.flying =
                metadata->bool_values.get(NavigationFlyingMetadataKey);
        }
        // Start the creator proof with Wicked's least restrictive grounded
        // clearance. Agent dimensions can become explicit authoring controls
        // later without making the first route fail on thin test grids.
        binding.settings.query.agentHeight = 1;
        binding.settings.query.agentWidth = 0;

        error.clear();
        return true;
    }

    std::vector<wi::ecs::Entity> CollectNavigationAgents(
        const wi::scene::Scene& scene)
    {
        std::vector<wi::ecs::Entity> result;
        result.reserve(scene.metadatas.GetCount());
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const auto entity = scene.metadatas.GetEntity(index);
            if (IsRenegadeNavigationAgent(scene, entity))
                result.push_back(entity);
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    bool QueryNavigationAgentPath(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity agentEntity,
        NavigationPathResult& result,
        std::string& error)
    {
        NavigationAgentBinding binding;
        if (!ResolveNavigationAgentBinding(scene, agentEntity, binding, error))
            return false;

        const auto* agentTransform = scene.transforms.GetComponent(binding.agent);
        const auto* destinationTransform =
            scene.transforms.GetComponent(binding.destination);
        if (agentTransform == nullptr || destinationTransform == nullptr)
        {
            error = "Navigation agent or destination lost its Transform component.";
            return false;
        }

        return QueryNavigationPath(
            scene,
            binding.grid,
            agentTransform->GetPosition(),
            destinationTransform->GetPosition(),
            binding.settings.query,
            result,
            error);
    }

    bool InitializeRuntimeNavigation(
        wi::scene::Scene& scene,
        NavigationRuntimeState& state,
        std::string& error)
    {
        state.agents.clear();
        const auto agents = CollectNavigationAgents(scene);
        state.agents.reserve(agents.size());
        for (const auto entity : agents)
        {
            NavigationAgentBinding binding;
            if (!ResolveNavigationAgentBinding(scene, entity, binding, error))
            {
                state.agents.clear();
                return false;
            }

            auto* character = scene.characters.GetComponent(binding.agent);
            if (character == nullptr)
                character = &scene.characters.Create(binding.agent);
            const auto* agentTransform = scene.transforms.GetComponent(binding.agent);
            const auto* destinationTransform =
                scene.transforms.GetComponent(binding.destination);
            if (agentTransform == nullptr || destinationTransform == nullptr)
            {
                state.agents.clear();
                error = "Navigation runtime binding lost a required transform component.";
                return false;
            }

            const XMFLOAT3 start = agentTransform->GetPosition();
            const XMFLOAT3 goal = destinationTransform->GetPosition();
            character->width = 0.3f;
            character->height = 1.8f;
            character->SetFootPlacementEnabled(false);
            character->SetPosition(start);
            XMFLOAT3 facing = agentTransform->GetForward();
            if (DistanceSquared(facing, XMFLOAT3(0.0f, 0.0f, 0.0f)) > 0.0001f)
                character->SetFacing(facing);
            character->SetActive(true);

            if (!SetCharacterNavigationGoal(
                    scene,
                    binding.agent,
                    binding.grid,
                    goal,
                    binding.settings.query,
                    error))
            {
                state.agents.clear();
                return false;
            }

            NavigationAgentRuntimeState runtime;
            runtime.binding = binding;
            runtime.lastGoal = goal;
            runtime.repathCountdown = RepathIntervalSeconds;
            runtime.goalSubmitted = true;
            runtime.arrived = false;
            state.agents.push_back(std::move(runtime));
        }

        error.clear();
        return true;
    }

    void UpdateRuntimeNavigation(
        wi::scene::Scene& scene,
        NavigationRuntimeState& state,
        const float dt) noexcept
    {
        if (!std::isfinite(dt) || dt <= 0.0f)
            return;

        for (auto& runtime : state.agents)
        {
            auto* character = scene.characters.GetComponent(runtime.binding.agent);
            const auto* destinationTransform =
                scene.transforms.GetComponent(runtime.binding.destination);
            if (character == nullptr || destinationTransform == nullptr ||
                !IsRenegadeNavigationGrid(scene, runtime.binding.grid))
            {
                continue;
            }

            const XMFLOAT3 goal = destinationTransform->GetPosition();
            runtime.repathCountdown -= dt;
            const bool movedGoal = DistanceSquared(goal, runtime.lastGoal) > 0.01f;
            if (!runtime.goalSubmitted || movedGoal ||
                runtime.repathCountdown <= 0.0f)
            {
                std::string ignored;
                if (SetCharacterNavigationGoal(
                        scene,
                        runtime.binding.agent,
                        runtime.binding.grid,
                        goal,
                        runtime.binding.settings.query,
                        ignored))
                {
                    runtime.lastGoal = goal;
                    runtime.goalSubmitted = true;
                    runtime.repathCountdown = RepathIntervalSeconds;
                }
            }

            const XMFLOAT3 position = character->GetPositionInterpolated();
            const float arrival = runtime.binding.settings.arrivalDistance;
            XMFLOAT3 toGoal{
                goal.x - position.x,
                goal.y - position.y,
                goal.z - position.z};
            if (!runtime.binding.settings.query.flying)
                toGoal.y = 0.0f;
            if (DistanceSquared(
                    XMFLOAT3(0.0f, 0.0f, 0.0f), toGoal) <=
                arrival * arrival)
            {
                runtime.arrived = true;
                continue;
            }
            runtime.arrived = false;

            if (!character->pathquery.is_succesful())
                continue;

            const XMFLOAT3 waypoint = character->pathquery.get_next_waypoint();
            XMFLOAT3 direction{
                waypoint.x - position.x,
                waypoint.y - position.y,
                waypoint.z - position.z};
            if (!runtime.binding.settings.query.flying)
                direction.y = 0.0f;

            float length = std::sqrt(
                direction.x * direction.x +
                direction.y * direction.y +
                direction.z * direction.z);
            if (length <= 0.001f)
            {
                direction = toGoal;
                length = std::sqrt(
                    direction.x * direction.x +
                    direction.y * direction.y +
                    direction.z * direction.z);
            }
            if (length <= 0.001f)
                continue;

            direction.x /= length;
            direction.y /= length;
            direction.z /= length;
            character->Turn(direction);
            const float amount = runtime.binding.settings.moveSpeed;
            character->Move(XMFLOAT3(
                direction.x * amount,
                direction.y * amount,
                direction.z * amount));
        }
    }

    CreateNavigationGridCommand::CreateNavigationGridCommand(
        wi::scene::Scene& scene,
        NavigationGridSettings settings)
        : scene_(&scene), settings_(std::move(settings))
    {
    }

    bool CreateNavigationGridCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;

        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, entity_))
                return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
        }

        std::string error;
        entity_ = CreateNavigationGrid(*scene_, settings_, error);
        if (entity_ == wi::ecs::INVALID_ENTITY)
            return false;

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateNavigationGridCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreateNavigationGridCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    RebuildNavigationGridCommand::RebuildNavigationGridCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity navigationGridEntity,
        NavigationGridSettings settings)
        : scene_(&scene), entity_(navigationGridEntity),
          settings_(std::move(settings))
    {
    }

    bool RebuildNavigationGridCommand::Apply(
        const wi::VoxelGrid& grid) noexcept
    {
        if (scene_ == nullptr || !IsRenegadeNavigationGrid(*scene_, entity_))
            return false;
        auto* target = scene_->voxel_grids.GetComponent(entity_);
        if (target == nullptr)
            return false;
        *target = grid;
        SyncGridTransform(*scene_, entity_, *target);
        return true;
    }

    bool RebuildNavigationGridCommand::Execute()
    {
        if (scene_ == nullptr || !IsRenegadeNavigationGrid(*scene_, entity_))
            return false;

        if (captured_)
            return Apply(after_);

        auto* grid = scene_->voxel_grids.GetComponent(entity_);
        if (grid == nullptr)
            return false;
        before_ = *grid;

        std::string error;
        if (!RebuildNavigationGrid(*scene_, entity_, settings_, error))
            return false;

        grid = scene_->voxel_grids.GetComponent(entity_);
        if (grid == nullptr)
            return false;
        after_ = *grid;
        captured_ = true;
        return true;
    }

    void RebuildNavigationGridCommand::Undo()
    {
        if (captured_)
            (void)Apply(before_);
    }

    CreateNavigationAgentPairCommand::CreateNavigationAgentPairCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity navigationGridEntity,
        const XMFLOAT3& agentPosition,
        const XMFLOAT3& destinationPosition)
        : scene_(&scene), grid_(navigationGridEntity),
          agentPosition_(agentPosition),
          destinationPosition_(destinationPosition)
    {
    }

    bool CreateNavigationAgentPairCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, agent_) || EntityExists(*scene_, destination_))
                return false;
            agentSnapshot_.SetReadModeAndResetPos(true);
            destinationSnapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer agentSerializer;
            wi::ecs::EntitySerializer destinationSerializer;
            agentSerializer.allow_remap = false;
            destinationSerializer.allow_remap = false;
            if (scene_->Entity_Serialize(agentSnapshot_, agentSerializer) != agent_)
                return false;
            if (scene_->Entity_Serialize(
                    destinationSnapshot_, destinationSerializer) != destination_)
            {
                scene_->Entity_Remove(agent_);
                return false;
            }
            return true;
        }

        std::string error;
        if (!CreateNavigationAgentPair(
                *scene_,
                grid_,
                agentPosition_,
                destinationPosition_,
                agent_,
                destination_,
                error))
        {
            return false;
        }

        agentSnapshot_.SetReadModeAndResetPos(false);
        destinationSnapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer agentSerializer;
        wi::ecs::EntitySerializer destinationSerializer;
        scene_->Entity_Serialize(agentSnapshot_, agentSerializer, agent_);
        scene_->Entity_Serialize(
            destinationSnapshot_, destinationSerializer, destination_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateNavigationAgentPairCommand::Undo()
    {
        if (scene_ == nullptr)
            return;
        if (EntityExists(*scene_, destination_))
            scene_->Entity_Remove(destination_);
        if (EntityExists(*scene_, agent_))
            scene_->Entity_Remove(agent_);
    }

    wi::ecs::Entity CreateNavigationAgentPairCommand::CreatedAgent() const noexcept
    {
        return agent_;
    }

    wi::ecs::Entity CreateNavigationAgentPairCommand::CreatedDestination() const noexcept
    {
        return destination_;
    }
}
