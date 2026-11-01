#include "renegade/bridge/WorldPickupController.h"
#include "renegade/bridge/IdentityService.h"
#include <WickedEngine.h>
#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#define CHECK(v) do { if (!(v)) { std::cerr << "WorldPickupControllerTests line " \
    << __LINE__ << ": " << #v << std::endl; return 1; } } while (false)
using namespace renegade::bridge;
int main(int argc,char** argv) {
    // Normal CTest is CPU-only and must run on a hosted runner with no GPU
    // or audio endpoint. The optional graphics proof exercises THE SAME
    // assertions, but explicitly creates a DX12 device on owner hardware.
    std::unique_ptr<wi::Application> graphicsProof;
    if(argc==2 && std::string(argv[1])=="--graphics-proof") {
        graphicsProof=std::make_unique<wi::Application>();
        HWND window=CreateWindowExW(0,L"STATIC",L"G5H GPU world drop proof",
            WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,
            GetModuleHandleW(nullptr),nullptr);
        CHECK(window!=nullptr);
        graphicsProof->allow_hdr=false;
        graphicsProof->SetWindow(window);
        CHECK(wi::graphics::GetDevice()!=nullptr);
    } else {
        CHECK(argc==1 && wi::graphics::GetDevice()==nullptr);
    }
    wi::scene::Scene scene;
    InventoryService inventory;
    CHECK(inventory.RegisterItem({"potion","Health Potion",3,0.2,{}})
          ==InventoryResult::Success);
    CHECK(inventory.CreateContainer("player",2)==InventoryResult::Success);
    InventoryGameplayGateway gateway(inventory);
    gateway.Grant("player",{true,true,true});

    const auto world=scene.Entity_CreateTransform("Bottle");
    const auto disabled=scene.Entity_CreateTransform("Tagged Prop");
    scene.metadatas.Create(disabled).preset=wi::scene::MetadataComponent::Preset::Pickup;
    auto& anchor=*scene.transforms.GetComponent(world);
    anchor.Translate(XMFLOAT3(0,0,1));
    anchor.UpdateTransform();

    InventoryPickupConfig config;
    config.canCollect=true; config.itemId="potion"; config.quantity=4;
    config.method=InventoryPickupMethod::PressE;
    config.interactDistance=3.0f;
    CommandService history;
    CHECK(history.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        scene,world,config)));
    const std::string originalPickupId=PersistentEntityId(scene,world);
    CHECK(IsValidStableId(originalPickupId));
    WorldPickupController pickups;
    std::vector<std::string> diagnostics;
    pickups.OpenForScene(scene,inventory,diagnostics);
    CHECK(diagnostics.empty() && pickups.CandidateCount()==1);
    const XMFLOAT3 player={0,0,0};
    auto frame=pickups.Update(scene,player,false,true,gateway);
    CHECK(!frame.collected && !frame.consumedInteract);
    CHECK(frame.prompt.find("Health Potion")!=std::string::npos);
    frame=pickups.Update(scene,player,true,false,gateway);
    CHECK(!frame.collected && scene.transforms.Contains(world));
    frame=pickups.Update(scene,player,true,true,gateway);
    CHECK(frame.collected && frame.consumedInteract);
    CHECK(inventory.FindContainer("player")->stacks.size()==2);
    CHECK(!scene.transforms.Contains(world));
    CHECK(pickups.CandidateCount()==0);
    const auto projectId=GenerateStableId();
    const auto sceneId=GenerateStableId();
    std::string worldBytes,saveError;
    CHECK(pickups.CaptureRuntimeState(projectId,sceneId,worldBytes,saveError));
    CHECK(worldBytes.find("RENEGADE_WORLD_PICKUPS 2")==0);
    CHECK(worldBytes.find(originalPickupId)!=std::string::npos);
    // Save Game restoration must be bound to both project and scene identity,
    // reject damage without mutating the world, and remove the same object on
    // a freshly loaded authored scene.
    wi::scene::Scene restoredScene;
    const auto restoredObject=restoredScene.Entity_CreateTransform("Saved Bottle");
    CHECK(CommandService{}.Execute(
        std::make_unique<SetInventoryPickupConfigCommand>(
            restoredScene,restoredObject,config)));
    std::string identityError;
    CHECK(AssignPersistentEntityId(restoredScene,restoredObject,
                                   originalPickupId,identityError));
    WorldPickupController reloadedPickups;
    reloadedPickups.OpenForScene(restoredScene,inventory,diagnostics);
    CHECK(diagnostics.empty() && reloadedPickups.CandidateCount()==1);
    // A valid dry-run must not retire the saved entity; Runtime uses this
    // preflight before committing inventory and world replay after reload.
    CHECK(reloadedPickups.RestoreRuntimeState(restoredScene,projectId,
        sceneId,worldBytes,saveError,true));
    CHECK(restoredScene.transforms.Contains(restoredObject));
    CHECK(reloadedPickups.CandidateCount()==1);
    CHECK(!reloadedPickups.RestoreRuntimeState(restoredScene,projectId,
        GenerateStableId(),worldBytes,saveError));
    CHECK(restoredScene.transforms.Contains(restoredObject));
    CHECK(!reloadedPickups.RestoreRuntimeState(restoredScene,projectId,
        sceneId,worldBytes+"CORRUPTION",saveError));
    CHECK(restoredScene.transforms.Contains(restoredObject));
    CHECK(reloadedPickups.RestoreRuntimeState(restoredScene,projectId,
        sceneId,worldBytes,saveError));
    CHECK(reloadedPickups.CandidateCount()==0);
    CHECK(!restoredScene.transforms.Contains(restoredObject));
    auto revision=inventory.Revision();
    frame=pickups.Update(scene,player,true,true,gateway);
    CHECK(!frame.collected && inventory.Revision()==revision);

    const auto fail=scene.Entity_CreateTransform("More");
    InventoryPickupConfig tooMany=config;
    tooMany.quantity=7;
    CHECK(history.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        scene,fail,tooMany)));
    pickups.OpenForScene(scene,inventory,diagnostics);
    CHECK(diagnostics.empty());
    frame=pickups.Update(scene,player,true,true,gateway);
    CHECK(!frame.collected && frame.consumedInteract);
    CHECK(frame.result==InventoryResult::CapacityExceeded);
    CHECK(scene.transforms.Contains(fail) && inventory.Revision()==revision);

    // On Touch is latched until the player leaves contact; do not award items
    // continuously each frame when the creator leaves the prop visible.
    wi::scene::Scene touchScene;
    const auto touch=touchScene.Entity_CreateTransform("Touch");
    InventoryPickupConfig touchConfig=config;
    touchConfig.method=InventoryPickupMethod::OnTouch;
    touchConfig.quantity=1;
    touchConfig.removeWhenCollected=false;
    CHECK(CommandService{}.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        touchScene,touch,touchConfig)));
    InventoryService touchInventory;
    CHECK(touchInventory.RegisterItem({"potion","Health Potion",20,0.0,{}})
          ==InventoryResult::Success);
    CHECK(touchInventory.CreateContainer("player",10)==InventoryResult::Success);
    InventoryGameplayGateway touchGateway(touchInventory);
    touchGateway.Grant("player",{true,true,true});
    pickups.OpenForScene(touchScene,touchInventory,diagnostics);
    CHECK(pickups.CandidateCount()==1);
    CHECK(pickups.Update(touchScene,player,false,true,touchGateway).collected);
    revision=touchInventory.Revision();
    CHECK(!pickups.Update(touchScene,player,false,true,touchGateway).collected);
    CHECK(touchInventory.Revision()==revision);
    CHECK(touchScene.transforms.Contains(touch));
    pickups.Update(touchScene,{10,0,0},false,true,touchGateway);
    CHECK(pickups.Update(touchScene,player,false,true,touchGateway).collected);
    CHECK(touchInventory.FindContainer("player")->stacks[0].quantity==2);

    // G5H: dynamically dropped physical pickups use the SAME item ID and
    // quantity, survive world state save, and can be collected back once.
    InventoryService dropInventory;
    CHECK(dropInventory.RegisterItem({"wood","Oak Wood",20,0.1,{}})
          ==InventoryResult::Success);
    CHECK(dropInventory.CreateContainer("player",4)==InventoryResult::Success);
    CHECK(dropInventory.Add("player","wood",5)==InventoryResult::Success);
    InventoryGameplayGateway dropGateway(dropInventory);
    dropGateway.Grant("player",{true,true,true});
    wi::scene::Scene dropScene;
    WorldPickupController drops;
    drops.OpenForScene(dropScene,dropInventory,diagnostics);
    const auto held=dropInventory.FindContainer("player")->stacks.front().instanceId;
    std::string dropError;
    CHECK(drops.DropFromPlayer(dropScene,dropInventory,dropGateway,
        held,3,{1.0f,0.25f,1.0f},dropError)==InventoryResult::Success);
    CHECK(dropInventory.FindContainer("player")->stacks.front().quantity==2);
    CHECK(drops.DroppedCount()==1 && drops.CandidateCount()==1);
    // The CPU-only native path must still create a fully shaped visible
    // sphere entity, with exactly Wicked's 12x12 mesh topology/material.
    CHECK(dropScene.objects.GetCount()==1);
    const auto worldSphere=dropScene.objects.GetEntity(0);
    const auto* worldObject=dropScene.objects.GetComponent(worldSphere);
    CHECK(worldObject && worldObject->meshID==worldSphere);
    const auto* worldMesh=dropScene.meshes.GetComponent(worldSphere);
    CHECK(worldMesh && worldMesh->vertex_positions.size()==169);
    CHECK(worldMesh->vertex_normals.size()==169 &&
          worldMesh->vertex_uvset_0.size()==169);
    CHECK(worldMesh->indices.size()==864 && worldMesh->subsets.size()==1);
    CHECK(worldMesh->subsets[0].indexCount==864 &&
          worldMesh->subsets[0].materialID==worldSphere);
    CHECK(dropScene.materials.Contains(worldSphere));
    const auto forbidden=dropInventory.Revision();
    InventoryGameplayGateway denied(dropInventory); // no withdraw permission
    CHECK(drops.DropFromPlayer(dropScene,dropInventory,denied,
        held,1,{1.4f,0.25f,1.0f},dropError)==InventoryResult::PermissionDenied);
    CHECK(dropInventory.Revision()==forbidden && drops.DroppedCount()==1);
    CHECK(drops.CaptureRuntimeState(projectId,sceneId,worldBytes,dropError));
    CHECK(worldBytes.find("RENEGADE_WORLD_PICKUPS 2 ")==0);
    wi::scene::Scene loadScene;
    WorldPickupController loadedDrops;
    loadedDrops.OpenForScene(loadScene,dropInventory,diagnostics);
    CHECK(!loadedDrops.RestoreRuntimeState(loadScene,projectId,sceneId,
        worldBytes,dropError,true,nullptr));
    CHECK(loadedDrops.DroppedCount()==0);
    CHECK(!loadedDrops.RestoreRuntimeState(loadScene,projectId,sceneId,
        worldBytes+"malformed",dropError,true,&dropInventory));
    CHECK(loadedDrops.DroppedCount()==0);
    CHECK(loadedDrops.RestoreRuntimeState(loadScene,projectId,sceneId,
        worldBytes,dropError,true,&dropInventory));
    CHECK(loadedDrops.DroppedCount()==0);
    CHECK(loadedDrops.RestoreRuntimeState(loadScene,projectId,sceneId,
        worldBytes,dropError,false,&dropInventory));
    CHECK(loadedDrops.DroppedCount()==1 && loadedDrops.CandidateCount()==1);
    CHECK(loadScene.objects.GetCount()==1);
    const auto* restoredMesh=loadScene.meshes.GetComponent(
        loadScene.objects.GetEntity(0));
    CHECK(restoredMesh && restoredMesh->vertex_positions.size()==169 &&
          restoredMesh->indices.size()==864 &&
          restoredMesh->subsets.size()==1);
    CHECK(loadedDrops.Update(loadScene,{1.0f,0.25f,0.0f},
        true,true,dropGateway).collected);
    CHECK(loadedDrops.DroppedCount()==0 && loadedDrops.CandidateCount()==0);
    CHECK(dropInventory.FindContainer("player")->stacks.front().quantity==5);
    CHECK(!loadedDrops.Update(loadScene,{1.0f,0.25f,0.0f},
        true,true,dropGateway).collected);
    // A filled backpack cannot collect the spawned item. It must remain
    // visible in the world and retain its exact quantity for later recovery.
    InventoryService fullInventory;
    CHECK(fullInventory.RegisterItem({"potion","Health Potion",3,0.2,{}})
        ==InventoryResult::Success);
    CHECK(fullInventory.CreateContainer("player",1)==InventoryResult::Success);
    CHECK(fullInventory.Add("player","potion",3)==InventoryResult::Success);
    InventoryGameplayGateway fullGateway(fullInventory);
    fullGateway.Grant("player",{true,true,true});
    wi::scene::Scene fullScene;
    WorldPickupController fullDrops;
    fullDrops.OpenForScene(fullScene,fullInventory,diagnostics);
    const auto fullId=fullInventory.FindContainer("player")->stacks[0].instanceId;
    CHECK(fullDrops.DropFromPlayer(fullScene,fullInventory,fullGateway,
        fullId,1,{1.0f,.25f,1.0f},dropError)==InventoryResult::Success);
    CHECK(fullInventory.Add("player","potion",1)==InventoryResult::Success);
    const auto blockedCollect=fullDrops.Update(fullScene,{1.0f,.25f,0.0f},
        true,true,fullGateway);
    CHECK(!blockedCollect.collected &&
          blockedCollect.result==InventoryResult::CapacityExceeded);
    CHECK(fullDrops.DroppedCount()==1 && fullDrops.CandidateCount()==1);
    CHECK(fullInventory.FindContainer("player")->stacks[0].quantity==3);
    WorldPickupController legacyDrops;
    wi::scene::Scene legacyScene;
    legacyDrops.OpenForScene(legacyScene,dropInventory,diagnostics);
    const std::string v1="RENEGADE_WORLD_PICKUPS 1 "+projectId+" "+sceneId+" 0\n";
    CHECK(legacyDrops.RestoreRuntimeState(legacyScene,projectId,sceneId,
        v1,dropError,true));
    // A no-inventory world and a legacy semantic-only PICKUP never grant items.
    InventoryService empty;
    WorldPickupController unbound;
    unbound.OpenForScene(scene,empty,diagnostics);
    CHECK(unbound.CandidateCount()==0 && !diagnostics.empty());
    std::cout<<"PASS: authored pickups, native sphere mesh, physical drop/collect, "
             <<"atomic rollback, v1/v2 save validation; device="
             <<(graphicsProof ? "DX12" : "CPU-only")<<std::endl;
    std::cout.flush();
    std::_Exit(0);
}
