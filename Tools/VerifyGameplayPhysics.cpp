#include "renegade/bridge/PhysicsGameplayService.h"
#include "renegade/bridge/PhysicsLuaService.h"

#include <Windows.h>
#include <wiLua.h>
#include <iostream>

namespace {
using wi::ecs::Entity;

Entity Box(wi::scene::Scene& scene, const char* name, XMFLOAT3 position, float mass)
{
    const auto entity = scene.Entity_CreateTransform(name);
    scene.transforms.GetComponent(entity)->Translate(position);
    auto& body = scene.rigidbodies.Create(entity);
    body.shape = wi::scene::RigidBodyPhysicsComponent::BOX;
    body.box.halfextents = {0.6f, 0.6f, 0.6f};
    body.mass = mass;
    return entity;
}

int Fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}
}

int main()
{
    HWND window = CreateWindowExW(0, L"STATIC", L"Physics gameplay proof", WS_OVERLAPPEDWINDOW,
        0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("could not create the DX12 proof window");
    wi::Application app;
    app.allow_hdr = false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    struct Drain {
        ~Drain() {
            while (wi::renderer::IsPipelineCreationActive()) Sleep(10);
            wi::graphics::GetDevice()->WaitForGPU();
        }
    } drain;
    wi::physics::SetEnabled(true);
    wi::physics::SetSimulationEnabled(false);

    using namespace renegade::bridge;
    wi::scene::Scene source;
    const auto authoredSensor = Box(source, "Sensor", {0, 2.5f, 0}, 0);
    const auto authoredWall = Box(source, "Wall", {3, 2.5f, 0}, 0);
    Box(source, "Falling body", {0, 5.5f, 0}, 1);
    const auto authoredPlatform = Box(source, "Filtered platform", {10, 2.5f, 0}, 0);
    const auto authoredRejected = Box(source, "Rejected body", {10, 5.5f, 0}, 1);
    const auto authoredAllowed = Box(source, "Allowed body", {14, 5.5f, 0}, 1);
    Box(source, "Live platform", {14, 2.5f, 0}, 0);
    Box(source, "Default platform", {18, 2.5f, 0}, 0);
    Box(source, "Default body", {18, 5.5f, 0}, 1);
    source.layers.Create(authoredSensor).layerMask = 1;
    source.layers.Create(authoredWall).layerMask = 2;
    CommandService commands;
    if (!commands.Execute(std::make_unique<SetBodySensorCommand>(source, authoredSensor, true)) ||
        !IsBodySensor(source, authoredSensor) ||
        !commands.Undo() || IsBodySensor(source, authoredSensor) ||
        !commands.Redo() || !IsBodySensor(source, authoredSensor))
        return Fail("sensor authoring did not survive Undo/Redo");

    const BodyCollisionFilter platformFilter{1, 1 << 2};
    if (!commands.Execute(std::make_unique<SetBodyCollisionFilterCommand>(source, authoredPlatform, platformFilter)) ||
        !commands.Undo() || !(GetBodyCollisionFilter(source, authoredPlatform) == BodyCollisionFilter{}) ||
        !commands.Redo() || !(GetBodyCollisionFilter(source, authoredPlatform) == platformFilter) ||
        !SetBodyCollisionFilter(source, authoredRejected, {3, 1 << 1}) ||
        !SetBodyCollisionFilter(source, authoredAllowed, {2, 1 << 0}))
        return Fail("collision filter authoring or Undo/Redo failed");

    wi::Archive archive;
    source.Serialize(archive);
    archive.SetReadModeAndResetPos(true);
    wi::scene::Scene scene;
    scene.Serialize(archive);
    const auto sensor = scene.Entity_FindByName("Sensor");
    const auto wall = scene.Entity_FindByName("Wall");
    const auto falling = scene.Entity_FindByName("Falling body");
    const auto platform = scene.Entity_FindByName("Filtered platform");
    const auto rejected = scene.Entity_FindByName("Rejected body");
    const auto allowed = scene.Entity_FindByName("Allowed body");
    const auto defaultBody = scene.Entity_FindByName("Default body");
    const auto livePlatform = scene.Entity_FindByName("Live platform");
    if (!(GetBodyCollisionFilter(scene, platform) == platformFilter) ||
        !(GetBodyCollisionFilter(scene, defaultBody) == BodyCollisionFilter{}))
        return Fail("collision filter archive did not round-trip");
    if (!IsBodySensor(scene, sensor) || !EnableContactEvents(scene, true))
        return Fail("sensor archive or contact subscription failed");

    const auto advance = [&] {
        scene.Update(1.f / 60.f);
        wi::graphics::GetDevice()->SubmitCommandLists();
    };
    advance();
    advance();
    auto& platformBody = *scene.rigidbodies.GetComponent(platform);
    wi::physics::SetGhostMode(platformBody, true);
    wi::physics::SetGhostMode(platformBody, false);
    const XMFLOAT3 from{-4, 2.5f, 0}, to{5, 2.5f, 0};
    wi::physics::terrain_props::NativeHit hit;
    std::string error;
    const auto checkRay = [&](uint32_t mask, Entity excluded, bool sensors, Entity expected) {
        return RaycastBodies(scene, from, to, mask, excluded, sensors, hit, error) &&
               hit.entity == expected;
    };
    const auto invalid = wi::ecs::INVALID_ENTITY;
    if (!checkRay(3, invalid, false, wall) || !checkRay(3, invalid, true, sensor) ||
        !checkRay(2, invalid, true, wall) || !checkRay(3, sensor, true, wall) ||
        !checkRay(1, invalid, false, invalid))
        return Fail("sensor, scene layer or entity filter returned the wrong body");

    auto lua = std::unique_ptr<lua_State, decltype(&lua_close)>(luaL_newstate(), lua_close);
    if (!lua || !BindPhysicsLua(scene, lua.get()))
        return Fail("could not bind Lua to the live proof scene");
    luaL_openlibs(lua.get());
    lua_pushinteger(lua.get(), static_cast<lua_Integer>(sensor));
    lua_setglobal(lua.get(), "sensor");
    lua_pushinteger(lua.get(), static_cast<lua_Integer>(livePlatform));
    lua_setglobal(lua.get(), "platform");
    const char* luaRay = "local p = renegade.physics; "
        "local layer, mask = p.body_get_collision_filter(platform); "
        "assert(layer == 0 and mask == 255); "
        "assert(not p.body_set_collision_filter(platform, 8, 255)); "
        "assert(not p.body_set_collision_filter(platform, 0, 256)); "
        "assert(not p.body_set_collision_filter(platform, 0, 1.5)); "
        "assert(p.body_is_sensor(sensor)); "
        "assert(p.enable_contact_events(true)); "
        "local hit = p.raycast_bodies(-4, 2.5, 0, 5, 2.5, 0, 1, nil, true); "
        "assert(hit and hit.entity == sensor and hit.fraction > 0 and hit.fraction < 1)";
    const bool luaRayPassed = luaL_dostring(lua.get(), luaRay) == LUA_OK;
    if (!luaRayPassed)
        std::cerr << lua_tostring(lua.get(), -1) << '\n';
    if (!luaRayPassed)
        return Fail("live Lua raycast or contact subscription failed");

    wi::physics::SetSimulationEnabled(true);
    int began = 0, ended = 0;
    for (int i = 0; i < 85; ++i)
    {
        advance();
        const auto batch = DrainContactEvents(scene);
        if (batch.dropped) return Fail("contact queue overflowed");
        for (const auto& event : batch.events)
        {
            const bool expected = (event.bodyA == sensor && event.bodyB == falling) ||
                                  (event.bodyB == sensor && event.bodyA == falling);
            if (!expected) continue;
            if (!event.sensor) return Fail("sensor pair was reported as a solid contact");
            if (event.began) ++began;
            else ++ended;
        }
    }
    const float finalY = wi::physics::GetPosition(*scene.rigidbodies.GetComponent(falling)).y;
    if (began != 1 || ended != 1 || finalY >= 1.0f)
    {
        std::cerr << "begin=" << began << ", end=" << ended << ", final Y=" << finalY << '\n';
        return Fail("sensor blocked the body or contact transitions were incorrect");
    }
    const auto positionY = [&](Entity entity) {
        return wi::physics::GetPosition(*scene.rigidbodies.GetComponent(entity)).y;
    };
    if (positionY(rejected) >= 1.0f || positionY(allowed) < 3.5f || positionY(defaultBody) < 3.5f)
        return Fail("native simulation did not honor bilateral filtering or legacy defaults");

    // Allow the supported body to fall asleep before removing its static support.
    for (int i = 0; i < 90; ++i) advance();
    const char* luaEdit = "local p = renegade.physics; "
        "assert(p.body_set_collision_filter(platform, 0, 0)); "
        "local layer, mask = p.body_get_collision_filter(platform); "
        "assert(layer == 0 and mask == 0); "
        "assert(not p.body_set_collision_filter(platform, 0, 0))";
    const bool luaEditPassed = luaL_dostring(lua.get(), luaEdit) == LUA_OK;
    if (!luaEditPassed)
        std::cerr << lua_tostring(lua.get(), -1) << '\n';
    UnbindPhysicsLua(&scene);
    if (!luaEditPassed) return Fail("Lua could not apply the live collision filter");
    for (int i = 0; i < 85; ++i) advance();
    if (positionY(allowed) >= 1.0f || positionY(defaultBody) < 3.5f)
        return Fail("live filter failed to wake and release the body resting on the platform");

    std::cout << "PASS: sensor contact begin/end, filtered rays, collision filter archive and Undo/Redo, "
        "one-sided rejection, legacy collision and live Lua filter wakeup\n";
    return 0;
}
