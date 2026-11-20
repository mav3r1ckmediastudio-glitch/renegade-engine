#pragma once

#include <WickedEngine.h>
#include "TrackedVehicleService.h"

#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    inline constexpr char FlightVehicleKey[] = "renegade.vehicle.flight";

    inline constexpr char FixedWingVehicleKey[] = "renegade.vehicle.fixed_wing";

    inline bool IsFixedWing(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata && metadata->bool_values.has(FixedWingVehicleKey) &&
            metadata->bool_values.get(FixedWingVehicleKey);
    }

    inline constexpr char PhysicalVtolVehicleKey[] = "renegade.vehicle.physical_vtol";

    inline bool IsPhysicalVtol(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata && metadata->bool_values.has(PhysicalVtolVehicleKey) &&
            metadata->bool_values.get(PhysicalVtolVehicleKey);
    }

    inline constexpr char HelicopterVehicleKey[] = "renegade.vehicle.helicopter";
    inline constexpr char SpacecraftVehicleKey[] = "renegade.vehicle.spacecraft";

    inline bool IsHelicopter(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata && metadata->bool_values.has(HelicopterVehicleKey) &&
            metadata->bool_values.get(HelicopterVehicleKey);
    }

    inline bool IsSpacecraft(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return metadata && metadata->bool_values.has(SpacecraftVehicleKey) &&
            metadata->bool_values.get(SpacecraftVehicleKey);
    }

    inline bool IsForceFlightVehicle(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        return IsPhysicalVtol(scene, entity) || IsHelicopter(scene, entity) || IsSpacecraft(scene, entity);
    }

    inline wi::ecs::Entity ResolveAircraftChassis(
        const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        for (auto remaining = scene.hierarchy.GetCount() + 1;
            entity != wi::ecs::INVALID_ENTITY && remaining > 0; --remaining)
        {
            if (scene.rigidbodies.Contains(entity) && (IsFixedWing(scene, entity) || IsForceFlightVehicle(scene, entity) || IsTrackedVehicle(scene, entity)))
                return entity;
            const auto* parent = scene.hierarchy.GetComponent(entity);
            if (parent == nullptr)
                break;
            entity = parent->parentID;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    inline bool IsFlightVehicle(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        return IsFixedWing(scene, entity) || IsForceFlightVehicle(scene, entity) || (metadata && metadata->bool_values.has(FlightVehicleKey) &&
            metadata->bool_values.get(FlightVehicleKey));
    }

    inline constexpr char GroundVehicleKey[] = "renegade.vehicle.player_drive";

    inline constexpr char ScriptedFlightVehicleKey[] = "renegade.vehicle.scripted_flight";

    enum class PlayerVehicleMode { Disabled, Ground, VTOL, ScriptedVTOL, FixedWing, PhysicalVTOL, Tracked, Helicopter, Spacecraft };

    inline PlayerVehicleMode GetPlayerVehicleMode(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* metadata = scene.metadatas.GetComponent(entity);
        if (IsTrackedVehicle(scene, entity))
            return PlayerVehicleMode::Tracked;
        if (IsFixedWing(scene, entity))
            return PlayerVehicleMode::FixedWing;
        if (IsPhysicalVtol(scene, entity))
            return PlayerVehicleMode::PhysicalVTOL;
        if (IsHelicopter(scene, entity))
            return PlayerVehicleMode::Helicopter;
        if (IsSpacecraft(scene, entity))
            return PlayerVehicleMode::Spacecraft;
        if (IsFlightVehicle(scene, entity))
            return metadata->bool_values.has(ScriptedFlightVehicleKey) &&
                metadata->bool_values.get(ScriptedFlightVehicleKey)
                ? PlayerVehicleMode::ScriptedVTOL : PlayerVehicleMode::VTOL;
        if (metadata && metadata->bool_values.has(GroundVehicleKey) && metadata->bool_values.get(GroundVehicleKey))
            return PlayerVehicleMode::Ground;
        return PlayerVehicleMode::Disabled;
    }

    inline bool SetPlayerVehicleMode(wi::scene::Scene& scene, wi::ecs::Entity entity, PlayerVehicleMode mode)
    {
        if (!scene.transforms.Contains(entity) || GetPlayerVehicleMode(scene, entity) == mode)
            return false;
        auto* metadata = scene.metadatas.GetComponent(entity);
        if (!metadata)
            metadata = &scene.metadatas.Create(entity);
        metadata->bool_values.set(FlightVehicleKey,
            mode == PlayerVehicleMode::VTOL || mode == PlayerVehicleMode::ScriptedVTOL ||
            mode == PlayerVehicleMode::PhysicalVTOL);
        metadata->bool_values.set(PhysicalVtolVehicleKey, mode == PlayerVehicleMode::PhysicalVTOL);
        metadata->bool_values.set(HelicopterVehicleKey, mode == PlayerVehicleMode::Helicopter);
        metadata->bool_values.set(SpacecraftVehicleKey, mode == PlayerVehicleMode::Spacecraft);
        metadata->bool_values.set(ScriptedFlightVehicleKey, mode == PlayerVehicleMode::ScriptedVTOL);
        metadata->bool_values.set(FixedWingVehicleKey, mode == PlayerVehicleMode::FixedWing);
        metadata->bool_values.set(GroundVehicleKey, mode == PlayerVehicleMode::Ground);
        metadata->bool_values.set(TrackedVehicleKey, mode == PlayerVehicleMode::Tracked);
        return true;
    }

    inline bool HasLiveFlightVehicle(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        const auto* body = scene.rigidbodies.GetComponent(entity);
        return IsFlightVehicle(scene, entity) && scene.transforms.Contains(entity) &&
            body && body->physicsobject != nullptr && !body->IsKinematic() && body->mass > 0.f;
    }

    // Inputs are normalized. Keep a target altitude per craft and initialize it
    // to the current height before the first update or pass NaN to initialize.
    struct FlightVehicleInput
    {
        float thrust = 0.f;
        float yaw = 0.f;
        float climb = 0.f;
    };

    // Commands persist until replaced or disabled. Physics applies the loads at the Jolt step rate.
    bool CommandPhysicalVtol(wi::scene::Scene& scene, wi::ecs::Entity entity,
        FlightVehicleInput input, bool enabled = true);
    bool IsPhysicalVtolGrounded(const wi::scene::Scene& scene, wi::ecs::Entity entity);

    // Rotor and thruster commands persist between native physics steps.
    bool CommandHelicopter(wi::scene::Scene& scene, wi::ecs::Entity entity,
        FlightVehicleInput input, bool enabled = true);
    bool IsHelicopterGrounded(const wi::scene::Scene& scene, wi::ecs::Entity entity);

    struct SpacecraftInput
    {
        float forward = 0.f, lateral = 0.f, vertical = 0.f;
        float pitch = 0.f, yaw = 0.f, roll = 0.f;
    };

    bool CommandSpacecraft(wi::scene::Scene& scene, wi::ecs::Entity entity,
        SpacecraftInput input, bool enabled = true);

    struct ScriptedFlightFrame
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        FlightVehicleInput input;
        float dt = 0.f;
    };

    inline bool DriveFlightVehicle(wi::scene::Scene& scene, wi::ecs::Entity entity,
        const FlightVehicleInput& input, float dt, float& targetAltitude)
    {
        if (IsFixedWing(scene, entity) || IsForceFlightVehicle(scene, entity) || !HasLiveFlightVehicle(scene, entity) || !std::isfinite(dt) || dt <= 0.f)
            return false;

        const auto normalized = [](float value) {
            return std::isfinite(value) ? std::clamp(value, -1.f, 1.f) : 0.f;
        };
        auto& body = *scene.rigidbodies.GetComponent(entity);
        const auto& pose = *scene.transforms.GetComponent(entity);
        const auto position = pose.GetPosition();
        auto velocity = wi::physics::GetVelocity(body);
        const XMMATRIX world = XMLoadFloat4x4(&pose.world);
        XMVECTOR forward = XMVectorSetY(world.r[2], 0.f);
        if (XMVectorGetX(XMVector3LengthSq(forward)) < .001f)
            forward = XMVectorSet(0, 0, 1, 0);
        forward = XMVector3Normalize(forward);
        const XMVECTOR desired = forward * (normalized(input.thrust) * 17.f);
        const float dtStep = std::min(dt, .05f);
        const float step = dtStep * 3.7f;
        velocity.x += (XMVectorGetX(desired) - velocity.x) * step;
        velocity.z += (XMVectorGetZ(desired) - velocity.z) * step;

        const float climb = normalized(input.climb);
        if (climb != 0.f || !std::isfinite(targetAltitude))
            targetAltitude = position.y + climb * 1.1f;
        const float desiredVertical = std::clamp((targetAltitude - position.y) * 3.1f, -3.6f, 4.3f);
        velocity.y += (desiredVertical - velocity.y) * std::min(1.f, dtStep * 11.f);
        wi::physics::SetLinearVelocity(body, velocity);

        const XMVECTOR up = XMVector3Normalize(world.r[1]);
        const XMVECTOR level = XMVector3Cross(up, XMVectorSet(0, 1, 0, 0)) * 3.8f;
        XMFLOAT3 rotation;
        XMStoreFloat3(&rotation, XMVectorSetY(level, normalized(input.yaw) * 1.25f));
        wi::physics::SetAngularVelocity(body, rotation);
        return true;
    }
}
