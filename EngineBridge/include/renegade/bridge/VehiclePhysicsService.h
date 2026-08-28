#pragma once

#include <WickedEngine.h>

#include <algorithm>
#include <cmath>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct VehiclePhysicsState
    {
        using Type = wi::scene::RigidBodyPhysicsComponent::Vehicle::Type;
        using CollisionMode =
            wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode;

        Type type = Type::None;
        CollisionMode collisionMode = CollisionMode::Ray;

        float chassisHalfWidth = 0.9f;
        float chassisHalfHeight = 0.2f;
        float chassisHalfLength = 2.0f;
        float frontWheelOffset = 0.0f;
        float rearWheelOffset = 0.0f;
        float wheelRadius = 0.3f;
        float wheelWidth = 0.1f;
        float maxEngineTorque = 500.0f;
        float clutchStrength = 10.0f;
        float maxRollAngle = wi::math::DegreesToRadians(60.0f);
        float maxSteeringAngle = wi::math::DegreesToRadians(30.0f);

        float frontSuspensionMinLength = 0.3f;
        float frontSuspensionMaxLength = 0.5f;
        float frontSuspensionFrequency = 1.5f;
        float frontSuspensionDamping = 0.5f;
        float rearSuspensionMinLength = 0.3f;
        float rearSuspensionMaxLength = 0.5f;
        float rearSuspensionFrequency = 1.5f;
        float rearSuspensionDamping = 0.5f;

        bool fourWheelDrive = false;

        float motorcycleFrontSuspensionAngle =
            wi::math::DegreesToRadians(30.0f);
        float motorcycleFrontBrakeTorque = 500.0f;
        float motorcycleRearBrakeTorque = 250.0f;
        bool motorcycleLeanControl = true;

        wi::ecs::Entity wheelEntityFrontLeft = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity wheelEntityFrontRight = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity wheelEntityRearLeft = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity wheelEntityRearRight = wi::ecs::INVALID_ENTITY;
    };

    namespace vehicle_physics_detail
    {
        inline constexpr float Epsilon = 0.00001f;

        inline float FiniteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        inline bool NearlyEqual(const float left, const float right) noexcept
        {
            return std::abs(left - right) <= Epsilon;
        }

        inline bool IsSupportedType(
            const wi::scene::RigidBodyPhysicsComponent::Vehicle::Type type) noexcept
        {
            using Type = wi::scene::RigidBodyPhysicsComponent::Vehicle::Type;
            switch (type)
            {
            case Type::None:
            case Type::Car:
            case Type::Motorcycle:
                return true;
            default:
                return false;
            }
        }

        inline bool IsSupportedCollisionMode(
            const wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode mode) noexcept
        {
            using Mode =
                wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode;
            switch (mode)
            {
            case Mode::Ray:
            case Mode::Sphere:
            case Mode::Cylinder:
                return true;
            default:
                return false;
            }
        }

        inline bool RequiresPhysicsRefresh(
            const VehiclePhysicsState& before,
            const VehiclePhysicsState& after) noexcept
        {
            return before.type != after.type ||
                before.collisionMode != after.collisionMode ||
                !NearlyEqual(before.chassisHalfWidth, after.chassisHalfWidth) ||
                !NearlyEqual(before.chassisHalfHeight, after.chassisHalfHeight) ||
                !NearlyEqual(before.chassisHalfLength, after.chassisHalfLength) ||
                !NearlyEqual(before.frontWheelOffset, after.frontWheelOffset) ||
                !NearlyEqual(before.rearWheelOffset, after.rearWheelOffset) ||
                !NearlyEqual(before.wheelRadius, after.wheelRadius) ||
                !NearlyEqual(before.wheelWidth, after.wheelWidth) ||
                !NearlyEqual(before.maxEngineTorque, after.maxEngineTorque) ||
                !NearlyEqual(before.clutchStrength, after.clutchStrength) ||
                !NearlyEqual(before.maxRollAngle, after.maxRollAngle) ||
                !NearlyEqual(before.maxSteeringAngle, after.maxSteeringAngle) ||
                !NearlyEqual(
                    before.frontSuspensionMinLength,
                    after.frontSuspensionMinLength) ||
                !NearlyEqual(
                    before.frontSuspensionMaxLength,
                    after.frontSuspensionMaxLength) ||
                !NearlyEqual(
                    before.frontSuspensionFrequency,
                    after.frontSuspensionFrequency) ||
                !NearlyEqual(
                    before.frontSuspensionDamping,
                    after.frontSuspensionDamping) ||
                !NearlyEqual(
                    before.rearSuspensionMinLength,
                    after.rearSuspensionMinLength) ||
                !NearlyEqual(
                    before.rearSuspensionMaxLength,
                    after.rearSuspensionMaxLength) ||
                !NearlyEqual(
                    before.rearSuspensionFrequency,
                    after.rearSuspensionFrequency) ||
                !NearlyEqual(
                    before.rearSuspensionDamping,
                    after.rearSuspensionDamping) ||
                before.fourWheelDrive != after.fourWheelDrive ||
                !NearlyEqual(
                    before.motorcycleFrontSuspensionAngle,
                    after.motorcycleFrontSuspensionAngle) ||
                !NearlyEqual(
                    before.motorcycleFrontBrakeTorque,
                    after.motorcycleFrontBrakeTorque) ||
                !NearlyEqual(
                    before.motorcycleRearBrakeTorque,
                    after.motorcycleRearBrakeTorque);
        }

        inline wi::scene::RigidBodyPhysicsComponent* FindLiveVehicle(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            auto* body = scene.rigidbodies.GetComponent(entity);
            if (body == nullptr ||
                !body->IsVehicle() ||
                body->IsCharacterPhysics() ||
                body->physicsobject == nullptr)
            {
                return nullptr;
            }
            return body;
        }
    }

    [[nodiscard]] inline VehiclePhysicsState CaptureVehiclePhysics(
        const wi::scene::RigidBodyPhysicsComponent& body) noexcept
    {
        VehiclePhysicsState state;
        const auto& vehicle = body.vehicle;
        state.type = vehicle.type;
        state.collisionMode = vehicle.collision_mode;
        state.chassisHalfWidth = vehicle.chassis_half_width;
        state.chassisHalfHeight = vehicle.chassis_half_height;
        state.chassisHalfLength = vehicle.chassis_half_length;
        state.frontWheelOffset = vehicle.front_wheel_offset;
        state.rearWheelOffset = vehicle.rear_wheel_offset;
        state.wheelRadius = vehicle.wheel_radius;
        state.wheelWidth = vehicle.wheel_width;
        state.maxEngineTorque = vehicle.max_engine_torque;
        state.clutchStrength = vehicle.clutch_strength;
        state.maxRollAngle = vehicle.max_roll_angle;
        state.maxSteeringAngle = vehicle.max_steering_angle;
        state.frontSuspensionMinLength = vehicle.front_suspension.min_length;
        state.frontSuspensionMaxLength = vehicle.front_suspension.max_length;
        state.frontSuspensionFrequency = vehicle.front_suspension.frequency;
        state.frontSuspensionDamping = vehicle.front_suspension.damping;
        state.rearSuspensionMinLength = vehicle.rear_suspension.min_length;
        state.rearSuspensionMaxLength = vehicle.rear_suspension.max_length;
        state.rearSuspensionFrequency = vehicle.rear_suspension.frequency;
        state.rearSuspensionDamping = vehicle.rear_suspension.damping;
        state.fourWheelDrive = vehicle.car.four_wheel_drive;
        state.motorcycleFrontSuspensionAngle =
            vehicle.motorcycle.front_suspension_angle;
        state.motorcycleFrontBrakeTorque =
            vehicle.motorcycle.front_brake_torque;
        state.motorcycleRearBrakeTorque =
            vehicle.motorcycle.rear_brake_torque;
        state.motorcycleLeanControl = vehicle.motorcycle.lean_control;
        state.wheelEntityFrontLeft = vehicle.wheel_entity_front_left;
        state.wheelEntityFrontRight = vehicle.wheel_entity_front_right;
        state.wheelEntityRearLeft = vehicle.wheel_entity_rear_left;
        state.wheelEntityRearRight = vehicle.wheel_entity_rear_right;
        return state;
    }

    [[nodiscard]] inline VehiclePhysicsState SanitizeVehiclePhysicsState(
        const VehiclePhysicsState& state) noexcept
    {
        using Type = wi::scene::RigidBodyPhysicsComponent::Vehicle::Type;
        using Mode =
            wi::scene::RigidBodyPhysicsComponent::Vehicle::CollisionMode;

        VehiclePhysicsState result = state;
        if (!vehicle_physics_detail::IsSupportedType(result.type))
        {
            result.type = Type::None;
        }
        if (!vehicle_physics_detail::IsSupportedCollisionMode(result.collisionMode))
        {
            result.collisionMode = Mode::Ray;
        }

        result.wheelRadius = std::clamp(
            vehicle_physics_detail::FiniteOr(result.wheelRadius, 0.3f),
            0.001f,
            10.0f);
        result.wheelWidth = std::clamp(
            vehicle_physics_detail::FiniteOr(result.wheelWidth, 0.1f),
            0.001f,
            10.0f);
        result.chassisHalfWidth = std::clamp(
            vehicle_physics_detail::FiniteOr(result.chassisHalfWidth, 0.9f),
            0.0f,
            10.0f);
        result.chassisHalfHeight = std::clamp(
            vehicle_physics_detail::FiniteOr(result.chassisHalfHeight, 0.2f),
            -10.0f,
            10.0f);
        result.chassisHalfLength = std::clamp(
            vehicle_physics_detail::FiniteOr(result.chassisHalfLength, 2.0f),
            0.0f,
            10.0f);
        result.frontWheelOffset = std::clamp(
            vehicle_physics_detail::FiniteOr(result.frontWheelOffset, 0.0f),
            -10.0f,
            10.0f);
        result.rearWheelOffset = std::clamp(
            vehicle_physics_detail::FiniteOr(result.rearWheelOffset, 0.0f),
            -10.0f,
            10.0f);
        result.maxEngineTorque = std::clamp(
            vehicle_physics_detail::FiniteOr(result.maxEngineTorque, 500.0f),
            0.0f,
            1000.0f);
        result.clutchStrength = std::clamp(
            vehicle_physics_detail::FiniteOr(result.clutchStrength, 10.0f),
            0.0f,
            100.0f);
        result.maxRollAngle = std::clamp(
            vehicle_physics_detail::FiniteOr(
                result.maxRollAngle,
                wi::math::DegreesToRadians(60.0f)),
            0.0f,
            XM_PI);
        result.maxSteeringAngle = std::clamp(
            vehicle_physics_detail::FiniteOr(
                result.maxSteeringAngle,
                wi::math::DegreesToRadians(30.0f)),
            0.0f,
            XM_PIDIV2);

        auto suspension = [](float value, float fallback) {
            return std::clamp(
                vehicle_physics_detail::FiniteOr(value, fallback),
                0.0f,
                2.0f);
        };
        result.frontSuspensionMinLength =
            suspension(result.frontSuspensionMinLength, 0.3f);
        result.frontSuspensionMaxLength =
            suspension(result.frontSuspensionMaxLength, 0.5f);
        result.frontSuspensionFrequency =
            suspension(result.frontSuspensionFrequency, 1.5f);
        result.frontSuspensionDamping =
            suspension(result.frontSuspensionDamping, 0.5f);
        result.rearSuspensionMinLength =
            suspension(result.rearSuspensionMinLength, 0.3f);
        result.rearSuspensionMaxLength =
            suspension(result.rearSuspensionMaxLength, 0.5f);
        result.rearSuspensionFrequency =
            suspension(result.rearSuspensionFrequency, 1.5f);
        result.rearSuspensionDamping =
            suspension(result.rearSuspensionDamping, 0.5f);

        result.motorcycleFrontSuspensionAngle = std::clamp(
            vehicle_physics_detail::FiniteOr(
                result.motorcycleFrontSuspensionAngle,
                wi::math::DegreesToRadians(30.0f)),
            0.0f,
            XM_PIDIV2);
        result.motorcycleFrontBrakeTorque = std::clamp(
            vehicle_physics_detail::FiniteOr(
                result.motorcycleFrontBrakeTorque,
                500.0f),
            0.0f,
            2000.0f);
        result.motorcycleRearBrakeTorque = std::clamp(
            vehicle_physics_detail::FiniteOr(
                result.motorcycleRearBrakeTorque,
                250.0f),
            0.0f,
            2000.0f);
        return result;
    }

    [[nodiscard]] inline bool HasVehiclePhysicsStateChange(
        const VehiclePhysicsState& before,
        const VehiclePhysicsState& after) noexcept
    {
        const auto left = SanitizeVehiclePhysicsState(before);
        const auto right = SanitizeVehiclePhysicsState(after);
        return vehicle_physics_detail::RequiresPhysicsRefresh(left, right) ||
            left.motorcycleLeanControl != right.motorcycleLeanControl ||
            left.wheelEntityFrontLeft != right.wheelEntityFrontLeft ||
            left.wheelEntityFrontRight != right.wheelEntityFrontRight ||
            left.wheelEntityRearLeft != right.wheelEntityRearLeft ||
            left.wheelEntityRearRight != right.wheelEntityRearRight;
    }

    inline void ApplyVehiclePhysics(
        wi::scene::RigidBodyPhysicsComponent& body,
        const VehiclePhysicsState& state) noexcept
    {
        const auto before = CaptureVehiclePhysics(body);
        const auto safe = SanitizeVehiclePhysicsState(state);
        const bool needsRefresh =
            vehicle_physics_detail::RequiresPhysicsRefresh(before, safe);

        auto& vehicle = body.vehicle;
        vehicle.type = safe.type;
        vehicle.collision_mode = safe.collisionMode;
        vehicle.chassis_half_width = safe.chassisHalfWidth;
        vehicle.chassis_half_height = safe.chassisHalfHeight;
        vehicle.chassis_half_length = safe.chassisHalfLength;
        vehicle.front_wheel_offset = safe.frontWheelOffset;
        vehicle.rear_wheel_offset = safe.rearWheelOffset;
        vehicle.wheel_radius = safe.wheelRadius;
        vehicle.wheel_width = safe.wheelWidth;
        vehicle.max_engine_torque = safe.maxEngineTorque;
        vehicle.clutch_strength = safe.clutchStrength;
        vehicle.max_roll_angle = safe.maxRollAngle;
        vehicle.max_steering_angle = safe.maxSteeringAngle;
        vehicle.front_suspension.min_length = safe.frontSuspensionMinLength;
        vehicle.front_suspension.max_length = safe.frontSuspensionMaxLength;
        vehicle.front_suspension.frequency = safe.frontSuspensionFrequency;
        vehicle.front_suspension.damping = safe.frontSuspensionDamping;
        vehicle.rear_suspension.min_length = safe.rearSuspensionMinLength;
        vehicle.rear_suspension.max_length = safe.rearSuspensionMaxLength;
        vehicle.rear_suspension.frequency = safe.rearSuspensionFrequency;
        vehicle.rear_suspension.damping = safe.rearSuspensionDamping;
        vehicle.car.four_wheel_drive = safe.fourWheelDrive;
        vehicle.motorcycle.front_suspension_angle =
            safe.motorcycleFrontSuspensionAngle;
        vehicle.motorcycle.front_brake_torque =
            safe.motorcycleFrontBrakeTorque;
        vehicle.motorcycle.rear_brake_torque =
            safe.motorcycleRearBrakeTorque;
        vehicle.motorcycle.lean_control = safe.motorcycleLeanControl;
        vehicle.wheel_entity_front_left = safe.wheelEntityFrontLeft;
        vehicle.wheel_entity_front_right = safe.wheelEntityFrontRight;
        vehicle.wheel_entity_rear_left = safe.wheelEntityRearLeft;
        vehicle.wheel_entity_rear_right = safe.wheelEntityRearRight;

        if (needsRefresh)
        {
            // Wicked Editor routes every solver-affecting vehicle property
            // through the rigid-body refresh path. Lean control and visual
            // wheel mappings are intentionally live and need no rebuild.
            body.SetRefreshParametersNeeded(true);
        }
    }

    class SetVehiclePhysicsCommand final : public ICommand
    {
    public:
        SetVehiclePhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const VehiclePhysicsState& state)
            : scene_(&scene)
            , entity_(entity)
            , after_(SanitizeVehiclePhysicsState(state))
        {
            if (const auto* body = scene.rigidbodies.GetComponent(entity))
            {
                before_ = CaptureVehiclePhysics(*body);
            }
        }

        SetVehiclePhysicsCommand(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity,
            const VehiclePhysicsState& before,
            const VehiclePhysicsState& after)
            : scene_(&scene)
            , entity_(entity)
            , before_(SanitizeVehiclePhysicsState(before))
            , after_(SanitizeVehiclePhysicsState(after))
        {
        }

        bool Execute() override
        {
            return HasVehiclePhysicsStateChange(before_, after_) && Apply(after_);
        }

        void Undo() override
        {
            Apply(before_);
        }

    private:
        bool Apply(const VehiclePhysicsState& state) noexcept
        {
            if (scene_ == nullptr)
            {
                return false;
            }
            auto* body = scene_->rigidbodies.GetComponent(entity_);
            if (body == nullptr)
            {
                return false;
            }
            ApplyVehiclePhysics(*body, state);
            return true;
        }

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        VehiclePhysicsState before_;
        VehiclePhysicsState after_;
    };

    [[nodiscard]] inline bool HasCharacterVehiclePhysicsConflict(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* body = scene.rigidbodies.GetComponent(entity);
        return body != nullptr && body->IsVehicle() && body->IsCharacterPhysics();
    }

    [[nodiscard]] inline bool HasLiveVehiclePhysics(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* body = scene.rigidbodies.GetComponent(entity);
        return body != nullptr &&
            body->IsVehicle() &&
            !body->IsCharacterPhysics() &&
            body->physicsobject != nullptr;
    }

    [[nodiscard]] inline bool DrivePhysicsVehicle(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const float forward,
        const float right,
        const float brake,
        const float handbrake) noexcept
    {
        auto* body = vehicle_physics_detail::FindLiveVehicle(scene, entity);
        if (body == nullptr)
        {
            return false;
        }

        wi::physics::DriveVehicle(
            *body,
            std::clamp(vehicle_physics_detail::FiniteOr(forward, 0.0f), -1.0f, 1.0f),
            std::clamp(vehicle_physics_detail::FiniteOr(right, 0.0f), -1.0f, 1.0f),
            std::clamp(vehicle_physics_detail::FiniteOr(brake, 0.0f), 0.0f, 1.0f),
            std::clamp(vehicle_physics_detail::FiniteOr(handbrake, 0.0f), 0.0f, 1.0f));
        return true;
    }

    [[nodiscard]] inline bool GetVehicleForwardVelocity(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        float& velocity) noexcept
    {
        auto* body = vehicle_physics_detail::FindLiveVehicle(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        velocity = wi::physics::GetVehicleForwardVelocity(*body);
        return true;
    }

    [[nodiscard]] inline bool UpdateVehicleWheelTransforms(
        wi::scene::Scene& scene) noexcept
    {
        if (scene.physics_scene == nullptr || !wi::physics::IsSimulationEnabled())
        {
            return false;
        }
        wi::physics::OverrideWehicleWheelTransforms(scene);
        return true;
    }
}
