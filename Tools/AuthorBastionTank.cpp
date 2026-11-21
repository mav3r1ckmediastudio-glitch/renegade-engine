#include <WickedEngine.h>
#include <Windows.h>
#include <ModelImporter.h>

#include "../Runtime/src/RuntimeTrackedVehicle.h"
#include "renegade/bridge/PlayerService.h"
#include <cmath>
#include <iostream>

namespace wi::physics
{
    void RunPhysicsUpdateSystem(wi::jobsystem::context& context, wi::scene::Scene& scene, float dt);
}

namespace
{
    using wi::scene::Scene;
    using wi::ecs::Entity;
    constexpr char ChassisName[] = "Bastion T9 Chassis";
    constexpr char ModelRoot[] = "Bastion_T9_Root";

    Entity CreateCollider(Scene& scene, const char* name, XMFLOAT3 position, XMFLOAT3 extent, float mass)
    {
        const auto entity = scene.Entity_CreateTransform(name);
        scene.transforms.GetComponent(entity)->Translate(position);
        auto& body = scene.rigidbodies.Create(entity);
        body.shape = wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
        body.box.halfextents = extent;
        body.mass = mass;
        body.friction = .9f;
        return entity;
    }

    void Paint(Scene& scene, const char* name, XMFLOAT3 position, XMFLOAT3 extent, XMFLOAT4 colour)
    {
        const auto entity = scene.Entity_CreateCube(name);
        auto& pose = *scene.transforms.GetComponent(entity);
        pose.Scale(extent);
        pose.Translate(position);
        auto& material = *scene.materials.GetComponent(entity);
        material.SetBaseColor(colour);
        material.SetRoughness(.88f);
    }

    bool CheckDrive(Scene& scene, Entity chassis)
    {
        using namespace renegade::bridge;
        renegade::runtime::RuntimeTrackedVehicle rig;
        if (!rig.Register(scene, chassis)) return false;
        wi::physics::SetSimulationEnabled(true);
        wi::jobsystem::context jobs;
        const auto run = [&](float forward, float turn, float brake, int steps)
        {
            if (!DriveTrackedVehicle(scene, chassis, forward, turn, brake)) return false;
            for (int i = 0; i < steps; ++i)
            {
                wi::physics::RunPhysicsUpdateSystem(jobs, scene, 1.f / 60.f);
                wi::jobsystem::Wait(jobs);
            }
            return true;
        };
        if (!run(0, 0, 1, 100) || !ReadTrackedVehicle(scene, chassis).ready) return false;
        auto& body = *scene.rigidbodies.GetComponent(chassis);
        const auto origin = wi::physics::GetPosition(body);
        const auto parked = ReadTrackedVehicle(scene, chassis);
        run(1, 0, 0, 240);
        const auto straight = wi::physics::GetPosition(body);
        const auto driven = ReadTrackedVehicle(scene, chassis);
        run(0, 0, 1, 150);
        const auto stopped = wi::physics::GetPosition(body);
        const auto stoppedVelocity = wi::physics::GetVelocity(body);
        run(-1, 0, 0, 200);
        const auto reversed = wi::physics::GetPosition(body);
        run(0, 0, 1, 150);
        const auto beforePivot = wi::physics::GetPosition(body);
        // Sample before a full pivot wraps past the opposite heading.
        run(0, 1, 0, 65);
        const auto afterPivot = wi::physics::GetPosition(body);
        const auto turn = ReadTrackedVehicle(scene, chassis);
        const auto orientation = wi::physics::GetRotation(body);
        const float directionX = XMVectorGetX(XMMatrixRotationQuaternion(XMLoadFloat4(&orientation)).r[2]);
        const float drift = std::hypot(afterPivot.x-beforePivot.x, afterPivot.z-beforePivot.z);
        std::cout << "Grounded wheels " << parked.contacts << " height " << origin.y
                  << " | forward " << straight.z-origin.z << " m tracks "
                  << driven.trackSpeed[0] << ", " << driven.trackSpeed[1] << '\n';
        std::cout << "Brake speed " << std::hypot(stoppedVelocity.x, stoppedVelocity.z)
                  << " | reverse " << reversed.z-stopped.z << " m\n";
        std::cout << "Pivot heading X " << directionX << " drift " << drift << " m tracks "
                  << turn.trackSpeed[0] << ", " << turn.trackSpeed[1] << '\n';
        return parked.contacts >= 8 && straight.z-origin.z > 3.f &&
            driven.trackSpeed[0] * driven.trackSpeed[1] > 1.f &&
            std::hypot(stoppedVelocity.x, stoppedVelocity.z) < .5f &&
            reversed.z < stopped.z-2.f && turn.trackSpeed[0] * turn.trackSpeed[1] < -1.f &&
            directionX > .2f && drift < 2.f;
    }
}

