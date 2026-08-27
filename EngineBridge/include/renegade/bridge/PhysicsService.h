#pragma once

#include <WickedEngine.h>

namespace renegade::bridge
{
    // JP01 foundation contract. Renegade owns this API; Wicked remains the
    // authoritative physics implementation and scene/serialization owner.
    // Studio and future Lua bindings consume this facade rather than reaching
    // directly into Wicked/Jolt internals.
    struct PhysicsWorldState
    {
        bool enabled = true;
        bool simulationEnabled = true;
        bool interpolationEnabled = true;
        bool debugDrawEnabled = false;
        int accuracy = 4;
        float frameRate = 120.0f;
        float constraintDebugSize = 1.0f;
        float debugDrawMaxDistance = 1000.0f;
        float characterCollisionTolerance = 0.1f;
    };

    [[nodiscard]] PhysicsWorldState CapturePhysicsWorldState() noexcept;
    [[nodiscard]] PhysicsWorldState SanitizePhysicsWorldState(
        const PhysicsWorldState& state) noexcept;
    void ApplyPhysicsWorldState(const PhysicsWorldState& state) noexcept;

    [[nodiscard]] wi::scene::RigidBodyPhysicsComponent* FindRigidBody(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;
    [[nodiscard]] const wi::scene::RigidBodyPhysicsComponent* FindRigidBody(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    // Entity-oriented runtime operations form the stable seam that future Lua
    // bindings can expose safely. They deliberately avoid raw Jolt pointers.
    [[nodiscard]] bool SetPhysicsPosition(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const XMFLOAT3& position) noexcept;
    [[nodiscard]] bool SetPhysicsPositionAndRotation(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const XMFLOAT3& position,
        const XMFLOAT4& rotation) noexcept;
    [[nodiscard]] bool SetLinearVelocity(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const XMFLOAT3& velocity) noexcept;
    [[nodiscard]] bool SetAngularVelocity(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const XMFLOAT3& velocity) noexcept;
    [[nodiscard]] bool ApplyForce(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const XMFLOAT3& force) noexcept;
    [[nodiscard]] bool ApplyImpulse(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const XMFLOAT3& impulse) noexcept;
    [[nodiscard]] bool ApplyTorque(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        const XMFLOAT3& torque) noexcept;
    [[nodiscard]] bool SetBodyActive(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        bool active) noexcept;
    [[nodiscard]] bool SetGhostMode(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        bool ghost) noexcept;

    struct PhysicsRayHit
    {
        wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
        XMFLOAT3 position = XMFLOAT3(0, 0, 0);
        XMFLOAT3 localPosition = XMFLOAT3(0, 0, 0);
        XMFLOAT3 normal = XMFLOAT3(0, 0, 0);
        wi::ecs::Entity ragdollEntity = wi::ecs::INVALID_ENTITY;
        int softBodyTriangle = -1;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return entity != wi::ecs::INVALID_ENTITY;
        }
    };

    [[nodiscard]] PhysicsRayHit Raycast(
        const wi::scene::Scene& scene,
        const wi::primitive::Ray& ray) noexcept;
}
