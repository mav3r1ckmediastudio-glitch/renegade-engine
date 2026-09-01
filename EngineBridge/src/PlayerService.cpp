#include "renegade/bridge/PlayerService.h"

#include "renegade/bridge/CharacterPhysicsService.h"
#include "renegade/bridge/PhysicsService.h"

#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    namespace
    {
        constexpr float Epsilon = 0.00001f;

        float FiniteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        float ClampAxis(const float value) noexcept
        {
            return std::clamp(FiniteOr(value, 0.0f), -1.0f, 1.0f);
        }

        bool EntityExists(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY &&
                (scene.transforms.Contains(entity) ||
                    scene.names.Contains(entity) ||
                    scene.metadatas.Contains(entity) ||
                    scene.rigidbodies.Contains(entity));
        }
    }

    bool IsPlayerStart(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata != nullptr &&
            metadata->string_values.has(PlayerStartMetadataKey) &&
            metadata->string_values.get(PlayerStartMetadataKey) ==
                PlayerStartMetadataVersion;
    }

    PlayerStartResult ResolvePlayerStart(const wi::scene::Scene& scene)
    {
        PlayerStartResult result;
        for (std::size_t index = 0; index < scene.metadatas.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.metadatas.GetEntity(index);
            if (!IsPlayerStart(scene, entity))
                continue;

            if (result.start.entity != wi::ecs::INVALID_ENTITY)
            {
                result.resolution = PlayerStartResolution::Multiple;
                result.start = {};
                result.message =
                    "Level contains multiple Player Start markers.";
                return result;
            }

            const auto* transform = scene.transforms.GetComponent(entity);
            if (transform == nullptr)
            {
                result.resolution = PlayerStartResolution::Invalid;
                result.message =
                    "Player Start is missing its native Transform component.";
                return result;
            }
            result.start.entity = entity;
            result.start.transform = CaptureTransform(*transform);
        }

        if (!result.start.IsValid())
        {
            result.resolution = PlayerStartResolution::Missing;
            result.message = "Level has no Player Start marker.";
            return result;
        }

        result.resolution = PlayerStartResolution::Success;
        result.message = "Player Start resolved.";
        return result;
    }

    CreatePlayerStartCommand::CreatePlayerStartCommand(
        wi::scene::Scene& scene,
        const TransformState& transform)
        : scene_(&scene)
        , transform_(transform)
    {
    }

    bool CreatePlayerStartCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;

        const auto existing = ResolvePlayerStart(*scene_);
        if (existing.resolution != PlayerStartResolution::Missing)
        {
            return false;
        }

        if (hasSnapshot_)
        {
            if (EntityExists(*scene_, entity_))
                return false;
            snapshot_.SetReadModeAndResetPos(true);
            wi::ecs::EntitySerializer serializer;
            serializer.allow_remap = false;
            return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
        }

        entity_ = scene_->Entity_CreateTransform("Player Start");
        auto* transform = scene_->transforms.GetComponent(entity_);
        if (entity_ == wi::ecs::INVALID_ENTITY || transform == nullptr)
        {
            if (entity_ != wi::ecs::INVALID_ENTITY)
                scene_->Entity_Remove(entity_);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        transform->translation_local = transform_.translation;
        transform->rotation_local = transform_.rotation;
        transform->scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
        transform->SetDirty();
        transform->UpdateTransform();

        auto& metadata = scene_->metadatas.Create(entity_);
        metadata.preset = wi::scene::MetadataComponent::Preset::Player;
        metadata.string_values.set(
            PlayerStartMetadataKey,
            PlayerStartMetadataVersion);

        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreatePlayerStartCommand::Undo()
    {
        if (scene_ != nullptr && EntityExists(*scene_, entity_))
            scene_->Entity_Remove(entity_);
    }

    wi::ecs::Entity CreatePlayerStartCommand::CreatedEntity() const noexcept
    {
        return entity_;
    }

    PlayerControllerSettings SanitizePlayerControllerSettings(
        const PlayerControllerSettings& settings) noexcept
    {
        PlayerControllerSettings result = settings;
        result.walkSpeed = std::max(0.0f, FiniteOr(result.walkSpeed, 4.5f));
        result.sprintSpeed = std::max(
            result.walkSpeed,
            FiniteOr(result.sprintSpeed, 7.5f));
        result.jumpSpeed = std::max(0.0f, FiniteOr(result.jumpSpeed, 5.0f));
        result.lookSensitivity = std::max(
            0.0f,
            FiniteOr(result.lookSensitivity, 1.0f));
        result.minimumPitch = std::clamp(
            FiniteOr(result.minimumPitch, wi::math::DegreesToRadians(-89.0f)),
            -XM_PIDIV2,
            XM_PIDIV2);
        result.maximumPitch = std::clamp(
            FiniteOr(result.maximumPitch, wi::math::DegreesToRadians(89.0f)),
            result.minimumPitch,
            XM_PIDIV2);
        result.capsuleRadius = std::max(
            0.01f,
            FiniteOr(result.capsuleRadius, 0.4f));
        result.capsuleHeight = std::max(
            0.01f,
            FiniteOr(result.capsuleHeight, 0.5f));
        result.eyeHeight = std::max(
            0.01f,
            FiniteOr(result.eyeHeight, 1.65f));
        return result;
    }

    PlayerMotion EvaluatePlayerInput(
        const PlayerInputFrame& input,
        const float currentYaw,
        const float currentPitch,
        const PlayerControllerSettings& settings) noexcept
    {
        const auto safe = SanitizePlayerControllerSettings(settings);
        PlayerMotion motion;
        motion.yaw = std::remainder(
            FiniteOr(currentYaw, 0.0f) +
                FiniteOr(input.lookYaw, 0.0f) * safe.lookSensitivity,
            XM_PI * 2.0f);
        motion.pitch = std::clamp(
            FiniteOr(currentPitch, 0.0f) +
                FiniteOr(input.lookPitch, 0.0f) * safe.lookSensitivity,
            safe.minimumPitch,
            safe.maximumPitch);

        float right = ClampAxis(input.moveRight);
        float forward = ClampAxis(input.moveForward);
        const float length = std::sqrt(right * right + forward * forward);
        if (length > 1.0f)
        {
            right /= length;
            forward /= length;
        }

        const float sine = std::sin(motion.yaw);
        const float cosine = std::cos(motion.yaw);
        motion.direction = XMFLOAT3(
            right * cosine + forward * sine,
            0.0f,
            forward * cosine - right * sine);
        motion.speed = length > Epsilon
            ? (input.sprintDown ? safe.sprintSpeed : safe.walkSpeed)
            : 0.0f;
        motion.jump = input.jumpPressed ? safe.jumpSpeed : 0.0f;
        return motion;
    }

    bool SpawnRuntimePlayer(
        wi::scene::Scene& scene,
        const PlayerStart& start,
        RuntimePlayerState& state,
        std::string& error,
        const PlayerControllerSettings& settings)
    {
        error.clear();
        if (!start.IsValid() || !IsPlayerStart(scene, start.entity))
        {
            error = "Runtime player requires a valid authored Player Start.";
            return false;
        }
        if (state.IsSpawned())
        {
            error = "Runtime player is already spawned.";
            return false;
        }

        const auto safe = SanitizePlayerControllerSettings(settings);
        const wi::ecs::Entity entity =
            scene.Entity_CreateTransform(RuntimePlayerEntityName);
        auto* transform = scene.transforms.GetComponent(entity);
        if (entity == wi::ecs::INVALID_ENTITY || transform == nullptr)
        {
            error = "Wicked could not create the Runtime player transform.";
            return false;
        }

        transform->translation_local = start.transform.translation;
        transform->rotation_local = start.transform.rotation;
        transform->scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
        transform->SetDirty();
        transform->UpdateTransform();

        auto& body = scene.rigidbodies.Create(entity);
        body.shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape::CAPSULE;
        body.mass = 80.0f;
        body.friction = 0.0f;
        body.restitution = 0.0f;
        body.capsule.radius = safe.capsuleRadius;
        body.capsule.height = safe.capsuleHeight;
        body.SetCharacterPhysics(true);
        body.character.maxSlopeAngle = wi::math::DegreesToRadians(50.0f);
        body.character.gravityFactor = 1.0f;
        body.SetRefreshParametersNeeded(true);

        const XMFLOAT3 rotation =
            wi::math::QuaternionToRollPitchYaw(start.transform.rotation);
        state.entity = entity;
        state.startEntity = start.entity;
        state.spawnPosition = start.transform.translation;
        state.yaw = rotation.y;
        state.pitch = 0.0f;
        return true;
    }

    void DespawnRuntimePlayer(
        wi::scene::Scene& scene,
        RuntimePlayerState& state) noexcept
    {
        if (state.IsSpawned() && EntityExists(scene, state.entity))
            scene.Entity_Remove(state.entity);
        state = {};
    }

    bool UpdateRuntimePlayer(
        wi::scene::Scene& scene,
        RuntimePlayerState& state,
        const PlayerInputFrame& input,
        const PlayerControllerSettings& settings) noexcept
    {
        if (!state.IsSpawned() || !EntityExists(scene, state.entity))
            return false;
        const auto motion = EvaluatePlayerInput(
            input,
            state.yaw,
            state.pitch,
            settings);
        state.yaw = motion.yaw;
        state.pitch = motion.pitch;
        return MovePhysicsCharacter(
            scene,
            state.entity,
            motion.direction,
            motion.speed,
            motion.jump,
            true);
    }

    void ApplyRuntimePlayerCamera(
        wi::scene::Scene& scene,
        const RuntimePlayerState& state,
        wi::scene::CameraComponent& camera,
        const PlayerControllerSettings& settings) noexcept
    {
        if (!state.IsSpawned())
            return;
        const auto safe = SanitizePlayerControllerSettings(settings);
        XMFLOAT3 position = state.spawnPosition;
        if (!GetPhysicsPosition(scene, state.entity, position))
        {
            if (const auto* transform = scene.transforms.GetComponent(state.entity))
                position = transform->GetPosition();
        }
        position.y += safe.eyeHeight;

        wi::scene::TransformComponent cameraTransform;
        cameraTransform.Translate(position);
        cameraTransform.RotateRollPitchYaw(
            XMFLOAT3(state.pitch, state.yaw, 0.0f));
        cameraTransform.UpdateTransform();
        camera.TransformCamera(cameraTransform);
        camera.UpdateCamera();
    }
}
