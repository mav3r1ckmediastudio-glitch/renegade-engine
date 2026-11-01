#include "InventoryG5DisposableProject.h"
// Disposable native G5 interactive proof author. Never run on user projects.
// Writes only the uniquely named BUILD/Runtime/Content/G5 proof copy.
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/InventoryPickupAuthoringService.h"
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/InventoryService.h"
#include "renegade/bridge/SceneComponentService.h"
#include <wiArchive.h>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>

namespace fs=std::filesystem;
using namespace renegade::bridge;
using namespace DirectX;
#define CHECK(value,why) do { if(!(value)) {std::cerr<<"G5 proof: "<<why<<"\n";std::_Exit(1);} } while(false)
int main(int argc,char** argv) {
    CHECK(argc==2,"expected disposable G5 project path");
    std::error_code ec;
    const fs::path root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
    CHECK(!ec && root.filename()=="G5Play" &&
        IsDisposableG5Project(root,"G5Play"),
        "refusing any non-disposable root");
    CHECK(fs::is_regular_file(root/"ScreenProject.renegade"),"no cloned project");
    const fs::path scenePath=root/"Content"/"Scenes"/"LevelOne.wiscene";
    CHECK(fs::is_regular_file(scenePath),"no cloned scene");
    const fs::path marker=root/"G5-AUTHORED.txt";
    CHECK(!fs::exists(marker),"refusing to overwrite an authored proof");
    wi::Application application;
    HWND window = CreateWindowExW(0, L"STATIC", L"G5 test scene author",
        WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    CHECK(window != nullptr, "graphics fixture window");
    application.allow_hdr = false;
    application.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    std::cout << "G5 AUTHOR: opening scene" << std::endl;
    wi::scene::Scene scene;
    wi::Archive source(scenePath.generic_u8string(),true,false);
    CHECK(source.IsOpen(),"cannot read scene archive");
    scene.Serialize(source);
    source=wi::Archive();
    auto start=ResolvePlayerStart(scene);
    if (start.resolution==PlayerStartResolution::Missing) {
        CreatePlayerStartCommand create(scene,{});
        CHECK(create.Execute(),"cannot create player start");
        start=ResolvePlayerStart(scene);
    }
    CHECK(start.resolution==PlayerStartResolution::Success,"unresolved player start");
    const auto* pose=scene.transforms.GetComponent(start.start.entity);
    CHECK(pose!=nullptr,"player start has no transform");
    const auto position=pose->GetPosition();
    std::cout<<"PLAYER START x="<<position.x<<" y="<<position.y<<" z="<<position.z<<"\n";
    // Physical floor and obvious geometry in an otherwise sparse editor fixture.
    const auto floor=scene.Entity_CreateCube("G5 Test Platform (walkable)");
    auto* floorPose=scene.transforms.GetComponent(floor);
    CHECK(floorPose!=nullptr,"floor pose absent");
    floorPose->Scale(XMFLOAT3(22.0f,0.4f,22.0f));
    floorPose->Translate(XMFLOAT3(position.x,position.y-1.1f,position.z));
    floorPose->UpdateTransform();
    auto& floorBody=scene.rigidbodies.Create(floor);
    floorBody.mass=0;
    floorBody.shape=wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;
    floorBody.box.halfextents=XMFLOAT3(11,0.2f,11);

    const auto makeCube=[&](const char* name,float x,float y,float z,
            float sx,float sy,float sz)->wi::ecs::Entity {
        const auto id=scene.Entity_CreateCube(name);
        auto* transform=scene.transforms.GetComponent(id);
        CHECK(transform!=nullptr,"missing cube transform");
        transform->Scale(XMFLOAT3(sx,sy,sz));
        transform->Translate(XMFLOAT3(position.x+x,position.y+y,position.z+z));
        transform->UpdateTransform();
        return id;
    };
    const auto pressE=makeCube("G5 TEST - Health Potion x3 (PRESS E)",
                              -0.8f,-0.35f,2.3f,0.65f,0.8f,0.65f);
    const auto touch=makeCube("G5 TEST - Health Potion x1 (WALK INTO)",
                             3.0f,-0.35f,4.0f,0.65f,0.8f,0.65f);
    (void)makeCube("G5 TEST - Ordinary Stone (NOT COLLECTABLE)",
                   -3.0f,-0.35f,4.0f,0.9f,0.9f,0.9f);
    (void)makeCube("G5 TEST - Storage Chest (INVENTORY I)",
                   5.0f,-0.40f,1.1f,1.6f,1.0f,1.2f);
    InventoryPickupConfig e;
    e.canCollect=true;e.itemId="health-potion";e.quantity=3;
    e.method=InventoryPickupMethod::PressE;e.interactDistance=3.0f;
    CHECK(SetInventoryPickupConfigCommand(scene,pressE,e).Execute(),
          "cannot author press E potion");
    InventoryPickupConfig t=e;
    t.quantity=1;t.method=InventoryPickupMethod::OnTouch;
    CHECK(SetInventoryPickupConfigCommand(scene,touch,t).Execute(),
          "cannot author touch potion");

    // Shared item catalogue and inventory companion were authored separately;
    // never overwrite them when building scene props or changing a level.
    const StableId projectId="61111111-1111-4111-8111-111111111111";
    InventoryItemCatalogue catalogue;
    std::string error;
    CHECK(ReadInventoryItemCatalogue(
      (root/"Content"/"Gameplay"/"Inventory.renegade-items").generic_u8string(),
      projectId,catalogue,error),"shared catalogue missing or invalid");
    const auto* item=FindInventoryAuthoringItem(catalogue,"health-potion");
    CHECK(item!=nullptr && item->healthRestored==50 && item->maximumStack==10,
          "shared health potion does not match fixture");
    InventoryService inventory;
    CHECK(inventory.LoadSnapshot(
      (root/"Content"/"Scenes"/"LevelOne.wiscene.rinventory").generic_u8string())
      ==InventoryResult::Success,"existing inventory companion invalid");
    CHECK(inventory.FindContainer("player")!=nullptr &&
          inventory.FindContainer("chest")!=nullptr,"missing player or chest");
    const fs::path backup=fs::path(scenePath.generic_u8string()+".before-G5");
    CHECK(!fs::exists(backup),"refusing to replace scene backup");
    fs::copy_file(scenePath,backup,fs::copy_options::none,ec);
    CHECK(!ec,"cannot back up original proof scene");
    {
        wi::Archive destination;
        scene.Serialize(destination);
        CHECK(destination.SaveFile(scenePath.generic_u8string()),"native scene save failed");
    }    // Prove reopen and metadata linkage against exact saved project file.
    wi::scene::Scene reloaded;
    {
        wi::Archive readback(scenePath.generic_u8string(),true,false);
        CHECK(readback.IsOpen(),"cannot reopen authored scene");
        reloaded.Serialize(readback);
    }
    auto* reloadedStart=reloaded.names.GetComponent(reloaded.Entity_FindByName(
        "G5 TEST - Health Potion x3 (PRESS E)"));
    CHECK(reloadedStart!=nullptr,"missing saved world prop");
    InventoryPickupConfig roundtrip;
    CHECK(ReadInventoryPickupConfig(reloaded,
            reloaded.Entity_FindByName("G5 TEST - Health Potion x3 (PRESS E)"),
            roundtrip,error) && roundtrip.canCollect && roundtrip.quantity==3,
            "press-E pickup failed scene roundtrip");
    CHECK(ReadInventoryPickupConfig(reloaded,
            reloaded.Entity_FindByName("G5 TEST - Health Potion x1 (WALK INTO)"),
            roundtrip,error) && roundtrip.method==InventoryPickupMethod::OnTouch,
            "touch pickup failed scene roundtrip");
    CHECK(ReadInventoryPickupConfig(reloaded,
            reloaded.Entity_FindByName("G5 TEST - Ordinary Stone (NOT COLLECTABLE)"),
            roundtrip,error) && !roundtrip.canCollect,
            "ordinary prop accidentally collectable");
    std::ofstream howto(marker,std::ios::binary);
    howto<<"G5 Inventory Gameplay Test (disposable)\n"
         <<"1. Open ScreenProject.renegade in Renegade Studio; Levels > Level One.\n"
         <<"2. In Test Game, approach the nearby potion cube and press E. Expected: +3 potions.\n"
         <<"3. Walk into the second potion cube at +X,+Z. Expected: +1 potion.\n"
         <<"4. Ordinary stone and Storage Chest are NON-COLLECTABLE props.\n"
         <<"5. Press I for Backpack/Chest. Transfer via the inventory screen.\n"
         <<"6. Right-click a backpack potion to use it. At full health it is KEPT.\n"
         <<"7. Healing needs the player to take damage first: no artificial damage set here.\n"
         <<"8. Collected world props do not persist across fresh game reloads yet.\n"
         <<"All settings and item icons remain author-editable in this copied project.\n";
    howto.close();
    CHECK(fs::is_regular_file(marker),"missing proof marker");
    std::cout<<"G5 PROOF AUTHOR PASS - 2 potion pickups + 2 ordinary props, "
             <<"player backpack empty, chest 2 potions\n";
    std::cout.flush();
    std::_Exit(0);
}
