#pragma once

#include <WickedEngine.h>
#include <algorithm>
#include <cmath>

#include "CommandService.h"

namespace renegade::bridge
{
    struct SteeringWheelSettings
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 axis{0.f, 0.f, 1.f};
        float degrees = 180.f; // Centre to full lock.
    };

    inline constexpr char SteeringWheelKey[] = "renegade.vehicle.steering_wheel";
    inline constexpr char SteeringAxisXKey[] = "renegade.vehicle.steering_axis_x";
    inline constexpr char SteeringAxisYKey[] = "renegade.vehicle.steering_axis_y";
    inline constexpr char SteeringAxisZKey[] = "renegade.vehicle.steering_axis_z";
    inline constexpr char SteeringTravelKey[] = "renegade.vehicle.steering_travel";

    inline wi::ecs::Entity SteeringWheelChassis(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        if (!scene.transforms.Contains(entity) || scene.rigidbodies.Contains(entity))
            return wi::ecs::INVALID_ENTITY;
        for (size_t depth = 0; depth < scene.hierarchy.GetCount(); ++depth)
        {
            const auto* hierarchy = scene.hierarchy.GetComponent(entity);
            if (!hierarchy)
                break;
            entity = hierarchy->parentID;
            if (const auto* body = scene.rigidbodies.GetComponent(entity))
                return body->IsCar() ? entity : wi::ecs::INVALID_ENTITY;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    inline bool CanBindSteeringWheel(const wi::scene::Scene& scene, const wi::ecs::Entity chassis,
        const wi::ecs::Entity wheel)
    {
        if (chassis == wi::ecs::INVALID_ENTITY || SteeringWheelChassis(scene, wheel) != chassis)
            return false;
        const auto& vehicle = scene.rigidbodies.GetComponent(chassis)->vehicle;
        return wheel != vehicle.wheel_entity_front_left && wheel != vehicle.wheel_entity_front_right &&
            wheel != vehicle.wheel_entity_rear_left && wheel != vehicle.wheel_entity_rear_right;
    }

    inline SteeringWheelSettings ReadSteeringWheel(const wi::scene::MetadataComponent& metadata, wi::ecs::Entity entity)
    {
        const auto& values = metadata.float_values;
        const auto get = [&](const char* key, const float fallback) {
            const float value = values.has(key) ? values.get(key) : fallback;
            return std::isfinite(value) ? value : fallback;
        };
        return {entity, {get(SteeringAxisXKey, 0.f), get(SteeringAxisYKey, 0.f), get(SteeringAxisZKey, 1.f)},
            std::clamp(get(SteeringTravelKey, 180.f), 0.f, 1080.f)};
    }

    inline SteeringWheelSettings CaptureSteeringWheel(const wi::scene::Scene& scene, wi::ecs::Entity chassis)
    {
        for (size_t i = 0; i < scene.metadatas.GetCount(); ++i)
        {
            const auto& metadata = scene.metadatas[i];
            if (!metadata.bool_values.has(SteeringWheelKey) || !metadata.bool_values.get(SteeringWheelKey))
                continue;
            const auto entity = scene.metadatas.GetEntity(i);
            if (CanBindSteeringWheel(scene, chassis, entity))
                return ReadSteeringWheel(metadata, entity);
        }
        return {};
    }

    class SetSteeringWheelCommand final : public ICommand
    {
    public:
        SetSteeringWheelCommand(wi::scene::Scene& scene, wi::ecs::Entity chassis, SteeringWheelSettings after)
            : scene_(scene), chassis_(chassis), before_(CaptureSteeringWheel(scene, chassis)), after_(after) {}

        bool Execute() override
        {
            if (after_.entity != wi::ecs::INVALID_ENTITY && !CanBindSteeringWheel(scene_, chassis_, after_.entity))
                return false;
            if (before_.entity == after_.entity &&
                before_.axis.x == after_.axis.x && before_.axis.y == after_.axis.y &&
                before_.axis.z == after_.axis.z && before_.degrees == after_.degrees)
                return false;
            Apply(before_, after_);
            return true;
        }
        void Undo() override { Apply(after_, before_); }

    private:
        void Apply(const SteeringWheelSettings& old, const SteeringWheelSettings& value)
        {
            if (old.entity != value.entity)
                if (auto* metadata = scene_.metadatas.GetComponent(old.entity))
                    metadata->bool_values.set(SteeringWheelKey, false);
            if (value.entity == wi::ecs::INVALID_ENTITY)
                return;
            auto* metadata = scene_.metadatas.GetComponent(value.entity);
            if (!metadata)
                metadata = &scene_.metadatas.Create(value.entity);
            metadata->bool_values.set(SteeringWheelKey, true);
            metadata->float_values.set(SteeringAxisXKey, value.axis.x);
            metadata->float_values.set(SteeringAxisYKey, value.axis.y);
            metadata->float_values.set(SteeringAxisZKey, value.axis.z);
            metadata->float_values.set(SteeringTravelKey, value.degrees);
        }

        wi::scene::Scene& scene_;
        wi::ecs::Entity chassis_;
        SteeringWheelSettings before_;
        SteeringWheelSettings after_;
    };

    // Read after the scene's previous physics step completes.
    bool ReadFrontSteeringAngle(const wi::scene::RigidBodyPhysicsComponent& body, float& radians);

    inline float SmoothSteeringAngle(const float current, const float target, const float dt)
    {
        const float blend = 1.f - std::exp(-12.f * std::clamp(dt, 0.f, 0.1f));
        const float next = current + (target - current) * blend;
        return std::abs(next - target) < 0.0001f ? target : next;
    }

    inline XMFLOAT4 SteeringWheelRotation(const SteeringWheelSettings& settings, const XMFLOAT4& rest,
        const float steering, const float maxSteering)
    {
        const auto axis = XMLoadFloat3(&settings.axis);
        if (maxSteering <= 0.f || XMVectorGetX(XMVector3LengthSq(axis)) < 0.000001f)
            return rest;
        const float angle = std::clamp(steering / maxSteering, -1.f, 1.f) *
            wi::math::DegreesToRadians(settings.degrees);
        XMFLOAT4 result;
        XMStoreFloat4(&result, XMQuaternionMultiply(XMQuaternionRotationAxis(axis, angle), XMLoadFloat4(&rest)));
        return result;
    }
}
