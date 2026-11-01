#include "RuntimeGameSaveSlot.h"
#include "renegade/bridge/IdentityService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs=std::filesystem;
using renegade::runtime::RuntimeGameSaveSlot;
using renegade::runtime::RuntimeGameSaveState;
using renegade::bridge::GenerateStableId;
#define CHECK(v) do { if(!(v)) { std::cerr<<"G5 Save slot failed at "<<__LINE__<<": "<<#v<<"\n"; return 1; } } while(false)
int main() {
    std::string error;
    const auto root=fs::temp_directory_path()/("renegade-g5-save-slot-"+GenerateStableId());
    std::error_code ec;
    fs::create_directories(root,ec);CHECK(!ec);
    const auto local=root.generic_u8string();
    const auto productId=GenerateStableId();
    const auto sceneId=GenerateStableId();
    const auto saveDataId=GenerateStableId();
    RuntimeGameSaveState written;
    written.projectId=productId;
    written.sceneId=sceneId;
    written.inventoryBytes="RENEGADE_INVENTORY_SNAPSHOT 2\nDATA"; // opaque to the slot layer
    const auto droppedId=GenerateStableId();
    // The save-slot owner must preserve the complete v2 world payload,
    // including a dynamically dropped stack with stable identity and pose.
    written.retiredWorldBytes="RENEGADE_WORLD_PICKUPS 2 "+productId+" "+
        sceneId+" 0 1\n"+droppedId+" potion 3 2.125 0.25 -4.5\n";
    written.playerHealth=45.125f;
    written.playerMaxHealth=100.0f;
    written.playerX=8.25f;
    written.playerY=2.75f;
    written.playerZ=-4.5f;
    written.playerYaw=0.75f;
    written.playerPitch=-0.5f;
    RuntimeGameSaveState retrieved;
    CHECK(!RuntimeGameSaveSlot::ReadQuick(local,saveDataId,
        productId,sceneId,retrieved,error));
    if(!RuntimeGameSaveSlot::WriteQuick(local,saveDataId,written,error)) {
        std::cerr << "G5 WriteQuick failure: " << error << '\n'; return 1;
    }
    CHECK(RuntimeGameSaveSlot::ReadQuick(local,saveDataId,
        productId,sceneId,retrieved,error));
    CHECK(retrieved.inventoryBytes==written.inventoryBytes);
    CHECK(retrieved.retiredWorldBytes==written.retiredWorldBytes);
    CHECK(retrieved.playerHealth==45.125f && retrieved.playerMaxHealth==100.f);
    CHECK(retrieved.playerX==8.25f && retrieved.playerY==2.75f &&
          retrieved.playerZ==-4.5f && retrieved.playerYaw==0.75f &&
          retrieved.playerPitch==-0.5f);
    CHECK(!RuntimeGameSaveSlot::ReadQuick(local,saveDataId,
        GenerateStableId(),sceneId,retrieved,error));
    CHECK(!RuntimeGameSaveSlot::ReadQuick(local,saveDataId,
        productId,GenerateStableId(),retrieved,error));
    auto bad=written;
    bad.playerHealth=200.0f;
    CHECK(!RuntimeGameSaveSlot::WriteQuick(local,saveDataId,bad,error));
    CHECK(RuntimeGameSaveSlot::ReadQuick(local,saveDataId,
        productId,sceneId,retrieved,error));
    CHECK(retrieved.playerHealth==45.125f); // rejected write preserved good slot
    const auto path=root/"RenegadeEngine"/saveDataId/"Saves"/"quick.rsave";
    CHECK(fs::is_regular_file(path));
    {
        std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);
        CHECK(file.good());
        file.seekp(-1,std::ios::end);
        file.put('X');
    }
    CHECK(!RuntimeGameSaveSlot::ReadQuick(local,saveDataId,
        productId,sceneId,retrieved,error));
    CHECK(error.find("corruption")!=std::string::npos);
    CHECK(RuntimeGameSaveSlot::WriteQuick(local,saveDataId,written,error));
    CHECK(RuntimeGameSaveSlot::ReadQuick(local,saveDataId,
        productId,sceneId,retrieved,error));
    fs::remove_all(root,ec);
    CHECK(!ec);
    std::cout<<"PASS G5 governed quick-save slot atomic replacement, corruption and identity\n";
    return 0;
}
