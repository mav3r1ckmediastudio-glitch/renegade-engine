#include <cmath>
#include <iostream>
#include <memory>

#include "renegade/bridge/SoftBodyPhysicsService.h"

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
    using renegade::bridge::SoftBodyPhysicsState;

    {
        SoftBodyPhysicsState dirty;
        dirty.detail = -1.0f;
        dirty.mass = 500.0f;
        dirty.friction = -1.0f;
        dirty.restitution = 5.0f;
        dirty.pressure = -10.0f;
        dirty.vertexRadius = 5.0f;
        const auto safe = renegade::bridge::SanitizeSoftBodyPhysicsState(dirty);
        if (!NearlyEqual(safe.detail, 0.001f) ||
            !NearlyEqual(safe.mass, 100.0f) ||
            safe.friction != 0.0f || safe.restitution != 1.0f ||
            safe.pressure != 0.0f || !NearlyEqual(safe.vertexRadius, 1.0f))
        {
            return Fail("soft-body sanitization diverged from Wicked Editor ranges");
        }
    }

    wi::scene::Scene scene;
    const auto meshEntity = wi::ecs::CreateEntity();
    auto& mesh = scene.meshes.Create(meshEntity);
    mesh.vertex_positions = {
        XMFLOAT3(0, 0, 0),
        XMFLOAT3(1, 0, 0),
        XMFLOAT3(0, 1, 0),
    };
    mesh.indices = { 0, 1, 2 };

    const auto objectEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(objectEntity);
    scene.objects.Create(objectEntity).meshID = meshEntity;

    if (renegade::bridge::ResolveSoftBodyMeshEntity(scene, meshEntity) != meshEntity ||
        renegade::bridge::ResolveSoftBodyMeshEntity(scene, objectEntity) != meshEntity)
    {
        return Fail("soft-body target did not mirror Wicked Object-to-Mesh mapping");
    }

    const auto invalidEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(invalidEntity);
    if (renegade::bridge::ResolveSoftBodyMeshEntity(scene, invalidEntity) !=
        wi::ecs::INVALID_ENTITY)
    {
        return Fail("soft-body target accepted an entity with no mesh");
    }

    SoftBodyPhysicsState initial;
    initial.detail = 0.8f;
    initial.mass = 4.0f;
    initial.friction = 0.25f;
    initial.restitution = 0.35f;
    initial.pressure = 120.0f;
    initial.vertexRadius = 0.12f;
    initial.windEnabled = true;

    renegade::bridge::CommandService createCommands;
    if (!createCommands.Execute(
            std::make_unique<renegade::bridge::CreateSoftBodyPhysicsCommand>(
                scene,
                objectEntity,
                initial)))
    {
        return Fail("CreateSoftBodyPhysicsCommand did not execute through object selection");
    }

    auto* body = scene.softbodies.GetComponent(meshEntity);
    if (body == nullptr ||
        !NearlyEqual(body->detail, 0.8f) ||
        !NearlyEqual(body->mass, 4.0f) ||
        !NearlyEqual(body->friction, 0.25f) ||
        !NearlyEqual(body->restitution, 0.35f) ||
        !NearlyEqual(body->pressure, 120.0f) ||
        !NearlyEqual(body->vertex_radius, 0.12f) ||
        !body->IsWindEnabled())
    {
        return Fail("soft-body creator fields did not reach Wicked component");
    }

    if (!createCommands.Undo() || scene.softbodies.Contains(meshEntity) ||
        !createCommands.Redo() || !scene.softbodies.Contains(meshEntity))
    {
        return Fail("CreateSoftBodyPhysicsCommand Undo/Redo lifecycle failed");
    }
    body = scene.softbodies.GetComponent(meshEntity);
    if (body == nullptr)
    {
        return Fail("soft-body Redo lost the Mesh component target");
    }

    const auto before = renegade::bridge::CaptureSoftBodyPhysics(*body);
    auto after = before;
    after.detail = 0.5f;
    after.mass = 8.0f;
    after.friction = 0.65f;
    after.restitution = 0.15f;
    after.pressure = 850.0f;
    after.vertexRadius = 0.3f;
    after.windEnabled = false;

    renegade::bridge::CommandService editCommands;
    if (!editCommands.Execute(
            std::make_unique<renegade::bridge::SetSoftBodyPhysicsCommand>(
                scene,
                objectEntity,
                before,
                after)))
    {
        return Fail("SetSoftBodyPhysicsCommand did not execute");
    }
    if (!NearlyEqual(body->detail, 0.5f) ||
        !NearlyEqual(body->mass, 8.0f) ||
        !NearlyEqual(body->friction, 0.65f) ||
        !NearlyEqual(body->restitution, 0.15f) ||
        !NearlyEqual(body->pressure, 850.0f) ||
        !NearlyEqual(body->vertex_radius, 0.3f) ||
        body->IsWindEnabled() || body->physicsobject != nullptr)
    {
        return Fail("soft-body editor fields did not update with Wicked rebuild semantics");
    }

    if (!editCommands.Undo() || !NearlyEqual(body->detail, 0.8f) ||
        !NearlyEqual(body->mass, 4.0f) || !body->IsWindEnabled() ||
        !editCommands.Redo() || !NearlyEqual(body->detail, 0.5f) ||
        body->IsWindEnabled())
    {
        return Fail("soft-body authoring Undo/Redo lifecycle failed");
    }

    const auto current = renegade::bridge::CaptureSoftBodyPhysics(*body);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetSoftBodyPhysicsCommand>(
                scene,
                objectEntity,
                current,
                current)) ||
        noOpCommands.UndoCount() != 0)
    {
        return Fail("identical soft-body state polluted command history");
    }

    body->physicsIndices = { 0, 1, 2 };
    if (!renegade::bridge::ResetSoftBodyPhysics(scene, objectEntity) ||
        !body->physicsIndices.empty() || body->physicsobject != nullptr)
    {
        return Fail("soft-body Reset did not delegate to Wicked component Reset");
    }

    XMFLOAT3 position(9, 9, 9);
    if (renegade::bridge::HasLiveSoftBodyPhysics(scene, objectEntity) ||
        renegade::bridge::SetSoftBodyActive(scene, objectEntity, true) ||
        renegade::bridge::GetSoftBodyNodePosition(
            scene,
            objectEntity,
            0,
            position))
    {
        return Fail("soft-body runtime API treated an uncreated body as live");
    }
    if (!NearlyEqual(position.x, 9.0f))
    {
        return Fail("failed soft-body readback modified output state");
    }

    renegade::bridge::CommandService removeCommands;
    if (!removeCommands.Execute(
            std::make_unique<renegade::bridge::RemoveSoftBodyPhysicsCommand>(
                scene,
                objectEntity)) ||
        scene.softbodies.Contains(meshEntity) ||
        !removeCommands.Undo() || !scene.softbodies.Contains(meshEntity))
    {
        return Fail("RemoveSoftBodyPhysicsCommand lifecycle failed");
    }
    body = scene.softbodies.GetComponent(meshEntity);
    if (body == nullptr || !NearlyEqual(body->detail, 0.5f) ||
        !NearlyEqual(body->mass, 8.0f) || body->physicsobject != nullptr)
    {
        return Fail("soft-body Remove Undo did not restore native state safely");
    }

    renegade::bridge::CommandService invalidCreate;
    if (invalidCreate.Execute(
            std::make_unique<renegade::bridge::CreateSoftBodyPhysicsCommand>(
                scene,
                invalidEntity,
                initial)) ||
        invalidCreate.UndoCount() != 0)
    {
        return Fail("soft-body creator accepted an entity with no mesh");
    }

    std::cout << "PASS: JP01 Wicked soft-body physics parity\n";
    return 0;
}
