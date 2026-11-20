#include <WickedEngine.h>
#include <Windows.h>
#include <ModelImporter.h>
#include <wiLua.h>

#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/PhysicsLuaService.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string_view>

namespace wi::physics
{
    void RunPhysicsUpdateSystem(wi::jobsystem::context& context, wi::scene::Scene& scene, float dt);
}

namespace
{
    using wi::ecs::Entity;
    using wi::scene::Scene;
    using Body = wi::scene::RigidBodyPhysicsComponent;
    using namespace renegade::bridge;

    Entity Collider(Scene& scene, const char* name, XMFLOAT3 position, XMFLOAT3 extent, float mass)
    {
        const auto entity = scene.Entity_CreateTransform(name);
        scene.transforms.GetComponent(entity)->Translate(position);
        auto& body = scene.rigidbodies.Create(entity);
        body.shape = Body::CollisionShape::BOX;
        body.box.halfextents = extent;
        body.mass = mass;
        body.friction = .82f;
        return entity;
    }

    void Paint(Scene& scene, const char* name, XMFLOAT3 position, XMFLOAT3 extent, XMFLOAT4 colour,
        float glow = 0.f)
    {
        const auto entity = scene.Entity_CreateCube(name);
        auto& pose = *scene.transforms.GetComponent(entity);
        pose.Scale(extent);
        pose.Translate(position);
        auto& material = *scene.materials.GetComponent(entity);
        material.SetBaseColor(colour);
        material.SetRoughness(.84f);
        if (glow > 0.f)
            material.SetEmissiveColor({colour.x, colour.y, colour.z, glow});
    }

    void BuildRange(Scene& scene)
    {
        Collider(scene, "Airfield collision", {0, -.5f, 0}, {220, .5f, 220}, 0.f);
        Paint(scene, "Airfield", {0, -.22f, 0}, {220, .20f, 220}, {.20f, .27f, .21f, 1});
        Paint(scene, "Helipad", {0, -.018f, 0}, {10, .016f, 10}, {.22f, .25f, .28f, 1});
        const XMFLOAT4 line{.80f, .71f, .40f, 1};
        for (float edge : {-7.6f, 7.6f})
        {
            Paint(scene, "Pad perimeter", {edge, .005f, 0}, {.08f, .006f, 7.6f}, line);
            Paint(scene, "Pad perimeter", {0, .005f, edge}, {7.6f, .006f, .08f}, line);
        }
        for (float side : {-1.2f, 1.2f})
            Paint(scene, "H landing mark", {side, .008f, 0}, {.13f, .007f, 1.9f}, line);
        Paint(scene, "H landing mark", {0, .008f, 0}, {1.2f, .007f, .13f}, line);
        for (float x : {-18.f, 18.f})
            for (float z : {24.f, 55.f})
            {
                Collider(scene, "Flight course pylon collision", {x, 1.75f, z}, {.18f, 1.75f, .18f}, 0.f);
                Paint(scene, "Flight course pylon", {x, 1.75f, z}, {.18f, 1.75f, .18f}, {.91f, .34f, .09f, 1});
                Paint(scene, "Course beacon", {x, 3.57f, z}, {.23f, .09f, .23f}, {.88f, .61f, .19f, 1}, 2.f);
            }
    }

    void BuildDock(Scene& scene)
    {
        const XMFLOAT4 steel{.12f, .17f, .22f, 1};
        const XMFLOAT4 amber{.89f, .47f, .13f, 1};
        Collider(scene, "Dock deck collision", {0, -.5f, -13}, {10, .5f, 24}, 0.f);
        Paint(scene, "Dock deck", {0, -.42f, -13}, {10, .4f, 24}, steel);
        Paint(scene, "Deck keel", {0, -1.9f, -13}, {5.6f, 1.1f, 23}, {.06f, .10f, .15f, 1});
        for (float x : {-8.7f, 8.7f})
        {
            Paint(scene, "Approach stripe", {x, .006f, -9}, {.12f, .01f, 18.5f}, amber);
            for (float z : {-30.f, -22.f, -14.f, -6.f, 2.f, 10.f})
                Paint(scene, "Dock guidance light", {x, .08f, z}, {.24f, .06f, .22f}, {.17f, .59f, 1.f, 1}, 3.f);
        }
        for (float z : {25.f, 65.f, 120.f})
        {
            for (float x : {-10.f, 10.f})
            {
                Collider(scene, "Navigation frame post collision", {x, 4, z}, {.35f, 7, .35f}, 0.f);
                Paint(scene, "Navigation frame post", {x, 4, z}, {.35f, 7, .35f}, steel);
                Paint(scene, "Navigation frame light", {x, 6.4f, z - .36f}, {.25f, 1.05f, .05f}, amber, 2.f);
            }
            Collider(scene, "Navigation frame beam collision", {0, 11, z}, {10.35f, .3f, .35f}, 0.f);
            Paint(scene, "Navigation frame beam", {0, 11, z}, {10.35f, .3f, .35f}, steel);
        }
        // Place small luminous landmarks around the training volume.
        constexpr float GoldenAngle = 2.39996323f;
        for (int i = 0; i < 120; ++i)
        {
            const float y = 1.f - (i + .5f) / 60.f;
            const float horizontal = std::sqrt(1.f - y * y);
            const float angle = i * GoldenAngle;
            const float radius = 420.f + float(i % 11) * 14.f;
            const float size = .13f + float(i % 5) * .085f;
            Paint(scene, "Star", {radius * horizontal * std::cos(angle), radius * y,
                radius * horizontal * std::sin(angle)}, {size, size, size},
                i % 3 == 0 ? XMFLOAT4{.90f, .83f, .69f, 1} : XMFLOAT4{.62f, .78f, 1, 1}, 4.f);
        }
    }