int main(int argc, char** argv)
{
    if (argc != 3 && !(argc == 4 && std::string(argv[3]) == "--tuning-proof"))
    {
        std::cerr << "Usage: RenegadeAuthorBastionTank <model.glb> <range.wiscene> [--tuning-proof]\n";
        return 2;
    }
    const auto window = CreateWindowExW(0, L"STATIC", L"Bastion tank authoring",
        WS_OVERLAPPEDWINDOW, 0, 0, 960, 640, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    wi::Application app;
    app.allow_hdr = false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain
    {
        ~Drain()
        {
            while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;

    using namespace renegade::bridge;
    wi::physics::SetEnabled(true);
    wi::physics::SetSimulationEnabled(false);
    Scene scene;
    ImportModel_GLTF(argv[1], scene);
    const auto model = scene.Entity_FindByName(ModelRoot);
    if (!scene.transforms.Contains(model)) return 3;
    const auto chassis = CreateCollider(scene, ChassisName, {0, 1.09f, 0}, {1.70f, .50f, 3.20f}, 4200.f);
    scene.Component_Attach(model, chassis, true);
    scene.transforms.GetComponent(model)->RotateRollPitchYaw(XMFLOAT3(0, XM_PI, 0));
    scene.metadatas.Create(chassis).bool_values.set(TrackedVehicleKey, true);

    CreateCollider(scene, "Range ground collision", {0, -.5f, 0}, {450, .5f, 450}, 0.f);
    Paint(scene, "Range", {0, -.23f, 0}, {450, .17f, 450}, {.30f, .32f, .22f, 1});
    Paint(scene, "Trial lane", {0, -.065f, 27}, {10, .04f, 56}, {.37f, .36f, .31f, 1});
    const XMFLOAT4 paint(.82f, .77f, .57f, 1);
    for (float z : {-9.f, 12.f, 28.f, 44.f, 60.f})
        for (float x : {-7.f, 7.f})
            Paint(scene, "Lane marker", {x, -.024f, z}, {.16f, .005f, 1.2f}, paint);
    for (float x : {-15.f, 15.f})
        for (float z : {5.f, 26.f, 55.f})
        {
            Paint(scene, "Range bollard", {x, .48f, z}, {.45f, .48f, .45f}, {.51f, .35f, .17f, 1});
            CreateCollider(scene, "Bollard collision", {x, .48f, z}, {.45f, .48f, .45f}, 0.f);
        }

    TransformState spawn;
    spawn.translation = {-3.15f, .37f, .4f};
    CreatePlayerStartCommand player(scene, spawn);
    if (!player.Execute()) return 4;
    const auto sun = scene.Entity_CreateLight("Daylight", {0, 80, -20}, {1, .96f, .86f}, 2.6f, 200,
        wi::scene::LightComponent::DIRECTIONAL);
    scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3(1.1f, -.67f, 0));
    const auto sky = scene.Entity_CreateTransform("Daylight Sky");
    auto& weather = scene.weathers.Create(sky);
    weather.ambient = {.21f, .22f, .24f};
    weather.horizon = {.65f, .74f, .81f};
    weather.zenith = {.23f, .41f, .62f};

    const auto tuningField = std::find_if(std::begin(TrackedTuningFields), std::end(TrackedTuningFields),
        [](const auto& field) { return field.member == &TrackedVehicleTuning::engineTorque; });
    if (tuningField == std::end(TrackedTuningFields)) return 9;
    const bool tuningProof = argc == 4;
    if (tuningProof && !WriteTrackedTuningValue(scene, chassis, *tuningField, 350.f)) return 9;
    scene.Update(0.f);
    wi::graphics::GetDevice()->SubmitCommandLists();
    wi::Archive output;
    scene.Serialize(output);
    if (!output.SaveFile(argv[2])) return 5;
    Scene reopened;
    wi::Archive input(argv[2], true, false);
    if (!input.IsOpen()) return 6;
    reopened.Serialize(input);
    const auto saved = reopened.Entity_FindByName(ChassisName);
    renegade::runtime::RuntimeTrackedVehicle savedRig;
    if (!savedRig.Register(reopened, saved) ||
        ResolvePlayerStart(reopened).resolution != PlayerStartResolution::Success)
        return 7;
    if (tuningProof)
    {
        if (ReadTrackedTuning(reopened, saved).engineTorque != 350.f ||
            !WriteTrackedTuningValue(scene, chassis, *tuningField, std::nullopt) ||
            ReadTrackedTuning(scene, chassis).engineTorque != DefaultTrackedVehicleTuning.engineTorque)
            return 9;
        std::cout << "Tracked tuning persisted through save/reopen and reset to the default.\n";
    }
    if (!CheckDrive(scene, chassis)) return 8;
    std::cout << "Bastion GLB import, scene save/reopen and Jolt drive passed.\n";
}
