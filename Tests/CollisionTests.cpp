#include <array>
#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/CollisionService.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    bool NearlyEqual(const XMFLOAT3& left, const XMFLOAT3& right)
    {
        return NearlyEqual(left.x, right.x) &&
            NearlyEqual(left.y, right.y) &&
            NearlyEqual(left.z, right.z);
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    using renegade::bridge::CollisionState;
    using Body = wi::scene::RigidBodyPhysicsComponent;
    using Shape = Body::CollisionShape;

    // JP01 rigid-body authoring parity must recognize every collision shape
    // exposed by Wicked Editor.
    {
        constexpr std::array<Shape, 7> shapes = {
            Shape::BOX,
            Shape::SPHERE,
            Shape::CAPSULE,
            Shape::CYLINDER,
            Shape::CONVEX_HULL,
            Shape::TRIANGLE_MESH,
            Shape::HEIGHTFIELD,
        };
        for (const auto shape : shapes)
        {
            CollisionState state;
            state.shape = shape;
            if (renegade::bridge::SanitizeCollisionState(state).shape != shape)
            {
                return Fail("a Wicked rigid-body collision shape was rejected");
            }
        }

        CollisionState invalid;
        invalid.shape = static_cast<Shape>(999u);
        if (renegade::bridge::SanitizeCollisionState(invalid).shape != Shape::BOX)
        {
            return Fail("invalid collision shape was not normalized safely");
        }
    }

    // SanitizeCollisionState: retain Wicked Editor's practical ranges while
    // preventing degenerate dimensions and invalid negative mass values.
    {
        CollisionState raw;
        raw.mass = -5.0f;
        raw.friction = -1.0f;
        raw.restitution = 4.0f;
        raw.dampingLinear = -2.0f;
        raw.dampingAngular = 8.0f;
        raw.buoyancy = 9.0f;
        raw.meshLod = 999;
        raw.boxHalfExtents = XMFLOAT3(0.0f, -2.0f, 0.005f);
        raw.sphereRadius = 0.0f;
        raw.capsuleRadius = -1.0f;
        raw.capsuleHeight = 0.0f;
        const auto safe = renegade::bridge::SanitizeCollisionState(raw);
        if (safe.mass != 0.0f || safe.friction != 0.0f ||
            safe.restitution != 1.0f || safe.dampingLinear != 0.0f ||
            safe.dampingAngular != 1.0f || safe.buoyancy != 2.0f ||
            safe.meshLod != 6 || safe.boxHalfExtents.x <= 0.0f ||
            safe.boxHalfExtents.y <= 0.0f || safe.boxHalfExtents.z <= 0.0f ||
            safe.sphereRadius <= 0.0f || safe.capsuleRadius <= 0.0f ||
            safe.capsuleHeight <= 0.0f)
        {
            return Fail("SanitizeCollisionState let an invalid authoring value through");
        }
    }

    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    scene.transforms.Create(entity);

    // CreateCollisionCommand: attach the complete standard Wicked rigid-body
    // authoring state and participate in the shared Undo/Redo history.
    renegade::bridge::CommandService commands;
    CollisionState initial;
    initial.shape = Shape::SPHERE;
    initial.mass = 2.5f;
    initial.friction = 0.4f;
    initial.restitution = 0.6f;
    initial.dampingLinear = 0.22f;
    initial.dampingAngular = 0.33f;
    initial.buoyancy = 1.7f;
    initial.localOffset = XMFLOAT3(1.0f, 2.0f, 3.0f);
    initial.meshLod = 2;
    initial.kinematic = true;
    initial.locked2D = true;
    initial.disableDeactivation = true;
    initial.startDeactivated = true;
    initial.sphereRadius = 0.75f;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::CreateCollisionCommand>(
                scene,
                entity,
                initial)))
    {
        return Fail("CreateCollisionCommand did not execute");
    }

    auto* rigidbody = scene.rigidbodies.GetComponent(entity);
    if (rigidbody == nullptr || rigidbody->shape != Shape::SPHERE ||
        !NearlyEqual(rigidbody->mass, 2.5f) ||
        !NearlyEqual(rigidbody->sphere.radius, 0.75f) ||
        !NearlyEqual(rigidbody->damping_linear, 0.22f) ||
        !NearlyEqual(rigidbody->damping_angular, 0.33f) ||
        !NearlyEqual(rigidbody->buoyancy, 1.7f) ||
        !NearlyEqual(rigidbody->local_offset, XMFLOAT3(1.0f, 2.0f, 3.0f)) ||
        rigidbody->mesh_lod != 2 || !rigidbody->IsKinematic() ||
        !rigidbody->IsLocked2D() || !rigidbody->IsDisableDeactivation() ||
        !rigidbody->IsStartDeactivated() ||
        !rigidbody->IsRefreshParametersNeeded())
    {
        return Fail("CreateCollisionCommand did not attach the full authoring state");
    }
    if (!scene.transforms.Contains(entity))
    {
        return Fail("CreateCollisionCommand touched an unrelated component");
    }

    if (!commands.Undo() || scene.rigidbodies.Contains(entity))
    {
        return Fail("CreateCollisionCommand Undo did not remove the rigidbody");
    }
    if (!scene.transforms.Contains(entity))
    {
        return Fail("CreateCollisionCommand Undo removed the whole entity");
    }
    if (!commands.Redo() || !scene.rigidbodies.Contains(entity))
    {
        return Fail("CreateCollisionCommand Redo did not recreate the rigidbody");
    }
    rigidbody = scene.rigidbodies.GetComponent(entity);
    if (rigidbody == nullptr)
    {
        return Fail("CreateCollisionCommand Redo did not restore the component pointer");
    }

    // A second create against an entity that already has a rigid body must
    // fail cleanly rather than overwrite native state.
    renegade::bridge::CommandService duplicateCommands;
    if (duplicateCommands.Execute(
            std::make_unique<renegade::bridge::CreateCollisionCommand>(
                scene,
                entity,
                initial)) ||
        duplicateCommands.UndoCount() != 0)
    {
        return Fail("CreateCollisionCommand overwrote an existing rigidbody");
    }

    // Seed fields intentionally outside CollisionState. The regular rigid-body
    // editing path must preserve them because character and vehicle authoring
    // are separate JP01 domains sharing Wicked's native component.
    rigidbody->SetCharacterPhysics(true);
    rigidbody->character.gravityFactor = 2.25f;
    rigidbody->vehicle.type = Body::Vehicle::Type::Car;
    rigidbody->vehicle.wheel_radius = 0.55f;
    rigidbody->vehicle.max_engine_torque = 725.0f;

    const auto before = renegade::bridge::CaptureCollision(*rigidbody);
    auto after = before;
    after.shape = Shape::BOX;
    after.mass = 0.0f; // static
    after.friction = 0.73f;
    after.restitution = 0.12f;
    after.dampingLinear = 0.44f;
    after.dampingAngular = 0.51f;
    after.buoyancy = 0.65f;
    after.localOffset = XMFLOAT3(-1.0f, 0.5f, 2.0f);
    after.meshLod = 4;
    after.kinematic = false;
    after.locked2D = false;
    after.disableDeactivation = false;
    after.startDeactivated = false;
    after.boxHalfExtents = XMFLOAT3(0.5f, 1.5f, 0.25f);

    renegade::bridge::CommandService editCommands;
    if (!editCommands.Execute(
            std::make_unique<renegade::bridge::SetCollisionCommand>(
                scene,
                entity,
                before,
                after)))
    {
        return Fail("SetCollisionCommand did not execute");
    }
    if (rigidbody->shape != Shape::BOX || rigidbody->mass != 0.0f ||
        !NearlyEqual(rigidbody->box.halfextents.y, 1.5f) ||
        !NearlyEqual(rigidbody->friction, 0.73f) ||
        !NearlyEqual(rigidbody->damping_linear, 0.44f) ||
        !NearlyEqual(rigidbody->damping_angular, 0.51f) ||
        !NearlyEqual(rigidbody->buoyancy, 0.65f) ||
        !NearlyEqual(rigidbody->local_offset, XMFLOAT3(-1.0f, 0.5f, 2.0f)) ||
        rigidbody->mesh_lod != 4 || rigidbody->IsKinematic() ||
        rigidbody->IsLocked2D() || rigidbody->IsDisableDeactivation() ||
        rigidbody->IsStartDeactivated())
    {
        return Fail("SetCollisionCommand did not reach the full native authoring surface");
    }
    if (!rigidbody->IsCharacterPhysics() ||
        !NearlyEqual(rigidbody->character.gravityFactor, 2.25f) ||
        rigidbody->vehicle.type != Body::Vehicle::Type::Car ||
        !NearlyEqual(rigidbody->vehicle.wheel_radius, 0.55f) ||
        !NearlyEqual(rigidbody->vehicle.max_engine_torque, 725.0f))
    {
        return Fail("SetCollisionCommand overwrote character or vehicle state");
    }

    if (!editCommands.Undo() || rigidbody->shape != Shape::SPHERE ||
        !NearlyEqual(rigidbody->mass, 2.5f) ||
        !NearlyEqual(rigidbody->damping_linear, 0.22f) ||
        !NearlyEqual(rigidbody->buoyancy, 1.7f) ||
        !rigidbody->IsKinematic() || !rigidbody->IsLocked2D() ||
        !rigidbody->IsDisableDeactivation() ||
        !rigidbody->IsStartDeactivated())
    {
        return Fail("SetCollisionCommand Undo did not restore the prior authoring state");
    }
    if (!rigidbody->IsCharacterPhysics() ||
        rigidbody->vehicle.type != Body::Vehicle::Type::Car)
    {
        return Fail("SetCollisionCommand Undo damaged unrelated native fields");
    }
    if (!editCommands.Redo() || rigidbody->shape != Shape::BOX ||
        rigidbody->mass != 0.0f || !NearlyEqual(rigidbody->buoyancy, 0.65f))
    {
        return Fail("SetCollisionCommand Redo did not restore the edited state");
    }

    const auto current = renegade::bridge::CaptureCollision(*rigidbody);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetCollisionCommand>(
                scene,
                entity,
                current,
                current)) ||
        noOpCommands.UndoCount() != 0)
    {
        return Fail("identical collision state produced a command-history entry");
    }

    // Remove Collision must now snapshot the complete native component. This
    // proves future vehicle/character work cannot be erased by Remove -> Undo.
    renegade::bridge::CommandService removeCommands;
    if (!removeCommands.Execute(
            std::make_unique<renegade::bridge::RemoveCollisionCommand>(
                scene,
                entity)) ||
        scene.rigidbodies.Contains(entity))
    {
        return Fail("RemoveCollisionCommand did not remove the rigidbody");
    }
    if (!removeCommands.Undo() || !scene.rigidbodies.Contains(entity))
    {
        return Fail("RemoveCollisionCommand Undo did not restore the rigidbody");
    }
    rigidbody = scene.rigidbodies.GetComponent(entity);
    if (rigidbody == nullptr || rigidbody->shape != Shape::BOX ||
        rigidbody->mass != 0.0f ||
        !NearlyEqual(rigidbody->box.halfextents.y, 1.5f) ||
        !NearlyEqual(rigidbody->damping_linear, 0.44f) ||
        !NearlyEqual(rigidbody->buoyancy, 0.65f) ||
        !rigidbody->IsCharacterPhysics() ||
        !NearlyEqual(rigidbody->character.gravityFactor, 2.25f) ||
        rigidbody->vehicle.type != Body::Vehicle::Type::Car ||
        !NearlyEqual(rigidbody->vehicle.wheel_radius, 0.55f) ||
        !NearlyEqual(rigidbody->vehicle.max_engine_torque, 725.0f) ||
        !rigidbody->IsRefreshParametersNeeded())
    {
        return Fail("RemoveCollisionCommand Undo did not restore complete native state");
    }

    // Removing an entity with no rigidbody still fails cleanly.
    const auto bareEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(bareEntity);
    renegade::bridge::CommandService bareRemoveCommands;
    if (bareRemoveCommands.Execute(
            std::make_unique<renegade::bridge::RemoveCollisionCommand>(
                scene,
                bareEntity)) ||
        bareRemoveCommands.UndoCount() != 0)
    {
        return Fail("RemoveCollisionCommand accepted an entity with no rigidbody");
    }

    // Cylinder continues to share Wicked's CapsuleParams storage.
    const auto cylinderEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(cylinderEntity);
    CollisionState cylinder;
    cylinder.shape = Shape::CYLINDER;
    cylinder.capsuleRadius = 0.4f;
    cylinder.capsuleHeight = 2.2f;
    renegade::bridge::CommandService cylinderCommands;
    if (!cylinderCommands.Execute(
            std::make_unique<renegade::bridge::CreateCollisionCommand>(
                scene,
                cylinderEntity,
                cylinder)))
    {
        return Fail("CreateCollisionCommand did not execute for Cylinder");
    }
    const auto* cylinderBody = scene.rigidbodies.GetComponent(cylinderEntity);
    if (cylinderBody == nullptr || cylinderBody->shape != Shape::CYLINDER ||
        !NearlyEqual(cylinderBody->capsule.radius, 0.4f) ||
        !NearlyEqual(cylinderBody->capsule.height, 2.2f))
    {
        return Fail("Cylinder dimensions did not reach native capsule storage");
    }

    // Complex Wicked shapes are no longer artificially filtered out by the
    // Renegade authoring contract. Runtime geometry still comes from Wicked's
    // normal mesh/terrain plumbing; this test only proves creator state parity.
    constexpr std::array<Shape, 3> complexShapes = {
        Shape::CONVEX_HULL,
        Shape::TRIANGLE_MESH,
        Shape::HEIGHTFIELD,
    };
    for (const auto shape : complexShapes)
    {
        const auto complexEntity = wi::ecs::CreateEntity();
        scene.transforms.Create(complexEntity);
        CollisionState complex;
        complex.shape = shape;
        complex.mass = 0.0f;
        complex.meshLod = 3;
        renegade::bridge::CommandService complexCommands;
        if (!complexCommands.Execute(
                std::make_unique<renegade::bridge::CreateCollisionCommand>(
                    scene,
                    complexEntity,
                    complex)))
        {
            return Fail("complex Wicked collision shape could not be authored");
        }
        const auto* complexBody = scene.rigidbodies.GetComponent(complexEntity);
        if (complexBody == nullptr || complexBody->shape != shape ||
            complexBody->mesh_lod != 3)
        {
            return Fail("complex Wicked collision shape did not reach native state");
        }
    }

    std::cout << "PASS: JP01 rigid-body authoring parity and native-state Undo\n";
    return 0;
}
