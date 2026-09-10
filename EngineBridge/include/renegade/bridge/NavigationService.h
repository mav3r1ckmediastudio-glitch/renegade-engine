#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    inline constexpr const char* NavigationGridMetadataKey =
        "renegade.navigation.grid";
    inline constexpr const char* NavigationGridMetadataVersion = "1";

    // Creator-facing generation settings over Wicked's native VoxelGrid.
    // The VoxelGrid component itself remains the serialized/runtime source of
    // truth; these settings only describe how Renegade should rebuild it.
    struct NavigationGridSettings
    {
        std::uint32_t resolutionX = 128;
        std::uint32_t resolutionY = 32;
        std::uint32_t resolutionZ = 128;
        XMFLOAT3 center = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float voxelSize = 0.25f;
        bool fitToSceneBounds = false;
        std::uint32_t filterMask =
            wi::enums::FILTER_NAVIGATION_MESH |
            wi::enums::FILTER_COLLIDER;
        std::uint32_t layerMask = ~0u;
        std::uint32_t lod = 0;
    };

    struct NavigationQuerySettings
    {
        // Grounded is Wicked's normal surface-navigation mode. Flying switches
        // PathQuery to traversal through empty voxels for true 3D agents.
        bool flying = false;
        int agentHeight = 1;
        int agentWidth = 0;
    };

    struct NavigationPathResult
    {
        bool successful = false;
        XMFLOAT3 requestedStart = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT3 requestedGoal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT3 resolvedGoal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        std::vector<XMFLOAT3> waypoints;
    };

    [[nodiscard]] bool ValidateNavigationGridSettings(
        const NavigationGridSettings& settings,
        std::string& error) noexcept;

    [[nodiscard]] bool IsRenegadeNavigationGrid(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    // Creates one persistent, serialized Wicked VoxelGrid entity and fills it
    // from Scene geometry using Wicked's own Scene::VoxelizeScene path.
    [[nodiscard]] wi::ecs::Entity CreateNavigationGrid(
        wi::scene::Scene& scene,
        const NavigationGridSettings& settings,
        std::string& error);

    // Rebuilds an existing Renegade navigation grid from current Scene data.
    // No competing navmesh or pathfinding world is created.
    [[nodiscard]] bool RebuildNavigationGrid(
        wi::scene::Scene& scene,
        wi::ecs::Entity navigationGridEntity,
        const NavigationGridSettings& settings,
        std::string& error);

    [[nodiscard]] NavigationGridSettings CaptureNavigationGridSettings(
        const wi::scene::Scene& scene,
        wi::ecs::Entity navigationGridEntity) noexcept;

    // Direct synchronous query over Wicked's native PathQuery. Returned
    // waypoints are ordered from start toward goal.
    [[nodiscard]] bool QueryNavigationPath(
        const wi::scene::Scene& scene,
        wi::ecs::Entity navigationGridEntity,
        const XMFLOAT3& start,
        const XMFLOAT3& goal,
        const NavigationQuerySettings& settings,
        NavigationPathResult& result,
        std::string& error);

    // Gives a native Wicked CharacterComponent a goal on the same VoxelGrid.
    // Wicked processes the goal during its normal Scene update and owns the
    // resulting PathQuery on the character component.
    [[nodiscard]] bool SetCharacterNavigationGoal(
        wi::scene::Scene& scene,
        wi::ecs::Entity characterEntity,
        wi::ecs::Entity navigationGridEntity,
        const XMFLOAT3& goal,
        const NavigationQuerySettings& settings,
        std::string& error);

    // Creator-facing mutations are command-backed so creating/rebuilding a
    // navigation grid participates in the same Undo/Redo and dirty-state
    // contract as the rest of Renegade Studio.
    class CreateNavigationGridCommand final : public ICommand
    {
    public:
        CreateNavigationGridCommand(
            wi::scene::Scene& scene,
            NavigationGridSettings settings = {});
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_ = nullptr;
        NavigationGridSettings settings_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    class RebuildNavigationGridCommand final : public ICommand
    {
    public:
        RebuildNavigationGridCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity navigationGridEntity,
            NavigationGridSettings settings);
        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const wi::VoxelGrid& grid) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        NavigationGridSettings settings_;
        wi::VoxelGrid before_;
        wi::VoxelGrid after_;
        bool captured_ = false;
    };
}
