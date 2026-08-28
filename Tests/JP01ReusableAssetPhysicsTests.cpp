#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"

#include <WickedEngine.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
    constexpr const char* AssetId =
        "99999999-9999-4999-8999-999999999999";

    int Fail(const char* message)
    {
        std::cerr << "JP01 REUSABLE PHYSICS FAIL // " << message << '\n';
        return 1;
    }

    bool Near(const float left, const float right)
    {
        return std::fabs(left - right) < 0.0001f;
    }

    struct ReusableFixture
    {
        wi::ecs::Entity wrapper = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payload = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity nested = wi::ecs::INVALID_ENTITY;
    };

    ReusableFixture CreateReusableFixture(wi::scene::Scene& scene)
    {
        ReusableFixture fixture;
        fixture.wrapper = scene.Entity_CreateTransform("Reusable Asset Instance");
        fixture.payload = scene.Entity_CreateTransform("Imported Payload Root");
        fixture.nested = scene.Entity_CreateTransform("crate002");

        scene.Component_Attach(fixture.payload, fixture.wrapper, true);
        scene.Component_Attach(fixture.nested, fixture.payload, true);

        auto& wrapperMetadata = scene.metadatas.Create(fixture.wrapper);
        wrapperMetadata.string_values.set(
            renegade::bridge::ReusableAssetInstanceIdMetadataKey,
            AssetId);
        wrapperMetadata.int_values.set(
            renegade::bridge::ReusableAssetInstanceVersionMetadataKey,
            renegade::bridge::ReusableAssetInstanceVersion);

        auto& payloadMetadata = scene.metadatas.Create(fixture.payload);
        payloadMetadata.bool_values.set(
            renegade::bridge::ReusableAssetPayloadRootMetadataKey,
            true);
        return fixture;
    }
}

int main()
{
    using namespace renegade::bridge;
    using Body = wi::scene::RigidBodyPhysicsComponent;

    // Creator-facing target resolution must never attach a dynamic Jolt body
    // to an arbitrary imported child of a reusable asset. The stable wrapper
    // owns authored transform/state and is the only safe whole-asset target.
    wi::scene::Scene scene;
    const auto fixture = CreateReusableFixture(scene);
    const auto outside = scene.Entity_CreateTransform("Standalone Object");

    if (ResolveCollisionAuthoringTarget(scene, fixture.nested) != fixture.wrapper ||
        ResolveCollisionAuthoringTarget(scene, fixture.payload) != fixture.wrapper ||
        ResolveCollisionAuthoringTarget(scene, fixture.wrapper) != fixture.wrapper ||
        ResolveCollisionAuthoringTarget(scene, outside) != outside)
    {
        return Fail("reusable asset collision target did not resolve to stable wrapper");
    }

    CollisionState initial;
    if (!initial.startDeactivated)
    {
        return Fail("new rigid bodies no longer match Wicked Editor start-deactivated default");
    }
    initial.mass = 3.25f;
    initial.friction = 0.42f;
    initial.buoyancy = 1.35f;

    CommandService commands;
    if (!commands.Execute(std::make_unique<CreateCollisionCommand>(
            scene, fixture.nested, initial)))
    {
        return Fail("nested reusable rigid-body authoring request did not execute");
    }
    if (scene.rigidbodies.Contains(fixture.nested) ||
        !scene.rigidbodies.Contains(fixture.wrapper))
    {
        return Fail("CreateCollisionCommand attached body to imported child instead of wrapper");
    }

    const auto* created = scene.rigidbodies.GetComponent(fixture.wrapper);
    if (created == nullptr || !created->IsStartDeactivated() ||
        !Near(created->mass, 3.25f) ||
        !Near(created->friction, 0.42f) ||
        !Near(created->buoyancy, 1.35f))
    {
        return Fail("wrapper-owned rigid body lost creator authoring state");
    }

    if (!commands.Undo() || scene.rigidbodies.Contains(fixture.wrapper) ||
        !commands.Redo() || !scene.rigidbodies.Contains(fixture.wrapper))
    {
        return Fail("wrapper-owned rigid body Undo/Redo lifecycle failed");
    }

    // Recovery boundary for scenes saved by the bad owner-validation build:
    // exactly one nested body is moved intact to the stable wrapper before
    // Wicked's first physics update can run its parent matrix feedback loop.
    wi::scene::Scene recoveryScene;
    const auto recovery = CreateReusableFixture(recoveryScene);
    auto& badBody = recoveryScene.rigidbodies.Create(recovery.nested);
    badBody.shape = Body::CollisionShape::CAPSULE;
    badBody.mass = 7.5f;
    badBody.friction = 0.66f;
    badBody.restitution = 0.27f;
    badBody.buoyancy = 1.8f;
    badBody.capsule.radius = 0.75f;
    badBody.capsule.height = 1.25f;
    badBody.SetKinematic(true);
    badBody.SetLocked2D(true);
    badBody.SetDisableDeactivation(true);
    badBody.SetStartDeactivated(false);
    badBody.SetRefreshParametersNeeded(false);

    const auto repaired = RepairReusableAssetCollisionTargets(recoveryScene);
    if (repaired.migratedBodyCount != 1 || repaired.conflictCount != 0 ||
        recoveryScene.rigidbodies.Contains(recovery.nested) ||
        !recoveryScene.rigidbodies.Contains(recovery.wrapper))
    {
        return Fail("unambiguous nested reusable rigid body was not migrated");
    }

    const auto* migrated = recoveryScene.rigidbodies.GetComponent(recovery.wrapper);
    if (migrated == nullptr ||
        migrated->shape != Body::CollisionShape::CAPSULE ||
        !Near(migrated->mass, 7.5f) ||
        !Near(migrated->friction, 0.66f) ||
        !Near(migrated->restitution, 0.27f) ||
        !Near(migrated->buoyancy, 1.8f) ||
        !Near(migrated->capsule.radius, 0.75f) ||
        !Near(migrated->capsule.height, 1.25f) ||
        !migrated->IsKinematic() || !migrated->IsLocked2D() ||
        !migrated->IsDisableDeactivation() || migrated->IsStartDeactivated() ||
        !migrated->IsRefreshParametersNeeded() ||
        migrated->physicsobject != nullptr)
    {
        return Fail("nested-body migration did not preserve complete Wicked authoring state safely");
    }

    // Never guess when the reusable asset already has a root body and also
    // contains a nested body. That is an explicit multi-body authoring case,
    // not the one-body corruption signature this recovery is allowed to fix.
    wi::scene::Scene conflictScene;
    const auto conflict = CreateReusableFixture(conflictScene);
    conflictScene.rigidbodies.Create(conflict.wrapper).mass = 1.0f;
    conflictScene.rigidbodies.Create(conflict.nested).mass = 2.0f;
    const auto conflictResult = RepairReusableAssetCollisionTargets(conflictScene);
    if (conflictResult.migratedBodyCount != 0 ||
        conflictResult.conflictCount != 1 ||
        !conflictScene.rigidbodies.Contains(conflict.wrapper) ||
        !conflictScene.rigidbodies.Contains(conflict.nested))
    {
        return Fail("ambiguous root/nested multi-body case was modified destructively");
    }

    std::cout <<
        "JP01 REUSABLE PHYSICS PASS // stable wrapper targeting, Undo/Redo, and pre-physics nested-body recovery\n";
    return 0;
}
