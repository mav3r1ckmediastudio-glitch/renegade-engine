#pragma once

#include <cstddef>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // Creator-facing rigid-body authoring state for JP01. Wicked's
    // RigidBodyPhysicsComponent remains the serialized source of truth; this
    // type is only the Renegade command/UI facade over it.
    struct CollisionState
    {
        wi::scene::RigidBodyPhysicsComponent::CollisionShape shape =
            wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;

        // Standard body properties.
        // mass == 0 creates a static body.
        float mass = 1.0f;
        float friction = 0.2f;
        float restitution = 0.1f;
        float dampingLinear = 0.05f;
        float dampingAngular = 0.05f;
        float buoyancy = 1.2f;
        XMFLOAT3 localOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);
        uint32_t meshLod = 0;

        bool kinematic = false;
        bool locked2D = false;
        bool disableDeactivation = false;
        // Newly added bodies start deactivated so authoring does not
        // immediately throw a selected asset into simulation.
        bool startDeactivated = true;

        // Primitive shape parameters. Convex Hull, Triangle Mesh and Height
        // Field derive their geometry from the normal scene/mesh/terrain path.
        XMFLOAT3 boxHalfExtents = XMFLOAT3(1.0f, 1.0f, 1.0f);
        float sphereRadius = 1.0f;
        // Capsule and Cylinder dimensions share the same underlying params.
        float capsuleRadius = 1.0f;
        float capsuleHeight = 1.0f;
    };

    enum class CollisionTargetStatus
    {
        Supported,
        InvalidEntity,
        MissingTransform,
        MissingMesh,
        InvalidMeshData,
        InvalidHeightFieldGrid,
    };

    // Reusable assets keep creator-owned transform/state on their stable
    // instance root while imported payload nodes below it remain replaceable.
    // Whole-asset rigid-body authoring therefore resolves to that root.
    [[nodiscard]] wi::ecs::Entity ResolveCollisionAuthoringTarget(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selectedEntity) noexcept;

    // Fits BOX/SPHERE/CAPSULE/CYLINDER authoring state to descendant render
    // geometry in the resolved target's local space. The target's own scale is
    // deliberately excluded: the physics backend applies transform scale when
    // it creates the real shape, so baking it into dimensions here would scale
    // the collider twice. Returns false when no usable render geometry exists
    // or the selected shape is mesh-derived.
    [[nodiscard]] bool FitPrimitiveCollisionStateToTarget(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        CollisionState& state) noexcept;

    // Requests the normal backend-owned body recreation path. This is used by
    // creator tooling after an authored transform scale changes so the live
    // primitive shape is rebuilt from the current transform without creating
    // a second physics representation.
    [[nodiscard]] bool RequestCollisionShapeRefresh(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    struct ReusableAssetCollisionRepairResult
    {
        std::size_t migratedBodyCount = 0;
        std::size_t conflictCount = 0;
    };

    // Repairs the unambiguous legacy/owner-failure case where exactly one
    // rigid body was serialized on a descendant of a reusable asset root.
    // Ambiguous multi-body/root-conflict cases are left untouched.
    [[nodiscard]] ReusableAssetCollisionRepairResult
    RepairReusableAssetCollisionTargets(wi::scene::Scene& scene) noexcept;

    [[nodiscard]] CollisionTargetStatus CheckCollisionTarget(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept;
    [[nodiscard]] bool CanAuthorCollisionShape(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept;

    [[nodiscard]] CollisionState CaptureCollision(
        const wi::scene::RigidBodyPhysicsComponent& rigidbody) noexcept;
    [[nodiscard]] CollisionState SanitizeCollisionState(
        const CollisionState& state) noexcept;
    [[nodiscard]] bool HasCollisionStateChange(
        const CollisionState& before,
        const CollisionState& after) noexcept;

    // Applies only the standard rigid-body authoring surface represented by
    // CollisionState. Character/vehicle state and every unrelated native field
    // remain untouched.
    void ApplyCollision(
        wi::scene::RigidBodyPhysicsComponent& rigidbody,
        const CollisionState& state) noexcept;

    // Adds a rigid body to the creator-safe target. Primitive shapes auto-fit
    // by default when usable geometry exists. Advanced/native callers can opt
    // out and supply exact dimensions.
    class CreateCollisionCommand final : public ICommand
    {
    public:
        CreateCollisionCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity targetEntity,
            const CollisionState& initial,
            bool autoFitPrimitive = true);

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CollisionState initial_;
        bool autoFitPrimitive_ = true;
    };

    class SetCollisionCommand final : public ICommand
    {
    public:
        SetCollisionCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const CollisionState& collision);
        SetCollisionCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity,
            const CollisionState& before,
            const CollisionState& after);

        bool Execute() override;
        void Undo() override;

    private:
        bool Apply(const CollisionState& state) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CollisionState before_;
        CollisionState after_;
    };

    class RemoveCollisionCommand final : public ICommand
    {
    public:
        RemoveCollisionCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity entity);

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::scene::RigidBodyPhysicsComponent removedNative_;
        bool hasRemoved_ = false;
    };
}
