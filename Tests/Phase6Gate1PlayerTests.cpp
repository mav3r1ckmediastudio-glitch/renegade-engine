#include "renegade/bridge/PlayerService.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    bool Near(const float left, const float right, const float epsilon = 0.001f)
    {
        return std::abs(left - right) <= epsilon;
    }

    [[noreturn]] void Fail(const std::string& message)
    {
        std::cerr << "PHASE6 GATE1 PLAYER FAIL // " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    using namespace renegade::bridge;

    wi::scene::Scene scene;

    // Wicked's broad Player preset is classification, not spawn authority.
    const auto genericPlayer = scene.Entity_CreateTransform("Generic Player");
    scene.metadatas.Create(genericPlayer).preset =
        wi::scene::MetadataComponent::Preset::Player;
    if (ResolvePlayerStart(scene).resolution != PlayerStartResolution::Missing)
        Fail("generic Player metadata was mistaken for Player Start authority");

    TransformState pose;
    pose.translation = XMFLOAT3(4.0f, 2.0f, -8.0f);
    XMStoreFloat4(
        &pose.rotation,
        XMQuaternionRotationRollPitchYaw(0.0f, XM_PIDIV2, 0.0f));

    CommandService commands;
    auto create = std::make_unique<CreatePlayerStartCommand>(scene, pose);
    auto* createRaw = create.get();
    if (!commands.Execute(std::move(create)))
        Fail("command-backed Player Start creation failed");
    const auto startEntity = createRaw->CreatedEntity();
    const auto resolved = ResolvePlayerStart(scene);
    if (resolved.resolution != PlayerStartResolution::Success ||
        resolved.start.entity != startEntity ||
        !Near(resolved.start.transform.translation.x, 4.0f) ||
        !Near(resolved.start.transform.translation.y, 2.0f) ||
        !Near(resolved.start.transform.translation.z, -8.0f))
    {
        Fail("created Player Start did not resolve with its authored pose");
    }

    const auto* metadata = scene.metadatas.GetComponent(startEntity);
    if (metadata == nullptr ||
        metadata->preset != wi::scene::MetadataComponent::Preset::Player ||
        !metadata->string_values.has(PlayerStartMetadataKey))
    {
        Fail("Player Start did not use governed native Wicked metadata");
    }

    CommandService duplicateCommands;
    if (duplicateCommands.Execute(
            std::make_unique<CreatePlayerStartCommand>(scene, pose)))
    {
        Fail("a second Player Start was not rejected");
    }
    if (!commands.Undo() ||
        ResolvePlayerStart(scene).resolution != PlayerStartResolution::Missing ||
        !commands.Redo() ||
        ResolvePlayerStart(scene).start.entity != startEntity)
    {
        Fail("Player Start Undo/Redo did not preserve entity identity");
    }

    wi::Archive snapshot;
    snapshot.SetReadModeAndResetPos(false);
    wi::ecs::EntitySerializer writer;
    scene.Entity_Serialize(snapshot, writer, startEntity);
    snapshot.SetReadModeAndResetPos(true);
    wi::scene::Scene reopened;
    wi::ecs::EntitySerializer reader;
    reader.allow_remap = false;
    const auto reopenedEntity = reopened.Entity_Serialize(snapshot, reader);
    const auto reopenedStart = ResolvePlayerStart(reopened);
    if (reopenedEntity != startEntity ||
        reopenedStart.resolution != PlayerStartResolution::Success ||
        !Near(reopenedStart.start.transform.translation.x, 4.0f))
    {
        Fail("Player Start did not survive native Wicked serialization");
    }

    RuntimePlayerState runtime;
    std::string error;
    if (!SpawnRuntimePlayer(
            reopened,
            reopenedStart.start,
            runtime,
            error))
    {
        Fail("Runtime player spawn failed: " + error);
    }
    const auto* body = reopened.rigidbodies.GetComponent(runtime.entity);
    const auto* runtimeTransform = reopened.transforms.GetComponent(runtime.entity);
    if (body == nullptr || runtimeTransform == nullptr ||
        !body->IsCharacterPhysics() ||
        body->shape !=
            wi::scene::RigidBodyPhysicsComponent::CollisionShape::CAPSULE ||
        !Near(body->capsule.radius, 0.4f) ||
        !Near(body->capsule.height, 0.5f) ||
        !Near(runtimeTransform->translation_local.x, 4.0f))
    {
        Fail("Runtime player did not use the accepted Wicked character capsule");
    }

    PlayerInputFrame input;
    input.moveForward = 1.0f;
    auto motion = EvaluatePlayerInput(input, 0.0f, 0.0f);
    if (!Near(motion.direction.x, 0.0f) ||
        !Near(motion.direction.z, 1.0f) ||
        !Near(motion.speed, 4.5f))
    {
        Fail("forward action did not produce camera-relative movement");
    }

    input.sprintDown = true;
    input.jumpPressed = true;
    input.lookYaw = XM_PIDIV2;
    input.lookPitch = XM_PI;
    motion = EvaluatePlayerInput(input, 0.0f, 0.0f);
    if (!Near(motion.direction.x, 1.0f) ||
        !Near(motion.direction.z, 0.0f) ||
        !Near(motion.speed, 7.5f) ||
        !Near(motion.jump, 5.0f) ||
        motion.pitch > wi::math::DegreesToRadians(89.0f) + 0.001f)
    {
        Fail("look/sprint/jump action evaluation is not bounded and deterministic");
    }

    // No Jolt body exists in this headless fixture until Wicked advances the
    // Scene. Gate 1 must fail safely rather than inventing a parallel body.
    if (UpdateRuntimePlayer(reopened, runtime, input))
        Fail("headless Runtime player bypassed Wicked live-body ownership");

    const auto runtimeEntity = runtime.entity;
    DespawnRuntimePlayer(reopened, runtime);
    if (runtime.IsSpawned() || reopened.transforms.Contains(runtimeEntity))
        Fail("Runtime-only player did not despawn cleanly");

    std::cout << "PASS: Phase 6 Gate 1 Player Start and Runtime possession\n";
    return EXIT_SUCCESS;
}
