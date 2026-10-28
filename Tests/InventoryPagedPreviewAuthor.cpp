// Guarded G4-B3C1B sample authoring utility: only a copied disposable
// Inventory-Paged-Proof fixture under the isolated BUILD output is writable.
#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include "renegade/bridge/IdentityService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
using namespace renegade::bridge;
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::error_code ec;
    const fs::path root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
    const bool allowedName=root.filename()=="Inventory-Paged-Proof" ||
        root.filename()=="Inventory-Scrollbar-Proof";
    if(ec||!allowedName||
       root.generic_u8string().find(
          "/BUILD/inventory-full/Runtime/Release/Content/Inventory-")
           ==std::string::npos ||
       !fs::is_regular_file(root/"ScreenProject.renegade")) {
        std::cerr<<"Refusing non-disposable inventory fixture\n";return 3;
    }
    const std::string relative="Content/UI/InventoryG4B3C1BGrid.renegade-screen";
    const auto target=root/fs::u8path(relative);
    const auto marker=root/"Content/UI/InventoryG4B3C1BGrid.rinvui";
    if(fs::exists(target)||fs::exists(marker)) {
        std::cerr<<"Refusing to overwrite paged Screen or marker\n";return 4;
    }
    DocumentEnvelope sceneMeta;
    std::string error;
    if(!ReadDocumentEnvelope(
          (root/"Content/Scenes/LevelOne.wiscene.rmeta").generic_u8string(),
          sceneMeta,error)) {
        std::cerr<<"Invalid scene identity: "<<error<<"\n";return 5;
    }
    InventoryGridLayout layout;
    layout.visibleRows=4;layout.columns=5;
    const auto doc=MakeInventoryPagedGridScreen(
        sceneMeta.projectId,relative,layout);
    if(!ValidateScreenDocument(doc,sceneMeta.projectId,error) ||
       !WriteScreenDocument(target.generic_u8string(),doc,error)) {
        std::cerr<<"Paged Screen invalid: "<<error<<"\n";return 6;
    }
    ScreenDocument reread;
    if(!ReadScreenDocument(target.generic_u8string(),
            sceneMeta.projectId,reread,error)||
       reread.widgets.size()!=doc.widgets.size() ||
       reread.focusOrder.size()!=44 ||
       reread.enabledInGame) {
        std::cerr<<"Paged Screen round trip failed: "<<error<<"\n";return 7;
    }
    InventoryService inventory;
    if(inventory.RegisterItem({"health-potion","Health Potion",3,0.2,{}})
            !=InventoryResult::Success ||
       inventory.CreateContainer("player",100,0)!=InventoryResult::Success ||
       inventory.CreateContainer("chest",100,0)!=InventoryResult::Success)
        return 8;
    for(int i=0;i<55;++i)
        if(inventory.Add("player","health-potion",3)!=InventoryResult::Success)
            return 9;
    for(int i=0;i<6;++i)
        if(inventory.Add("chest","health-potion",3)!=InventoryResult::Success)
            return 10;
    for(const char* scene:{"LevelOne","LevelTwo"}) {
        const auto companion=root/"Content/Scenes"/
            (std::string(scene)+".wiscene.rinventory");
        if(!fs::is_regular_file(companion) ||
           inventory.SaveSnapshot(companion.generic_u8string())!=
                InventoryResult::Success) {
            std::cerr<<"Cannot author paged sample inventory\n";return 11;
        }
    }
    {
        std::ofstream file(marker,std::ios::binary|std::ios::trunc);
        file<<"RENEGADE_INVENTORY_UI_PREVIEW 1\n";
        if(!file.good())return 12;
    }
    std::cout<<"Created editable G4B3C1B Screen "<<doc.widgets.size()
        <<" widgets, "<<doc.focusOrder.size()
        <<" actions, backpack 55 / chest 6 potion stacks\n";
}
