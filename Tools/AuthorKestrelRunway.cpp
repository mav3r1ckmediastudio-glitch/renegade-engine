#include <WickedEngine.h>
#include <Windows.h>
#include <ModelImporter.h>
#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/PlayerService.h"
#include "../Runtime/src/RuntimeFixedWing.h"

namespace wi::physics
{
    void RunPhysicsUpdateSystem(wi::jobsystem::context& context, wi::scene::Scene& scene, float dt);
}

#include <array>
#include <cmath>
#include <iostream>

namespace
{
    using Body = wi::scene::RigidBodyPhysicsComponent;
    constexpr float CentreHeight = 1.43f;
    constexpr float CentreForward = .24f;
    constexpr const char* ChassisName = "Kestrel T7 Chassis";
    constexpr std::array<const char*, 12> RequiredParts{
        "Aileron_L", "Aileron_R", "Elevator", "Rudder", "Propeller_Spin",
        "Wheel_Main_L", "Wheel_Main_R", "Wheel_Nose", "Nose_Steer",
        "REF_WheelContact_Main_L", "REF_WheelContact_Main_R", "REF_WheelContact_Nose"};
    constexpr char ModelRoot[] = "Kestrel_T7_Root";

    void Place(wi::scene::Scene& scene, wi::ecs::Entity entity,
        XMFLOAT3 position, XMFLOAT3 scale = {1, 1, 1})
    {
        auto& transform = *scene.transforms.GetComponent(entity);
        transform.translation_local = position;
        transform.scale_local = scale;
        transform.SetDirty();
    }

    wi::ecs::Entity CreateBody(wi::scene::Scene& scene, const char* name,
        XMFLOAT3 position, XMFLOAT3 halfExtents, float mass)
    {
        const auto entity = scene.Entity_CreateTransform(name);
        Place(scene, entity, position);
        auto& body = scene.rigidbodies.Create(entity);
        body.shape = Body::CollisionShape::BOX;
        body.box.halfextents = halfExtents;
        body.mass = mass;
        body.friction = .8f;
        return entity;
    }

    void Paint(wi::scene::Scene& scene, const char* name, XMFLOAT3 position,
        XMFLOAT3 halfExtents, XMFLOAT4 colour)
    {
        const auto entity = scene.Entity_CreateCube(name);
        Place(scene, entity, position, halfExtents);
        scene.materials.GetComponent(entity)->SetBaseColor(colour);
        scene.materials.GetComponent(entity)->SetRoughness(.86f);
    }

