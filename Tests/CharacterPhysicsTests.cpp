#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "renegade/bridge/CharacterPhysicsService.h"

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
    using renegade::bridge::CharacterPhysicsState;
    using Body = wi::scene::RigidBodyPhysicsComponent;

    {
        CharacterPhysicsState dirty;
        dirty.maxSlopeAngle = std::numeric_limits<float>::quiet_NaN();
        dirty.gravityFactor = -10.0f;
        const auto safe = renegade::bridge::SanitizeCharacterPhysicsState(dirty);
        if (!NearlyEqual(
                safe.maxSlopeAngle,
                wi::math::DegreesToRadians(50.0f)) ||
            safe.gravityFactor != 0.0f)
        {
            return Fail("character authoring sanitization diverged from Wicked defaults");
        }

        dirty.maxSlopeAngle = 10.0f;
        dirty.gravityFactor = 100.0f;
        const auto clamped =
            renegade::bridge::SanitizeCharacterPhysicsState(dirty);
        if (!NearlyEqual(clamped.maxSlopeAngle, XM_PIDIV2) ||
            !NearlyEqual(clamped.gravityFactor, 4.0f))
        {
            return Fail("character authoring exceeded Wicked Editor ranges");
        }
    }

    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    scene.transforms.Create(entity);
    auto& body = scene.rigidbodies.Create(entity);
    body.shape = Body::CollisionShape::CAPSULE;
    body.mass = 82.0f;
    body.friction = 0.35f;
    body.capsule.radius = 0.42f;
    body.capsule.height = 0.9f;

    // Seed vehicle configuration to prove character authoring does not erase
    // settings from the shared Wicked rigid-body component.
    body.vehicle.type = Body::Vehicle::Type::Car;
    body.vehicle.max_engine_torque = 725.0f;
    body.vehicle.wheel_radius = 0.55f;

    CharacterPhysicsState enabled;
    enabled.enabled = true;
    enabled.maxSlopeAngle = wi::math::DegreesToRadians(62.0f);
    enabled.gravityFactor = 0.25f;

    renegade::bridge::CommandService commands;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetCharacterPhysicsCommand>(
                scene,
                entity,
                enabled)))
    {
        return Fail("SetCharacterPhysicsCommand did not execute");
    }

    if (!body.IsCharacterPhysics() ||
        !NearlyEqual(
            body.character.maxSlopeAngle,
            wi::math::DegreesToRadians(62.0f)) ||
        !NearlyEqual(body.character.gravityFactor, 0.25f) ||
        body.shape != Body::CollisionShape::CAPSULE ||
        !NearlyEqual(body.mass, 82.0f) ||
        !NearlyEqual(body.friction, 0.35f) ||
        body.vehicle.type != Body::Vehicle::Type::Car ||
        !NearlyEqual(body.vehicle.max_engine_torque, 725.0f) ||
        !NearlyEqual(body.vehicle.wheel_radius, 0.55f) ||
        !body.IsRefreshParametersNeeded())
    {
        return Fail("character authoring overwrote unrelated rigid-body state");
    }

    if (!commands.Undo() || body.IsCharacterPhysics() ||
        !NearlyEqual(
            body.character.maxSlopeAngle,
            wi::math::DegreesToRadians(50.0f)) ||
        !NearlyEqual(body.character.gravityFactor, 1.0f) ||
        body.vehicle.type != Body::Vehicle::Type::Car ||
        !commands.Redo() || !body.IsCharacterPhysics() ||
        !NearlyEqual(body.character.gravityFactor, 0.25f))
    {
        return Fail("character authoring Undo/Redo lifecycle failed");
    }

    const auto current = renegade::bridge::CaptureCharacterPhysics(body);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetCharacterPhysicsCommand>(
                scene,
                entity,
                current,
                current)) ||
        noOpCommands.UndoCount() != 0)
    {
        return Fail("identical character state polluted command history");
    }

    // A component is not a live Jolt character until Wicked creates its
    // implementation-owned physics object during the physics update.
    renegade::bridge::CharacterGroundInfo ground;
    ground.position = XMFLOAT3(9, 9, 9);
    if (renegade::bridge::HasLiveCharacterPhysics(scene, entity) ||
        renegade::bridge::GetCharacterGroundInfo(scene, entity, ground) ||
        renegade::bridge::ChangeCharacterCapsule(
            scene,
            entity,
            0.3f,
            0.8f) ||
        renegade::bridge::MovePhysicsCharacter(
            scene,
            entity,
            XMFLOAT3(1, 0, 0),
            5.0f,
            3.0f))
    {
        return Fail("character runtime API treated an uncreated body as live");
    }
    if (!NearlyEqual(ground.position.x, 9.0f))
    {
        return Fail("failed character readback modified output state");
    }

    // Character physics lives on an existing RigidBodyPhysicsComponent, just
    // as it does in Wicked Editor. It must not silently create one.
    const auto noBodyEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(noBodyEntity);
    renegade::bridge::CommandService missingBodyCommands;
    if (missingBodyCommands.Execute(
            std::make_unique<renegade::bridge::SetCharacterPhysicsCommand>(
                scene,
                noBodyEntity,
                enabled)) ||
        missingBodyCommands.UndoCount() != 0)
    {
        return Fail("character authoring silently created a rigid body");
    }

    std::cout << "PASS: JP01 Wicked character physics parity\n";
    return 0;
}
