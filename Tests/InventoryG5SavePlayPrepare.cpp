#include "InventoryG5DisposableProject.h"
// G5D disposable proof ONLY: upgrade old G5 test pickups to stable scene IDs.
#include "renegade/bridge/InventoryPickupAuthoringService.h"
#include "renegade/bridge/IdentityService.h"
#include <wiArchive.h>
#include <windows.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs=std::filesystem;
using namespace renegade::bridge;
#define CHECK(x,msg) do {if(!(x)){std::cerr<<"G5D prepare: "<<msg<<"\n";return 1;}}while(false)
int main(int argc,char** argv) {
    CHECK(argc==2,"exactly one disposable project path required");
    std::error_code ec;
    const auto root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
    CHECK(!ec && root.filename()=="G5SavePlay" &&
        IsDisposableG5Project(root,"G5SavePlay"),
        "refusing to edit outside G5SavePlay sandbox");
    const auto path=root/"Content"/"Scenes"/"LevelOne.wiscene";
    const auto backup=fs::u8path(path.generic_u8string()+".before-G5D");
    CHECK(fs::is_regular_file(path) && !fs::exists(backup),"scene missing or test already authored");
    wi::Application application;
    HWND window=CreateWindowExW(0,L"STATIC",L"G5D sandbox setup",
        WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,
        GetModuleHandleW(nullptr),nullptr);
    CHECK(window!=nullptr,"could not initialize graphics proof window");
    application.allow_hdr=false;
    application.SetWindow(window);
    wi::initializer::InitializeComponentsImmediate();
    wi::scene::Scene scene;
    {
        wi::Archive reader(path.generic_u8string(),true,false);
        CHECK(reader.IsOpen(),"cannot open proof scene");
        scene.Serialize(reader);
    }
    std::string error;
    const char* names[]={
        "G5 TEST - Health Potion x3 (PRESS E)",
        "G5 TEST - Health Potion x1 (WALK INTO)"
    };
    for(const auto* name:names) {
        const auto entity=scene.Entity_FindByName(name);
        CHECK(entity!=wi::ecs::INVALID_ENTITY,"pickup proof entity missing");
        InventoryPickupConfig config;
        CHECK(ReadInventoryPickupConfig(scene,entity,config,error) &&
              config.canCollect && config.itemId=="health-potion",
              "pickup metadata missing");
        CHECK(PersistentEntityId(scene,entity).empty(),
              "fixture already upgraded; stop rather than overwrite");
        CHECK(SetInventoryPickupConfigCommand(scene,entity,config).Execute(),
              "could not reapply pickup settings");
        CHECK(IsValidStableId(PersistentEntityId(scene,entity)),
              "new persistent entity identity absent");
    }
    fs::copy_file(path,backup,fs::copy_options::none,ec);
    CHECK(!ec,"could not back up original sandbox scene");
    {
        wi::Archive writer;
        scene.Serialize(writer);
        CHECK(writer.SaveFile(path.generic_u8string()),"native scene write failed");
    }
    wi::scene::Scene reopened;
    {
        wi::Archive reader(path.generic_u8string(),true,false);
        CHECK(reader.IsOpen(),"reopen failed");
        reopened.Serialize(reader);
    }
    for(const auto* name:names) {
        const auto entity=reopened.Entity_FindByName(name);
        CHECK(IsValidStableId(PersistentEntityId(reopened,entity)),
              "stable ID did not persist in WISCENE");
    }
    std::cout<<"G5D isolated scene upgraded: two stable pickup IDs PASS\n";
    std::cout.flush();
    std::_Exit(0);
}
