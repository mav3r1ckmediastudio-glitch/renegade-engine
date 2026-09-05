#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "renegade/bridge/VehiclePhysicsService.h"

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
    using renegade::bridge::VehiclePhysicsState;
    using Body = wi::scene::RigidBodyPhysicsComponent;
    using Type = Body::Vehicle::Type;
    using Mode = Body::Vehicle::CollisionMode;

    {
        VehiclePhysicsState dirty;
        dirty.type = static_cast<Type>(999);
        dirty.collisionMode = static_cast<Mode>(999);
        dirty.wheelRadius = -10.0f;
        dirty.wheelWidth = 100.0f;
        dirty.chassisHalfWidth = -5.0f;
        dirty.chassisHalfHeight = 50.0f;
        dirty.maxEngineTorque = -1.0f;
        dirty.clutchStrength = 500.0f;
        dirty.maxRollAngle = 100.0f;
        dirty.maxSteeringAngle = 100.0f;
        dirty.frontSuspensionDamping = 10.0f;
        dirty.motorcycleFrontBrakeTorque = -5.0f;
        dirty.motorcycleRearBrakeTorque = 5000.0f;
        const auto safe = renegade::bridge::SanitizeVehiclePhysicsState(dirty);
        if (safe.type != Type::None || safe.collisionMode != Mode::Ray ||
            !NearlyEqual(safe.wheelRadius, 0.001f) ||
            !NearlyEqual(safe.wheelWidth, 10.0f) ||
            safe.chassisHalfWidth != 0.0f ||
            !NearlyEqual(safe.chassisHalfHeight, 10.0f) ||
            safe.maxEngineTorque != 0.0f ||
            !NearlyEqual(safe.clutchStrength, 100.0f) ||
            !NearlyEqual(safe.maxRollAngle, XM_PI) ||
            !NearlyEqual(safe.maxSteeringAngle, XM_PIDIV2) ||
            !NearlyEqual(safe.frontSuspensionDamping, 2.0f) ||
            safe.motorcycleFrontBrakeTorque != 0.0f ||
            !NearlyEqual(safe.motorcycleRearBrakeTorque, 2000.0f))
        {
            return Fail("vehicle sanitization diverged from Wicked Editor ranges");
        }
    }

    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    scene.transforms.Create(entity);
    auto& body = scene.rigidbodies.Create(entity);
    body.shape = Body::CollisionShape::BOX;
    body.mass = 1200.0f;
    body.box.halfextents = XMFLOAT3(1.0f, 0.5f, 2.0f);
    body.SetCharacterPhysics(true);
    body.character.gravityFactor = 0.75f;

    const auto wheelFL = wi::ecs::CreateEntity();
    const auto wheelFR = wi::ecs::CreateEntity();
    const auto wheelRL = wi::ecs::CreateEntity();
    const auto wheelRR = wi::ecs::CreateEntity();
    scene.transforms.Create(wheelFL);
    scene.transforms.Create(wheelFR);
    scene.transforms.Create(wheelRL);
    scene.transforms.Create(wheelRR);

    VehiclePhysicsState car;
    car.type = Type::Car;
    car.collisionMode = Mode::Cylinder;
    car.chassisHalfWidth = 1.1f;
    car.chassisHalfHeight = 0.35f;
    car.chassisHalfLength = 2.4f;
    car.frontWheelOffset = 0.2f;
    car.rearWheelOffset = -0.15f;
    car.wheelRadius = 0.42f;
    car.wheelWidth = 0.22f;
    car.maxEngineTorque = 850.0f;
    car.clutchStrength = 22.0f;
    car.maxRollAngle = wi::math::DegreesToRadians(45.0f);
    car.maxSteeringAngle = wi::math::DegreesToRadians(38.0f);
    car.frontSuspensionMinLength = 0.2f;
    car.frontSuspensionMaxLength = 0.6f;
    car.frontSuspensionFrequency = 1.8f;
    car.frontSuspensionDamping = 0.7f;
    car.rearSuspensionMinLength = 0.25f;
    car.rearSuspensionMaxLength = 0.65f;
    car.rearSuspensionFrequency = 1.6f;
    car.rearSuspensionDamping = 0.8f;
    car.fourWheelDrive = true;
    car.motorcycleFrontSuspensionAngle = wi::math::DegreesToRadians(25.0f);
    car.motorcycleFrontBrakeTorque = 900.0f;
    car.motorcycleRearBrakeTorque = 450.0f;
    car.motorcycleLeanControl = false;
    car.wheelEntityFrontLeft = wheelFL;
    car.wheelEntityFrontRight = wheelFR;
    car.wheelEntityRearLeft = wheelRL;
    car.wheelEntityRearRight = wheelRR;

    renegade::bridge::CommandService commands;
    if (!commands.Execute(
            std::make_unique<renegade::bridge::SetVehiclePhysicsCommand>(
                scene,
                entity,
                car)))
    {
        return Fail("SetVehiclePhysicsCommand did not execute");
    }

    const auto& vehicle = body.vehicle;
    if (vehicle.type != Type::Car || vehicle.collision_mode != Mode::Cylinder ||
        !NearlyEqual(vehicle.chassis_half_width, 1.1f) ||
        !NearlyEqual(vehicle.chassis_half_height, 0.35f) ||
        !NearlyEqual(vehicle.chassis_half_length, 2.4f) ||
        !NearlyEqual(vehicle.wheel_radius, 0.42f) ||
        !NearlyEqual(vehicle.wheel_width, 0.22f) ||
        !NearlyEqual(vehicle.max_engine_torque, 850.0f) ||
        !NearlyEqual(vehicle.clutch_strength, 22.0f) ||
        !NearlyEqual(
            vehicle.max_steering_angle,
            wi::math::DegreesToRadians(38.0f)) ||
        !NearlyEqual(vehicle.front_suspension.max_length, 0.6f) ||
        !NearlyEqual(vehicle.rear_suspension.damping, 0.8f) ||
        !vehicle.car.four_wheel_drive ||
        !NearlyEqual(vehicle.motorcycle.front_brake_torque, 900.0f) ||
        vehicle.motorcycle.lean_control ||
        vehicle.wheel_entity_front_left != wheelFL ||
        vehicle.wheel_entity_rear_right != wheelRR ||
        !body.IsCharacterPhysics() ||
        !NearlyEqual(body.character.gravityFactor, 0.75f) ||
        body.shape != Body::CollisionShape::BOX ||
        !NearlyEqual(body.mass, 1200.0f) ||
        !body.IsRefreshParametersNeeded())
    {
        return Fail("vehicle authoring missed fields or overwrote shared body state");
    }

    // Wicked permits both flags at component level, but its Jolt creation path
    // creates the Character and returns before building the vehicle constraint.
    // Renegade reports that conflict instead of claiming runtime driving works.
    if (!renegade::bridge::HasCharacterVehiclePhysicsConflict(scene, entity) ||
        renegade::bridge::HasLiveVehiclePhysics(scene, entity))
    {
        return Fail("character/vehicle mode conflict was not surfaced");
    }

    if (!commands.Undo() || body.vehicle.type != Type::None ||
        !body.IsCharacterPhysics() ||
        !commands.Redo() || body.vehicle.type != Type::Car ||
        !body.vehicle.car.four_wheel_drive)
    {
        return Fail("vehicle authoring Undo/Redo lifecycle failed");
    }

    const auto current = renegade::bridge::CaptureVehiclePhysics(body);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetVehiclePhysicsCommand>(
                scene,
                entity,
                current,
                current)) ||
        noOpCommands.UndoCount() != 0)
    {
        return Fail("identical vehicle state polluted command history");
    }

    // Lean control and visual wheel mappings are read live by Wicked and do
    // not require native vehicle recreation.
    body.SetRefreshParametersNeeded(false);
    auto liveOnly = current;
    liveOnly.motorcycleLeanControl = !current.motorcycleLeanControl;
    liveOnly.wheelEntityFrontLeft = wi::ecs::INVALID_ENTITY;
    renegade::bridge::CommandService liveOnlyCommands;
    if (!liveOnlyCommands.Execute(
            std::make_unique<renegade::bridge::SetVehiclePhysicsCommand>(
                scene,
                entity,
                current,
                liveOnly)) ||
        body.IsRefreshParametersNeeded() ||
        body.vehicle.motorcycle.lean_control != liveOnly.motorcycleLeanControl ||
        body.vehicle.wheel_entity_front_left != wi::ecs::INVALID_ENTITY)
    {
        return Fail("live vehicle mappings unnecessarily requested a rebuild");
    }

    // Remove the character conflict but keep the body component-only. Runtime
    // calls must still wait for Wicked to create its native Jolt vehicle.
    body.SetCharacterPhysics(false);
    float velocity = 123.0f;
    if (renegade::bridge::HasLiveVehiclePhysics(scene, entity) ||
        renegade::bridge::DrivePhysicsVehicle(
            scene,
            entity,
            1.0f,
            0.25f,
            0.0f,
            0.0f) ||
        renegade::bridge::GetVehicleForwardVelocity(scene, entity, velocity) ||
        renegade::bridge::UpdateVehicleWheelTransforms(scene))
    {
        return Fail("vehicle runtime API treated an uncreated vehicle as live");
    }
    if (!NearlyEqual(velocity, 123.0f))
    {
        return Fail("failed vehicle velocity readback modified output state");
    }

    const auto noBodyEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(noBodyEntity);
    renegade::bridge::CommandService missingBodyCommands;
    if (missingBodyCommands.Execute(
            std::make_unique<renegade::bridge::SetVehiclePhysicsCommand>(
                scene,
                noBodyEntity,
                car)) ||
        missingBodyCommands.UndoCount() != 0)
    {
        return Fail("vehicle authoring silently created a rigid body");
    }

    std::cout << "PASS: JP01 Wicked vehicle physics parity\n";
    return 0;
}
