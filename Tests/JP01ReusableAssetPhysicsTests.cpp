#include "renegade/bridge/CharacterPhysicsService.h"
#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/PhysicsService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/VehiclePhysicsService.h"

#include <WickedEngine.h>

#include <array>
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

    wi::ecs::Entity CreateHeadlessBoxObject(
        wi::scene::Scene& scene,
        const char* name)
    {
        // Do not use Scene::Entity_CreateCube() here. Wicked's primitive
        // factory builds GPU render data and these acceptance tests deliberately
        // run without a graphics device. A mesh/object component pair is enough
        // to exercise the exact hierarchy bounds and collision-authoring path.
        const wi::ecs::Entity meshEntity = wi::ecs::CreateEntity();
        auto& mesh = scene.meshes.Create(meshEntity);
        mesh.vertex_positions = {
            XMFLOAT3(-0.5f, -0.5f, -0.5f),
            XMFLOAT3( 0.5f, -0.5f, -0.5f),
            XMFLOAT3(-0.5f,  0.5f, -0.5f),
            XMFLOAT3( 0.5f,  0.5f, -0.5f),
            XMFLOAT3(-0.5f, -0.5f,  0.5f),
            XMFLOAT3( 0.5f, -0.5f,  0.5f),
            XMFLOAT3(-0.5f,  0.5f,  0.5f),
            XMFLOAT3( 0.5f,  0.5f,  0.5f),
        };

        const wi::ecs::Entity objectEntity = wi::ecs::CreateEntity();
        scene.names.Create(objectEntity).name = name == nullptr ? "Object" : name;
        scene.transforms.Create(objectEntity);
        scene.objects.Create(objectEntity).meshID = meshEntity;
        return objectEntity;
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
        fixture.nested = CreateHeadlessBoxObject(scene, "crate002");

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

    bool AdoptFixture(
        wi::scene::Scene& scene,
        renegade::bridge::CommandService& commands,
        const ReusableFixture& fixture)
    {
        if (auto* name = scene.names.GetComponent(fixture.wrapper))
            name->name = "Reusable Asset Drag Preview";
        return commands.Execute(
            std::make_unique<renegade::bridge::PlaceReusableModelCommand>(
                scene,
                AssetId,
                fixture.wrapper,
                fixture.payload,
                0));
    }
}

