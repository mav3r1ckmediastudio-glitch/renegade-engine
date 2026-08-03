#include "renegade/bridge/CollisionService.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Epsilon = 0.00001f;
    // A degenerate zero-size shape is worse than a small default one: it
    // would either be rejected by the physics backend or produce an
    // effectively invisible collider. Floor every dimension at this value
    // instead of allowing zero.
    constexpr float MinimumDimension = 0.01f;

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }
}

namespace renegade::bridge
{
    CollisionState CaptureCollision(
        const wi::scene::RigidBodyPhysicsComponent& rigidbody) noexcept
    {
        CollisionState state;
        state.shape = rigidbody.shape;
        state.mass = rigidbody.mass;
        state.friction = rigidbody.friction;
        state.restitution = rigidbody.restitution;
        state.boxHalfExtents = rigidbody.box.halfextents;
        state.sphereRadius = rigidbody.sphere.radius;
        state.capsuleRadius = rigidbody.capsule.radius;
        state.capsuleHeight = rigidbody.capsule.height;
        return state;
    }

    CollisionState SanitizeCollisionState(const CollisionState& state) noexcept
    {
        CollisionState result = state;
        result.mass = std::max(result.mass, 0.0f);
        result.friction = std::clamp(result.friction, 0.0f, 1.0f);
        result.restitution = std::clamp(result.restitution, 0.0f, 1.0f);
        result.boxHalfExtents.x = std::max(
            result.boxHalfExtents.x,
            MinimumDimension);
        result.boxHalfExtents.y = std::max(
            result.boxHalfExtents.y,
            MinimumDimension);
        result.boxHalfExtents.z = std::max(
            result.boxHalfExtents.z,
            MinimumDimension);
        result.sphereRadius = std::max(result.sphereRadius, MinimumDimension);
        result.capsuleRadius = std::max(
            result.capsuleRadius,
            MinimumDimension);
        result.capsuleHeight = std::max(
            result.capsuleHeight,
            MinimumDimension);
        return result;
    }

    bool HasCollisionStateChange(
        const CollisionState& before,
        const CollisionState& after) noexcept
    {
        const auto left = SanitizeCollisionState(before);
        const auto right = SanitizeCollisionState(after);
        return left.shape != right.shape ||
            !NearlyEqual(left.mass, right.mass) ||
            !NearlyEqual(left.friction, right.friction) ||
            !NearlyEqual(left.restitution, right.restitution) ||
            !NearlyEqual(left.boxHalfExtents.x, right.boxHalfExtents.x) ||
            !NearlyEqual(left.boxHalfExtents.y, right.boxHalfExtents.y) ||
            !NearlyEqual(left.boxHalfExtents.z, right.boxHalfExtents.z) ||
            !NearlyEqual(left.sphereRadius, right.sphereRadius) ||
            !NearlyEqual(left.capsuleRadius, right.capsuleRadius) ||
            !NearlyEqual(left.capsuleHeight, right.capsuleHeight);
    }

    void ApplyCollision(
        wi::scene::RigidBodyPhysicsComponent& rigidbody,
        const CollisionState& state) noexcept
    {
        const auto safe = SanitizeCollisionState(state);
        rigidbody.shape = safe.shape;
        rigidbody.mass = safe.mass;
        rigidbody.friction = safe.friction;
        rigidbody.restitution = safe.restitution;
        rigidbody.box.halfextents = safe.boxHalfExtents;
        rigidbody.sphere.radius = safe.sphereRadius;
        rigidbody.capsule.radius = safe.capsuleRadius;
        rigidbody.capsule.height = safe.capsuleHeight;
        rigidbody.SetRefreshParametersNeeded(true);
    }

    CreateCollisionCommand::CreateCollisionCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity targetEntity,
        const CollisionState& initial)
        : scene_(&scene)
        , entity_(targetEntity)
        , initial_(SanitizeCollisionState(initial))
    {
    }

    bool CreateCollisionCommand::Execute()
    {
        if (scene_ == nullptr ||
            entity_ == wi::ecs::INVALID_ENTITY ||
            scene_->rigidbodies.Contains(entity_))
        {
            return false;
        }
        auto& rigidbody = scene_->rigidbodies.Create(entity_);
        ApplyCollision(rigidbody, initial_);
        return true;
    }

    void CreateCollisionCommand::Undo()
    {
        if (scene_ != nullptr)
        {
            scene_->rigidbodies.Remove(entity_);
        }
    }

    SetCollisionCommand::SetCollisionCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const CollisionState& collision)
        : scene_(&scene)
        , entity_(entity)
        , after_(SanitizeCollisionState(collision))
    {
        if (const auto* existing = scene.rigidbodies.GetComponent(entity))
        {
            before_ = CaptureCollision(*existing);
        }
    }

    SetCollisionCommand::SetCollisionCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const CollisionState& before,
        const CollisionState& after)
        : scene_(&scene)
        , entity_(entity)
        , before_(SanitizeCollisionState(before))
        , after_(SanitizeCollisionState(after))
    {
    }

    bool SetCollisionCommand::Execute()
    {
        return HasCollisionStateChange(before_, after_) && Apply(after_);
    }

    void SetCollisionCommand::Undo()
    {
        Apply(before_);
    }

    bool SetCollisionCommand::Apply(const CollisionState& state) noexcept
    {
        if (scene_ == nullptr)
        {
            return false;
        }
        auto* rigidbody = scene_->rigidbodies.GetComponent(entity_);
        if (rigidbody == nullptr)
        {
            return false;
        }
        ApplyCollision(*rigidbody, state);
        return true;
    }

    RemoveCollisionCommand::RemoveCollisionCommand(
        wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
        : scene_(&scene)
        , entity_(entity)
    {
    }

    bool RemoveCollisionCommand::Execute()
    {
        if (scene_ == nullptr)
        {
            return false;
        }
        const auto* existing = scene_->rigidbodies.GetComponent(entity_);
        if (existing == nullptr)
        {
            return false;
        }
        removed_ = CaptureCollision(*existing);
        hasRemoved_ = true;
        scene_->rigidbodies.Remove(entity_);
        return true;
    }

    void RemoveCollisionCommand::Undo()
    {
        if (scene_ == nullptr ||
            !hasRemoved_ ||
            scene_->rigidbodies.Contains(entity_))
        {
            return;
        }
        auto& rigidbody = scene_->rigidbodies.Create(entity_);
        ApplyCollision(rigidbody, removed_);
    }
}
