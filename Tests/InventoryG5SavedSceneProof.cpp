#include "InventoryG5DisposableProject.h"
// Non-destructive verification of the *actual saved* G5 inventory gameplay level.
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/WorldPickupController.h"
#include "renegade/bridge/PlayerService.h"
#include <wiArchive.h>
#include <windows.h>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace renegade::bridge;
namespace fs=std::filesystem;
#define CHECK(x) do { if(!(x)){std::cerr<<"G5 scene verify failed line "<<__LINE__<<": "<<#x<<std::endl;return 1;} }while(false)
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::error_code ec;
    const auto root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
    CHECK(!ec && root.filename()=="G5Play");
    CHECK(IsDisposableG5Project(root,"G5Play"));
    wi::Application app;
    auto window=CreateWindowExW(0,L"STATIC",L"G5 saved pickup scene verification",
        WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    CHECK(window!=nullptr);
    app.allow_hdr=false;
    app.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    wi::scene::Scene scene;
    wi::Archive archive((root/"Content"/"Scenes"/"LevelOne.wiscene").generic_u8string(),true,false);
    CHECK(archive.IsOpen());
    scene.Serialize(archive);
    InventoryItemCatalogue catalogue;
    std::string error;
    CHECK(ReadInventoryItemCatalogue(
        (root/"Content"/"Gameplay"/"Inventory.renegade-items").generic_u8string(),
        "61111111-1111-4111-8111-111111111111",catalogue,error));
    const auto* potion=FindInventoryAuthoringItem(catalogue,"health-potion");
    CHECK(potion && potion->healthRestored==50 && potion->maximumStack==10);
    InventoryService inventory;
    CHECK(inventory.LoadSnapshot((root/"Content"/"Scenes"/"LevelOne.wiscene.rinventory").generic_u8string())==InventoryResult::Success);
    CHECK(inventory.FindContainer("player") && inventory.FindContainer("chest"));
    CHECK(inventory.FindContainer("player")->stacks.empty());
    CHECK(inventory.FindContainer("chest")->stacks.size()==1);
    CHECK(inventory.FindContainer("chest")->stacks[0].quantity==2);
    const auto player=ResolvePlayerStart(scene);
    CHECK(player.resolution==PlayerStartResolution::Success);
    const auto* pose=scene.transforms.GetComponent(player.start.entity);
    CHECK(pose!=nullptr);
    const auto atStart=pose->GetPosition();
    CHECK(scene.Entity_FindByName("G5 Test Platform (walkable)")!=wi::ecs::INVALID_ENTITY);
    CHECK(scene.Entity_FindByName("G5 TEST - Ordinary Stone (NOT COLLECTABLE)")!=wi::ecs::INVALID_ENTITY);
    CHECK(scene.Entity_FindByName("G5 TEST - Storage Chest (INVENTORY I)")!=wi::ecs::INVALID_ENTITY);
    WorldPickupController controller;
    std::vector<std::string> diagnostics;
    controller.OpenForScene(scene,inventory,diagnostics);
    CHECK(diagnostics.empty());
    CHECK(controller.CandidateCount()==2);
    InventoryGameplayGateway gateway(inventory);
    gateway.Grant("player",{true,true,true});
    auto frame=controller.Update(scene,atStart,false,true,gateway);
    CHECK(!frame.collected && !frame.prompt.empty());
    CHECK(frame.prompt.find("Health Potion")!=std::string::npos);
    frame=controller.Update(scene,atStart,true,true,gateway);
    CHECK(frame.collected && frame.consumedInteract);
    CHECK(inventory.FindContainer("player")->stacks.size()==1);
    CHECK(inventory.FindContainer("player")->stacks[0].quantity==3);
    CHECK(scene.Entity_FindByName("G5 TEST - Health Potion x3 (PRESS E)")==wi::ecs::INVALID_ENTITY);
    frame=controller.Update(scene,DirectX::XMFLOAT3(atStart.x+3.0f,atStart.y,atStart.z+4.0f),false,true,gateway);
    CHECK(frame.collected && !frame.consumedInteract);
    CHECK(inventory.FindContainer("player")->stacks[0].quantity==4);
    CHECK(controller.CandidateCount()==0);
    CHECK(scene.Entity_FindByName("G5 TEST - Ordinary Stone (NOT COLLECTABLE)")!=wi::ecs::INVALID_ENTITY);
    std::cout<<"PASS: G5 actual saved scene, E pickup x3, touch pickup x1, item catalogue, chest and ordinary props"<<std::endl;
    std::cout.flush();
    std::_Exit(0); // Exit after Wicked GPU fixture; no mutation of saved project.
}
