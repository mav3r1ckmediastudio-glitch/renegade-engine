#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/PhysicsService.h"

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

    wi::ecs::Entity CreateMeshBackedObject(wi::scene::Scene& scene)
    {
        const auto meshEntity = wi::ecs::CreateEntity();
        auto& mesh = scene.meshes.Create(meshEntity);
        // Four non-degenerate points are enough to exercise Convex Hull, form
        // two triangles for Triangle Mesh and form a 2x2 Height Field grid.
        mesh.vertex_positions = {
            XMFLOAT3(0.0f, 0.0f, 0.0f),
            XMFLOAT3(1.0f, 0.0f, 0.0f),
            XMFLOAT3(0.0f, 0.0f, 1.0f),
            XMFLOAT3(1.0f, 0.5f, 1.0f),
        };
        mesh.indices = { 0, 1, 2, 1, 3, 2 };
        auto& subset = mesh.subsets.emplace_back();
        subset.indexOffset = 0;
        subset.indexCount = static_cast<uint32_t>(mesh.indices.size());

        const auto objectEntity = wi::ecs::CreateEntity();
        scene.transforms.Create(objectEntity);
        scene.objects.Create(objectEntity).meshID = meshEntity;
        return objectEntity;
    }
}

int main()
{
    using renegade::bridge::CollisionState;
    using renegade::bridge::CollisionTargetStatus;
    using Body = wi::scene::RigidBodyPhysicsComponent;
    using Shape = Body::CollisionShape;

    // JP01 runtime façade: use Wicked's real pinned defaults, keep gravity on
    // the serialized Scene, and never claim a component-only body is live.
    {
        renegade::bridge::PhysicsWorldState dirtyWorld;
        dirtyWorld.accuracy = -20;
        dirtyWorld.frameRate = std::numeric_limits<float>::quiet_NaN();
        dirtyWorld.constraintDebugSize = -5.0f;
        dirtyWorld.debugDrawMaxDistance = std::numeric_limits<float>::quiet_NaN();
        dirtyWorld.characterCollisionTolerance = std::numeric_limits<float>::quiet_NaN();
        const auto safeWorld =
            renegade::bridge::SanitizePhysicsWorldState(dirtyWorld);
        if (safeWorld.accuracy != 1 || !NearlyEqual(safeWorld.frameRate, 60.0f) ||
            safeWorld.constraintDebugSize != 0.0f ||
            !NearlyEqual(safeWorld.debugDrawMaxDistance, 500.0f) ||
            !NearlyEqual(safeWorld.characterCollisionTolerance, 0.05f))
        {
            return Fail("physics world sanitization diverged from pinned Wicked defaults");
        }

        wi::scene::Scene physicsScene;
        renegade::bridge::SetPhysicsGravity(
            physicsScene,
            XMFLOAT3(0.0f, 0.0f, 0.0f));
        if (!NearlyEqual(
                renegade::bridge::GetPhysicsGravity(physicsScene),
                XMFLOAT3(0.0f, 0.0f, 0.0f)))
        {
            return Fail("zero gravity did not reach Wicked scene state");
        }
        renegade::bridge::SetPhysicsGravity(
            physicsScene,
            XMFLOAT3(12345.0f, -0.25f, 77.0f));
        if (!NearlyEqual(
                renegade::bridge::GetPhysicsGravity(physicsScene),
                XMFLOAT3(12345.0f, -0.25f, 77.0f)))
        {
            return Fail("physics gravity was unnecessarily magnitude-clamped");
        }
        const auto safeGravity = renegade::bridge::SanitizePhysicsGravity(
            XMFLOAT3(
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN()));
        if (!NearlyEqual(safeGravity, XMFLOAT3(0.0f, -10.0f, 0.0f)))
        {
            return Fail("non-finite gravity did not fall back safely");
        }

        const auto componentOnlyEntity = wi::ecs::CreateEntity();
        physicsScene.transforms.Create(componentOnlyEntity);
        physicsScene.rigidbodies.Create(componentOnlyEntity);
        XMFLOAT3 vectorOut(9.0f, 9.0f, 9.0f);
        XMFLOAT4 rotationOut(9.0f, 9.0f, 9.0f, 9.0f);
        if (renegade::bridge::HasLivePhysicsBody(
                physicsScene, componentOnlyEntity) ||
            renegade::bridge::SetPhysicsPosition(
                physicsScene, componentOnlyEntity, XMFLOAT3(1, 2, 3)) ||
            renegade::bridge::SetPhysicsPositionAndRotation(
                physicsScene,
                componentOnlyEntity,
                XMFLOAT3(1, 2, 3),
                XMFLOAT4(0, 0, 0, 1)) ||
            renegade::bridge::GetPhysicsPosition(
                physicsScene, componentOnlyEntity, vectorOut) ||
            renegade::bridge::GetPhysicsRotation(
                physicsScene, componentOnlyEntity, rotationOut) ||
            renegade::bridge::SetLinearVelocity(
                physicsScene, componentOnlyEntity, XMFLOAT3(1, 0, 0)) ||
            renegade::bridge::GetLinearVelocity(
                physicsScene, componentOnlyEntity, vectorOut) ||
            renegade::bridge::SetAngularVelocity(
                physicsScene, componentOnlyEntity, XMFLOAT3(0, 1, 0)) ||
            renegade::bridge::ApplyForce(
                physicsScene, componentOnlyEntity, XMFLOAT3(0, 1, 0)) ||
            renegade::bridge::ApplyForceAt(
                physicsScene,
                componentOnlyEntity,
                XMFLOAT3(0, 1, 0),
                XMFLOAT3(1, 0, 0)) ||
            renegade::bridge::ApplyImpulse(
                physicsScene, componentOnlyEntity, XMFLOAT3(0, 1, 0)) ||
            renegade::bridge::ApplyImpulseAt(
                physicsScene,
                componentOnlyEntity,
                XMFLOAT3(0, 1, 0),
                XMFLOAT3(1, 0, 0)) ||
            renegade::bridge::ApplyTorque(
                physicsScene, componentOnlyEntity, XMFLOAT3(0, 1, 0)) ||
            renegade::bridge::SetBodyActive(
                physicsScene, componentOnlyEntity, true) ||
            renegade::bridge::SetGhostMode(
                physicsScene, componentOnlyEntity, true) ||
            renegade::bridge::ActivateAllRigidBodies(physicsScene) ||
            renegade::bridge::OptimizePhysicsBroadPhase(physicsScene) ||
            renegade::bridge::ResetPhysicsObjects(physicsScene))
        {
            return Fail("runtime façade treated an uncreated native body as live");
        }
        if (!NearlyEqual(vectorOut, XMFLOAT3(9.0f, 9.0f, 9.0f)) ||
            !NearlyEqual(rotationOut.x, 9.0f))
        {
            return Fail("failed physics readback modified its output value");
        }

        const auto miss = renegade::bridge::Raycast(
            physicsScene,
            wi::primitive::Ray(
                XMFLOAT3(0, 0, 0),
                XMFLOAT3(0, -1, 0)));
        if (miss.IsValid())
        {
            return Fail("raycast reported a hit without a native physics scene");
        }
    }

    // JP01 rigid-body parity recognizes all seven shapes exposed by Wicked.
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

    // Sanitization follows Wicked Editor's exposed ranges and prevents
    // degenerate primitive dimensions.
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

    // Target validation mirrors Wicked's runtime plumbing: every body needs a
    // Transform, and mesh-derived collision shapes need Object -> Mesh data.
    {
        const auto noTransform = wi::ecs::CreateEntity();
        if (renegade::bridge::CheckCollisionTarget(
                scene, noTransform, Shape::BOX) !=
            CollisionTargetStatus::MissingTransform)
        {
            return Fail("collision target accepted an entity without Transform");
        }

        const auto noMesh = wi::ecs::CreateEntity();
        scene.transforms.Create(noMesh);
        if (renegade::bridge::CheckCollisionTarget(
                scene, noMesh, Shape::CONVEX_HULL) !=
                CollisionTargetStatus::MissingMesh ||
            renegade::bridge::CanAuthorCollisionShape(
                scene, noMesh, Shape::TRIANGLE_MESH))
        {
            return Fail("mesh-derived collision accepted an entity without mesh");
        }

        CollisionState invalidComplex;
        invalidComplex.shape = Shape::HEIGHTFIELD;
        renegade::bridge::CommandService invalidComplexCommands;
        if (invalidComplexCommands.Execute(
                std::make_unique<renegade::bridge::CreateCollisionCommand>(
                    scene, noMesh, invalidComplex)) ||
            invalidComplexCommands.UndoCount() != 0)
        {
            return Fail("CreateCollisionCommand accepted invalid mesh-derived target");
        }

        const auto malformedMesh = wi::ecs::CreateEntity();
        auto& mesh = scene.meshes.Create(malformedMesh);
        mesh.vertex_positions = {
            XMFLOAT3(0, 0, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1)
        };
        const auto malformedObject = wi::ecs::CreateEntity();
        scene.transforms.Create(malformedObject);
        scene.objects.Create(malformedObject).meshID = malformedMesh;
        if (renegade::bridge::CheckCollisionTarget(
                scene, malformedObject, Shape::HEIGHTFIELD) !=
            CollisionTargetStatus::InvalidHeightFieldGrid)
        {
            return Fail("Height Field accepted a non-square sample grid");
        }
    }

    const auto entity = wi::ecs::CreateEntity();
    scene.transforms.Create(entity);

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
                scene, entity, initial)))
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
        return Fail("CreateCollisionCommand did not attach full authoring state");
    }

    if (!commands.Undo() || scene.rigidbodies.Contains(entity) ||
        !scene.transforms.Contains(entity) ||
        !commands.Redo() || !scene.rigidbodies.Contains(entity))
    {
        return Fail("CreateCollisionCommand Undo/Redo lifecycle failed");
    }
    rigidbody = scene.rigidbodies.GetComponent(entity);
    if (rigidbody == nullptr)
    {
        return Fail("CreateCollisionCommand Redo did not restore component");
    }

    renegade::bridge::CommandService duplicateCommands;
    if (duplicateCommands.Execute(
            std::make_unique<renegade::bridge::CreateCollisionCommand>(
                scene, entity, initial)) ||
        duplicateCommands.UndoCount() != 0)
    {
        return Fail("CreateCollisionCommand overwrote existing rigidbody");
    }

    rigidbody->SetCharacterPhysics(true);
    rigidbody->character.gravityFactor = 2.25f;
    rigidbody->vehicle.type = Body::Vehicle::Type::Car;
    rigidbody->vehicle.wheel_radius = 0.55f;
    rigidbody->vehicle.max_engine_torque = 725.0f;

    const auto before = renegade::bridge::CaptureCollision(*rigidbody);
    auto after = before;
    after.shape = Shape::BOX;
    after.mass = 0.0f;
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
                scene, entity, before, after)))
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
        return Fail("SetCollisionCommand missed standard body fields");
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
        !rigidbody->IsStartDeactivated() ||
        !rigidbody->IsCharacterPhysics() ||
        rigidbody->vehicle.type != Body::Vehicle::Type::Car)
    {
        return Fail("SetCollisionCommand Undo did not restore prior state");
    }
    if (!editCommands.Redo() || rigidbody->shape != Shape::BOX ||
        rigidbody->mass != 0.0f || !NearlyEqual(rigidbody->buoyancy, 0.65f))
    {
        return Fail("SetCollisionCommand Redo did not restore edited state");
    }

    const auto current = renegade::bridge::CaptureCollision(*rigidbody);
    renegade::bridge::CommandService noOpCommands;
    if (noOpCommands.Execute(
            std::make_unique<renegade::bridge::SetCollisionCommand>(
                scene, entity, current, current)) ||
        noOpCommands.UndoCount() != 0)
    {
        return Fail("identical collision state polluted command history");
    }

    renegade::bridge::CommandService removeCommands;
    if (!removeCommands.Execute(
            std::make_unique<renegade::bridge::RemoveCollisionCommand>(
                scene, entity)) ||
        scene.rigidbodies.Contains(entity) ||
        !removeCommands.Undo() || !scene.rigidbodies.Contains(entity))
    {
        return Fail("RemoveCollisionCommand lifecycle failed");
    }
    rigidbody = scene.rigidbodies.GetComponent(entity);
    if (rigidbody == nullptr || rigidbody->shape != Shape::BOX ||
        !NearlyEqual(rigidbody->damping_linear, 0.44f) ||
        !NearlyEqual(rigidbody->buoyancy, 0.65f) ||
        !rigidbody->IsCharacterPhysics() ||
        !NearlyEqual(rigidbody->character.gravityFactor, 2.25f) ||
        rigidbody->vehicle.type != Body::Vehicle::Type::Car ||
        !NearlyEqual(rigidbody->vehicle.wheel_radius, 0.55f) ||
        !NearlyEqual(rigidbody->vehicle.max_engine_torque, 725.0f) ||
        !rigidbody->IsRefreshParametersNeeded())
    {
        return Fail("Remove Undo did not restore complete native state");
    }

    const auto bareEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(bareEntity);
    renegade::bridge::CommandService bareRemoveCommands;
    if (bareRemoveCommands.Execute(
            std::make_unique<renegade::bridge::RemoveCollisionCommand>(
                scene, bareEntity)) ||
        bareRemoveCommands.UndoCount() != 0)
    {
        return Fail("RemoveCollisionCommand accepted entity with no rigidbody");
    }

    const auto cylinderEntity = wi::ecs::CreateEntity();
    scene.transforms.Create(cylinderEntity);
    CollisionState cylinder;
    cylinder.shape = Shape::CYLINDER;
    cylinder.capsuleRadius = 0.4f;
    cylinder.capsuleHeight = 2.2f;
    renegade::bridge::CommandService cylinderCommands;
    if (!cylinderCommands.Execute(
            std::make_unique<renegade::bridge::CreateCollisionCommand>(
                scene, cylinderEntity, cylinder)))
    {
        return Fail("CreateCollisionCommand did not execute for Cylinder");
    }
    const auto* cylinderBody = scene.rigidbodies.GetComponent(cylinderEntity);
    if (cylinderBody == nullptr || cylinderBody->shape != Shape::CYLINDER ||
        !NearlyEqual(cylinderBody->capsule.radius, 0.4f) ||
        !NearlyEqual(cylinderBody->capsule.height, 2.2f))
    {
        return Fail("Cylinder dimensions did not reach native storage");
    }

    constexpr std::array<Shape, 3> complexShapes = {
        Shape::CONVEX_HULL,
        Shape::TRIANGLE_MESH,
        Shape::HEIGHTFIELD,
    };
    for (const auto shape : complexShapes)
    {
        const auto complexEntity = CreateMeshBackedObject(scene);
        if (!renegade::bridge::CanAuthorCollisionShape(
                scene, complexEntity, shape))
        {
            return Fail("valid mesh-backed Wicked shape was rejected");
        }
        CollisionState complex;
        complex.shape = shape;
        complex.mass = 0.0f;
        complex.meshLod = 0;
        renegade::bridge::CommandService complexCommands;
        if (!complexCommands.Execute(
                std::make_unique<renegade::bridge::CreateCollisionCommand>(
                    scene, complexEntity, complex)))
        {
            return Fail("complex Wicked collision shape could not be authored");
        }
        const auto* complexBody = scene.rigidbodies.GetComponent(complexEntity);
        if (complexBody == nullptr || complexBody->shape != shape)
        {
            return Fail("complex Wicked collision shape missed native state");
        }
    }

    std::cout << "PASS: JP01 physics façade and rigid-body authoring parity\n";
    return 0;
}
