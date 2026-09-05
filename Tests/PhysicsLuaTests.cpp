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
        "jp01_constraint_nil = renegade.physics.constraint_broken(" + entityText + ") == nil\n";

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
        !GlobalBool(L, "jp01_constraint_nil"))
    {
        renegade::bridge::UnbindPhysicsLua(&scene);
        lua_close(L);
        return Fail("Lua physics contract returned unsafe or incorrect runtime state");
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
