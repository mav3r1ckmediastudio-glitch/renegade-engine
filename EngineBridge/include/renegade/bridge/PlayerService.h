#pragma once

#include <WickedEngine.h>

#include <string>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    inline constexpr const char* PlayerStartMetadataKey =
        "renegade.player.start";
    inline constexpr const char* PlayerStartMetadataVersion = "1";
    inline constexpr const char* RuntimePlayerEntityName =
        "__renegade_runtime_player";

    struct PlayerStart
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        TransformState transform;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY;
        }
    };

    enum class PlayerStartResolution
    {
        Success,
        Missing,
        Multiple,
        Invalid,
    };

    struct PlayerStartResult
    {
        PlayerStartResolution resolution = PlayerStartResolution::Missing;
        PlayerStart start;
        std::string message;
    };

    [[nodiscard]] bool IsPlayerStart(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] PlayerStartResult ResolvePlayerStart(
        const wi::scene::Scene& scene);

    class CreatePlayerStartCommand final : public ICommand
    {
    public:
        CreatePlayerStartCommand(
            wi::scene::Scene& scene,
            const TransformState& transform);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity CreatedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_ = nullptr;
        TransformState transform_;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };

    struct PlayerInputFrame
    {
        float moveRight = 0.0f;
        float moveForward = 0.0f;
        float lookYaw = 0.0f;
        float lookPitch = 0.0f;
        bool jumpPressed = false;
        bool sprintDown = false;
    };

    struct PlayerMotion
    {
        XMFLOAT3 direction = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float speed = 0.0f;
        float jump = 0.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;
    };

    struct PlayerControllerSettings
    {
        float walkSpeed = 4.5f;
        float sprintSpeed = 7.5f;
        float jumpSpeed = 5.0f;
        float lookSensitivity = 1.0f;
        float minimumPitch = wi::math::DegreesToRadians(-89.0f);
        float maximumPitch = wi::math::DegreesToRadians(89.0f);
        float capsuleRadius = 0.4f;
        // Wicked/Jolt stores this as the half-height of the cylinder between
        // the capsule caps. 0.5 + a 0.4 radius produces a 1.8 m total capsule.
        float capsuleHeight = 0.5f;
        float eyeHeight = 1.65f;
    };

    struct RuntimePlayerState
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity startEntity = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 spawnPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
        float yaw = 0.0f;
        float pitch = 0.0f;

        [[nodiscard]] constexpr bool IsSpawned() const noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY;
        }
    };

    [[nodiscard]] PlayerControllerSettings SanitizePlayerControllerSettings(
        const PlayerControllerSettings& settings) noexcept;
    [[nodiscard]] PlayerMotion EvaluatePlayerInput(
        const PlayerInputFrame& input,
        float currentYaw,
        float currentPitch,
        const PlayerControllerSettings& settings = {}) noexcept;
    [[nodiscard]] bool SpawnRuntimePlayer(
        wi::scene::Scene& scene,
        const PlayerStart& start,
        RuntimePlayerState& state,
        std::string& error,
        const PlayerControllerSettings& settings = {});
    void DespawnRuntimePlayer(
        wi::scene::Scene& scene,
        RuntimePlayerState& state) noexcept;
    [[nodiscard]] bool UpdateRuntimePlayer(
        wi::scene::Scene& scene,
        RuntimePlayerState& state,
        const PlayerInputFrame& input,
        const PlayerControllerSettings& settings = {}) noexcept;
    void ApplyRuntimePlayerCamera(
        wi::scene::Scene& scene,
        const RuntimePlayerState& state,
        wi::scene::CameraComponent& camera,
        const PlayerControllerSettings& settings = {}) noexcept;
}
