#include "renegade/bridge/PhysicsService.h"

#include <algorithm>
#include <cmath>

namespace renegade::bridge
{
    namespace
    {
        float finiteOr(const float value, const float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        wi::scene::RigidBodyPhysicsComponent* FindLiveRigidBody(
            wi::scene::Scene& scene,
            const wi::ecs::Entity entity) noexcept
        {
            auto* body = FindRigidBody(scene, entity);
            if (body == nullptr || body->physicsobject == nullptr)
            {
                return nullptr;
            }
            return body;
        }
    }

    PhysicsWorldState CapturePhysicsWorldState() noexcept
    {
        PhysicsWorldState state;
        state.enabled = wi::physics::IsEnabled();
        state.simulationEnabled = wi::physics::IsSimulationEnabled();
        state.interpolationEnabled = wi::physics::IsInterpolationEnabled();
        state.debugDrawEnabled = wi::physics::IsDebugDrawEnabled();
        state.accuracy = wi::physics::GetAccuracy();
        state.frameRate = wi::physics::GetFrameRate();
        state.constraintDebugSize = wi::physics::GetConstraintDebugSize();
        state.debugDrawMaxDistance = wi::physics::GetDebugDrawMaxDistance();
        state.characterCollisionTolerance = wi::physics::GetCharacterCollisionTolerance();
        return state;
    }

    PhysicsWorldState SanitizePhysicsWorldState(const PhysicsWorldState& state) noexcept
    {
        PhysicsWorldState result = state;
        result.accuracy = std::clamp(result.accuracy, 1, 16);
        result.frameRate = std::clamp(
            finiteOr(result.frameRate, 60.0f),
            1.0f,
            1000.0f);
        result.constraintDebugSize = std::max(
            0.0f,
            finiteOr(result.constraintDebugSize, 1.0f));
        result.debugDrawMaxDistance = std::max(
            0.0f,
            finiteOr(result.debugDrawMaxDistance, 500.0f));
        result.characterCollisionTolerance = std::max(
            0.0f,
            finiteOr(result.characterCollisionTolerance, 0.05f));
        return result;
    }

    void ApplyPhysicsWorldState(const PhysicsWorldState& state) noexcept
    {
        const auto sanitized = SanitizePhysicsWorldState(state);
        wi::physics::SetEnabled(sanitized.enabled);
        wi::physics::SetSimulationEnabled(sanitized.simulationEnabled);
        wi::physics::SetInterpolationEnabled(sanitized.interpolationEnabled);
        wi::physics::SetDebugDrawEnabled(sanitized.debugDrawEnabled);
        wi::physics::SetAccuracy(sanitized.accuracy);
        wi::physics::SetFrameRate(sanitized.frameRate);
        wi::physics::SetConstraintDebugSize(sanitized.constraintDebugSize);
        wi::physics::SetDebugDrawMaxDistance(sanitized.debugDrawMaxDistance);
        wi::physics::SetCharacterCollisionTolerance(
            sanitized.characterCollisionTolerance);
    }

    XMFLOAT3 GetPhysicsGravity(const wi::scene::Scene& scene) noexcept
    {
        return scene.weather.gravity;
    }

    XMFLOAT3 SanitizePhysicsGravity(const XMFLOAT3& gravity) noexcept
    {
        return XMFLOAT3(
            finiteOr(gravity.x, 0.0f),
            finiteOr(gravity.y, -10.0f),
            finiteOr(gravity.z, 0.0f));
    }

    void SetPhysicsGravity(
        wi::scene::Scene& scene,
        const XMFLOAT3& gravity) noexcept
    {
        scene.weather.gravity = SanitizePhysicsGravity(gravity);
    }

    wi::scene::RigidBodyPhysicsComponent* FindRigidBody(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return nullptr;
        }
        return scene.rigidbodies.GetComponent(entity);
    }

    const wi::scene::RigidBodyPhysicsComponent* FindRigidBody(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return nullptr;
        }
        return scene.rigidbodies.GetComponent(entity);
    }

    bool HasLivePhysicsBody(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity) noexcept
    {
        const auto* body = FindRigidBody(scene, entity);
        return body != nullptr && body->physicsobject != nullptr;
    }

    bool SetPhysicsPosition(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& position) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::SetPosition(*body, position);
        return true;
    }

    bool SetPhysicsPositionAndRotation(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& position,
        const XMFLOAT4& rotation) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::SetPositionAndRotation(*body, position, rotation);
        return true;
    }

    bool GetPhysicsPosition(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        XMFLOAT3& position) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        position = wi::physics::GetPosition(*body);
        return true;
    }

    bool GetPhysicsRotation(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        XMFLOAT4& rotation) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        rotation = wi::physics::GetRotation(*body);
        return true;
    }

    bool SetLinearVelocity(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& velocity) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::SetLinearVelocity(*body, velocity);
        return true;
    }

    bool GetLinearVelocity(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        XMFLOAT3& velocity) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        velocity = wi::physics::GetVelocity(*body);
        return true;
    }

    bool SetAngularVelocity(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& velocity) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::SetAngularVelocity(*body, velocity);
        return true;
    }

    bool ApplyForce(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& force) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::ApplyForce(*body, force);
        return true;
    }

    bool ApplyForceAt(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& force,
        const XMFLOAT3& position,
        const bool positionIsLocal) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::ApplyForceAt(*body, force, position, positionIsLocal);
        return true;
    }

    bool ApplyImpulse(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& impulse) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::ApplyImpulse(*body, impulse);
        return true;
    }

    bool ApplyImpulseAt(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& impulse,
        const XMFLOAT3& position,
        const bool positionIsLocal) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::ApplyImpulseAt(
            *body,
            impulse,
            position,
            positionIsLocal);
        return true;
    }

    bool ApplyTorque(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const XMFLOAT3& torque) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::ApplyTorque(*body, torque);
        return true;
    }

    bool SetBodyActive(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const bool active) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::SetActivationState(
            *body,
            active
                ? wi::physics::ActivationState::Active
                : wi::physics::ActivationState::Inactive);
        return true;
    }

    bool SetGhostMode(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const bool ghost) noexcept
    {
        auto* body = FindLiveRigidBody(scene, entity);
        if (body == nullptr)
        {
            return false;
        }
        wi::physics::SetGhostMode(*body, ghost);
        return true;
    }

    bool ActivateAllRigidBodies(wi::scene::Scene& scene) noexcept
    {
        if (scene.physics_scene == nullptr)
        {
            return false;
        }
        wi::physics::ActivateAllRigidBodies(scene);
        return true;
    }

    bool OptimizePhysicsBroadPhase(wi::scene::Scene& scene) noexcept
    {
        if (scene.physics_scene == nullptr)
        {
            return false;
        }
        wi::physics::OptimizeBroadPhase(scene);
        return true;
    }

    bool ResetPhysicsObjects(wi::scene::Scene& scene) noexcept
    {
        if (scene.physics_scene == nullptr)
        {
            return false;
        }
        wi::physics::ResetPhysicsObjects(scene);
        return true;
    }

    PhysicsRayHit Raycast(
        const wi::scene::Scene& scene,
        const wi::primitive::Ray& ray) noexcept
    {
        const auto hit = wi::physics::Intersects(scene, ray);
        PhysicsRayHit result;
        result.entity = hit.entity;
        result.position = hit.position;
        result.localPosition = hit.position_local;
        result.normal = hit.normal;
        result.ragdollEntity = hit.humanoid_ragdoll_entity;
        result.softBodyTriangle = hit.softbody_triangleID;
        return result;
    }
}
