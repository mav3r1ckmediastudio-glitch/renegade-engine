#include "InventoryUiInputPolicy.h"
#include <cstdlib>
#include <iostream>
namespace {
struct PlayerInput { bool jumpPressed=false; };
struct ActionInput {
    PlayerInput player;
    bool interactPressed=false,pausePressed=false,firePressed=false,
         fireDown=false,reloadPressed=false,aimDown=false;
};
}
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL: "<<#x<<" line "<<__LINE__<<"\n";std::abort();}}while(false)
int main() {
    using namespace renegade::runtime;
    CHECK(InventoryInteractHasFocus(true,true,true,false,false));
    CHECK(!InventoryInteractHasFocus(false,true,true,false,false));
    CHECK(!InventoryInteractHasFocus(true,false,true,false,false));
    CHECK(!InventoryInteractHasFocus(true,true,false,false,false));
    CHECK(!InventoryInteractHasFocus(true,true,true,true,false));
    CHECK(!InventoryInteractHasFocus(true,true,true,false,true));
    ActionInput incoming;
    incoming.interactPressed=true;incoming.pausePressed=true;
    incoming.firePressed=true;incoming.fireDown=true;
    incoming.reloadPressed=true;incoming.aimDown=true;
    incoming.player.jumpPressed=true;
    const auto gated=GateWorldInputForInventory(incoming,true);
    CHECK(!gated.interactPressed&&gated.pausePressed);
    CHECK(!gated.firePressed&&!gated.fireDown&&!gated.reloadPressed&&
          !gated.aimDown&&!gated.player.jumpPressed);
    const auto normal=GateWorldInputForInventory(incoming,false);
    CHECK(normal.firePressed&&normal.player.jumpPressed);
    std::cout<<"G5G modal inventory owns E; world scripts blocked, pause preserved PASS\n";
}
