#include <cmath>
#include <iostream>
#include <string>

#include <wiLua.h>

#include "renegade/bridge/FlightVehicleService.h"
#include "renegade/bridge/PhysicsLuaService.h"
#include "renegade/bridge/PhysicsService.h"
#include "renegade/bridge/TerrainPropCollisionWorld.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool RunScript(lua_State* L, const std::string& script)
    {
        if (luaL_loadstring(L, script.c_str()) != LUA_OK)
        {
            const char* error = lua_tostring(L, -1);
            std::cerr << "Lua load error: " << (error == nullptr ? "unknown" : error) << '\n';
            lua_pop(L, 1);
            return false;
        }
        if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        {
            const char* error = lua_tostring(L, -1);
            std::cerr << "Lua runtime error: " << (error == nullptr ? "unknown" : error) << '\n';
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    bool GlobalBool(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        const bool value = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return value;
    }

    lua_Integer GlobalInteger(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        const auto value = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return value;
    }

    double GlobalNumber(lua_State* L, const char* name)
    {
        lua_getglobal(L, name);
        const auto value = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return value;
    }
}

int main()
{
    wi::scene::Scene scene;
    const auto entity = wi::ecs::CreateEntity();
    scene.transforms.Create(entity);
    scene.rigidbodies.Create(entity); // component exists, native Jolt body does not
    scene.metadatas.Create(entity).bool_values.set(renegade::bridge::FlightVehicleKey, true);

    const auto setupEntity = scene.Entity_CreateTransform("Lua authoring fixture");
    const auto mesh = scene.Entity_CreateMesh("Lua soft-body fixture");
    scene.meshes.GetComponent(mesh)->vertex_positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    scene.humanoids.Create(setupEntity);

    // This is a binding contract test, not a Wicked application-startup test.
    // Use a private Lua state so the test cannot accidentally initialize the
    // renderer/audio/job system or change Wicked's process-global lifecycle.
    lua_State* L = luaL_newstate();
    if (L == nullptr)
    {
        return Fail("could not create isolated Lua state");
    }
    luaL_openlibs(L);

    if (!renegade::bridge::BindPhysicsLua(scene, L) ||
        !renegade::bridge::IsPhysicsLuaBound() ||
        !renegade::bridge::IsPhysicsLuaBoundTo(scene))
    {
        lua_close(L);
        return Fail("Lua physics namespace did not bind to the requested scene");
    }

    const std::string entityText = std::to_string(entity);
    const std::string script =
        "jp01_contract = renegade.physics.contract_version\n"
        "jp01_world_table = type(renegade.physics.get_world()) == 'table'\n"
        "jp01_set_gravity = renegade.physics.set_gravity(0, 0, 0)\n"
        "local g = renegade.physics.get_gravity()\n"
        "jp01_gravity_y = g.y\n"
        "jp01_body_live = renegade.physics.has_body(" + entityText + ")\n"
        "jp01_force_result = renegade.physics.apply_force(" + entityText + ", 0, 1, 0)\n"
        "jp01_position_nil = renegade.physics.get_position(" + entityText + ") == nil\n"
        "jp01_constraint_nil = renegade.physics.constraint_broken(" + entityText + ") == nil\n"
        "jp01_flight_live = renegade.physics.has_flight_vehicle(" + entityText + ")\n"
        "jp01_flight_input_rejected = renegade.physics.flight_vehicle_drive(" + entityText + ", 0, 0, 1, 0.016) == nil\n";

    if (!RunScript(L, script))
    {
        renegade::bridge::UnbindPhysicsLua(&scene);
        lua_close(L);
        return Fail("isolated Lua VM rejected renegade.physics contract");
    }

    if (GlobalInteger(L, "jp01_contract") != 1 ||
        !GlobalBool(L, "jp01_world_table") ||
        !GlobalBool(L, "jp01_set_gravity") ||
        std::abs(GlobalNumber(L, "jp01_gravity_y")) > 0.0001 ||
        GlobalBool(L, "jp01_body_live") ||
        GlobalBool(L, "jp01_force_result") ||
        !GlobalBool(L, "jp01_position_nil") ||
        !GlobalBool(L, "jp01_constraint_nil") ||
        GlobalBool(L, "jp01_flight_live") ||
        !GlobalBool(L, "jp01_flight_input_rejected"))
    {
        renegade::bridge::UnbindPhysicsLua(&scene);
        lua_close(L);
        return Fail("Lua physics contract returned unsafe or incorrect runtime state");
    }

    lua_pushinteger(L, static_cast<lua_Integer>(setupEntity));
    lua_setglobal(L, "authoring_fixture");
    lua_pushinteger(L, static_cast<lua_Integer>(mesh));
    lua_setglobal(L, "mesh_fixture");
    if (!RunScript(L, R"lua(
        local p = renegade.physics
        local e = authoring_fixture
        assert(p.get_rigid_body_settings(e) == nil)
        assert(p.add_rigid_body(e, { mass = 8, box_half_extents = { x = 2, y = 3, z = 4 } }))
        assert(p.set_rigid_body_settings(e, { mass = 12, box_half_extents = { y = 5 } }))
        local body = p.get_rigid_body_settings(e)
        assert(body.mass == 12 and body.box_half_extents.x == 2 and body.box_half_extents.y == 5)
        assert(not pcall(p.set_rigid_body_settings, e, { mass = 3, kinematic = 1 }))
        assert(p.get_rigid_body_settings(e).mass == 12)
        assert(p.set_character_settings(e, { gravity_factor = 0.5 }))
        assert(p.get_character_settings(e).gravity_factor == 0.5)
        assert(p.set_vehicle_settings(e, { wheel_radius = 0.5, four_wheel_drive = true }))
        assert(p.get_vehicle_settings(e).wheel_radius == 0.5 and p.get_vehicle_settings(e).four_wheel_drive)
        assert(p.set_ragdoll_settings(e, { head_size = 1.5 }))
        assert(p.get_ragdoll_settings(e).head_size == 1.5)
        assert(p.add_constraint(e, { break_distance = 4 }))
        assert(p.set_constraint_settings(e, { break_distance = 8 }))
        assert(p.get_constraint_settings(e).break_distance == 8)
        assert(p.remove_constraint(e) and p.get_constraint_settings(e) == nil)
        assert(p.add_collider(e, { radius = 2, cpu_enabled = false }))
        assert(p.set_collider_settings(e, { radius = 3 }))
        local collider = p.get_collider_settings(e)
        assert(collider.radius == 3 and not collider.cpu_enabled and collider.gpu_enabled)
        assert(p.remove_collider(e) and p.get_collider_settings(e) == nil)
        assert(p.add_soft_body(mesh_fixture, { pressure = 2 }))
        assert(p.set_soft_body_settings(mesh_fixture, { vertex_radius = 0.5 }))
        local soft = p.get_soft_body_settings(mesh_fixture)
        assert(soft.pressure == 2 and soft.vertex_radius == 0.5)
        assert(#p.soft_body_get_pinned_vertices(mesh_fixture) == 0)
        assert(p.soft_body_set_pinned_vertices(mesh_fixture, {2, 0, 2}))
        local pins = p.soft_body_get_pinned_vertices(mesh_fixture)
        assert(#pins == 2 and pins[1] == 0 and pins[2] == 2)
        assert(not p.soft_body_set_pinned_vertices(mesh_fixture, {1, 3}))
        assert(not p.soft_body_set_pinned_vertices(mesh_fixture, {[9] = 1}))
        assert(not p.soft_body_set_pinned_vertices(mesh_fixture, {0, 2}))
        assert(p.soft_body_set_pinned_vertices(mesh_fixture, {}))
        assert(#p.soft_body_get_pinned_vertices(mesh_fixture) == 0)
        assert(p.remove_soft_body(mesh_fixture) and p.get_soft_body_settings(mesh_fixture) == nil)
        assert(p.body_is_sensor(e) == false)
        assert(p.body_set_sensor(e, true) and p.body_is_sensor(e))
        assert(not p.body_set_sensor(e, true))
        assert(p.body_set_sensor(e, false) and not p.body_is_sensor(e))
        assert(p.body_is_sensor(mesh_fixture) == nil)
        assert(not p.enable_contact_events(true))
        local events, dropped = p.drain_contact_events()
        assert(#events == 0 and dropped == 0)
        local hit, error = p.raycast_bodies(0, 0, 0, 1, 0, 0)
        assert(hit == nil and error == nil)
        hit, error = p.raycast_bodies(0, 0, 0, 1, 0, 0, -1)
        assert(hit == nil and type(error) == "string")
        hit, error = p.raycast_bodies(0, 0, 0, 1, 0, 0, nil, nil, "true")
        assert(hit == nil and type(error) == "string")
        hit, error = p.raycast_bodies(0/0, 0, 0, 1, 0, 0)
        assert(hit == nil and type(error) == "string")
        assert(p.remove_rigid_body(e) and p.get_rigid_body_settings(e) == nil)
    )lua"))
    {
        renegade::bridge::UnbindPhysicsLua(&scene);
        lua_close(L);
        return Fail("Lua component authoring settings contract");
    }

    const auto gravity = renegade::bridge::GetPhysicsGravity(scene);
    if (std::abs(gravity.x) > 0.0001f ||
        std::abs(gravity.y) > 0.0001f ||
        std::abs(gravity.z) > 0.0001f)
    {
        renegade::bridge::UnbindPhysicsLua(&scene);
        lua_close(L);
        return Fail("Lua gravity did not update Wicked serialized scene state");
    }

    std::string collisionError;
    auto collisions=renegade::bridge::TerrainPropCollisionWorld::Create(scene,{},false,collisionError);
    if(!collisions||!RunScript(L,
        "local ok=renegade.physics.set_terrain_interest(7,1,2,3,2)\n"
        "jp03_interest=ok and renegade.physics.terrain_collision_state().native_capacity==65536\n"
        "jp03_clear=renegade.physics.clear_terrain_interest(7)\n"
        "jp03_cancel=renegade.physics.cancel_terrain_destination("+entityText+")\n")||
        !GlobalBool(L,"jp03_interest")||!GlobalBool(L,"jp03_clear")||!GlobalBool(L,"jp03_cancel"))
    {
        renegade::bridge::UnbindPhysicsLua(&scene);lua_close(L);
        return Fail("P3 isolated scripted collision interest/cancellation contract");
    }
    collisions->Detach();

    renegade::bridge::UnbindPhysicsLua(&scene);
    if (renegade::bridge::IsPhysicsLuaBound())
    {
        lua_close(L);
        return Fail("Lua physics scene binding did not release safely");
    }

    if (!RunScript(
            L,
            "jp01_unbound_gravity_nil = renegade.physics.get_gravity() == nil\n"
            "jp01_unbound_force_false = not renegade.physics.apply_force(" +
                entityText + ", 0, 1, 0)\n"))
    {
        lua_close(L);
        return Fail("Lua physics namespace became invalid after scene unbind");
    }
    if (!GlobalBool(L, "jp01_unbound_gravity_nil") ||
        !GlobalBool(L, "jp01_unbound_force_false"))
    {
        lua_close(L);
        return Fail("unbound Lua physics namespace retained unsafe scene access");
    }

    lua_close(L);
    std::cout << "PASS: JP01 renegade.physics Lua contract\n";
    return 0;
}
