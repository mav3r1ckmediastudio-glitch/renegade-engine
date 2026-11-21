#include <WickedEngine.h>
#include <Windows.h>
#include <ModelImporter.h>

#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/PlayerService.h"

#include <cmath>
#include <iostream>

namespace wi::physics
{
    void RunPhysicsUpdateSystem(wi::jobsystem::context& context, wi::scene::Scene& scene, float dt);
}

namespace
{
    using wi::ecs::Entity;
    using wi::scene::Scene;
    using RigidBody = wi::scene::RigidBodyPhysicsComponent;

    constexpr char ChassisName[] = "Vesper V9 Chassis";
    constexpr char ModelRoot[] = "Vesper_V9_Root";
    constexpr float ChassisHeight = 1.54f;

    Entity CreateBox(Scene& scene, const char* name, XMFLOAT3 position, XMFLOAT3 halfSize, float mass)
    {
        const auto entity = scene.Entity_CreateTransform(name);
        scene.transforms.GetComponent(entity)->Translate(position);
        auto& body = scene.rigidbodies.Create(entity);
        body.shape = RigidBody::CollisionShape::BOX;
        body.box.halfextents = halfSize;
        body.mass = mass;
        body.friction = .85f;
        return entity;
    }

    void Paint(Scene& scene, const char* name, XMFLOAT3 position, XMFLOAT3 halfSize, XMFLOAT4 colour)
    {
        const auto entity = scene.Entity_CreateCube(name);
        auto& transform = *scene.transforms.GetComponent(entity);
        transform.Scale(halfSize);
        transform.Translate(position);
        auto& material = *scene.materials.GetComponent(entity);
        material.SetBaseColor(colour);
        material.SetRoughness(.82f);
    }

    bool HasParts(Scene& scene)
    {
        for (const auto* name : {ModelRoot, "REF_Pilot_Eye", "Nacelle_Tilt_L", "Nacelle_Tilt_R",
            "REF_Ground_Nose", "REF_Ground_Main_L", "REF_Ground_Main_R"})
        {
            if (!scene.transforms.Contains(scene.Entity_FindByName(name)))
            {
                std::cerr << "Missing Vesper node " << name << '\n';
                return false;
            }
        }
        return true;
    }

    bool CheckFlight(Scene& scene, Entity chassis)
    {
        using namespace renegade::bridge;
        wi::physics::SetSimulationEnabled(true);
        wi::jobsystem::context jobs;
        const auto step = [&] {
            wi::physics::RunPhysicsUpdateSystem(jobs, scene, 1.f / 60.f);
            wi::jobsystem::Wait(jobs);
        };
        for (int tick = 0; tick < 90; ++tick) step();

        auto& body = *scene.rigidbodies.GetComponent(chassis);
        if (!body.physicsobject || !CommandPhysicalVtol(scene, chassis, {}))
            return false;
        const auto run = [&](FlightVehicleInput control, int ticks) {
            (void)CommandPhysicalVtol(scene, chassis, control);
            for (int tick = 0; tick < ticks; ++tick) step();
        };

        const auto ground = wi::physics::GetPosition(body);
        run({}, 30);
        if (!IsPhysicalVtolGrounded(scene, chassis))
            return false;
        run({0, 0, 1}, 180);
        const auto climb = wi::physics::GetPosition(body);
        run({}, 180);
        const auto hover = wi::physics::GetPosition(body);
        const auto hoverVelocity = wi::physics::GetVelocity(body);
        std::cout << "Takeoff and hover: ground=" << ground.y << " climb=" << climb.y
                  << " hold=" << hover.y << " vertical speed=" << hoverVelocity.y << '\n';
        const bool hovered = climb.y > ground.y + 6.f && std::abs(hover.y - climb.y) < 1.5f &&
            std::abs(hoverVelocity.y) < .3f;

        const auto attitude = [&] {
            const auto q = wi::physics::GetRotation(body);
            return XMMatrixRotationQuaternion(XMLoadFloat4(&q));
        };
        run({0, 1, 0}, 75);
        const auto pivot = wi::physics::GetPosition(body);
        const float turn = XMVectorGetX(attitude().r[2]);
        const float drift = std::hypot(pivot.x - hover.x, pivot.z - hover.z);
        const float pivotTilt = std::hypot(XMVectorGetX(attitude().r[1]), XMVectorGetZ(attitude().r[1]));
        std::cout << "Hover pivot: heading X=" << turn << " drift=" << drift << " tilt=" << pivotTilt << '\n';
        const bool pivoted = turn > .3f && drift < .75f && pivotTilt < .12f;

        run({1, 0, 0}, 300);
        const auto straight = wi::physics::GetVelocity(body);
        run({1, 1, 0}, 100);
        const auto bent = wi::physics::GetVelocity(body);
        const auto pose = attitude();
        const float bank = XMVectorGetY(pose.r[0]);
        const float courseChange = std::abs(straight.x * bent.z - straight.z * bent.x);
        std::cout << "Moving turn: roll sine=" << bank << " course cross=" << courseChange << '\n';
        const bool banked = bank < -.08f && courseChange > 3.f;

        run({0, 0, -1}, 330);
        run({}, 150);
        const auto landed = wi::physics::GetPosition(body);
        const auto landingVelocity = wi::physics::GetVelocity(body);
        std::cout << "Landing: altitude=" << landed.y << " speed="
                  << std::hypot(landingVelocity.x, landingVelocity.y, landingVelocity.z) << '\n';
        return hovered && pivoted && banked && IsPhysicalVtolGrounded(scene, chassis) &&
            std::abs(landed.y - ground.y) < .10f &&
            std::hypot(landingVelocity.x, landingVelocity.y, landingVelocity.z) < .5f &&
            CommandPhysicalVtol(scene, chassis, {}, false);
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: AuthorVesperVtol <vesper.glb> <pad.wiscene>\n";
        return 2;
    }

