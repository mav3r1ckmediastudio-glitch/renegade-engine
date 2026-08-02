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

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    using renegade::bridge::CollisionState;
    using Shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape;

    // SanitizeCollisionState: never let a degenerate zero/negative
    // dimension or a negative mass/friction/restitution through.
    {
        CollisionState raw;
        raw.mass = -5.0f;
        raw.friction = -1.0f;
        raw.restitution = 4.0f;
        raw.boxHalfExtents = XMFLOAT3(0.0f, -2.0f, 0.005f);
        raw.sphereRadius = 0.0f;
        raw.capsuleRadius = -1.0f;
        raw.capsuleHeight = 0.0f;
        const auto safe = renegade::bridge::SanitizeCollisionState(raw);
        if (safe.mass != 0.0f || safe.friction != 0.0f ||
            safe.restitution != 1.0f ||
            safe.boxHalfExtents.x <= 0.0f || safe.boxHalfExtents.y <= 0.0f ||
            safe.boxHalfExtents.z <= 0.0f || safe.sphereRadius <= 0.0f ||
            safe.capsuleRadius <= 0.0f || safe.capsuleHeight <= 0.0f)
        {
            return Fail("SanitizeCollisionState let a degenerate value through");
        }
    }

    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    scene.transforms.Create(entity);

    // CreateCollisionCommand: attaches a new rigidbody, Undo removes just
    // that component (the entity and its TransformComponent survive), Redo
    // (Execute again) recreates it.
    renegade::bridge::CommandService commands;
    CollisionState initial;
    initial.shape = Shape::SPHERE;
    initial.mass = 2.5f;
    initial.friction = 0.4f;
    initial.restitution = 0.6f;
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
        !NearlyEqual(rigidbody->sphere.radius, 0.75f))
    {
        return Fail("CreateCollisionCommand did not attach the initial state");
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

    // A second CreateCollisionCommand against an entity that already has one
    // must fail cleanly rather than overwrite it.
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

    // SetCollisionCommand: curated edit, Undo/Redo, no-op filtering -- same
    // contract as SetMaterialCommand/SetLightCommand.
    const auto before = renegade::bridge::CaptureCollision(*rigidbody);
    auto after = before;
    after.shape = Shape::BOX;
    after.mass = 0.0f; // static
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
        !NearlyEqual(rigidbody->box.halfextents.y, 1.5f))
    {
        return Fail("SetCollisionCommand did not reach the native component");
    }
    if (!editCommands.Undo() || rigidbody->shape != Shape::SPHERE ||
        !NearlyEqual(rigidbody->mass, 2.5f))
    {
        return Fail("SetCollisionCommand Undo did not restore the prior shape");
    }
    if (!editCommands.Redo() || rigidbody->shape != Shape::BOX ||
        rigidbody->mass != 0.0f)
    {
        return Fail("SetCollisionCommand Redo did not restore the edited shape");
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

    // RemoveCollisionCommand ("No Collision"): captures the removed state so
    // Undo can restore it exactly, mirroring CreateCollisionCommand inverted.
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
    const auto* restored = scene.rigidbodies.GetComponent(entity);
    if (restored == nullptr || restored->shape != Shape::BOX ||
        restored->mass != 0.0f ||
        !NearlyEqual(restored->box.halfextents.y, 1.5f))
    {
        return Fail("RemoveCollisionCommand Undo did not restore the exact prior state");
    }

    // Removing (or targeting) an entity with no rigidbody fails cleanly.
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

    std::cout << "PASS: collision create/edit/remove and Undo/Redo\n";
    return 0;
}