    wi::ecs::Entity FindNamedTransform(const wi::scene::Scene& scene, const char* name)
    {
        for (size_t i = 0; i < scene.names.GetCount(); ++i)
        {
            const auto entity = scene.names.GetEntity(i);
            if (scene.names[i].name == name && scene.transforms.Contains(entity))
                return entity;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    bool ValidateModel(const wi::scene::Scene& scene)
    {
        for (const char* part : RequiredParts)
            if (!scene.transforms.Contains(FindNamedTransform(scene, part)))
            {
                std::cerr << "Missing imported part " << part << '\n';
                return false;
            }
        const auto nose = FindNamedTransform(scene, "Wheel_Nose");
        const auto* noseParent = scene.hierarchy.GetComponent(nose);
        return noseParent && noseParent->parentID == FindNamedTransform(scene, "Nose_Steer") &&
            scene.transforms.Contains(FindNamedTransform(scene, "REF_Eye_Pilot"));
    }

    // The same aircraft and wheel-marker bindings used in the player.
    bool ProveFlight(wi::scene::Scene& scene, wi::ecs::Entity chassis)
    {
        using namespace renegade::bridge;
        wi::physics::SetSimulationEnabled(true);
        wi::physics::SetInterpolationEnabled(false);
        constexpr float dt = 1.f / 60.f;
        scene.dt = dt;
        scene.weather.gravity = XMFLOAT3(0, -9.81f, 0);
        wi::jobsystem::context jobs;
        const auto authoredPosition = scene.transforms.GetComponent(chassis)->translation_local;
        auto& rigid = *scene.rigidbodies.GetComponent(chassis);
        for (int tick = 0; tick < 120; ++tick)
        {
            wi::physics::RunPhysicsUpdateSystem(jobs, scene, dt);
            wi::jobsystem::Wait(jobs);
        }
        if (!rigid.physicsobject)
            return false;
        const auto parkedPosition = wi::physics::GetPosition(rigid);
        if (std::abs(parkedPosition.x - authoredPosition.x) > .001f ||
            std::abs(parkedPosition.y - authoredPosition.y) > .001f ||
            std::abs(parkedPosition.z - authoredPosition.z) > .001f)
        {
            std::cerr << "Unregistered aircraft fell from its authored position\n";
            return false;
        }
        std::cout << "Unregistered aircraft held its authored pose for 120 physics steps.\n";
        renegade::runtime::RuntimeFixedWing rig;
        if (!rig.Register(scene, chassis))
            return false;
        const auto step = [&](FixedWingInput controls, float brakes = 0.f) {
            CommandFixedWing(scene, chassis, controls, brakes);
            wi::physics::RunPhysicsUpdateSystem(jobs, scene, dt);
            wi::jobsystem::Wait(jobs);
        };
        for (int tick=0; tick<60; ++tick)
            step({}, 1.f);
        if (ReadFixedWing(scene, chassis).groundContacts < 2)
        {
            std::cerr << "Preflight wheel contact count " << ReadFixedWing(scene, chassis).groundContacts << '\n';
            return false;
        }

        enum class Phase { Takeoff, Bank, Approach, Brake };
        Phase phase = Phase::Takeoff;
        int phaseTicks = 0;
        int clearTicks = 0;
        float startLateralSpeed = 0.f;
        float largestLateralChange = 0.f;
        float highest = 0.f;
        for (int tick=0; tick<4800; ++tick, ++phaseTicks)
        {
            const auto state = ReadFixedWing(scene, chassis);
            const auto position = wi::physics::GetPosition(rigid);
            const auto velocity = wi::physics::GetVelocity(rigid);
            const auto rotation = wi::physics::GetRotation(rigid);
            const auto basis = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
            const float pitch = std::asin(std::clamp(XMVectorGetY(basis.r[2]), -1.f, 1.f));
            const float bank = std::atan2(-XMVectorGetY(basis.r[0]), XMVectorGetY(basis.r[1]));
            highest = std::max(highest, position.y);
            auto pitchTarget = state.airspeed < 24.f ? 0.f : .20f;
            auto bankTarget = 0.f;
            float throttle = 1.f;
            float brakes = 0.f;
            if (phase == Phase::Takeoff && position.y > 14.f && state.groundContacts == 0)
            {
                phase = Phase::Bank;
                phaseTicks = 0;
                startLateralSpeed = velocity.x;
                std::cout << "Airborne at " << state.airspeed << " m/s, height " << position.y << " m\n";
            }
            if (phase == Phase::Bank)
            {
                pitchTarget = .09f;
                bankTarget = .42f;
                largestLateralChange = std::max(largestLateralChange, velocity.x - startLateralSpeed);
                if (phaseTicks > 140)
                {
                    if (largestLateralChange < 1.75f || bank < .16f)
                    {
                        std::cerr << "Bank/trajectory change too small: " << bank << " rad, "
                            << largestLateralChange << " m/s\n";
                        return false;
                    }
                    std::cout << "Banked " << bank << " rad, lateral velocity changed by "
                        << largestLateralChange << " m/s\n";
                    phase = Phase::Approach;
                    phaseTicks = 0;
                }
            }
            if (phase == Phase::Approach)
            {
                throttle = .13f;
                pitchTarget = position.y < 3.3f ? .075f : -.065f;
                if (state.groundContacts > 0 && phaseTicks > 90)
                {
                    phase = Phase::Brake;
                    phaseTicks = 0;
                    std::cout << "Touchdown at " << state.airspeed << " m/s\n";
                }
            }
            if (phase == Phase::Brake)
            {
                throttle = 0.f;
                brakes = 1.f;
                pitchTarget = 0.f;
                if (state.groundContacts > 0 && state.airspeed < 1.f)
                {
                    std::cout << "Stopped on gear. Highest point " << highest << " m\n";
                    return true;
                }
            }
            if (state.groundContacts == 0)
                ++clearTicks;
            if (tick % 300 == 0)
                std::cout << "phase=" << static_cast<int>(phase) << " speed=" << state.airspeed
                    << " altitude=" << position.y << " downrange=" << position.z << " pitch=" << pitch
                    << " wheels=" << state.groundContacts << '\n';
            step({throttle, std::clamp((pitchTarget-pitch) * 5.f, -1.f, 1.f),
                std::clamp((bankTarget-bank) * 4.f, -1.f, 1.f), 0.f}, brakes);
        }
        std::cerr << "Flight proof timed out, peak height " << highest << ", airborne ticks " << clearTicks << '\n';
        return false;
    }
}

int main(int argc, char** argv)
{
    if (argc < 3 || argc > 4)
    {
        std::cerr << "Usage: AuthorKestrelRunway <trainer.glb> <runway.wiscene> [--prove-flight]\n";
        return 2;
    }

    const auto window = CreateWindowExW(0, L"STATIC", L"Kestrel runway authoring",
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

    wi::scene::Scene scene;
    ImportModel_GLTF(argv[1], scene);
    const auto root = FindNamedTransform(scene, ModelRoot);
    if (!scene.transforms.Contains(root) || !ValidateModel(scene))
        return 3;

    using namespace renegade::bridge;
    const auto chassis = CreateBody(scene, ChassisName, {0, CentreHeight + .05f, CentreForward}, {.68f, .53f, 2.40f}, 740.f);
    auto* plane = scene.rigidbodies.GetComponent(chassis);
    plane->damping_linear = .007f;
    plane->damping_angular = .025f;
    if (!SetPlayerVehicleMode(scene, chassis, PlayerVehicleMode::FixedWing)) return 3;
    scene.Component_Attach(root, chassis, true);
    Place(scene, root, {0, -CentreHeight, -CentreForward});
    scene.transforms.GetComponent(root)->RotateRollPitchYaw(XMFLOAT3(0, XM_PI, 0));

    const XMFLOAT4 asphalt{.12f, .14f, .16f, 1};
    const XMFLOAT4 grass{.24f, .35f, .17f, 1};
    const XMFLOAT4 marking{.84f, .83f, .77f, 1};
    Paint(scene, "Airfield", {0, -.21f, 0}, {850, .20f, 1500}, grass);
    Paint(scene, "Runway", {0, -.041f, 465}, {16, .041f, 530}, asphalt);
    CreateBody(scene, "Airfield collision", {0, -.5f, 0}, {850, .5f, 1500}, 0.f);
    for (const float x : {-15.3f, 15.3f})
        Paint(scene, "Runway edge", {x, .003f, 465}, {.105f, .003f, 528}, marking);
    for (int z = -44; z < 985; z += 24)
        Paint(scene, "Runway centre line", {0, .004f, float(z)}, {.16f, .003f, 5.f}, marking);
    for (const float z : {-53.f, 983.f})
        for (const float x : {-6.f, -4.f, -2.f, 2.f, 4.f, 6.f})
            Paint(scene, "Runway threshold", {x, .004f, z}, {.42f, .003f, 4.6f}, marking);

    TransformState spawn;
    spawn.translation = {-3.0f, .37f, .5f};
    CreatePlayerStartCommand player(scene, spawn);
    if (!player.Execute()) return 4;
    const auto sun = scene.Entity_CreateLight("Daylight", {0, 80, -20}, {1, .96f, .86f}, 2.2f, 200,
        wi::scene::LightComponent::DIRECTIONAL);
    scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3(1.10f, -.67f, 0));
    const auto sky = scene.Entity_CreateTransform("Daylight Sky");
    auto& weather = scene.weathers.Create(sky);
    weather.ambient = {.18f, .19f, .21f};
    weather.horizon = {.67f, .76f, .87f};
    weather.zenith = {.24f, .43f, .68f};

    scene.Update(0);
    wi::graphics::GetDevice()->SubmitCommandLists();
    wi::Archive output;
    scene.Serialize(output);
    if (!output.SaveFile(argv[2])) return 5;

    wi::scene::Scene reopened;
    wi::Archive input(argv[2], true, false);
    if (!input.IsOpen()) return 6;
    reopened.Serialize(input);
    if (!ValidateModel(reopened) ||
        GetPlayerVehicleMode(reopened, reopened.Entity_FindByName(ChassisName)) != PlayerVehicleMode::FixedWing ||
        !reopened.rigidbodies.Contains(reopened.Entity_FindByName(ChassisName)) ||
        ResolvePlayerStart(reopened).resolution != PlayerStartResolution::Success)
        return 7;
    std::cout << "Kestrel native GLB import and runway save/reopen passed.\n";
    if (argc == 4 && std::string_view(argv[3]) == "--prove-flight" &&
        !ProveFlight(reopened, reopened.Entity_FindByName(ChassisName)))
        return 8;
}