    bool Verify(Scene& scene, Entity entity, bool spacecraft)
    {
        auto* body = scene.rigidbodies.GetComponent(entity);
        if (!body || !scene.transforms.Contains(scene.Entity_FindByName("REF_Eye_Pilot", entity)) ||
            ResolvePlayerStart(scene).resolution != PlayerStartResolution::Success ||
            GetPlayerVehicleMode(scene, entity) != (spacecraft ? PlayerVehicleMode::Spacecraft : PlayerVehicleMode::Helicopter))
            return false;
        wi::physics::SetSimulationEnabled(true);
        wi::jobsystem::context jobs;
        const auto step = [&](int count) {
            for (int i = 0; i < count; ++i)
            {
                wi::physics::RunPhysicsUpdateSystem(jobs, scene, 1.f / 60.f);
                wi::jobsystem::Wait(jobs);
            }
        };
        step(85);
        const auto runLua = [&](const char* source) {
            const auto state = std::unique_ptr<lua_State, decltype(&lua_close)>(luaL_newstate(), lua_close);
            if (!state || !BindPhysicsLua(scene, state.get())) return false;
            luaL_openlibs(state.get());
            lua_pushinteger(state.get(), static_cast<lua_Integer>(entity));
            lua_setglobal(state.get(), "craft");
            const bool ok = luaL_dostring(state.get(), source) == LUA_OK;
            if (!ok)
                std::cerr << lua_tostring(state.get(), -1) << '\n';
            UnbindPhysicsLua(&scene);
            return ok;
        };
        const auto initial = wi::physics::GetPosition(*body);
        if (spacecraft)
        {
            const XMFLOAT3 clear{0, 18, 0};
            const auto reset = [&] {
                wi::physics::SetPositionAndRotation(*body, clear, {0, 0, 0, 1});
                wi::physics::SetLinearVelocity(*body, {0, 0, 0});
                wi::physics::SetAngularVelocity(*body, {0, 0, 0});
            };
            reset();
            if (!runLua("assert(renegade.physics.spacecraft_command(craft, 1, 0, 0, 0, 0, 0))")) return false;
            step(100);
            const auto forward = wi::physics::GetVelocity(*body);
            if (!CommandSpacecraft(scene, entity, {})) return false;
            step(100);
            const auto coast = wi::physics::GetVelocity(*body);
            const auto altitude = wi::physics::GetPosition(*body).y;
            const auto sample = [&](SpacecraftInput input, bool rotation) {
                reset();
                (void)CommandSpacecraft(scene, entity, input);
                step(44);
                if (!rotation) return wi::physics::GetVelocity(*body);
                const auto q = wi::physics::GetRotation(*body);
                return XMFLOAT3{q.x, q.y, q.z};
            };
            const auto backward = sample({-1}, false);
            const auto right = sample({0, 1}, false);
            const auto lift = sample({0, 0, 1}, false);
            const auto pitch = sample({0, 0, 0, 1}, true);
            const auto yaw = sample({0, 0, 0, 0, 1}, true);
            const auto roll = sample({0, 0, 0, 0, 0, 1}, true);
            std::cout << "Oriole forward=" << forward.z << " coast=" << coast.z
                << " altitude=" << altitude << " reverse=" << backward.z << " strafe=" << right.x
                << " lift=" << lift.y << " pitch=" << pitch.x << " yaw=" << yaw.y << " roll=" << roll.z << '\n';
            return forward.z > 5.f && std::abs(coast.z - forward.z) < .1f && std::abs(altitude - clear.y) < .08f &&
                backward.z < -1.f && right.x > .5f && lift.y > .5f &&
                pitch.x < -.02f && yaw.y > .02f && roll.z > .02f;
        }
        if (!runLua("assert(renegade.physics.helicopter_command(craft, 0, 0, 0))")) return false;
        step(16);
        if (!runLua("assert(renegade.physics.helicopter_grounded(craft))")) return false;
        (void)CommandHelicopter(scene, entity, {0, 0, 1});
        step(180);
        const auto raised = wi::physics::GetPosition(*body);
        (void)CommandHelicopter(scene, entity, {1, 0, 0});
        step(170);
        const auto travelled = wi::physics::GetPosition(*body);
        (void)CommandHelicopter(scene, entity, {0, .7f, 0});
        step(100);
        const auto heading = wi::physics::GetRotation(*body);
        (void)CommandHelicopter(scene, entity, {0, 0, -1});
        step(340);
        (void)CommandHelicopter(scene, entity, {});
        step(80);
        const auto landed = wi::physics::GetPosition(*body);
        std::cout << "Merlin takeoff=" << raised.y - initial.y << " forward=" << travelled.z - raised.z
            << " yaw=" << heading.y << " touchdown=" << landed.y << '\n';
        return raised.y > initial.y + 6.f && travelled.z > raised.z + 3.f && heading.y > .1f &&
            IsHelicopterGrounded(scene, entity) && std::abs(landed.y - initial.y) < .12f;
    }
}

