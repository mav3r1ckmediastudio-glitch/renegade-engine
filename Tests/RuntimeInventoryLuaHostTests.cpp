#include "../Runtime/src/RuntimeInventorySession.h"
#include "../Runtime/src/InventoryLuaBridge.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
namespace fs=std::filesystem;
using namespace renegade;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" "<<__LINE__<<"\n";std::abort();}}while(false)
static void Write(const fs::path& p,const std::string& data){
    std::ofstream f(p,std::ios::binary|std::ios::trunc);CHECK(f.good());
    f<<data;CHECK(f.good());
}
int main(){
    const auto project=fs::temp_directory_path()/"renegade-inventory-g3-lua-host";
    std::error_code ec;
    fs::remove_all(project,ec);
    fs::create_directories(project/"Content"/"Scenes",ec);
    CHECK(!ec);
    const auto scene=project/"Content"/"Scenes"/"Starter.wiscene";
    Write(scene,"fixture");
    bridge::InventoryService original;
    CHECK(original.RegisterItem({"healing","Healing Potion",10,0.1,{}})==bridge::InventoryResult::Success);
    CHECK(original.CreateContainer("player",12,5.0)==bridge::InventoryResult::Success);
    CHECK(original.CreateContainer("chest",12,5.0)==bridge::InventoryResult::Success);
    CHECK(original.Add("player","healing",5)==bridge::InventoryResult::Success);
    Write(scene.generic_u8string()+".rinventory",original.SerializeSnapshot());
    Write(scene.generic_u8string()+".rinventory-access",
        "RENEGADE_INVENTORY_ACCESS 1 5\n"
        "CONTAINER \"level-pickup-script\" \"-\" \"player\" 1 1 1\n"
        "CONTAINER \"level-pickup-script\" \"-\" \"chest\" 1 1 1\n"
        "CONTAINER \"chest-observer-script\" \"-\" \"chest\" 1 0 0\n"
        "DEFINE \"level-pickup-script\" \"-\"\n"
        "CREATE \"level-pickup-script\" \"-\" \"script:level-pickup:\"\n");
    runtime::RuntimeInventorySession host;
    std::string error;
    CHECK(host.OpenForScene(project.generic_u8string(),scene.generic_u8string(),error));
    CHECK(host.Enabled());
    auto* authorised=host.GatewayForScript("level-pickup-script","");
    auto* observer=host.GatewayForScript("chest-observer-script","");
    CHECK(authorised && observer);
    CHECK(host.GatewayForScript("level-pickup-script","entity-elsewhere")==nullptr);
    lua_State* L=luaL_newstate();CHECK(L);
    luaL_openlibs(L); // Only this test uses full Lua; Runtime uses governed sandbox.
    runtime::InventoryLuaContext ctx{authorised,true};
    lua_newtable(L);runtime::RegisterInventoryLua(L,-1,&ctx);
    lua_setglobal(L,"renegade");
    const char* script=R"LUA(
      local bag=renegade.inventory.list("player")
      assert(bag.total_stacks==1 and bag.stacks[1].quantity==5)
      local ok,err=renegade.inventory.transfer("player","chest",bag.stacks[1].instance_id,2)
      assert(ok and not err)
      local afterward=renegade.inventory.list("player")
      local chest=renegade.inventory.list("chest")
      assert(afterward.stacks[1].quantity==3 and chest.stacks[1].quantity==2)
      ok,err=renegade.inventory.define("herb","Herb",20,0.01)
      assert(ok and not err)
      ok,err=renegade.inventory.create("script:level-pickup:stash",12,10)
      assert(ok and not err)
      ok,err=renegade.inventory.add("script:level-pickup:stash","herb",3)
      assert(ok and not err)
      ok,err=renegade.inventory.create("other:stash",12,10)
      assert(ok==false and err=="permission_denied")
    )LUA";
    if(luaL_dostring(L,script)!=LUA_OK){
        std::cerr<<lua_tostring(L,-1)<<"\n";return 2;
    }
    bridge::InventoryContainer out;
    CHECK(observer->ReadContainer("chest",out)==bridge::InventoryResult::Success);
    CHECK(out.stacks.front().quantity==2);
    CHECK(observer->ReadContainer("player",out)==bridge::InventoryResult::PermissionDenied);
    const auto saved=host.Inventory()->SerializeSnapshot();
    bridge::InventoryService reloaded;
    CHECK(reloaded.RestoreSnapshot(saved)==bridge::InventoryResult::Success);
    CHECK(reloaded.FindContainer("player")->stacks.front().quantity==3);
    CHECK(reloaded.FindContainer("chest")->stacks.front().quantity==2);
    ctx.active=false;
    lua_close(L);host.Reset();
    fs::remove_all(project,ec);
    std::cout<<"G3 authored scene -> scoped Lua -> Player/chest transaction PASS\n";
}
