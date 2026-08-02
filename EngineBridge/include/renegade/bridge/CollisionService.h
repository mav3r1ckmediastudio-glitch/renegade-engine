#pragma once

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    // A curated view of wi::scene::RigidBodyPhysicsComponent, mirroring the
    // MaterialState/LightState pattern: fields a creator actually reaches
    // for, applied non-destructively over whatever else is on the
    // component. Deliberately scoped to the three primitive shapes that are
    // sized from explicit dimensions rather than derived from mesh
    // geometry (Box/Sphere/Capsule) -- Convex Hull and Triangle Mesh need a
    // MeshComponent-bearing entity and a mesh_lod to derive their shape
    // from, which is a different, not-yet-designed targeting problem than
    // attaching a bounding primitive to any transform. Cylinder and
    // Heightfield are native shapes too but have no curated authoring
    // surface here yet.
    struct CollisionState
    {
        wi::scene::RigidBodyPhysicsComponent::CollisionShape shape =
            wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
        // 0 makes the body static, matching Wicked's own field comment.
        float mass = 1.0f;
        float friction = 0.2f;
        float restitution = 0.1f;
        XMFLOAT3 boxHalfExtents = XMFLOAT3(1.0f, 1.0f, 1.0f);
        float sphereRadius = 1.0f;
        float capsuleRadius = 1.0f;
        float capsuleHeight = 1.0f;
    };

    [[nodiscard]] CollisionState CaptureCollision(
        const wi::scene::RigidBodyPhysicsComponent& rigidbody) noexcept;
    [[nodiscard]] CollisionState SanitizeCollisionState(
        const CollisionState& state) noexcept;
    [[nodiscard]] bool HasCollisionStateChange(
        const CollisionState& before,
        const CollisionState& after) noexcept;

    // Applies shape, mass, friction, restitution, and only the dimension
    // fields relevant to the chosen shape. Vehicle/character physics and
    // every other unexposed field on the component remain untouched. Marks
    // the component for a physics-parameter refresh so a live simulation
    // rebuilds the underlying collision shape, matching the component's own
    // documented purpose for SetRefreshParametersNeeded().
    void ApplyCollision(
        wi::scene::RigidBodyPhysicsComponent& rigidbody,
        const CollisionState& state) noexcept;

    // Attaches a new RigidBodyPhysicsComponent to targetEntity if it does
    // not already have one. Unlike CreateLightCommand, this never creates a
    // new entity -- targetEntity must already exist and persist for the
    // life of this command -- so Undo/Redo is a plain component
    // add/remove, not an entity-level snapshot.
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

    // Edits an existing RigidBodyPhysicsComponent. Mirrors SetMaterialCommand/
    // SetLightCommand exactly.
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
    // choice). Undo recreates it with the captured prior state, the inverse
    // of CreateCollisionCommand.
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
        CollisionState removed_;
        bool hasRemoved_ = false;
    };
}
