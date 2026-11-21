#include <WickedEngine.h>
#include <Windows.h>
#include <ModelImporter.h>

#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/VehiclePhysicsService.h"
#include "renegade/bridge/VehicleSteeringService.h"

#include <cmath>
#include <iostream>

namespace wi::physics
{
    void RunPhysicsUpdateSystem(wi::jobsystem::context& context, wi::scene::Scene& scene, float dt);
}

namespace
{
    using wi::ecs::Entity;
    using wi::scene::RigidBodyPhysicsComponent;
    using wi::scene::Scene;
    using namespace renegade::bridge;

    constexpr char ChassisName[] = "Workshop Van Chassis";

    Entity AddBox(Scene& scene, const char* name, XMFLOAT3 position, XMFLOAT3 halfExtent, XMFLOAT4 colour)
    {
        const auto entity = scene.Entity_CreateCube(name);
        auto& transform = *scene.transforms.GetComponent(entity);
        transform.Translate(position);
        transform.Scale(halfExtent);
        auto& material = *scene.materials.GetComponent(entity);
        material.SetBaseColor(colour);
        material.SetRoughness(.93f);
        return entity;
    }

    bool VerifyDrive(Scene& scene)
    {
        const auto chassis = scene.Entity_FindByName(ChassisName);
        auto* body = scene.rigidbodies.GetComponent(chassis);
        const auto binding = CaptureSteeringWheel(scene, chassis);
        if (!body || ResolvePlayerStart(scene).resolution != PlayerStartResolution::Success ||
            GetPlayerVehicleMode(scene, chassis) != PlayerVehicleMode::Ground ||
            binding.entity == wi::ecs::INVALID_ENTITY ||
            binding.entity != scene.Entity_FindByName("SteeringWheel") ||
            !scene.objects.Contains(scene.Entity_FindByName("BrakeLights")) ||
            !scene.materials.Contains(scene.Entity_FindByName("M_VehicleBrakeLights")))
            return false;
        for (auto wheel : {body->vehicle.wheel_entity_front_left, body->vehicle.wheel_entity_front_right,
                           body->vehicle.wheel_entity_rear_left, body->vehicle.wheel_entity_rear_right})
        {
            if (!scene.transforms.Contains(wheel) || !scene.Entity_IsDescendant(wheel, chassis))
                return false;
        }
        for (size_t i = 0; i < scene.materials.GetCount(); ++i)
            for (const auto& texture : scene.materials[i].textures)
                if (!texture.name.empty())
                    return false;

        wi::physics::SetSimulationEnabled(true);
        wi::jobsystem::context context;
        const auto tick = [&]()
        {
            wi::physics::RunPhysicsUpdateSystem(context, scene, 1.f / 60.f);
            wi::jobsystem::Wait(context);
        };
        const auto run = [&](float throttle, float brake, int count)
        {
            for (int frame = 0; frame < count; ++frame)
            {
                if (!DrivePhysicsVehicle(scene, chassis, throttle, 0.f, brake, 0.f))
                    return false;
                tick();
            }
            return true;
        };
        for (int frame = 0; frame < 90; ++frame)
            tick();
        const auto origin = wi::physics::GetPosition(*body);
        if (!run(1.f, 0.f, 210)) return false;
        const auto driven = wi::physics::GetPosition(*body);
        const auto runningVelocity = wi::physics::GetVelocity(*body);
        if (!run(0.f, 1.f, 180)) return false;
        const auto stopped = wi::physics::GetVelocity(*body);
        std::cout << "Drive distance " << driven.z - origin.z << " m, running speed "
                  << std::hypot(runningVelocity.x, runningVelocity.z) << " m/s, braked speed "
                  << std::hypot(stopped.x, stopped.z) << " m/s\n";
        return driven.z - origin.z > 8.f && std::hypot(stopped.x, stopped.z) < .5f;
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: RenegadeAuthorWorkshopVan <WorkshopVan.glb> <Main.wiscene>\n";
        return 2;
    }

