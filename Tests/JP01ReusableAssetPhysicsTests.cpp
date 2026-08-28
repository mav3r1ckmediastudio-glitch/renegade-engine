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

    bool Near(const XMFLOAT3& left, const XMFLOAT3& right)
    {
        return Near(left.x, right.x) &&
            Near(left.y, right.y) &&
            Near(left.z, right.z);
    }

    struct ReusableFixture
    {
        wi::ecs::Entity wrapper = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payload = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity nested = wi::ecs::INVALID_ENTITY;
    };

    ReusableFixture CreateReusableFixture(
        wi::scene::Scene& scene,
        const bool stampInstanceMetadata = true)
    {
        ReusableFixture fixture;
        fixture.wrapper = scene.Entity_CreateTransform("Reusable Asset Instance");
        fixture.payload = scene.Entity_CreateTransform("Imported Payload Root");
        fixture.nested = scene.Entity_CreateCube("crate002");

        scene.Component_Attach(fixture.payload, fixture.wrapper, true);
        scene.Component_Attach(fixture.nested, fixture.payload, true);

        if (auto* transform = scene.transforms.GetComponent(fixture.nested))
        {
            // Deliberately non-uniform imported geometry. Auto-fit must account
            // for this child-local scale but must not bake the wrapper scale in.
            transform->scale_local = XMFLOAT3(2.0f, 0.75f, 0.5f);
            transform->SetDirty();
        }

        if (stampInstanceMetadata)
        {
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
        }
        return fixture;
    }
}

int main()
{
    using namespace renegade::bridge;
    using Body = wi::scene::RigidBodyPhysicsComponent;

    wi::scene::Scene scene;
    const auto fixture = CreateReusableFixture(scene);
    const auto outside = scene.Entity_CreateTransform("Standalone Object");

    if (ResolveCollisionAuthoringTarget(scene, fixture.nested) != fixture.wrapper ||
        ResolveCollisionAuthoringTarget(scene, fixture.payload) != fixture.wrapper ||
        ResolveCollisionAuthoringTarget(scene, fixture.wrapper) != fixture.wrapper ||
        ResolveCollisionAuthoringTarget(scene, outside) != outside)
    {
        return Fail("reusable asset collision target did not resolve to stable root");
    }

    // Primitive fit is measured below the asset root, excluding the root's own
    // authored scale because the backend applies that scale when creating the
    // actual physics shape. Changing root scale must therefore leave authored
    // primitive dimensions unchanged.
    CollisionState fitted;
    auto* wrapperTransform = scene.transforms.GetComponent(fixture.wrapper);
    if (wrapperTransform == nullptr)
        return Fail("fixture root transform missing");
    wrapperTransform->scale_local = XMFLOAT3(3.0f, 3.0f, 3.0f);
    CollisionState firstFit = fitted;
    if (!FitPrimitiveCollisionStateToTarget(
            scene, fixture.wrapper, firstFit))
    {
        return Fail("primitive auto-fit could not measure reusable geometry");
    }
    if (firstFit.boxHalfExtents.x <= 0.01f ||
        firstFit.boxHalfExtents.y <= 0.01f ||
        firstFit.boxHalfExtents.z <= 0.01f)
    {
        return Fail("primitive auto-fit produced invalid dimensions");
    }

    wrapperTransform->scale_local = XMFLOAT3(7.0f, 5.0f, 4.0f);
    CollisionState secondFit = fitted;
    if (!FitPrimitiveCollisionStateToTarget(
            scene, fixture.wrapper, secondFit) ||
        !Near(firstFit.boxHalfExtents, secondFit.boxHalfExtents))
    {
        return Fail("root scale was baked into fitted collider dimensions");
    }

    CollisionState initial;
    if (!initial.startDeactivated)
        return Fail("new rigid bodies no longer start deactivated");
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
        return Fail("CreateCollisionCommand attached body to imported child instead of root");
    }

    const auto* created = scene.rigidbodies.GetComponent(fixture.wrapper);
    if (created == nullptr || !created->IsStartDeactivated() ||
        !Near(created->mass, 3.25f) ||
        !Near(created->friction, 0.42f) ||
        !Near(created->buoyancy, 1.35f) ||
        !Near(created->box.halfextents, secondFit.boxHalfExtents))
    {
        return Fail("root-owned rigid body lost fitted creator authoring state");
    }

    auto* mutableCreated = scene.rigidbodies.GetComponent(fixture.wrapper);
    if (mutableCreated == nullptr)
        return Fail("created rigid body disappeared");
    mutableCreated->SetRefreshParametersNeeded(false);
    if (!RequestCollisionShapeRefresh(scene, fixture.nested) ||
        !mutableCreated->IsRefreshParametersNeeded())
    {
        return Fail("scale refresh did not reach the root-owned rigid body");
    }

    if (!commands.Undo() || scene.rigidbodies.Contains(fixture.wrapper) ||
        !commands.Redo() || !scene.rigidbodies.Contains(fixture.wrapper))
    {
        return Fail("root-owned rigid body Undo/Redo lifecycle failed");
    }

    // A placed reusable asset must expose a creator-facing root name rather
    // than the implementation wrapper label. The imported render-object name
    // is the deterministic fallback when no separate product display name is
    // persisted in the scene payload.
    wi::scene::Scene namingScene;
    const auto naming = CreateReusableFixture(namingScene, false);
    if (auto* name = namingScene.names.GetComponent(naming.wrapper))
        name->name = "Reusable Asset Drag Preview";
    CommandService namingCommands;
    if (!namingCommands.Execute(std::make_unique<PlaceReusableModelCommand>(
            namingScene,
            AssetId,
            naming.wrapper,
            naming.payload,
            0)))
    {
        return Fail("reusable placement naming command did not execute");
    }
    const auto* rootName = namingScene.names.GetComponent(naming.wrapper);
    if (rootName == nullptr || rootName->name != "crate002")
        return Fail("reusable root retained an implementation-facing name");
    if (!namingCommands.Undo() ||
        namingScene.transforms.Contains(naming.wrapper) ||
        !namingCommands.Redo())
    {
        return Fail("named reusable root Undo/Redo failed");
    }
    rootName = namingScene.names.GetComponent(naming.wrapper);
    if (rootName == nullptr || rootName->name != "crate002")
        return Fail("creator-facing root name did not survive Redo");

    // Recovery boundary for scenes saved by the bad owner-validation build:
    // exactly one nested body moves intact to the stable root before physics.
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
        return Fail("nested-body migration did not preserve authoring state safely");
    }

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
        "JP01 REUSABLE PHYSICS PASS // root authoring, creator naming, primitive fit, scale refresh, Undo/Redo, recovery\n";
    return 0;
}
