#include "renegade/bridge/NavigationService.h"

#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr std::uint32_t MinimumResolution = 4;
    constexpr std::uint32_t MaximumResolution = 1024;
    constexpr std::uint64_t MaximumVoxelCount =
        128ull * 1024ull * 1024ull;
    constexpr int MaximumAgentExtentVoxels = 64;

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
}

namespace renegade::bridge
{
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
        if (!std::isfinite(settings.center.x) ||
            !std::isfinite(settings.center.y) ||
            !std::isfinite(settings.center.z))
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
}
