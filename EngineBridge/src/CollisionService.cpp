#include "renegade/bridge/CollisionService.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Epsilon = 0.00001f;
    constexpr float MinimumDimension = 0.01f;
    constexpr uint32_t MaximumEditorMeshLod = 6;

    bool NearlyEqual(const float left, const float right) noexcept
    {
        return std::abs(left - right) <= Epsilon;
    }

    bool NearlyEqual(
        const XMFLOAT3& left,
        const XMFLOAT3& right) noexcept
    {
        return NearlyEqual(left.x, right.x) &&
            NearlyEqual(left.y, right.y) &&
            NearlyEqual(left.z, right.z);
    }

    bool IsSupportedShape(
        const wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept
    {
        using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
        switch (shape)
        {
        case Shape::BOX:
        case Shape::SPHERE:
        case Shape::CAPSULE:
        case Shape::CYLINDER:
        case Shape::CONVEX_HULL:
        case Shape::TRIANGLE_MESH:
        case Shape::HEIGHTFIELD:
            return true;
        default:
            return false;
        }
    }

    bool RequiresPhysicsObjectRecreation(
        const renegade::bridge::CollisionState& before,
        const renegade::bridge::CollisionState& after) noexcept
    {
        using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;
        if (before.shape != after.shape)
        {
            return true;
        }

        switch (after.shape)
        {
        case Shape::BOX:
            return !NearlyEqual(before.boxHalfExtents, after.boxHalfExtents);
        case Shape::SPHERE:
            return !NearlyEqual(before.sphereRadius, after.sphereRadius);
        case Shape::CAPSULE:
        case Shape::CYLINDER:
            return !NearlyEqual(before.capsuleRadius, after.capsuleRadius) ||
                !NearlyEqual(before.capsuleHeight, after.capsuleHeight);
        case Shape::TRIANGLE_MESH:
            return before.meshLod != after.meshLod;
        case Shape::CONVEX_HULL:
        case Shape::HEIGHTFIELD:
        default:
            return false;
        }
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
        state.dampingLinear = rigidbody.damping_linear;
        state.dampingAngular = rigidbody.damping_angular;
        state.buoyancy = rigidbody.buoyancy;
        state.localOffset = rigidbody.local_offset;
        state.meshLod = rigidbody.mesh_lod;
        state.kinematic = rigidbody.IsKinematic();
        state.locked2D = rigidbody.IsLocked2D();
        state.disableDeactivation = rigidbody.IsDisableDeactivation();
        state.startDeactivated = rigidbody.IsStartDeactivated();
        state.boxHalfExtents = rigidbody.box.halfextents;
        state.sphereRadius = rigidbody.sphere.radius;
        state.capsuleRadius = rigidbody.capsule.radius;
        state.capsuleHeight = rigidbody.capsule.height;
        return state;
    }

    CollisionState SanitizeCollisionState(const CollisionState& state) noexcept
    {
        CollisionState result = state;
        if (!IsSupportedShape(result.shape))
        {
            result.shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
        }

        result.mass = std::max(result.mass, 0.0f);
        result.friction = std::clamp(result.friction, 0.0f, 1.0f);
        result.restitution = std::clamp(result.restitution, 0.0f, 1.0f);
        result.dampingLinear = std::clamp(result.dampingLinear, 0.0f, 1.0f);
        result.dampingAngular = std::clamp(result.dampingAngular, 0.0f, 1.0f);
        result.buoyancy = std::clamp(result.buoyancy, 0.0f, 2.0f);
        result.meshLod = std::min(result.meshLod, MaximumEditorMeshLod);

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
            !NearlyEqual(left.dampingLinear, right.dampingLinear) ||
            !NearlyEqual(left.dampingAngular, right.dampingAngular) ||
            !NearlyEqual(left.buoyancy, right.buoyancy) ||
            !NearlyEqual(left.localOffset, right.localOffset) ||
            left.meshLod != right.meshLod ||
            left.kinematic != right.kinematic ||
            left.locked2D != right.locked2D ||
            left.disableDeactivation != right.disableDeactivation ||
            left.startDeactivated != right.startDeactivated ||
            !NearlyEqual(left.boxHalfExtents, right.boxHalfExtents) ||
            !NearlyEqual(left.sphereRadius, right.sphereRadius) ||
            !NearlyEqual(left.capsuleRadius, right.capsuleRadius) ||
            !NearlyEqual(left.capsuleHeight, right.capsuleHeight);
    }

    void ApplyCollision(
        wi::scene::RigidBodyPhysicsComponent& rigidbody,
        const CollisionState& state) noexcept
    {
        const auto before = CaptureCollision(rigidbody);
        const auto safe = SanitizeCollisionState(state);

        if (RequiresPhysicsObjectRecreation(before, safe))
        {
            // Wicked Editor resets this implementation-owned handle whenever
            // shape topology changes. The Jolt wrapper then recreates the body
            // against the same Wicked-owned physics scene on the next update.
            rigidbody.physicsobject.reset();
        }

        rigidbody.shape = safe.shape;
        rigidbody.mass = safe.mass;
        rigidbody.friction = safe.friction;
        rigidbody.restitution = safe.restitution;
        rigidbody.damping_linear = safe.dampingLinear;
        rigidbody.damping_angular = safe.dampingAngular;
        rigidbody.buoyancy = safe.buoyancy;
        rigidbody.local_offset = safe.localOffset;
        rigidbody.mesh_lod = safe.meshLod;
        rigidbody.SetKinematic(safe.kinematic);
        rigidbody.SetLocked2D(safe.locked2D);
        rigidbody.SetDisableDeactivation(safe.disableDeactivation);
        rigidbody.SetStartDeactivated(safe.startDeactivated);
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
