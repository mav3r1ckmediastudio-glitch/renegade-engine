#include <cmath>
#include <iostream>
#include <string>

#include <wiLua.h>

#include "renegade/bridge/PhysicsLuaService.h"
#include "renegade/bridge/PhysicsService.h"

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
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

    renegade::bridge::BindPhysicsLua(scene);
    if (!renegade::bridge::IsPhysicsLuaBound() ||
        !renegade::bridge::IsPhysicsLuaBoundTo(scene))
    {
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
        "jp01_constraint_nil = renegade.physics.constraint_broken(" + entityText + ") == nil\n";

    if (!wi::lua::RunText(script))
    {
        return Fail("real Wicked Lua VM rejected renegade.physics contract");
    }

    auto* L = wi::lua::GetLuaState();
    if (GlobalInteger(L, "jp01_contract") != 1 ||
        !GlobalBool(L, "jp01_world_table") ||
        !GlobalBool(L, "jp01_set_gravity") ||
        std::abs(GlobalNumber(L, "jp01_gravity_y")) > 0.0001 ||
        GlobalBool(L, "jp01_body_live") ||
        GlobalBool(L, "jp01_force_result") ||
        !GlobalBool(L, "jp01_position_nil") ||
        !GlobalBool(L, "jp01_constraint_nil"))
    {
        return Fail("Lua physics contract returned unsafe or incorrect runtime state");
    }

    const auto gravity = renegade::bridge::GetPhysicsGravity(scene);
    if (std::abs(gravity.x) > 0.0001f ||
        std::abs(gravity.y) > 0.0001f ||
        std::abs(gravity.z) > 0.0001f)
    {
        return Fail("Lua gravity did not update Wicked serialized scene state");
    }

    renegade::bridge::UnbindPhysicsLua(&scene);
    if (renegade::bridge::IsPhysicsLuaBound())
    {
        return Fail("Lua physics scene binding did not release safely");
    }

    if (!wi::lua::RunText(
            "jp01_unbound_gravity_nil = renegade.physics.get_gravity() == nil\n"
            "jp01_unbound_force_false = not renegade.physics.apply_force(" +
            entityText + ", 0, 1, 0)\n"))
    {
        return Fail("Lua physics namespace became invalid after scene unbind");
    }
    if (!GlobalBool(L, "jp01_unbound_gravity_nil") ||
        !GlobalBool(L, "jp01_unbound_force_false"))
    {
        return Fail("unbound Lua physics namespace retained unsafe scene access");
    }

    std::cout << "PASS: JP01 renegade.physics Lua contract\n";
    return 0;
}
