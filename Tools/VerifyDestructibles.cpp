#include "renegade/bridge/PhysicsGameplayService.h"
#include "renegade/bridge/DestructibleBlastPreset.h"
#include "renegade/bridge/PhysicsLuaService.h"
#include "renegade/bridge/PhysicsService.h"
#include "renegade/bridge/PhysicsDiagnosticsService.h"
#include "renegade/bridge/ConstraintService.h"
#include "renegade/bridge/CompoundColliderService.h"
#include "renegade/bridge/SoftBodyPhysicsService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/PlayerViewHandAnimation.h"
#include "../Runtime/src/RuntimeProjectileAim.h"

#include <Windows.h>
#include <wiLua.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
    using wi::ecs::Entity;
    using namespace renegade::bridge;

    Entity Box(wi::scene::Scene& scene, const char* name, XMFLOAT3 position,
        XMFLOAT3 half, XMFLOAT4 color, float mass, Entity parent = wi::ecs::INVALID_ENTITY)
    {
        const auto entity = scene.Entity_CreateCube(name);
        if (parent != wi::ecs::INVALID_ENTITY)
            scene.Component_Attach(entity, parent, true);
        auto& transform = *scene.transforms.GetComponent(entity);
        transform.translation_local = position;
        transform.scale_local = half;
        transform.SetDirty();
        scene.materials.GetComponent(entity)->SetBaseColor(color);
        if (mass >= 0.f)
        {
            auto& body = scene.rigidbodies.Create(entity);
            body.shape = wi::scene::RigidBodyPhysicsComponent::BOX;
            body.box.halfextents = {1, 1, 1};
            body.mass = mass;
            body.friction = .7f;
        }
        return entity;
    }

    Entity Barricade(wi::scene::Scene& scene, const char* name, float x)
    {
        const auto entity = scene.Entity_CreateTransform(name);
        scene.transforms.GetComponent(entity)->Translate(XMFLOAT3{x, .85f, 0});
        auto& body = scene.rigidbodies.Create(entity);
        body.shape = wi::scene::RigidBodyPhysicsComponent::BOX;
        body.box.halfextents = {1.04f, .74f, .11f};
        body.mass = 0.f;
        const auto importedRoot = scene.Entity_CreateTransform("Imported model");
        scene.Component_Attach(importedRoot, entity, true);
        const auto boardGroup = scene.Entity_CreateTransform("Board group");
        scene.Component_Attach(boardGroup, importedRoot, true);
        const auto materialResource = scene.Entity_CreateMaterial("Imported resource");
        scene.Component_Attach(materialResource, importedRoot, true);
        for (int i = 0; i < 4; ++i)
        {
            const float tint = .06f * (i % 2);
            Box(scene, "Oak board", {-.78f + i * .52f, 0.f, 0.f}, {.253f, .74f, .1f},
                {.53f + tint, .29f + tint, .12f, 1.f}, -1.f, boardGroup);
        }
        return entity;
    }

    Entity ImportedBarricade(wi::scene::Scene& scene, const char* path)
    {
        auto candidate = ModelImportCandidateService().PrepareStaticModel(path);
        if (!candidate.IsReady())
        {
            std::cerr << candidate.Error() << '\n';
            return wi::ecs::INVALID_ENTITY;
        }
        auto& imported = *candidate.PeekMutableScene();
        std::vector<Entity> roots;
        for (size_t i = 0; i < imported.transforms.GetCount(); ++i)
        {
            const auto entity = imported.transforms.GetEntity(i);
            if (!imported.hierarchy.Contains(entity))
                roots.push_back(entity);
        }
        const auto wrapper = scene.Entity_CreateTransform("Heavy impact barricade");
        scene.transforms.GetComponent(wrapper)->Translate(XMFLOAT3{2.5f, .11f, 0.f});
        scene.Merge(imported);
        for (const auto root : roots)
            scene.Component_Attach(root, wrapper, true);
        auto& body = scene.rigidbodies.Create(wrapper);
        body.shape = wi::scene::RigidBodyPhysicsComponent::BOX;
        body.box.halfextents = {1.04f, .74f, .11f};
        body.local_offset = {0.f, .74f, 0.f};
        body.mass = 0.f;
        return wrapper;
    }

    void SpawnRagdoll(wi::scene::Scene& scene)
    {
        using Bone = wi::scene::HumanoidComponent::HumanoidBone;
        const auto root = scene.Entity_CreateTransform("Impact ragdoll");
        scene.transforms.GetComponent(root)->Translate(XMFLOAT3{2.5f, 7.6f, 0.f});
        auto& humanoid = scene.humanoids.Create(root);
        humanoid.SetRagdollPhysicsEnabled(true);

        const XMFLOAT4 bodyColor{.27f, .48f, .43f, 1.f};
        const XMFLOAT4 accent{.94f, .57f, .17f, 1.f};
        const auto add = [&](Bone bone, const char* name, XMFLOAT3 position,
            XMFLOAT3 shapeOffset = {}, XMFLOAT3 halfExtents = {}, bool accentPiece = false)
        {
            const auto joint = scene.Entity_CreateTransform(name);
            scene.Component_Attach(joint, root, true);
            scene.transforms.GetComponent(joint)->Translate(position);
            humanoid.bones[static_cast<size_t>(bone)] = joint;
            if (halfExtents.x > 0.f)
                Box(scene, "Mannequin shell", shapeOffset, halfExtents, accentPiece ? accent : bodyColor, -1.f, joint);
        };

        add(Bone::Hips, "Hips", {0, 0, 0}, {0, .06f, 0}, {.18f, .10f, .10f});
        add(Bone::Spine, "Spine", {0, .2f, 0}, {0, .14f, 0}, {.21f, .18f, .10f});
        add(Bone::Head, "Head", {0, .64f, 0}, {0, .025f, 0}, {.10f, .12f, .10f}, true);
        add(Bone::LeftUpperLeg, "Left thigh", {-.12f, -.04f, 0}, {0, -.18f, 0}, {.07f, .19f, .075f});
        add(Bone::RightUpperLeg, "Right thigh", {.12f, -.04f, 0}, {0, -.18f, 0}, {.07f, .19f, .075f});
        add(Bone::LeftLowerLeg, "Left shin", {-.12f, -.43f, .025f}, {0, -.16f, 0}, {.06f, .16f, .065f});
        add(Bone::RightLowerLeg, "Right shin", {.12f, -.43f, .025f}, {0, -.16f, 0}, {.06f, .16f, .065f});
        add(Bone::LeftFoot, "Left foot", {-.12f, -.80f, 0}, {0, -.025f, .07f}, {.07f, .06f, .11f}, true);
        add(Bone::RightFoot, "Right foot", {.12f, -.80f, 0}, {0, -.025f, .07f}, {.07f, .06f, .11f}, true);
        add(Bone::LeftToes, "Left toes", {-.12f, -.80f, .18f});
        add(Bone::RightToes, "Right toes", {.12f, -.80f, .18f});
        add(Bone::LeftUpperArm, "Left upper arm", {-.23f, .44f, 0}, {-.12f, 0, 0}, {.12f, .055f, .055f});
        add(Bone::RightUpperArm, "Right upper arm", {.23f, .44f, 0}, {.12f, 0, 0}, {.12f, .055f, .055f});
        add(Bone::LeftLowerArm, "Left forearm", {-.49f, .44f, 0}, {-.10f, 0, 0}, {.10f, .045f, .045f}, true);
        add(Bone::RightLowerArm, "Right forearm", {.49f, .44f, 0}, {.10f, 0, 0}, {.10f, .045f, .045f}, true);
        add(Bone::LeftHand, "Left hand", {-.71f, .44f, 0});
        add(Bone::RightHand, "Right hand", {.71f, .44f, 0});
    }

    void AddUnconfiguredCurtain(wi::scene::Scene& scene)
    {
        const auto meshEntity = scene.Entity_CreateMesh("Curtain grid");
        const auto object = scene.Entity_CreateObject("Curtain");
        const auto material = scene.Entity_CreateMaterial("Curtain cloth");
        scene.materials.GetComponent(material)->SetBaseColor({.12f, .43f, .78f, 1.f});
        scene.materials.GetComponent(material)->SetDoubleSided(true);
        scene.objects.GetComponent(object)->meshID = meshEntity;
        scene.transforms.GetComponent(object)->Translate(XMFLOAT3{.75f, .3f, 0.f});
        auto& mesh = *scene.meshes.GetComponent(meshEntity);
        constexpr uint32_t side = 9;
        for (uint32_t y = 0; y < side; ++y)
        {
            for (uint32_t x = 0; x < side; ++x)
            {
                const float row = static_cast<float>(y) / (side - 1);
                mesh.vertex_positions.push_back({static_cast<float>(x) * .18f, row * 1.8f, (1.f - row) * .72f});
                mesh.vertex_normals.push_back({0.f, 0.f, -1.f});
                if (x + 1 == side || y + 1 == side)
                    continue;
                const auto i = y * side + x;
                mesh.indices.insert(mesh.indices.end(), {i, i + side, i + 1, i + 1, i + side, i + side + 1});
            }
        }
        auto& subset = mesh.subsets.emplace_back();
        subset.materialID = material;
        subset.indexCount = static_cast<uint32_t>(mesh.indices.size());
        mesh.CreateRenderData();
    }

    bool SaveStudioAuthoringFixture(const char* output)
    {
        wi::scene::Scene scene;
        Box(scene, "Floor", {0.f, -.25f, 0.f}, {10.f, .25f, 9.f}, {.34f, .36f, .39f, 1.f}, 0.f);
        Box(scene, "Door frame", {-5.05f, 1.f, 0.f}, {.12f, 1.f, .16f}, {.46f, .35f, .23f, 1.f}, 0.f);
        Box(scene, "Door leaf", {-4.2f, 1.f, 0.f}, {.68f, .95f, .08f}, {.55f, .37f, .19f, 1.f}, -1.f);
        const auto anchor = scene.Entity_CreateTransform("Door hinge");
        scene.transforms.GetComponent(anchor)->Translate(XMFLOAT3{-4.91f, 1.f, 0.f});
        const auto sensor = Box(scene, "Sensor volume", {-1.55f, .85f, 0.f}, {.51f, .30f, .51f},
            {.15f, .71f, .38f, 1.f}, -1.f);
        scene.objects.GetComponent(sensor)->color.w = .3f;
        Box(scene, "Sensor drop", {-1.55f, 3.1f, 0.f}, {.17f, .17f, .17f}, {.8f, .3f, .1f, 1.f}, 2.f);
        AddUnconfiguredCurtain(scene);
        const auto barricade = Barricade(scene, "Barricade", 4.2f);
        scene.rigidbodies.Remove(barricade);
        Box(scene, "Barricade drop", {4.2f, 7.8f, 0.f}, {.21f, .21f, .21f}, {.8f, .3f, .1f, 1.f}, 9.f);

        TransformState spawn;
        spawn.translation = {0.f, .4f, -7.f};
        if (!CreatePlayerStartCommand(scene, spawn).Execute())
            return false;
        const auto sun = scene.Entity_CreateLight("Daylight", {0, 30, -20}, {1, .95f, .84f},
            2.1f, 100.f, wi::scene::LightComponent::DIRECTIONAL);
        scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3{1.1f, -.45f, 0.f});
        const auto sky = scene.Entity_CreateTransform("Sky");
        auto& weather = scene.weathers.Create(sky);
        weather.ambient = {.25f, .25f, .26f};
        weather.horizon = {.66f, .76f, .87f};
        scene.Update(0.f);
        wi::graphics::GetDevice()->SubmitCommandLists();
        wi::Archive archive;
        scene.Serialize(archive);
        return archive.SaveFile(output);
    }

    bool VerifyStudioAuthoredFixture(const char* input)
    {
        wi::Archive archive(input, true, false);
        if (!archive.IsOpen()) return false;
        wi::scene::Scene scene;
        scene.Serialize(archive);
        const auto door = scene.Entity_FindByName("Door leaf");
        const auto frame = scene.Entity_FindByName("Door frame");
        const auto hinge = scene.Entity_FindByName("Door hinge");
        const auto sensor = scene.Entity_FindByName("Sensor volume");
        const auto sensorDrop = scene.Entity_FindByName("Sensor drop");
        const auto curtain = scene.Entity_FindByName("Curtain");
        const auto barricade = scene.Entity_FindByName("Barricade");
        const auto* joint = scene.constraints.GetComponent(hinge);
        const auto* doorBody = scene.rigidbodies.GetComponent(door);
        if (!joint || joint->type != ConstraintState::Type::Hinge || joint->bodyA != frame ||
            joint->bodyB != door || !doorBody || doorBody->mass <= 0.f)
        {
            std::cerr << "Hinge type, body bindings or door mass did not survive reopen\n";
            return false;
        }
        wi::vector<uint32_t> pins;
        if (!GetSoftBodyPinnedVertices(scene, curtain, pins) || pins.size() != 9 ||
            !scene.rigidbodies.Contains(sensor) || !IsBodySensor(scene, sensor) ||
            !scene.rigidbodies.Contains(barricade) || GetDestructibleImpactSpeed(scene, barricade) != 6.f)
        {
            std::cerr << "Curtain pins, sensor or barricade settings did not survive reopen\n";
            return false;
        }
        PrepareDestructibles(scene);
        if (!EnableContactEvents(scene, true)) return false;
        const auto advance = [&]() {
            constexpr float dt = 1.f / 60.f;
            StepDestructibles(scene, dt);
            scene.Update(dt);
            wi::graphics::GetDevice()->SubmitCommandLists();
        };
        advance();
        advance();
        const auto startStats = SamplePhysicsDiagnostics(scene);
        const float heightBeforeStep = scene.transforms.GetComponent(sensorDrop)->GetPosition().y;
        if (!startStats.ready || !RequestPhysicsStep(scene) || RequestPhysicsStep(scene))
            return false;
        scene.Update(.15f);
        wi::graphics::GetDevice()->SubmitCommandLists();
        const auto stepped = SamplePhysicsDiagnostics(scene);
        const float heightAfterStep = scene.transforms.GetComponent(sensorDrop)->GetPosition().y;
        advance();
        const auto paused = SamplePhysicsDiagnostics(scene);
        if (wi::physics::IsSimulationEnabled() || stepped.lastUpdateSteps != 1 ||
            stepped.totalFixedSteps != startStats.totalFixedSteps + 1 ||
            paused.totalFixedSteps != stepped.totalFixedSteps || stepped.constraints == 0 ||
            stepped.awakeRigidBodies == 0 || stepped.lastUpdateMilliseconds <= 0.f ||
            heightAfterStep >= heightBeforeStep)
        {
            std::cerr << "Step Once did not advance exactly one paused fixed update\n";
            return false;
        }
        const auto restoredMarker = CaptureTransform(*scene.transforms.GetComponent(hinge));
        auto editedMarker = restoredMarker;
        editedMarker.translation.z += .15f;
        XMStoreFloat4(&editedMarker.rotation, XMQuaternionRotationRollPitchYaw(.3f, 0, 0));
        CommandService commands;
        if (!joint->physicsobject || !commands.Execute(std::make_unique<SetTransformCommand>(scene, hinge, editedMarker)) ||
            joint->physicsobject)
        {
            std::cerr << "Pivot or axis edit did not invalidate the live hinge\n";
            return false;
        }
        advance();
        if (!joint->physicsobject || !commands.Undo() || joint->physicsobject)
            return false;
        advance();
        if (!joint->physicsobject || std::abs(scene.transforms.GetComponent(hinge)->GetPosition().z -
            restoredMarker.translation.z) > .001f)
            return false;

        const auto curtainMesh = ResolveSoftBodyMeshEntity(scene, curtain);
        const auto* softBody = scene.softbodies.GetComponent(curtainMesh);
        if (!softBody || !HasLiveSoftBodyPhysics(scene, curtain)) return false;
        const auto nodeFor = [&](uint32_t vertex) {
            const auto& map = softBody->physicsToGraphicsVertexMapping;
            return static_cast<uint32_t>(std::find(map.begin(), map.end(), vertex) - map.begin());
        };
        const auto pinNode = nodeFor(76);
        const auto freeNode = nodeFor(4);
        XMFLOAT3 pinStart, freeStart, pinEnd, freeEnd;
        if (!GetSoftBodyNodePosition(scene, curtain, pinNode, pinStart) ||
            !GetSoftBodyNodePosition(scene, curtain, freeNode, freeStart) ||
            !ApplyImpulse(scene, door, {0.f, 0.f, 2.5f}))
            return false;

        float maxDoorTravel = 0.f;
        uint32_t maxContactManifolds = 0;
        bool entered = false, exited = false, broke = false;
        wi::physics::SetSimulationEnabled(true);
        for (int frameIndex = 0; frameIndex < 220; ++frameIndex)
        {
            advance();
            maxContactManifolds = std::max(maxContactManifolds, SamplePhysicsDiagnostics(scene).lastStepContactManifolds);
            maxDoorTravel = std::max(maxDoorTravel, std::abs(scene.transforms.GetComponent(door)->GetPosition().z));
            for (const auto& event : DrainContactEvents(scene).events)
            {
                if (event.sensor && ((event.bodyA == sensor && event.bodyB == sensorDrop) ||
                    (event.bodyB == sensor && event.bodyA == sensorDrop)))
                {
                    entered |= event.began;
                    exited |= !event.began;
                }
            }
            for (const auto& event : DrainDestructibleBreakEvents(scene))
                broke |= event.entity == barricade && event.pieceCount == 4;
        }
        const auto doorPosition = scene.transforms.GetComponent(door)->GetPosition();
        const auto weightPosition = scene.transforms.GetComponent(sensorDrop)->GetPosition();
        if (!GetSoftBodyNodePosition(scene, curtain, pinNode, pinEnd) ||
            !GetSoftBodyNodePosition(scene, curtain, freeNode, freeEnd))
            return false;
        const float pinShift = std::abs(pinStart.y - pinEnd.y);
        const float freeShift = std::abs(freeStart.y - freeEnd.y) + std::abs(freeStart.z - freeEnd.z);
        std::cout << "door swing=" << maxDoorTravel << ", sensor enter=" << entered << ", exit=" << exited
                  << ", weight y=" << weightPosition.y << ", curtain pin displacement=" << pinShift
                  << ", curtain free displacement=" << freeShift << ", barricade broke=" << broke << '\n';
        return maxDoorTravel > .12f && doorPosition.y > .65f && entered && exited &&
            weightPosition.y < .4f && pinShift < .005f && freeShift > .08f && broke && maxContactManifolds > 0;
    }

    bool VerifyCompoundCollider(const char* path)
    {
        const auto advance = [](wi::scene::Scene& scene) {
            // Give native creation and shape refresh a fixed update with no simulation.
            for (int i = 0; i < 2; ++i)
            {
                scene.Update(1.f / 60.f);
                wi::graphics::GetDevice()->SubmitCommandLists();
            }
        };
        const auto castAt = [](const wi::scene::Scene& scene, float x, Entity expected) {
            wi::physics::terrain_props::NativeHit hit;
            std::string error;
            if (!RaycastBodies(scene, {x, 1.f, -3.f}, {x, 1.f, 3.f}, 3, wi::ecs::INVALID_ENTITY, false, hit, error) ||
                hit.entity != expected)
            {
                std::cerr << "Compound ray at " << x << " hit " << hit.entity << ", expected " << expected
                          << ' ' << error << '\n';
                return false;
            }
            return true;
        };
        const auto empty = wi::ecs::INVALID_ENTITY;
        {
            wi::scene::Scene authored;
            const auto root = authored.Entity_CreateTransform("Compound test");
            authored.transforms.GetComponent(root)->Translate(XMFLOAT3{0.f, 1.f, 0.f});
            auto& body = authored.rigidbodies.Create(root);
            body.shape = wi::scene::RigidBodyPhysicsComponent::BOX;
            body.box.halfextents = {1.4f, .65f, .35f};
            body.mass = 0.f;
            CommandService commands;
            const auto add = [&](CompoundPartShape shape, XMFLOAT3 translation, XMFLOAT3 scale) {
                auto command = std::make_unique<AddCompoundPartCommand>(authored, root, shape);
                const auto part = command->PartEntity();
                if (!commands.Execute(std::move(command)))
                    return false;
                auto partTransform = CaptureTransform(*authored.transforms.GetComponent(part));
                partTransform.translation = translation;
                partTransform.scale = scale;
                return commands.Execute(std::make_unique<SetTransformCommand>(authored, part, partTransform));
            };
            if (!add(CompoundPartShape::Box, {-.9f, 0, 0}, {.22f, .65f, .35f}) ||
                !add(CompoundPartShape::Box, {.9f, 0, 0}, {.22f, .65f, .35f}) ||
                !commands.Undo() || !commands.Undo() || !commands.Redo() || !commands.Redo() ||
                !add(CompoundPartShape::Sphere, {1.9f, 0, 0}, {.28f, .28f, .28f}) ||
                !add(CompoundPartShape::Capsule, {-1.9f, 0, 0}, {.28f, .36f, .28f}))
                return false;
            advance(authored);
            wi::Archive saved;
            authored.Serialize(saved);
            if (!saved.SaveFile(path)) return false;
        }

        wi::Archive input(path, true, false);
        if (!input.IsOpen()) return false;
        wi::scene::Scene scene;
        scene.Serialize(input);
        advance(scene);
        const auto root = scene.Entity_FindByName("Compound test");
        const auto shapes = GetCompoundParts(scene, root);
        if (root == empty || !shapes.valid || shapes.count != 4 ||
            !castAt(scene, -.9f, root) || !castAt(scene, .9f, root) || !castAt(scene, 1.9f, root) ||
            !castAt(scene, -1.9f, root) || !castAt(scene, 0.f, empty))
            return false;

        const auto part = std::find_if(shapes.parts.begin(), shapes.parts.begin() + shapes.count,
            [](const CompoundPart& candidate) { return candidate.position.x < -.8f && candidate.position.x > -1.f; });
        if (part == shapes.parts.begin() + shapes.count)
            return false;
        auto pose = CaptureTransform(*scene.transforms.GetComponent(part->entity));
        pose.translation.x = 0.f;
        CommandService commands;
        if (!commands.Execute(std::make_unique<SetTransformCommand>(scene, part->entity, pose)))
            return false;
        advance(scene);
        if (!castAt(scene, 0.f, root) || !castAt(scene, -.9f, empty) || !commands.Undo())
            return false;
        advance(scene);
        if (!castAt(scene, 0.f, empty) || !castAt(scene, -.9f, root))
            return false;
        std::cout << "Compound collider: four persisted primitives on one body; open gap, move and Undo verified\n";
        return true;
    }

    int Fail(const char* text)
    {
        std::cerr << "FAIL: " << text << '\n';
        return 1;
    }

    struct SwapFixture
    {
        const char* asset;
        float x;
        XMFLOAT3 half;
        uint32_t pieceCount;
        float blastSpeed = 0.f;
    };

    constexpr DestructibleDebrisTiming OilDrumDebris{5.f, 1.5f};

    constexpr SwapFixture SwapFixtures[] = {
        {"Crate", -2.7f, {.74f, .75f, .74f}, 7},
        {"Barrel", 0.f, {.74f, .97f, .74f}, 7},
        {"ExplosiveBarrel", 2.7f, {.37f, .55f, .37f}, 5, StandardDestructibleBlastSpeed},
    };

    bool AddSwapProp(wi::scene::Scene& scene, const std::filesystem::path& assets, const SwapFixture& fixture)
    {
        const auto path = assets / (std::string(fixture.asset) + ".glb");
        auto candidate = ModelImportCandidateService().PrepareStaticModel(path.string());
        if (!candidate.IsReady())
        {
            std::cerr << candidate.Error() << '\n';
            return false;
        }

        auto& imported = *candidate.PeekMutableScene();
        const auto assetRoot = imported.Entity_FindByName(fixture.asset);
        const auto intact = imported.Entity_FindByName("Intact Mesh", assetRoot);
        const auto fragments = imported.Entity_FindByName("Broken Pieces", assetRoot);
        if (!imported.transforms.Contains(assetRoot) || !imported.objects.Contains(intact) ||
            !imported.transforms.Contains(fragments))
            return false;

        uint32_t pieceCount = 0;
        for (size_t i = 0; i < imported.objects.GetCount(); ++i)
        {
            const auto entity = imported.objects.GetEntity(i);
            if (entity == intact) continue;
            if (!imported.Entity_IsDescendant(entity, fragments)) return false;
            ++pieceCount;
        }
        if (pieceCount != fixture.pieceCount) return false;

        const auto wrapper = scene.Entity_CreateTransform(std::string(fixture.asset) + " Breakable");
        auto& transform = *scene.transforms.GetComponent(wrapper);
        transform.Translate(XMFLOAT3{fixture.x, 0.f, 0.f});
        scene.Merge(imported);
        scene.Component_Attach(assetRoot, wrapper, true);

        auto& body = scene.rigidbodies.Create(wrapper);
        body.shape = wi::scene::RigidBodyPhysicsComponent::BOX;
        body.box.halfextents = fixture.half;
        body.local_offset.y = fixture.half.y;
        body.mass = 0.f;
        CommandService commands;
        if (!commands.Execute(std::make_unique<SetDestructibleIntactMeshCommand>(scene, wrapper, intact)) ||
            !IsDestructibleIntactVisual(scene, intact) || !commands.Undo() ||
            IsDestructibleIntactVisual(scene, intact) || !commands.Redo())
            return false;
        if (commands.Execute(std::make_unique<SetDestructibleBlastPresetCommand>(scene, wrapper, true)) ||
            !SetDestructibleImpactSpeed(scene, wrapper, 6.f) || !CanUseDestructibleBlastPreset(scene, wrapper) ||
            ResolveAuthoredDestructibleRoot(scene, intact) != wrapper)
            return false;
        if (fixture.blastSpeed > 0.f)
        {
            if (!SetDestructibleBlastSpeed(scene, wrapper, 4.f) ||
                !commands.Execute(std::make_unique<SetDestructibleBlastPresetCommand>(scene, wrapper, false)) ||
                GetDestructibleBlastSpeed(scene, wrapper) != 0.f || !commands.Undo() ||
                GetDestructibleBlastSpeed(scene, wrapper) != 4.f || !commands.Redo() ||
                !commands.Execute(std::make_unique<SetDestructibleBlastPresetCommand>(scene, wrapper, true)) ||
                GetDestructibleBlastSpeed(scene, wrapper) != fixture.blastSpeed ||
                !commands.Undo() || GetDestructibleBlastSpeed(scene, wrapper) != 0.f || !commands.Redo())
                return false;
            if (!commands.Execute(std::make_unique<SetDestructibleDebrisTimingCommand>(scene, wrapper, OilDrumDebris)) ||
                !commands.Undo() || GetDestructibleDebrisTiming(scene, wrapper) != DestructibleDebrisTiming{} ||
                !commands.Redo())
                return false;
        }
        Box(scene, (std::string(fixture.asset) + " Impact Weight").c_str(),
            {fixture.x, 7.5f, 0.f}, {.18f, .18f, .18f}, {.58f, .19f, .08f, 1.f}, 7.f);
        return true;
    }

    bool VerifyMeleeContactTiming()
    {
        using namespace renegade::runtime;
        wi::scene::Scene scene;
        const auto makeClip = [&]() {
            const auto entity = wi::ecs::CreateEntity();
            scene.animations.Create(entity).end = 1.f;
            return entity;
        };
        RuntimePlayerHandAnimationState hands;
        hands.enabled = true;
        hands.base = makeClip();
        hands.primary[0] = {makeClip()};
        hands.primary[3] = {makeClip()};
        hands.offMovement[0] = makeClip();
        UpdateRuntimePlayerHandAnimations(scene, hands, PlayerViewAction::Idle, .2f, true, false);
        if (!hands.attacking || hands.attackContact) return false;
        UpdateRuntimePlayerHandAnimations(scene, hands, PlayerViewAction::Idle, .2f, false, false);
        if (!hands.attackContact) return false;
        UpdateRuntimePlayerHandAnimations(scene, hands, PlayerViewAction::Idle, .2f, true, false);
        return hands.attacking && !hands.attackContact;
    }

    bool RunWeaponHits(wi::scene::Scene& scene)
    {
        using namespace renegade::runtime;
        const auto sourceEntity = scene.Entity_FindByName("Weapon proof source");
        const auto crate = scene.Entity_FindByName("Crate Breakable");
        const auto barrel = scene.Entity_FindByName("Barrel Breakable");
        const ProjectileOwnerBinding owner{"weapon-proof", sourceEntity};
        const RuntimeCharacterSystemState characters;
        ProjectileSource source;
        source.ownerSubjectId = owner.subjectId;
        source.sourceAssetId = "breakables-proof";
        source.factionId = "Player";
        Entity hit = wi::ecs::INVALID_ENTITY;
        const ProjectileSegmentQuery query = [&](const auto& record, const auto& from, const auto& to) {
            return QueryProjectileSceneSegment(scene, characters, owner, record, from, to, ~0u, &hit);
        };

        const auto reject = [](const char* reason) { std::cerr << "Weapon proof: " << reason << '\n'; return false; };
        ProjectileQueryResult contact;
        std::string error;
        const XMFLOAT3 origin{SwapFixtures[0].x, .8f, -4.f};
        const XMFLOAT3 direction{0.f, 0.f, 1.f};
        if (!QueryHitscan(source, origin, direction, 8.f, query, contact, error) ||
            contact.status != ProjectileQueryStatus::Hit)
        {
            std::cerr << error << '\n';
            return reject("no hitscan contact");
        }
        ProjectileImpact firstShot;
        firstShot.contact = contact.contact;
        firstShot.damage = 10.f;
        firstShot.incomingVelocity = {0.f, 0.f, 1.f};
        if (!ApplyBreakableShot(scene, hit, sourceEntity, firstShot)) return reject("the hitscan target did not take damage");
        StepDestructibles(scene, 1.f / 60.f);
        if (!scene.rigidbodies.Contains(crate) || !DrainDestructibleBreakEvents(scene).empty())
            return reject("the crate broke before losing all hit points");

        ProjectileSimulation simulation;
        ProjectileLaunch travelling;
        travelling.source = source;
        travelling.position = ProjectileBridgeVector(origin);
        travelling.velocity = {0.f, 0.f, 20.f};
        travelling.definition.acceleration = {};
        travelling.definition.damage = 10.f;
        travelling.definition.impactProfileId = "breakables-test";
        uint64_t projectile = 0;
        std::vector<ProjectileImpact> shots;
        if (!simulation.Launch(travelling, projectile, error) ||
            !simulation.Update(.25f, query, shots, error) || shots.size() != 1)
        {
            std::cerr << error << " hits=" << shots.size() << '\n';
            return reject("travelling projectile did not hit once");
        }
        if (!ApplyBreakableShot(scene, hit, sourceEntity, shots.front()) ||
            ApplyBreakableShot(scene, hit, sourceEntity, shots.front()))
            return reject("travelling projectile damage or duplicate rejection failed");

        if (TraceBreakableMelee(scene, characters, owner, {0.f, .8f, -3.8f}, direction, 12.f))
            return reject("melee damaged the barrel from out of range");
        if (!TraceBreakableMelee(scene, characters, owner, {0.f, .8f, -1.8f}, direction, 12.f))
            return reject("melee did not reach the barrel");
        StepDestructibles(scene, 1.f / 60.f);
        if (scene.rigidbodies.Contains(crate) || !scene.rigidbodies.Contains(barrel))
            return reject("crate or barrel body did not match accumulated damage");
        if (!TraceBreakableMelee(scene, characters, owner, {0.f, .8f, -1.8f}, direction, 12.f))
            return reject("melee did not accept the second strike");
        return true;
    }

    int VerifyPropSwaps(const std::filesystem::path& assets, const char* output, bool weaponTrial)
    {
        wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
        wi::scene::Scene authored;
        const auto floor = Box(authored, "Range floor", {0.f, -.23f, 0.f}, {9.f, .23f, 9.f},
            {.23f, .26f, .29f, 1.f}, 0.f);
        authored.materials.GetComponent(floor)->SetRoughness(.9f);
        if (!SetDestructibleImpactSpeed(authored, floor, 6.f) || CanUseDestructibleBlastPreset(authored, floor) ||
            !SetDestructibleImpactSpeed(authored, floor, 0.f))
            return Fail("an ordinary collider accepted the explosion preset");
        for (const auto& fixture : SwapFixtures)
            if (!AddSwapProp(authored, assets, fixture))
                return Fail("could not bind the imported intact mesh and broken pieces");

        if (weaponTrial)
        {
            authored.Entity_CreateTransform("Weapon proof source");
            for (size_t i = 0; i < 2; ++i)
            {
                const std::string name = SwapFixtures[i].asset;
                const auto root = authored.Entity_FindByName(name + " Breakable");
                const auto weight = authored.Entity_FindByName(name + " Impact Weight");
                authored.Entity_Remove(weight);
                CommandService commands;
                if (!commands.Execute(std::make_unique<SetDestructibleHitPointsCommand>(authored, root, 20.f)) ||
                    !commands.Undo() || GetDestructibleHitPoints(authored, root) != 0.f ||
                    !commands.Redo() || !SetDestructibleImpactSpeed(authored, root, 0.f))
                    return Fail("weapon hit point authoring or Undo/Redo failed");
            }
        }

        TransformState spawn;
        spawn.translation = {0.f, .4f, -5.8f};
        if (!CreatePlayerStartCommand(authored, spawn).Execute())
            return Fail("could not author the range Player Start");
        const auto sun = authored.Entity_CreateLight("Daylight", {0, 30, -20}, {1, .95f, .84f},
            2.1f, 100.f, wi::scene::LightComponent::DIRECTIONAL);
        authored.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3{1.1f, -.45f, 0.f});
        const auto sky = authored.Entity_CreateTransform("Sky");
        auto& weather = authored.weathers.Create(sky);
        weather.ambient = {.24f, .24f, .25f};
        weather.horizon = {.66f, .76f, .87f};

        authored.Update(0.f);
        wi::graphics::GetDevice()->SubmitCommandLists();
        wi::Archive archive;
        authored.Serialize(archive);
        if (!archive.SaveFile(output)) return Fail("could not save the authored breakable props");

        wi::Archive saved(output, true, false);
        if (!saved.IsOpen()) return Fail("could not reopen the authored breakable props");
        wi::scene::Scene scene;
        scene.Serialize(saved);

        struct ReopenedProp
        {
            Entity root;
            Entity intact;
            Entity weight;
            std::vector<Entity> pieces;
        };
        std::array<ReopenedProp, std::size(SwapFixtures)> props{};
        for (size_t i = 0; i < props.size(); ++i)
        {
            const auto& fixture = SwapFixtures[i];
            auto& prop = props[i];
            prop.root = scene.Entity_FindByName(std::string(fixture.asset) + " Breakable");
            prop.intact = scene.Entity_FindByName("Intact Mesh", prop.root);
            prop.weight = scene.Entity_FindByName(std::string(fixture.asset) + " Impact Weight");
            const auto group = scene.Entity_FindByName("Broken Pieces", prop.root);
            const auto* shell = scene.objects.GetComponent(prop.intact);
            if (!shell || !shell->IsRenderable() || !IsDestructibleIntactVisual(scene, prop.intact) ||
                !scene.rigidbodies.Contains(prop.root) || !CanUseDestructibleBlastPreset(scene, prop.root) ||
                GetDestructibleImpactSpeed(scene, prop.root) != (weaponTrial && i < 2 ? 0.f : 6.f) ||
                GetDestructibleHitPoints(scene, prop.root) != (weaponTrial && i < 2 ? 20.f : 0.f) ||
                GetDestructibleBlastSpeed(scene, prop.root) != fixture.blastSpeed ||
                GetDestructibleDebrisTiming(scene, prop.root) !=
                    (fixture.blastSpeed > 0.f ? OilDrumDebris : DestructibleDebrisTiming{}))
                return Fail("intact mesh, role or impact threshold did not survive reopen");
            for (size_t j = 0; j < scene.objects.GetCount(); ++j)
            {
                const auto entity = scene.objects.GetEntity(j);
                if (!scene.Entity_IsDescendant(entity, group)) continue;
                if (scene.objects[j].IsRenderable() || scene.rigidbodies.Contains(entity))
                    return Fail("a broken piece was visible or physical before release");
                prop.pieces.push_back(entity);
            }
            if (prop.pieces.size() != fixture.pieceCount)
                return Fail("the number of bound fragments did not survive reopen");
        }

        PrepareDestructibles(scene);
        wi::physics::SetSimulationEnabled(true);
        if (weaponTrial)
        {
            scene.Update(1.f / 60.f);
            if (!VerifyMeleeContactTiming()) return Fail("an accepted swing did not emit exactly one contact");
            if (!RunWeaponHits(scene)) return Fail("confirmed shot or melee aim query failed");
        }
        constexpr int settlingFrames = 340;
        int blastFrame = -1;
        std::vector<XMFLOAT3> blasts;
        for (int frame = 0; frame < settlingFrames; ++frame)
        {
            const auto previousBlasts = blasts.size();
            StepDestructibles(scene, 1.f / 60.f, &blasts);
            if (blasts.size() > previousBlasts)
            {
                blastFrame = frame;
                if (blasts.size() != 1 || std::abs(blasts.front().x - SwapFixtures[2].x) > .01f ||
                    std::abs(blasts.front().y - SwapFixtures[2].half.y) > .01f)
                    return Fail("the blast was not emitted exactly once at the oil drum center");
                int outward = 0;
                for (const auto entity : props[2].pieces)
                {
                    XMFLOAT3 velocity;
                    if (!GetLinearVelocity(scene, entity, velocity) || velocity.y <= 0.f)
                        return Fail("an explosive fragment did not launch upward");
                    if (velocity.x * velocity.x + velocity.z * velocity.z > 9.f)
                        ++outward;
                }
                if (outward < 3)
                    return Fail("the oil drum fragments did not spread outward");
            }
            scene.Update(1.f / 60.f);
            wi::graphics::GetDevice()->SubmitCommandLists();
        }

        const auto events = DrainDestructibleBreakEvents(scene);
        if (events.size() != props.size()) return Fail("expected one break event per prop");
        if (blasts.size() != 1) return Fail("expected one blast for the oil drum");
        std::cout << "PASS: oil drum emitted one centered blast and launched fragments outward\n";
        const auto weaponSource = scene.Entity_FindByName("Weapon proof source");
        if (weaponTrial)
        {
            using namespace renegade::runtime;
            const ProjectileOwnerBinding owner{"weapon-proof", weaponSource};
            const RuntimeCharacterSystemState characters;
            ProjectileSource source;
            source.ownerSubjectId = owner.subjectId;
            ProjectileQueryResult contact;
            std::string error;
            const auto query = [&](const auto& record, const auto& from, const auto& to) {
                return QueryProjectileSceneSegment(scene, characters, owner, record, from, to);
            };
            if (!QueryHitscan(source, {SwapFixtures[0].x, 1.43f, -4.f}, {0.f, 0.f, 1.f},
                    8.f, query, contact, error) || contact.status != ProjectileQueryStatus::Miss)
                return Fail("hidden intact crate blocked a shot after release");
        }
        for (size_t i = 0; i < props.size(); ++i)
        {
            const auto& fixture = SwapFixtures[i];
            const auto& prop = props[i];
            const auto it = std::find_if(events.begin(), events.end(), [&](const auto& event) {
                const bool fromWeapon = weaponTrial && i < 2;
                return event.entity == prop.root &&
                    event.impactor == (fromWeapon ? weaponSource : prop.weight) &&
                    event.pieceCount == fixture.pieceCount &&
                    (fromWeapon ? event.impactSpeed == 0.f : event.impactSpeed >= 6.f);
            });
            if (it == events.end() || scene.rigidbodies.Contains(prop.root) ||
                scene.objects.GetComponent(prop.intact)->IsRenderable() || scene.rigidbodies.Contains(prop.intact))
                return Fail("the native impact did not remove the intact visual and collider");
            float lowest = 100.f;
            float highestContactPoint = -100.f;
            for (const auto piece : prop.pieces)
            {
                const auto* body = scene.rigidbodies.GetComponent(piece);
                const auto* object = scene.objects.GetComponent(piece);
                if (!object || !object->IsRenderable() || !body || body->mass <= 0.f ||
                    body->shape != wi::scene::RigidBodyPhysicsComponent::CONVEX_HULL ||
                    !body->physicsobject || scene.hierarchy.Contains(piece))
                    return Fail("a released fragment is missing a rendered mesh or live convex body");
                const auto* mesh = scene.meshes.GetComponent(object->meshID);
                const auto* transform = scene.transforms.GetComponent(piece);
                if (!mesh || !transform) return Fail("released fragment geometry is missing");
                const auto world = XMLoadFloat4x4(&transform->world);
                float bottom = 100.f;
                for (const auto& vertex : mesh->vertex_positions)
                    bottom = std::min(bottom, XMVectorGetY(XMVector3TransformCoord(XMLoadFloat3(&vertex), world)));
                if (!std::isfinite(bottom)) return Fail("a fragment transform is invalid");
                lowest = std::min(lowest, bottom);
                highestContactPoint = std::max(highestContactPoint, bottom);
            }
            const char* cause = weaponTrial && i < 2 ? (i == 0 ? "shot" : "melee") : "environmental impact";
            std::cout << "PASS: " << fixture.asset << " released " << prop.pieces.size()
                      << " pieces after " << cause << ", fragment bottom heights "
                      << lowest << " to " << highestContactPoint << " m\n";
            if (lowest < -.04f || highestContactPoint > .50f)
                return Fail("fragment floor contact outside the expected range");
        }

        // The authored scene was saved intact. Only the running session ages its pieces.
        const auto& drum = props[2];
        const float toFade = OilDrumDebris.staySeconds -
            static_cast<float>(settlingFrames - 1 - blastFrame) / 60.f;
        if (toFade <= 0.f)
            return Fail("debris elapsed before the settle check");
        const auto first = drum.pieces.front();
        const auto* intactOpacity = scene.objects.GetComponent(first);
        const float originalOpacity = intactOpacity->color.w;
        StepDestructibles(scene, 0.f);
        if (scene.objects.GetComponent(first)->color.w != originalOpacity)
            return Fail("zero simulation time advanced the debris timer");
        StepDestructibles(scene, toFade + OilDrumDebris.fadeSeconds * .5f);
        const auto* fading = scene.objects.GetComponent(first);
        if (!fading || std::abs(fading->color.w - originalOpacity * .5f) > .02f ||
            !scene.rigidbodies.Contains(first))
            return Fail("the debris did not fade gradually with its collider intact");
        StepDestructibles(scene, OilDrumDebris.fadeSeconds);
        for (const auto entity : drum.pieces)
            if (scene.objects.Contains(entity) || scene.rigidbodies.Contains(entity))
                return Fail("the debris mesh or collider survived cleanup");
        for (const auto entity : props[0].pieces)
            if (!scene.objects.Contains(entity) || !scene.rigidbodies.Contains(entity))
                return Fail("untimed debris was removed");
        std::cout << "PASS: oil drum debris held, faded with per-instance opacity, and removed five colliders\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    const bool weaponTrial = argc == 4 && std::string_view(argv[1]) == "--weapon-breaks";
    const bool swapTrial = argc == 4 && std::string_view(argv[1]) == "--prop-swaps";
    if (argc > 3 && !swapTrial && !weaponTrial) return 2;
    const bool ragdollTrial = argc == 3 && std::string_view(argv[2]) == "--ragdoll";
    HWND window = CreateWindowExW(0, L"STATIC", L"Destructibles proof", WS_OVERLAPPEDWINDOW,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("cannot create a DX12 proof window");
    wi::Application app;
    app.allow_hdr = false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct DrainGpu
    {
        ~DrainGpu()
        {
            while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drainGpu;
    wi::physics::SetEnabled(true);
    wi::physics::SetSimulationEnabled(false);
    if (swapTrial || weaponTrial) return VerifyPropSwaps(argv[2], argv[3], weaponTrial);
    if (argc == 3 && std::string_view(argv[1]) == "--studio-authoring-fixture")
        return SaveStudioAuthoringFixture(argv[2]) ? 0 : Fail("cannot save the raw Studio authoring fixture");
    if (argc == 3 && std::string_view(argv[1]) == "--verify-compound-collider")
        return VerifyCompoundCollider(argv[2]) ? 0 : Fail("compound collider proof failed");
    if (argc == 3 && std::string_view(argv[1]) == "--verify-studio-authoring")
        return VerifyStudioAuthoredFixture(argv[2]) ? 0 : Fail("the Studio-authored physics scene failed live verification");

    wi::scene::Scene authored;
    Box(authored, "Range floor", {0, -.25f, 0}, {9, .25f, 9}, {.26f, .28f, .29f, 1}, 0);
    const auto weak = Barricade(authored, "Light impact barricade", -2.5f);
    const auto strong = argc == 3 && !ragdollTrial ? ImportedBarricade(authored, argv[2])
                                 : Barricade(authored, "Heavy impact barricade", 2.5f);
    if (strong == wi::ecs::INVALID_ENTITY)
        return Fail("could not import the GLB barricade");
    const auto scripted = Barricade(authored, "Scripted barricade", 6.2f);
    Box(authored, "Light impact weight", {-2.5f, 2.06f, 0}, {.22f, .22f, .22f}, {.24f, .43f, .61f, 1}, 4.f);
    if (ragdollTrial)
        SpawnRagdoll(authored);
    else
        Box(authored, "Heavy impact weight", {2.5f, 8.25f, 0}, {.26f, .26f, .26f}, {.76f, .28f, .12f, 1}, 12.f);
    CommandService commands;
    if (!commands.Execute(std::make_unique<SetDestructibleImpactSpeedCommand>(authored, weak, 6.f)) ||
        !commands.Undo() || GetDestructibleImpactSpeed(authored, weak) != 0.f ||
        !commands.Redo() || GetDestructibleImpactSpeed(authored, weak) != 6.f ||
        !SetDestructibleImpactSpeed(authored, strong, 6.f) ||
        !SetDestructibleImpactSpeed(authored, scripted, 6.f))
        return Fail("authoring or Undo/Redo failed");

    TransformState spawn;
    spawn.translation = {0, .4f, -6.f};
    if (!CreatePlayerStartCommand(authored, spawn).Execute())
        return Fail("failed to place the Player Start");
    const auto sun = authored.Entity_CreateLight("Daylight", {0, 30, -20}, {1, .95f, .84f},
        2.1f, 100.f, wi::scene::LightComponent::DIRECTIONAL);
    authored.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3{1.1f, -.45f, 0});
    const auto sky = authored.Entity_CreateTransform("Sky");
    auto& weather = authored.weathers.Create(sky);
    weather.ambient = {.20f, .21f, .23f};
    weather.horizon = {.66f, .76f, .87f};
    weather.zenith = {.24f, .44f, .69f};

    authored.Update(0.f);
    wi::graphics::GetDevice()->SubmitCommandLists();
    wi::Archive archive;
    authored.Serialize(archive);
    if (argc >= 2 && !archive.SaveFile(argv[1]))
        return Fail("failed to write the playtest scene");
    archive.SetReadModeAndResetPos(true);
    wi::scene::Scene scene;
    scene.Serialize(archive);
    const auto weakRoot = scene.Entity_FindByName("Light impact barricade");
    const auto strongRoot = scene.Entity_FindByName("Heavy impact barricade");
    const auto scriptRoot = scene.Entity_FindByName("Scripted barricade");
    const auto weight = scene.Entity_FindByName("Heavy impact weight");
    const auto lightWeight = scene.Entity_FindByName("Light impact weight");
    if (GetDestructibleImpactSpeed(scene, strongRoot) != 6.f)
        return Fail("threshold did not survive save/reopen");

    PrepareDestructibles(scene);
    if (!EnableContactEvents(scene, false))
        return Fail("could not check independent contact subscriptions");
    const auto step = [&]
    {
        StepDestructibles(scene, 1.f / 60.f);
        scene.Update(1.f / 60.f);
        wi::graphics::GetDevice()->SubmitCommandLists();
    };
    step();
    const auto ray = [&](float x, Entity expected)
    {
        wi::physics::terrain_props::NativeHit hit;
        std::string error;
        return RaycastBodies(scene, {x, 1.15f, -1}, {x, 1.15f, 1}, UINT32_MAX,
            wi::ecs::INVALID_ENTITY, false, hit, error) && hit.entity == expected;
    };
    if (!ray(2.5f, strongRoot))
        return Fail("intact barricade did not block the path");

    Entity weakPiece = wi::ecs::INVALID_ENTITY;
    for (size_t i = 0; i < scene.hierarchy.GetCount(); ++i)
    {
        const auto entity = scene.hierarchy.GetEntity(i);
        if (!scene.objects.Contains(entity))
            continue;
        auto parent = entity;
        while (const auto* node = scene.hierarchy.GetComponent(parent))
        {
            parent = node->parentID;
            if (parent == weakRoot)
            {
                weakPiece = entity;
                break;
            }
        }
        if (weakPiece != wi::ecs::INVALID_ENTITY)
            break;
    }
    const auto* board = scene.transforms.GetComponent(weakPiece);
    if (!board)
        return Fail("rattle fixture has no visible board");
    const float restY = board->GetPosition().y;
    float minOffset = 0.f, maxOffset = 0.f;
    wi::physics::SetSimulationEnabled(true);
    for (int i = 0; i < 160; ++i)
    {
        step();
        const float offset = board->GetPosition().y - restY;
        minOffset = std::min(minOffset, offset);
        maxOffset = std::max(maxOffset, offset);
        if (std::abs(offset) > .001f && !ray(-2.5f, weakRoot))
            return Fail("the intact collider moved during the rattle");
    }
    if (minOffset > -.01f || maxOffset < .005f || std::abs(board->GetPosition().y - restY) > .0001f)
    {
        std::cerr << "Board displacement " << minOffset << " to " << maxOffset
                  << " m, final offset " << board->GetPosition().y - restY << " m\n";
        return Fail("the light hit did not visibly rattle and return to rest");
    }
    const auto events = DrainDestructibleBreakEvents(scene);
    if (events.size() != 1 || events.front().entity != strongRoot ||
        events.front().pieceCount != 4 || events.front().impactSpeed < 6.f)
        return Fail("native strong/weak impact did not yield exactly one four-piece break");
    if (ragdollTrial)
    {
        const auto actor = scene.Entity_FindByName("Impact ragdoll");
        const auto* humanoid = scene.humanoids.GetComponent(actor);
        const auto impactor = events.front().impactor;
        if (!humanoid || !humanoid->ragdoll || impactor == wi::ecs::INVALID_ENTITY ||
            std::find(std::begin(humanoid->bones), std::end(humanoid->bones), impactor) == std::end(humanoid->bones))
            return Fail("the reported impactor was not a live ragdoll bone");
        const auto* part = scene.names.GetComponent(impactor);
        std::cout << "Native ragdoll contact: " << (part ? part->name : "bone") << '\n';
    }
    else if (events.front().impactor != weight)
        return Fail("the reported impactor was not the heavy weight");
    const float lightWeightY = wi::physics::GetPosition(*scene.rigidbodies.GetComponent(lightWeight)).y;
    if (lightWeightY < 1.75f || lightWeightY > 1.95f)
        return Fail("the light impact weight did not land on the intact barricade");
    if (!scene.rigidbodies.Contains(weakRoot) || scene.rigidbodies.Contains(strongRoot) ||
        !ray(-2.5f, weakRoot) || !ray(2.5f, wi::ecs::INVALID_ENTITY))
        return Fail("native path did not clear while the weak barricade remained solid");

    auto state = std::unique_ptr<lua_State, decltype(&lua_close)>(luaL_newstate(), lua_close);
    if (!state || !BindPhysicsLua(scene, state.get()))
        return Fail("could not bind physics Lua");
    luaL_openlibs(state.get());
    lua_pushinteger(state.get(), static_cast<lua_Integer>(scriptRoot));
    lua_setglobal(state.get(), "barricade");
    const int requested = luaL_dostring(state.get(),
        "assert(renegade.physics.break_destructible(barricade))");
    step();
    const int verified = requested == LUA_OK ? luaL_dostring(state.get(),
        "local e = renegade.physics.drain_destructible_breaks(); "
        "assert(#e == 1 and e[1].entity == barricade and e[1].piece_count == 4); "
        "assert(not renegade.physics.break_destructible(barricade))") : requested;
    if (verified != LUA_OK)
        std::cerr << lua_tostring(state.get(), -1) << '\n';
    UnbindPhysicsLua(&scene);
    if (verified != LUA_OK)
        return Fail("Lua explicit break or event failed");

    std::cout << "PASS: save/reopen, Undo/Redo, light hit rattled from " << minOffset << " to " << maxOffset
        << " m and settled with intact collision, strong impact ("
        << events.front().impactSpeed << " m/s) released four pieces and cleared the path, explicit Lua break\n";
    return 0;
}
