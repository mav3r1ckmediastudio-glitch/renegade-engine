#include "../Runtime/src/InventoryLuaBridge.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <cstdlib>
#include <iostream>

using namespace renegade;
#define CHECK(x) do{if(!(x)){std::cerr << "C++ fail " << #x << " at " << __LINE__ << "\n";std::abort();}}while(false)
int main(){
    bridge::InventoryService core;
    bridge::InventoryGameplayGateway player(core);
    player.AllowItemDefinitions(true);
    player.AllowContainerCreation("script:fixture:");
    CHECK(core.CreateContainer("external:secret")==bridge::InventoryResult::Success);
    CHECK(core.RegisterItem({"secret","Hidden",10,0.0,{}})==bridge::InventoryResult::Success);
    CHECK(core.Add("external:secret","secret",3)==bridge::InventoryResult::Success);
    runtime::InventoryLuaContext ctx{&player,true};
    lua_State* L=luaL_newstate();
    CHECK(L!=nullptr);
    // Full standard library is test-only; real Renegade uses its governed sandbox.
    luaL_openlibs(L);
    lua_newtable(L);
    runtime::RegisterInventoryLua(L,lua_gettop(L),&ctx);
    lua_setglobal(L,"renegade");
    const char* luaScript=R"LUA(
        local a=renegade.inventory
        assert(a.contract_version==1)
        local inaccessible,denied=a.list('external:secret')
        assert(inaccessible==nil and denied=='permission_denied')
        local bad,msg=a.create('external:secret2',3)
        assert(bad==false and msg=='permission_denied')
        local ok,err=a.define('potion','Health Potion',5,0.2)
        assert(ok and err==nil)
        ok,err=a.create('script:fixture:backpack',3,1.0)
        assert(ok and err==nil)
        ok,err=a.create('script:fixture:chest',10,0)
        assert(ok and err==nil)
        ok,err=a.add('script:fixture:backpack','potion',5)
        assert(ok and err==nil)
        local c=a.list('script:fixture:backpack')
        assert(c.id=='script:fixture:backpack' and c.maximum_slots==3)
        assert(c.total_stacks==1 and c.next_offset==1)
        assert(#c.stacks==1 and c.stacks[1].quantity==5)
        local emptyPage=a.list('script:fixture:backpack',1,1)
        assert(emptyPage.total_stacks==1 and #emptyPage.stacks==0)
        local noPage,pageError=a.list('script:fixture:backpack',0,257)
        assert(noPage==nil and pageError=='invalid_inventory_page')
        assert(c.stacks[1].definition_id=='potion')
        local id=c.stacks[1].instance_id
        ok,err=a.transfer('script:fixture:backpack','script:fixture:chest',id,2)
        assert(ok and err==nil)
        c=a.list('script:fixture:backpack')
        assert(c.stacks[1].quantity==3)
        local d=a.list('script:fixture:chest')
        assert(d.stacks[1].quantity==2)
        ok,err=a.add('script:fixture:backpack','potion',3)
        assert(ok==false and err=='capacity_exceeded')
        c=a.list('script:fixture:backpack')
        assert(c.stacks[1].quantity==3)
        ok,err=a.transfer('script:fixture:backpack','external:secret',id,1)
        assert(ok==false and err=='permission_denied')
        ok,err=a.remove('script:fixture:backpack',id,1)
        assert(ok and err==nil)
        c=a.list('script:fixture:backpack')
        assert(c.stacks[1].quantity==2)
        ok,err=a.add('script:fixture:backpack','potion',-1)
        assert(ok==false and err=='expected_container_definition_positive_quantity')
    )LUA";
    const auto code=luaL_dostring(L,luaScript);
    if(code!=LUA_OK){
        std::cerr<<"Lua error: "<<(lua_tostring(L,-1)?lua_tostring(L,-1):"unknown")<<"\n";
        lua_close(L);
        return 1;
    }
    CHECK(core.FindContainer("script:fixture:backpack")!=nullptr);
    CHECK(core.FindContainer("external:secret")->stacks.front().quantity==3);
    // Explicit teardown revokes access without destroying the shared Lua VM.
    ctx.active=false;
    CHECK(luaL_dostring(L,
        "local data,why=renegade.inventory.list('script:fixture:backpack'); "
        "assert(data==nil and why=='inventory_not_enabled')")==LUA_OK);
    lua_close(L);
    std::cout<<"Renegade optional inventory Lua 5.4 bridge PASS\n";
}