    const auto window = CreateWindowExW(0, L"STATIC", L"Vesper VTOL authoring",
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

    wi::physics::SetEnabled(true);
    wi::physics::SetSimulationEnabled(false);

    Scene scene;
    ImportModel_GLTF(argv[1], scene);
    if (!HasParts(scene)) return 3;

    const auto root = scene.Entity_FindByName(ModelRoot);
    const auto chassis = CreateBox(scene, ChassisName, {0, ChassisHeight + .02f, 0},
        {1.08f, ChassisHeight, 3.05f}, 2250.f);
    scene.Component_Attach(root, chassis, true);
    auto& visual = *scene.transforms.GetComponent(root);
    visual.Translate(XMFLOAT3{0, -ChassisHeight, -.16f});
    visual.RotateRollPitchYaw(XMFLOAT3{0, XM_PI, 0});
    for (const auto* name : {"Nacelle_Tilt_L", "Nacelle_Tilt_R"})
        scene.transforms.GetComponent(scene.Entity_FindByName(name))->RotateRollPitchYaw(XMFLOAT3{XM_PIDIV2, 0, 0});

    using namespace renegade::bridge;
    if (!SetPlayerVehicleMode(scene, chassis, PlayerVehicleMode::PhysicalVTOL)) return 4;

    const XMFLOAT4 asphalt{.16f, .19f, .22f, 1};
    const XMFLOAT4 marking{.82f, .85f, .87f, 1};
    const XMFLOAT4 yellow{.91f, .55f, .10f, 1};
    Paint(scene, "Flight range", {0, -.18f, 0}, {700, .17f, 700}, {.27f, .34f, .20f, 1});
    Paint(scene, "VTOL apron", {0, -.04f, 0}, {70, .04f, 70}, asphalt);
    for (const float edge : {-12.f, 12.f})
    {
        Paint(scene, "Landing pad side", {edge, .003f, 0}, {.13f, .003f, 12}, yellow);
        Paint(scene, "Landing pad end", {0, .003f, edge}, {12, .003f, .13f}, yellow);
        Paint(scene, "Pad designation", {edge * .23f, .004f, -7}, {.25f, .003f, 2.0f}, marking);
    }
    Paint(scene, "Pad designation crossbar", {0, .004f, -7}, {2.75f, .003f, .25f}, marking);
    for (int z = 18; z < 64; z += 9)
        Paint(scene, "Departure line", {0, .004f, float(z)}, {.16f, .003f, 2.3f}, marking);
    CreateBox(scene, "Flight range collision", {0, -.5f, 0}, {700, .5f, 700}, 0.f);

    TransformState spawn;
    spawn.translation = {-3.35f, .37f, .5f};
    CreatePlayerStartCommand player(scene, spawn);
    if (!player.Execute()) return 5;

    const auto sun = scene.Entity_CreateLight("Daylight", {0, 80, -20}, {1, .96f, .86f}, 2.5f, 200,
        wi::scene::LightComponent::DIRECTIONAL);
    scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3{1.1f, -.67f, 0});
    const auto sky = scene.Entity_CreateTransform("Daylight Sky");
    auto& weather = scene.weathers.Create(sky);
    weather.ambient = {.21f, .22f, .24f};
    weather.horizon = {.67f, .76f, .87f};
    weather.zenith = {.24f, .43f, .68f};

    scene.Update(0);
    wi::graphics::GetDevice()->SubmitCommandLists();

    wi::Archive output;
    scene.Serialize(output);
    if (!output.SaveFile(argv[2])) return 6;

    Scene reopened;
    wi::Archive input(argv[2], true, false);
    if (!input.IsOpen()) return 7;
    reopened.Serialize(input);
    const auto savedChassis = reopened.Entity_FindByName(ChassisName);
    if (!HasParts(reopened) || !IsPhysicalVtol(reopened, savedChassis) ||
        ResolvePlayerStart(reopened).resolution != PlayerStartResolution::Success)
        return 8;
    std::cout << "Native GLB import and project save/reopen passed.\n";

    if (!CheckFlight(scene, chassis)) return 9;
    std::cout << "Vesper native lift, hover, pivot, bank, course change and landing passed.\n";
}