int main(int argc, char** argv)
{
    if (argc != 4 || (std::string_view(argv[1]) != "merlin" && std::string_view(argv[1]) != "oriole"))
    {
        std::cerr << "Usage: RenegadeAuthorAdditionalFlight <merlin|oriole> <model.glb> <Main.wiscene>\n";
        return 2;
    }
    const bool spacecraft = std::string_view(argv[1]) == "oriole";
    const auto window = CreateWindowExW(0, L"STATIC", L"Flight scene authoring", WS_OVERLAPPEDWINDOW,
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
    ImportModel_GLTF(argv[2], scene);
    const auto model = scene.Entity_FindByName(spacecraft ? "Oriole_U2_Root" : "Merlin_H4_Root");
    if (!scene.transforms.Contains(model)) return 3;
    const char* chassisName = spacecraft ? "Oriole U2 Chassis" : "Merlin H4 Chassis";
    const auto chassis = Collider(scene, chassisName, spacecraft ? XMFLOAT3{0, .94f, 0} : XMFLOAT3{0, 1.14f, 0},
        spacecraft ? XMFLOAT3{2.05f, .94f, 4.05f} : XMFLOAT3{.82f, 1.14f, 2.5f},
        spacecraft ? 4100.f : 1350.f);
    auto& body = *scene.rigidbodies.GetComponent(chassis);
    body.damping_linear = spacecraft ? 0.f : .03f;
    body.damping_angular = spacecraft ? 0.f : .08f;
    scene.Component_Attach(model, chassis, true);
    auto& modelPose = *scene.transforms.GetComponent(model);
    modelPose.RotateRollPitchYaw(XMFLOAT3{0, XM_PI, 0});
    if (!spacecraft)
        modelPose.Translate(XMFLOAT3{0, -1.14f, 0});
    if (!SetPlayerVehicleMode(scene, chassis, spacecraft ? PlayerVehicleMode::Spacecraft : PlayerVehicleMode::Helicopter))
        return 4;
    if (spacecraft) BuildDock(scene); else BuildRange(scene);

    TransformState spawn;
    spawn.translation = {spacecraft ? -3.2f : -2.65f, .37f, .2f};
    if (!CreatePlayerStartCommand(scene, spawn).Execute()) return 5;
    const auto light = scene.Entity_CreateLight("Key light", {-15, 60, -24}, {1, .96f, .88f},
        3.0f, 400, wi::scene::LightComponent::DIRECTIONAL);
    scene.transforms.GetComponent(light)->RotateRollPitchYaw(XMFLOAT3{.9f, -.65f, 0});
    const auto environment = scene.Entity_CreateTransform(spacecraft ? "Deep space" : "Daylight sky");
    auto& weather = scene.weathers.Create(environment);
    weather.ambient = spacecraft ? XMFLOAT3{.18f, .20f, .27f} : XMFLOAT3{.22f, .23f, .25f};
    weather.horizon = spacecraft ? XMFLOAT3{.006f, .009f, .019f} : XMFLOAT3{.66f, .76f, .85f};
    weather.zenith = spacecraft ? XMFLOAT3{.006f, .009f, .019f} : XMFLOAT3{.27f, .47f, .68f};

    scene.Update(0.f);
    wi::graphics::GetDevice()->SubmitCommandLists();
    wi::Archive archive;
    scene.Serialize(archive);
    if (!archive.SaveFile(argv[3])) return 6;
    Scene reopened;
    wi::Archive input(argv[3], true, false);
    if (!input.IsOpen()) return 7;
    reopened.Serialize(input);
    reopened.Update(0.f);
    if (!Verify(reopened, reopened.Entity_FindByName(chassisName), spacecraft)) return 8;
    std::cout << argv[1] << " archive and native flight proof passed.\n";
    return 0;
}
