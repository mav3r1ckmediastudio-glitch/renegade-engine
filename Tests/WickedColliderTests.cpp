#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "renegade/bridge/WickedColliderService.h"

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
    using State = renegade::bridge::WickedColliderState;
    using Shape = wi::scene::ColliderComponent::Shape;

    State dirty;
    dirty.shape = static_cast<Shape>(999);
    dirty.radius = std::numeric_limits<float>::infinity();
    dirty.offset = XMFLOAT3(-20.0f, 30.0f, std::numeric_limits<float>::quiet_NaN());
    dirty.tail = XMFLOAT3(50.0f, -50.0f, 4.0f);
    const auto safe = renegade::bridge::SanitizeWickedColliderState(dirty);
    if (safe.shape != Shape::Sphere || safe.radius != 0.0f ||
        !NearlyEqual(safe.offset, XMFLOAT3(-10.0f, 10.0f, 0.0f)) ||
        !NearlyEqual(safe.tail, XMFLOAT3(10.0f, -10.0f, 4.0f)))
    {
        return Fail("Wicked collider sanitization diverged from editor ranges");
    }

    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();

    State initial;
    initial.shape = Shape::Capsule;
    initial.cpuEnabled = true;
    initial.gpuEnabled = false;
    initial.radius = 2.25f;
    initial.offset = XMFLOAT3(1.0f, 2.0f, 3.0f);
    initial.tail = XMFLOAT3(-1.0f, 4.0f, 0.5f);

    renegade::bridge::CommandService createCommands;
    if (!createCommands.Execute(
            std::make_unique<renegade::bridge::CreateWickedColliderCommand>(
                scene,
                entity,
                initial)))
    {
        return Fail("CreateWickedColliderCommand did not execute");
    }

    auto* collider = scene.colliders.GetComponent(entity);
    if (collider == nullptr || collider->shape != Shape::Capsule ||
        !collider->IsCPUEnabled() || collider->IsGPUEnabled() ||
        !NearlyEqual(collider->radius, 2.25f) ||
        !NearlyEqual(collider->offset, initial.offset) ||
        !NearlyEqual(collider->tail, initial.tail))
    {
        return Fail("Wicked collider creator fields missed native component state");
    }

    if (!createCommands.Undo() || scene.colliders.Contains(entity) ||
        !createCommands.Redo() || !scene.colliders.Contains(entity))
    {
        return Fail("Wicked collider Create Undo/Redo lifecycle failed");
    }
    collider = scene.colliders.GetComponent(entity);

    const auto before = renegade::bridge::CaptureWickedCollider(*collider);
    auto after = before;
    after.shape = Shape::Plane;
    after.cpuEnabled = false;
    after.gpuEnabled = true;
    after.radius = 7.5f;
    after.offset = XMFLOAT3(-3.0f, 0.25f, 6.0f);
    after.tail = XMFLOAT3(2.0f, -1.0f, -4.0f);

    renegade::bridge::CommandService editCommands;
    if (!editCommands.Execute(
            std::make_unique<renegade::bridge::SetWickedColliderCommand>(
                scene,
                entity,
                before,
                after)))
    {
        return Fail("SetWickedColliderCommand did not execute");
    }
    if (collider->shape != Shape::Plane || collider->IsCPUEnabled() ||
        !collider->IsGPUEnabled() || !NearlyEqual(collider->radius, 7.5f) ||
        !NearlyEqual(collider->offset, after.offset) ||
        !NearlyEqual(collider->tail, after.tail))
    {
        return Fail("Wicked collider edit missed editor-exposed fields");
    }
    if (!editCommands.Undo() || collider->shape != Shape::Capsule ||
        !collider->IsCPUEnabled() || collider->IsGPUEnabled() ||
        !editCommands.Redo() || collider->shape != Shape::Plane ||
        !collider->IsGPUEnabled())
    {
        return Fail("Wicked collider Edit Undo/Redo lifecycle failed");
    }

    const auto current = renegade::bridge::CaptureWickedCollider(*collider);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetWickedColliderCommand>(
                scene,
                entity,
                current,
                current)) ||
        noOpCommands.UndoCount() != 0)
    {
        return Fail("identical Wicked collider state polluted command history");
    }

    renegade::bridge::CommandService removeCommands;
    if (!removeCommands.Execute(
            std::make_unique<renegade::bridge::RemoveWickedColliderCommand>(
                scene,
                entity)) ||
        scene.colliders.Contains(entity) ||
        !removeCommands.Undo() || !scene.colliders.Contains(entity))
    {
        return Fail("Wicked collider Remove Undo lifecycle failed");
    }
    collider = scene.colliders.GetComponent(entity);
    if (collider == nullptr || collider->shape != Shape::Plane ||
        collider->IsCPUEnabled() || !collider->IsGPUEnabled() ||
        !NearlyEqual(collider->offset, after.offset))
    {
        return Fail("Wicked collider Remove Undo did not restore native state");
    }

    const auto duplicate = wi::ecs::CreateEntity();
    scene.colliders.Create(duplicate);
    renegade::bridge::CommandService duplicateCommands;
    if (duplicateCommands.Execute(
            std::make_unique<renegade::bridge::CreateWickedColliderCommand>(
                scene,
                duplicate,
                initial)) ||
        duplicateCommands.UndoCount() != 0)
    {
        return Fail("Wicked collider creator overwrote an existing component");
    }

    std::cout << "PASS: JP01 Wicked lightweight Collider parity\n";
    return 0;
}
