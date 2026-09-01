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
        constexpr const char* KeyWalkSpeed = "renegade.player.walk_speed";
        constexpr const char* KeySprintSpeed = "renegade.player.sprint_speed";
        constexpr const char* KeyJumpSpeed = "renegade.player.jump_speed";
        constexpr const char* KeyLookSensitivity = "renegade.player.look_sensitivity";
        constexpr const char* KeyMinimumPitch = "renegade.player.minimum_pitch";
        constexpr const char* KeyMaximumPitch = "renegade.player.maximum_pitch";
        constexpr const char* KeyCapsuleRadius = "renegade.player.capsule_radius";
        constexpr const char* KeyCapsuleHeight = "renegade.player.capsule_half_height";
        constexpr const char* KeyEyeHeight = "renegade.player.eye_height";
        constexpr const char* KeyMaximumSlope = "renegade.player.maximum_slope";
        constexpr const char* KeyGravityFactor = "renegade.player.gravity_factor";

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

        float ReadFloat(
            const wi::scene::MetadataComponent& metadata,
            const char* key,
            const float fallback) noexcept
        {
            return metadata.float_values.has(key)
                ? metadata.float_values.get(key)
                : fallback;
        }

        void WritePlayerSettings(
            wi::scene::MetadataComponent& metadata,
            const PlayerControllerSettings& settings)
        {
            metadata.float_values.set(KeyWalkSpeed, settings.walkSpeed);
            metadata.float_values.set(KeySprintSpeed, settings.sprintSpeed);
            metadata.float_values.set(KeyJumpSpeed, settings.jumpSpeed);
            metadata.float_values.set(KeyLookSensitivity, settings.lookSensitivity);
            metadata.float_values.set(KeyMinimumPitch, settings.minimumPitch);
            metadata.float_values.set(KeyMaximumPitch, settings.maximumPitch);
            metadata.float_values.set(KeyCapsuleRadius, settings.capsuleRadius);
            metadata.float_values.set(KeyCapsuleHeight, settings.capsuleHeight);
            metadata.float_values.set(KeyEyeHeight, settings.eyeHeight);
            metadata.float_values.set(KeyMaximumSlope, settings.maximumSlopeDegrees);
            metadata.float_values.set(KeyGravityFactor, settings.gravityFactor);
        }

        bool SettingsDiffer(
            const PlayerControllerSettings& left,
            const PlayerControllerSettings& right) noexcept
        {
            const auto different = [](const float a, const float b)
            {
                return std::abs(a - b) > Epsilon;
            };
            return different(left.walkSpeed, right.walkSpeed) ||
                different(left.sprintSpeed, right.sprintSpeed) ||
                different(left.jumpSpeed, right.jumpSpeed) ||
                different(left.lookSensitivity, right.lookSensitivity) ||
                different(left.minimumPitch, right.minimumPitch) ||
                different(left.maximumPitch, right.maximumPitch) ||
                different(left.capsuleRadius, right.capsuleRadius) ||
                different(left.capsuleHeight, right.capsuleHeight) ||
                different(left.eyeHeight, right.eyeHeight) ||
                different(left.maximumSlopeDegrees, right.maximumSlopeDegrees) ||
                different(left.gravityFactor, right.gravityFactor);
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
            result.start.settings = CapturePlayerControllerSettings(scene, entity);
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

    PlayerControllerSettings CapturePlayerControllerSettings(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        PlayerControllerSettings settings;
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (metadata == nullptr || !IsPlayerStart(scene, entity))
            return settings;
        settings.walkSpeed = ReadFloat(*metadata, KeyWalkSpeed, settings.walkSpeed);
        settings.sprintSpeed = ReadFloat(*metadata, KeySprintSpeed, settings.sprintSpeed);
        settings.jumpSpeed = ReadFloat(*metadata, KeyJumpSpeed, settings.jumpSpeed);
        settings.lookSensitivity = ReadFloat(*metadata, KeyLookSensitivity, settings.lookSensitivity);
        settings.minimumPitch = ReadFloat(*metadata, KeyMinimumPitch, settings.minimumPitch);
        settings.maximumPitch = ReadFloat(*metadata, KeyMaximumPitch, settings.maximumPitch);
        settings.capsuleRadius = ReadFloat(*metadata, KeyCapsuleRadius, settings.capsuleRadius);
        settings.capsuleHeight = ReadFloat(*metadata, KeyCapsuleHeight, settings.capsuleHeight);
        settings.eyeHeight = ReadFloat(*metadata, KeyEyeHeight, settings.eyeHeight);
        settings.maximumSlopeDegrees = ReadFloat(*metadata, KeyMaximumSlope, settings.maximumSlopeDegrees);
        settings.gravityFactor = ReadFloat(*metadata, KeyGravityFactor, settings.gravityFactor);
        return SanitizePlayerControllerSettings(settings);
    }

    float PlayerCapsuleTotalHeight(
        const PlayerControllerSettings& settings) noexcept
    {
        const auto safe = SanitizePlayerControllerSettings(settings);
        return safe.capsuleHeight * 2.0f + safe.capsuleRadius * 2.0f;
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
        WritePlayerSettings(metadata, SanitizePlayerControllerSettings({}));

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

    SetPlayerControllerSettingsCommand::SetPlayerControllerSettingsCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const PlayerControllerSettings& settings)
        : scene_(&scene)
        , entity_(entity)
        , before_(CapturePlayerControllerSettings(scene, entity))
        , after_(SanitizePlayerControllerSettings(settings))
    {
    }

    bool SetPlayerControllerSettingsCommand::Execute()
    {
        if (scene_ == nullptr || !IsPlayerStart(*scene_, entity_) ||
            !SettingsDiffer(before_, after_))
        {
            return false;
        }
        auto* metadata = scene_->metadatas.GetComponent(entity_);
        if (metadata == nullptr)
            return false;
        WritePlayerSettings(*metadata, after_);
        return true;
    }

    void SetPlayerControllerSettingsCommand::Undo()
    {
        if (scene_ == nullptr || !IsPlayerStart(*scene_, entity_))
            return;
        auto* metadata = scene_->metadatas.GetComponent(entity_);
        if (metadata != nullptr)
            WritePlayerSettings(*metadata, before_);
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
        result.maximumSlopeDegrees = std::clamp(
            FiniteOr(result.maximumSlopeDegrees, 50.0f),
            0.0f,
            89.0f);
        result.gravityFactor = std::clamp(
            FiniteOr(result.gravityFactor, 1.0f),
            0.0f,
            10.0f);
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
        body.character.maxSlopeAngle = wi::math::DegreesToRadians(
            safe.maximumSlopeDegrees);
        body.character.gravityFactor = safe.gravityFactor;
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
