#include "../Runtime/src/InventoryLuaBridge.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <cstdlib>
#include <iostream>
#include <string>

using namespace renegade;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAILED " << #x << " line " << __LINE__ << "\n"; std::abort(); } } while(false)
namespace {
bool pressing=false;
double playerX=12.0;
int prompts=0;
int Position(lua_State* L) {
    lua_newtable(L);
    lua_pushnumber(L,playerX);lua_setfield(L,-2,"x");
    lua_pushnumber(L,0);lua_setfield(L,-2,"y");
    lua_pushnumber(L,0);lua_setfield(L,-2,"z");
    return 1;
}
int EntityPosition(lua_State* L) {
    lua_newtable(L);
    lua_pushnumber(L,0);lua_setfield(L,-2,"x");
    lua_pushnumber(L,0);lua_setfield(L,-2,"y");
    lua_pushnumber(L,0);lua_setfield(L,-2,"z");
    return 1;
}
int Pressed(lua_State* L) {
    CHECK(std::string(luaL_checkstring(L,1))=="interact");
    lua_pushboolean(L,pressing);
    return 1;
}
int Prompt(lua_State* L) {
    CHECK(lua_type(L,1)==LUA_TSTRING);
    ++prompts;
    lua_pushboolean(L,1);
    return 1;
}
void Subtable(lua_State* L,const char* name,const char* fn,lua_CFunction callback) {
    lua_newtable(L);
    lua_pushcfunction(L,callback);
    lua_setfield(L,-2,fn);
    lua_setfield(L,-2,name);
}
void Tick(lua_State* L,int lifecycle,int self) {
    lua_getfield(L,lifecycle,"on_update");
    CHECK(lua_isfunction(L,-1));
    lua_pushvalue(L,self);
    lua_pushnumber(L,0.016);
    const int result=lua_pcall(L,2,0,0);
    if(result!=LUA_OK) {
        std::cerr << "Lua interaction error: " << lua_tostring(L,-1) << "\n";
        std::abort();
    }
}
}
int main() {
    bridge::InventoryService inventory;
    CHECK(inventory.RegisterItem({"potion","Potion",1,0.1,{}})==bridge::InventoryResult::Success);
    CHECK(inventory.CreateContainer("world",4,0)==bridge::InventoryResult::Success);
    CHECK(inventory.CreateContainer("player",1,1.0)==bridge::InventoryResult::Success);
    CHECK(inventory.Add("world","potion",3)==bridge::InventoryResult::Success);
    bridge::InventoryGameplayGateway gateway(inventory);
    gateway.Grant("world",{true,false,true});
    gateway.Grant("player",{true,true,false});
    runtime::InventoryLuaContext context{&gateway,true};
    lua_State* L=luaL_newstate();CHECK(L);
    luaL_openlibs(L); // Test-only mocks; production VM is governed and isolated.
    lua_newtable(L);
    runtime::RegisterInventoryLua(L,-1,&context);
    Subtable(L,"player","get_position",Position);
    Subtable(L,"transform","get_world_position",EntityPosition);
    Subtable(L,"input","was_pressed",Pressed);
    Subtable(L,"ui","show_prompt",Prompt);
    lua_setglobal(L,"renegade");

    CHECK(luaL_loadfile(L,RENEGADE_INV_EXAMPLE_PATH)==LUA_OK);
    CHECK(lua_pcall(L,0,1,0)==LUA_OK);
    const int lifecycle=lua_absindex(L,-1);
    lua_newtable(L);
    lua_pushstring(L,"entity-fixture");lua_setfield(L,-2,"entity");
    lua_newtable(L);
    lua_pushstring(L,"world");lua_setfield(L,-2,"source_container");
    lua_pushstring(L,"player");lua_setfield(L,-2,"target_container");
    lua_pushnumber(L,2.5);lua_setfield(L,-2,"interaction_radius");
    lua_pushinteger(L,1);lua_setfield(L,-2,"quantity");
    lua_pushstring(L,"Press Interact to collect");lua_setfield(L,-2,"prompt");
    lua_setfield(L,-2,"properties");
    const int self=lua_absindex(L,-1);
    const auto count=[&inventory](const std::string& containerId) {
        const auto* c=inventory.FindContainer(containerId);
        std::uint32_t n=0; for (const auto& stack:c->stacks) n+=stack.quantity;return n;
    };
    // Out of range never accepts an Interact press.
    pressing=true; Tick(L,lifecycle,self);
    CHECK(count("world")==3 && count("player")==0 && prompts==0);
    playerX=0;pressing=false;Tick(L,lifecycle,self);
    CHECK(count("world")==3 && count("player")==0 && prompts==1);
    pressing=true;Tick(L,lifecycle,self);
    CHECK(count("world")==2 && count("player")==1);
    const auto first=inventory.SerializeSnapshot();
    Tick(L,lifecycle,self); // destination full: transfer must be atomic
    CHECK(inventory.SerializeSnapshot()==first);
    context.active=false;
    lua_close(L);
    std::cout<<"G3B Lua world-pickup reference: distance input permission and atomicity PASS\n";
}
