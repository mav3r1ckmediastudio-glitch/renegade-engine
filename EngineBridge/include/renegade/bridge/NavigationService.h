#pragma once

#include <cstddef>
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
    inline constexpr const char* NavigationAgentMetadataKey =
        "renegade.navigation.agent";
    inline constexpr const char* NavigationAgentMetadataVersion = "1";
    inline constexpr const char* NavigationDestinationMetadataKey =
        "renegade.navigation.destination";
    inline constexpr const char* NavigationDestinationMetadataVersion = "1";
    inline constexpr const char* NavigationGridReferenceMetadataKey =
        "renegade.navigation.grid_id";
    inline constexpr const char* NavigationDestinationReferenceMetadataKey =
        "renegade.navigation.destination_id";
    inline constexpr const char* NavigationMoveSpeedMetadataKey =
        "renegade.navigation.move_speed";
    inline constexpr const char* NavigationFlyingMetadataKey =
        "renegade.navigation.flying";

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

    struct NavigationAgentSettings
    {
        // Wicked's own character-controller sample uses small per-frame Move()
        // amounts rather than a metres/second transform override. Renegade keeps
        // that native CharacterComponent movement contract and only exposes a
        // creator-friendly amount.
        float moveSpeed = 0.12f;
        float arrivalDistance = 0.55f;
        NavigationQuerySettings query;
    };

    struct NavigationAgentBinding
    {
        wi::ecs::Entity agent = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity grid = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity destination = wi::ecs::INVALID_ENTITY;
        NavigationAgentSettings settings;
    };

    struct NavigationAgentRuntimeState
    {
        NavigationAgentBinding binding;
        XMFLOAT3 lastGoal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float repathCountdown = 0.0f;
        bool goalSubmitted = false;
        bool arrived = false;
    };

    struct NavigationRuntimeState
    {
        std::vector<NavigationAgentRuntimeState> agents;
    };

    // Makes creator Jolt rigid-body geometry participate in Wicked navigation.
    // Returns the number of render objects newly admitted to navigation.
    [[nodiscard]] std::size_t PrepareRigidBodyNavigationGeometry(
        wi::scene::Scene& scene) noexcept;

    [[nodiscard]] bool ValidateNavigationGridSettings(
        const NavigationGridSettings& settings,
        std::string& error) noexcept;

    [[nodiscard]] bool IsRenegadeNavigationGrid(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] bool IsRenegadeNavigationAgent(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] bool IsRenegadeNavigationDestination(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] bool IsRenegadeNavigationEntity(
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

    // Creator proof helper. The agent and destination are ordinary serialized
    // Scene entities. The agent owns Wicked's native CharacterComponent and is
    // inactive in Studio; Runtime activates it and follows the linked target.
    [[nodiscard]] bool CreateNavigationAgentPair(
        wi::scene::Scene& scene,
        wi::ecs::Entity navigationGridEntity,
        const XMFLOAT3& agentPosition,
        const XMFLOAT3& destinationPosition,
        wi::ecs::Entity& agentEntity,
        wi::ecs::Entity& destinationEntity,
        std::string& error);

    [[nodiscard]] bool ResolveNavigationAgentBinding(
        const wi::scene::Scene& scene,
        wi::ecs::Entity agentEntity,
        NavigationAgentBinding& binding,
        std::string& error);
    [[nodiscard]] std::vector<wi::ecs::Entity> CollectNavigationAgents(
        const wi::scene::Scene& scene);
    [[nodiscard]] bool QueryNavigationAgentPath(
        const wi::scene::Scene& scene,
        wi::ecs::Entity agentEntity,
        NavigationPathResult& result,
        std::string& error);

    // Runtime keeps no competing navigation simulation. This state only caches
    // resolved authoring references and repath cadence; movement is issued to
    // Wicked CharacterComponent::Turn()/Move() and path ownership remains on
    // CharacterComponent::pathquery.
    [[nodiscard]] bool InitializeRuntimeNavigation(
        wi::scene::Scene& scene,
        NavigationRuntimeState& state,
        std::string& error);
    void UpdateRuntimeNavigation(
        wi::scene::Scene& scene,
        NavigationRuntimeState& state,
        float dt) noexcept;

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

    class CreateNavigationAgentPairCommand final : public ICommand
    {
    public:
        CreateNavigationAgentPairCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity navigationGridEntity,
            const XMFLOAT3& agentPosition,
            const XMFLOAT3& destinationPosition);
        bool Execute() override;
        void Undo() override;
        [[nodiscard]] wi::ecs::Entity CreatedAgent() const noexcept;
        [[nodiscard]] wi::ecs::Entity CreatedDestination() const noexcept;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity grid_ = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 agentPosition_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
        XMFLOAT3 destinationPosition_ = XMFLOAT3(0.0f, 0.0f, 0.0f);
        wi::ecs::Entity agent_ = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity destination_ = wi::ecs::INVALID_ENTITY;
        wi::Archive agentSnapshot_;
        wi::Archive destinationSnapshot_;
        bool hasSnapshot_ = false;
    };
}