    const auto window = CreateWindowExW(0, L"STATIC", L"Workshop Van authoring", WS_OVERLAPPEDWINDOW,
        0, 0, 960, 640, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
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
    const auto model = scene.Entity_FindByName("WorkshopVan");
    if (!scene.transforms.Contains(model)) return 3;
    const auto chassis = scene.Entity_CreateTransform(ChassisName);
    scene.transforms.GetComponent(chassis)->Translate(XMFLOAT3{0, .55f, 0});
    scene.Component_Attach(model, chassis, true);
    auto& body = scene.rigidbodies.Create(chassis);
    body.mass = 1640.f;
    body.friction = .82f;
    body.shape = RigidBodyPhysicsComponent::CollisionShape::BOX;
    body.box.halfextents = {.855f, .19f, 1.81f};
    auto& car = body.vehicle;
    car.type = RigidBodyPhysicsComponent::Vehicle::Type::Car;
    car.chassis_half_width = .855f;
    car.chassis_half_height = .19f;
    car.chassis_half_length = 1.81f;
    car.front_wheel_offset = -.39f;
    car.rear_wheel_offset = .39f;
    car.wheel_radius = .322f;
    car.wheel_width = .205f;
    car.front_suspension = {.27f, .48f, 1.9f, .55f};
    car.rear_suspension = {.27f, .48f, 1.85f, .55f};
    car.max_roll_angle = wi::math::DegreesToRadians(42.f);
    car.car.four_wheel_drive = true;
    car.wheel_entity_front_left = scene.Entity_FindByName("Wheel_FL");
    car.wheel_entity_front_right = scene.Entity_FindByName("Wheel_FR");
    car.wheel_entity_rear_left = scene.Entity_FindByName("Wheel_RL");
    car.wheel_entity_rear_right = scene.Entity_FindByName("Wheel_RR");

    const SteeringWheelSettings steering{scene.Entity_FindByName("SteeringWheel"), {0.f, 0.f, -1.f}, 180.f};
    if (!SetPlayerVehicleMode(scene, chassis, PlayerVehicleMode::Ground) ||
        !CanBindSteeringWheel(scene, chassis, steering.entity) ||
        !SetSteeringWheelCommand(scene, chassis, steering).Execute())
        return 4;

    const auto range = scene.Entity_CreateTransform("Proving ground collision");
    scene.transforms.GetComponent(range)->Translate(XMFLOAT3{0, -.5f, 0});
    auto& ground = scene.rigidbodies.Create(range);
    ground.shape = RigidBodyPhysicsComponent::CollisionShape::BOX;
    ground.box.halfextents = {220.f, .5f, 220.f};
    ground.mass = 0.f;
    ground.friction = 1.f;
    AddBox(scene, "Proving ground", {0, -.27f, 0}, {220, .21f, 220}, {.21f, .24f, .19f, 1});
    AddBox(scene, "Handling pad", {0, -.035f, 0}, {35, .03f, 114}, {.22f, .24f, .25f, 1});
    for (float x : {-18.f, 18.f})
        for (int z = -85; z <= 85; z += 10)
            AddBox(scene, "Training lane mark", {x, 0, float(z)}, {.13f, .003f, 1.65f}, {.55f, .51f, .32f, 1});
    for (float x : {-3.f, 3.f})
        for (int z = 16; z <= 64; z += 12)
            AddBox(scene, "Slalom marker", {x, .21f, float(z)}, {.21f, .20f, .21f}, {.78f, .33f, .07f, 1});

    TransformState marker;
    marker.translation = {-2.15f, .37f, .43f};
    if (!CreatePlayerStartCommand(scene, marker).Execute()) return 5;
    const auto sunlight = scene.Entity_CreateLight("Workshop daylight", {0, 80, -20}, {1, .96f, .88f},
        2.4f, 180, wi::scene::LightComponent::DIRECTIONAL);
    scene.transforms.GetComponent(sunlight)->RotateRollPitchYaw(XMFLOAT3(1.02f, -.65f, 0));
    const auto environment = scene.Entity_CreateTransform("Clear sky");
    auto& sky = scene.weathers.Create(environment);
    sky.ambient = {.23f, .24f, .26f};
    sky.horizon = {.68f, .76f, .82f};
    sky.zenith = {.23f, .42f, .64f};

    scene.Update(0.f);
    wi::graphics::GetDevice()->SubmitCommandLists();
    wi::Archive archive;
    scene.Serialize(archive);
    if (!archive.SaveFile(argv[2])) return 6;

    Scene reopened;
    wi::Archive saved(argv[2], true, false);
    if (!saved.IsOpen()) return 7;
    reopened.Serialize(saved);
    reopened.Update(0.f);
    if (!VerifyDrive(reopened)) return 8;
    std::cout << "Workshop Van saved and reloaded with original materials, four wheel bindings, steering and brakes.\n";
    return 0;
}
