#include "renegade/bridge/CommandService.h"

#include <WickedEngine.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "JP01 SCALE COMMIT FAIL // " << message << '\n';
        return 1;
    }

    bool Near(const XMFLOAT3& left, const XMFLOAT3& right)
    {
        constexpr float epsilon = 0.0001f;
        return std::fabs(left.x - right.x) < epsilon &&
            std::fabs(left.y - right.y) < epsilon &&
            std::fabs(left.z - right.z) < epsilon;
    }
}

int main()
{
    using namespace renegade::bridge;

    wi::scene::Scene scene;
    const wi::ecs::Entity entity = scene.Entity_CreateTransform("crate002");
    auto* transform = scene.transforms.GetComponent(entity);
    if (transform == nullptr)
        return Fail("fixture transform missing");

    auto& body = scene.rigidbodies.Create(entity);
    body.SetRefreshParametersNeeded(false);

    const TransformState before = CaptureTransform(*transform);
    TransformState after = before;
    after.scale = XMFLOAT3(2.0f, 3.0f, 4.0f);

    // Studio's transform gizmo applies its preview directly to the live
    // Transform before it commits a SetTransformCommand. Reproduce that exact
    // sequence: Execute must compare command history, not the already-updated
    // live Transform, or the collider rebuild is silently skipped.
    transform->scale_local = after.scale;
    transform->SetDirty();
    transform->UpdateTransform();

    CommandService commands;
    if (!commands.Execute(std::make_unique<SetTransformCommand>(
            scene, entity, before, after)))
    {
        return Fail("live-preview transform command did not commit");
    }
    if (!body.IsRefreshParametersNeeded() ||
        !Near(transform->scale_local, after.scale))
    {
        return Fail("gizmo live-preview scale commit did not rebuild collider");
    }

    body.SetRefreshParametersNeeded(false);
    if (!commands.Undo() ||
        !body.IsRefreshParametersNeeded() ||
        !Near(transform->scale_local, before.scale))
    {
        return Fail("gizmo scale Undo did not rebuild collider");
    }

    body.SetRefreshParametersNeeded(false);
    if (!commands.Redo() ||
        !body.IsRefreshParametersNeeded() ||
        !Near(transform->scale_local, after.scale))
    {
        return Fail("gizmo scale Redo did not rebuild collider");
    }

    std::cout <<
        "JP01 SCALE COMMIT PASS // live gizmo preview, commit, Undo and Redo rebuild the owner collider\n";
    return 0;
}