int main()
{
    using namespace renegade::bridge;
    using Body = wi::scene::RigidBodyPhysicsComponent;
    using Shape = Body::CollisionShape;

    // Exact creator targeting contract: clicking any imported descendant of a
    // reusable asset resolves to the stable root; unrelated entities do not.
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
    // native physics shape. Selecting the wrapper directly must auto-fit too.
    CollisionState fitted;
    auto* wrapperTransform = scene.transforms.GetComponent(fixture.wrapper);
    if (wrapperTransform == nullptr)
        return Fail("fixture root transform missing");
    wrapperTransform->scale_local = XMFLOAT3(3.0f, 3.0f, 3.0f);
    CollisionState firstFit = fitted;
    if (!FitPrimitiveCollisionStateToTarget(scene, fixture.wrapper, firstFit))
        return Fail("primitive auto-fit could not measure reusable geometry");
    if (firstFit.boxHalfExtents.x <= 0.01f ||
        firstFit.boxHalfExtents.y <= 0.01f ||
        firstFit.boxHalfExtents.z <= 0.01f)
    {
        return Fail("primitive auto-fit produced invalid dimensions");
    }

    wrapperTransform->scale_local = XMFLOAT3(7.0f, 5.0f, 4.0f);
    CollisionState secondFit = fitted;
    if (!FitPrimitiveCollisionStateToTarget(scene, fixture.wrapper, secondFit) ||
        !Near(firstFit.boxHalfExtents, secondFit.boxHalfExtents))
    {
        return Fail("root scale was baked into fitted collider dimensions");
    }

    CollisionState initial;
    if (initial.startDeactivated)
        return Fail("new rigid bodies no longer start active");
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
    if (created == nullptr || created->IsStartDeactivated() ||
        !Near(created->mass, 3.25f) ||
        !Near(created->friction, 0.42f) ||
        !Near(created->buoyancy, 1.35f) ||
        !Near(created->box.halfextents, secondFit.boxHalfExtents))
    {
        return Fail("root-owned rigid body lost fitted creator authoring state");
    }

    // Set and Remove must resolve descendant requests to the same root too.
    CollisionState changed = CaptureCollision(*created);
    changed.friction = 0.73f;
    if (!commands.Execute(std::make_unique<SetCollisionCommand>(
            scene, fixture.nested, changed)))
    {
        return Fail("nested rigid-body edit did not resolve to the stable root");
    }
    const auto* changedBody = scene.rigidbodies.GetComponent(fixture.wrapper);
    if (changedBody == nullptr || !Near(changedBody->friction, 0.73f))
        return Fail("root-owned rigid body did not receive nested edit");

    if (!commands.Execute(std::make_unique<RemoveCollisionCommand>(
            scene, fixture.nested)) ||
        scene.rigidbodies.Contains(fixture.wrapper) ||
        !commands.Undo() || !scene.rigidbodies.Contains(fixture.wrapper))
    {
        return Fail("nested rigid-body remove/undo did not resolve to root");
    }

    // Roll back the Set command and original Create command, then prove Redo
    // restores the root-owned body rather than the imported child.
    if (!commands.Undo() || !commands.Undo() ||
        scene.rigidbodies.Contains(fixture.wrapper) ||
        !commands.Redo() || !scene.rigidbodies.Contains(fixture.wrapper) ||
        scene.rigidbodies.Contains(fixture.nested))
    {
        return Fail("root-owned rigid body Undo/Redo lifecycle failed");
    }

    // Character and Vehicle pages share RigidBodyPhysicsComponent, but neither
    // service is allowed to fabricate a nested body. UI selection promotion is
    // the creator route; a raw nested command must fail closed instead.
    {
        CharacterPhysicsState character;
        character.enabled = true;
        CommandService characterCommands;
        if (characterCommands.Execute(
                std::make_unique<SetCharacterPhysicsCommand>(
                    scene, fixture.nested, character)) ||
            scene.rigidbodies.Contains(fixture.nested))
        {
            return Fail("Character authoring created or edited an unsafe nested body");
        }

        VehiclePhysicsState vehicle;
        vehicle.type = Body::Vehicle::Type::Car;
        CommandService vehicleCommands;
        if (vehicleCommands.Execute(
                std::make_unique<SetVehiclePhysicsCommand>(
                    scene, fixture.nested, vehicle)) ||
            scene.rigidbodies.Contains(fixture.nested))
        {
            return Fail("Vehicle authoring created or edited an unsafe nested body");
        }
    }

    // Three instances of the same visible crate are the owner acceptance case.
    // They need unique creator-facing roots, independent rigid bodies, identical
    // root-local fit despite different wrapper scale, and independent rebuilds.
    wi::scene::Scene threeScene;
    std::array<ReusableFixture, 3> three = {
        CreateReusableFixture(threeScene, false),
        CreateReusableFixture(threeScene, false),
        CreateReusableFixture(threeScene, false),
    };
    CommandService placementCommands;
    for (const auto& instance : three)
    {
        if (!AdoptFixture(threeScene, placementCommands, instance))
            return Fail("three-instance reusable placement did not execute");
    }

    const std::array<const char*, 3> expectedNames = {
        "crate002", "crate002 (2)", "crate002 (3)"
    };
    for (std::size_t index = 0; index < three.size(); ++index)
    {
        const auto* name = threeScene.names.GetComponent(three[index].wrapper);
        if (name == nullptr || name->name != expectedNames[index])
            return Fail("three same-asset instances did not receive unique creator names");

        auto* transform = threeScene.transforms.GetComponent(three[index].wrapper);
        if (transform == nullptr)
            return Fail("three-instance wrapper transform missing");
        const float scale = static_cast<float>(index + 1);
        transform->scale_local = XMFLOAT3(scale, scale + 0.5f, scale + 1.0f);
        transform->SetDirty();
    }

    std::array<XMFLOAT3, 3> fittedExtents = {};
    std::array<CommandService, 3> bodyCommands;
    for (std::size_t index = 0; index < three.size(); ++index)
    {
        CollisionState bodyState;
        bodyState.mass = static_cast<float>(index + 1);
        if (!bodyCommands[index].Execute(
                std::make_unique<CreateCollisionCommand>(
                    threeScene, three[index].nested, bodyState)))
        {
            return Fail("three-instance rigid-body add failed");
        }
        if (threeScene.rigidbodies.Contains(three[index].nested) ||
            !threeScene.rigidbodies.Contains(three[index].wrapper))
        {
            return Fail("three-instance body ownership escaped stable wrapper");
        }
        const auto* body = threeScene.rigidbodies.GetComponent(three[index].wrapper);
        if (body == nullptr)
            return Fail("three-instance root body missing");
        fittedExtents[index] = body->box.halfextents;
    }
    if (!Near(fittedExtents[0], fittedExtents[1]) ||
        !Near(fittedExtents[0], fittedExtents[2]))
    {
        return Fail("different wrapper scales changed root-local fitted dimensions");
    }

    // Clear parameter-refresh flags, scale only the second crate through the
    // central transform command and prove only that native body is invalidated.
    for (const auto& instance : three)
    {
        auto* body = threeScene.rigidbodies.GetComponent(instance.wrapper);
        if (body == nullptr)
            return Fail("three-instance body missing before scale test");
        body->SetRefreshParametersNeeded(false);
    }

    auto* scaledTransform = threeScene.transforms.GetComponent(three[1].wrapper);
    if (scaledTransform == nullptr)
        return Fail("scaled wrapper transform missing");
    const TransformState scaleBefore = CaptureTransform(*scaledTransform);
    TransformState scaleAfter = scaleBefore;
    scaleAfter.scale = XMFLOAT3(
        scaleBefore.scale.x * 2.0f,
        scaleBefore.scale.y * 2.0f,
        scaleBefore.scale.z * 2.0f);

    CommandService scaleCommands;
    if (!scaleCommands.Execute(std::make_unique<SetTransformCommand>(
            threeScene, three[1].wrapper, scaleBefore, scaleAfter)))
    {
        return Fail("committed reusable-root scale command did not execute");
    }
    if (!threeScene.rigidbodies.GetComponent(three[1].wrapper)
            ->IsRefreshParametersNeeded() ||
        threeScene.rigidbodies.GetComponent(three[0].wrapper)
            ->IsRefreshParametersNeeded() ||
        threeScene.rigidbodies.GetComponent(three[2].wrapper)
            ->IsRefreshParametersNeeded())
    {
        return Fail("scale rebuild was not isolated to the transformed root body");
    }
    if (!Near(
            threeScene.rigidbodies.GetComponent(three[1].wrapper)->box.halfextents,
            fittedExtents[1]))
    {
        return Fail("root scale rewrote stored root-local collider dimensions");
    }

    auto* scaledBody = threeScene.rigidbodies.GetComponent(three[1].wrapper);
    scaledBody->SetRefreshParametersNeeded(false);
    if (!scaleCommands.Undo() || !scaledBody->IsRefreshParametersNeeded() ||
        !Near(
            threeScene.transforms.GetComponent(three[1].wrapper)->scale_local,
            scaleBefore.scale))
    {
        return Fail("scale Undo did not rebuild and restore the root body");
    }
    scaledBody->SetRefreshParametersNeeded(false);
    if (!scaleCommands.Redo() || !scaledBody->IsRefreshParametersNeeded() ||
        !Near(
            threeScene.transforms.GetComponent(three[1].wrapper)->scale_local,
            scaleAfter.scale))
    {
        return Fail("scale Redo did not rebuild and restore the root body");
    }

    // Editing a child transform is not equivalent to scaling the whole asset.
    // It must not pretend a stale root-local fit has been rebuilt.
    scaledBody->SetRefreshParametersNeeded(false);
    auto* nestedTransform = threeScene.transforms.GetComponent(three[1].nested);
    if (nestedTransform == nullptr)
        return Fail("nested transform missing before child-scale test");
    TransformState childBefore = CaptureTransform(*nestedTransform);
    TransformState childAfter = childBefore;
    childAfter.scale.x += 0.25f;
    CommandService childScaleCommands;
    if (!childScaleCommands.Execute(std::make_unique<SetTransformCommand>(
            threeScene, three[1].nested, childBefore, childAfter)) ||
        scaledBody->IsRefreshParametersNeeded())
    {
        return Fail("child-only transform falsely refreshed the root collision shape");
    }

    // Legacy anonymous names are repaired deterministically on the same
    // load/save canonicalization boundary, while creator-renamed roots survive.
    wi::scene::Scene legacyNames;
    const auto legacyA = CreateReusableFixture(legacyNames);
    const auto legacyB = CreateReusableFixture(legacyNames);
    const auto legacyC = CreateReusableFixture(legacyNames);
    legacyNames.names.GetComponent(legacyB.wrapper)->name = "Boss Crate";
    if (RepairReusableAssetInstanceNames(legacyNames) != 2)
        return Fail("legacy anonymous wrapper-name repair count was incorrect");
    if (legacyNames.names.GetComponent(legacyA.wrapper)->name != "crate002" ||
        legacyNames.names.GetComponent(legacyB.wrapper)->name != "Boss Crate" ||
        legacyNames.names.GetComponent(legacyC.wrapper)->name != "crate002 (2)")
    {
        return Fail("legacy naming repair overwrote custom name or failed uniqueness");
    }

    // Recovery boundary for scenes saved by the bad owner-validation build:
    // exactly one nested body moves intact to the stable root before physics.
    wi::scene::Scene recoveryScene;
    const auto recovery = CreateReusableFixture(recoveryScene);
    auto& badBody = recoveryScene.rigidbodies.Create(recovery.nested);
    badBody.shape = Shape::CAPSULE;
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
    if (migrated == nullptr || migrated->shape != Shape::CAPSULE ||
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

    // Multiple nested bodies are an advanced/compound case. Never guess which
    // one the creator meant to promote to the whole-asset root.
    wi::scene::Scene conflictScene;
    const auto conflict = CreateReusableFixture(conflictScene);
    const auto secondNested =
        CreateHeadlessBoxObject(conflictScene, "crate002_part2");
    conflictScene.Component_Attach(secondNested, conflict.payload, true);
    conflictScene.rigidbodies.Create(conflict.nested).mass = 2.0f;
    conflictScene.rigidbodies.Create(secondNested).mass = 4.0f;
    const auto conflictResult = RepairReusableAssetCollisionTargets(conflictScene);
    if (conflictResult.migratedBodyCount != 0 ||
        conflictResult.conflictCount != 2 ||
        conflictScene.rigidbodies.Contains(conflict.wrapper) ||
        !conflictScene.rigidbodies.Contains(conflict.nested) ||
        !conflictScene.rigidbodies.Contains(secondNested))
    {
        return Fail("ambiguous multi-body reusable hierarchy was modified destructively");
    }

    // The creator-visible green collision overlay is driven by the existing
    // global physics visualizer. Keep that backend switch live through JP01.
    const auto visualizerBefore = CapturePhysicsWorldState();
    auto visualizerOn = visualizerBefore;
    visualizerOn.debugDrawEnabled = true;
    ApplyPhysicsWorldState(visualizerOn);
    if (!CapturePhysicsWorldState().debugDrawEnabled)
        return Fail("physics visualizer could not be enabled for collider inspection");
    ApplyPhysicsWorldState(visualizerBefore);

    std::cout <<
        "JP01 REUSABLE PHYSICS PASS // three instances, stable root ownership, unique naming, primitive fit, transform rebuild, Undo/Redo, recovery, visualizer\n";
    return 0;
}
