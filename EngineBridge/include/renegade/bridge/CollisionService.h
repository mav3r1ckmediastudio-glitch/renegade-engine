#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // Creator-facing rigid-body authoring state for JP01. This mirrors the
    // standard rigid-body controls exposed by Wicked Editor while leaving
    // character and vehicle configuration to their dedicated JP01 domains.
    // Wicked's RigidBodyPhysicsComponent remains the serialized source of
    // truth; this type is only the Renegade command/UI façade over it.
    struct CollisionState
    {
        wi::scene::RigidBodyPhysicsComponent::CollisionShape shape =
            wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;

        // Standard body properties exposed by Wicked Editor.
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
        bool startDeactivated = false;

        // Primitive shape parameters. Convex Hull, Triangle Mesh and Height
        // Field derive their geometry from Wicked's normal scene/mesh/terrain
        // plumbing rather than inventing a Renegade-side geometry format.
        XMFLOAT3 boxHalfExtents = XMFLOAT3(1.0f, 1.0f, 1.0f);
        float sphereRadius = 1.0f;
        // Wicked stores Capsule and Cylinder dimensions in the same
        // underlying CapsuleParams structure.
        float capsuleRadius = 1.0f;
        float capsuleHeight = 1.0f;
    };

    // Complex Wicked collision shapes are mesh-derived. Keep that requirement
    // explicit in Renegade so the Physics Lab can disable invalid choices with
    // a useful reason instead of allowing a component that Jolt can only reject
    // at simulation time.
    enum class CollisionTargetStatus
    {
        Supported,
        InvalidEntity,
        MissingTransform,
        MissingMesh,
        InvalidMeshData,
        InvalidHeightFieldGrid,
    };

    [[nodiscard]] CollisionTargetStatus CheckCollisionTarget(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept;
    [[nodiscard]] bool CanAuthorCollisionShape(
        const wi::scene::Scene& scene,
        wi::ecs::Entity entity,
        wi::scene::RigidBodyPhysicsComponent::CollisionShape shape) noexcept;

    // JP01 reusable-asset hardening. Physics authoring belongs to the stable
    // reusable-asset instance wrapper, never to a replaceable payload child.
    // Ordinary non-reusable entities are returned unchanged.
    [[nodiscard]] wi::ecs::Entity ResolveCollisionAuthoringEntity(
        const wi::scene::Scene& scene,
        wi::ecs::Entity selectedEntity) noexcept;

    // Fits a primitive collider around every mesh object below rootEntity in
    // root-local space. The root's authored transform (including scale) is
    // cancelled out of the stored dimensions; descendant transforms remain
    // part of the fit. Returns false if no valid descendant geometry exists.
    [[nodiscard]] bool FitPrimitiveCollisionToHierarchy(
        const wi::scene::Scene& scene,
        wi::ecs::Entity rootEntity,
        CollisionState& state) noexcept;

    // Wicked/Jolt owns the actual physics shape. When the reusable wrapper's
    // authored scale changes, force the existing native body to be recreated
    // so Wicked reapplies that transform scale without baking it into the
    // creator-authored primitive dimensions.
    bool RefreshCollisionShapeForScaleChange(
        wi::scene::Scene& scene,
        wi::ecs::Entity entity) noexcept;

    [[nodiscard]] CollisionState CaptureCollision(
        const wi::scene::RigidBodyPhysicsComponent& rigidbody) noexcept;
    [[nodiscard]] CollisionState SanitizeCollisionState(
        const CollisionState& state) noexcept;
    [[nodiscard]] bool HasCollisionStateChange(
        const CollisionState& before,
        const CollisionState& after) noexcept;

    // Applies only the standard rigid-body authoring surface represented by
    // CollisionState. Character/vehicle state and every other unrelated
    // native field remain untouched. Shape topology changes recreate Wicked's
    // implementation-owned physics object; parameter-only changes request the
    // normal Wicked refresh path. Target validity is checked by the commands
    // because this low-level helper intentionally has no Scene/Entity context.
    void ApplyCollision(
        wi::scene::RigidBodyPhysicsComponent& rigidbody,
        const CollisionState& state) noexcept;

    // Attaches a new RigidBodyPhysicsComponent to the collision-authoring
    // target. Reusable payload selections resolve to their stable wrapper root;
    // ordinary entities are unchanged. This never creates a new entity, so
    // Undo/Redo is a component add/remove rather than an entity snapshot.
    class CreateCollisionCommand final : public ICommand
    {
    public:
        CreateCollisionCommand(
            wi::scene::Scene& scene,
            wi::ecs::Entity targetEntity,
            const CollisionState& initial);

        bool Execute() override;
        void Undo() override;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        CollisionState initial_;
    };

    // Edits an existing RigidBodyPhysicsComponent through the shared Renegade
    // command history.
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

    // Removes an existing RigidBodyPhysicsComponent (the "No Collision"
    // choice). Undo restores the complete native component snapshot rather
    // than only CollisionState, so later character/vehicle settings that share
    // Wicked's component cannot be silently lost. The live physicsobject is
    // deliberately excluded from that snapshot and is recreated by Wicked.
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
