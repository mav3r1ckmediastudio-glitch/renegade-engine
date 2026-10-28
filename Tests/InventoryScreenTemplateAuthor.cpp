// G4-B2A deliberately creates an *ordinary* editable Screen in the
// disposable Inventory-Proof sample project, never a new UI framework.
#include "renegade/bridge/InventoryScreenAdapter.h"
#include "renegade/bridge/IdentityService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
using namespace renegade::bridge;
int main(int argc,char** argv){
    if(argc!=3){std::cerr<<"Usage: InventoryScreenTemplateAuthor <Inventory-Proof root> <source-icon-dir>\n";return 2;}
    std::error_code ec;
    const fs::path root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
    if(ec||root.filename()!="Inventory-Proof"||
        root.generic_u8string().find(
          "/BUILD/inventory-full/Runtime/Release/Content/Inventory-Proof")==std::string::npos||
        !fs::is_regular_file(root/"ScreenProject.renegade")) {
        std::cerr<<"Refusing anything outside the isolated Inventory-Proof BUILD fixture\n";
        return 3;
    }
    DocumentEnvelope meta;
    std::string error;
    if(!ReadDocumentEnvelope(
          (root/"Content/Scenes/LevelOne.wiscene.rmeta").generic_u8string(),meta,error)){
        std::cerr<<"Invalid Scene identity: "<<error<<"\n";return 4;
    }
    const std::string asset="Content/UI/InventoryG4B3B2Grid.renegade-screen";
    const fs::path target=root/fs::u8path(asset);
    if(fs::exists(target,ec)) {
        std::cerr<<"Refusing to overwrite an existing editable Screen\n";
        return 5;
    }
    fs::create_directories(target.parent_path(),ec);
    if(ec){std::cerr<<"Could not create UI directory\n";return 6;}
    // Copy original art into the confined fixture before writing the normal
    // Screen. Item icons are regular project-contained PNG resources.
    const fs::path icons=fs::weakly_canonical(fs::u8path(argv[2]),ec);
    if(ec || !fs::is_directory(icons))return 11;
    const fs::path dest=root/"Content/UI/InventoryIcons";
    fs::create_directories(dest,ec);
    if(ec)return 12;
    for(const char* name:{"potion.png","key.png","coin.png","unknown.png"}){
        if(!fs::is_regular_file(icons/name) ||
           (fs::exists(dest/name) && !fs::is_regular_file(dest/name))) {
            std::cerr<<"Original starter icon unavailable: "<<name<<"\n";
            return 13;
        }
    }
    for(const char* name:{"potion.png","key.png","coin.png","unknown.png"}) {
        if(fs::exists(dest/name))continue; // never replace existing creator art.
        fs::copy_file(icons/name,dest/name,fs::copy_options::none,ec);
        if(ec)return 14;
    }
    ScreenDocument doc=MakeInventoryIconGridScreen(meta.projectId,asset);
    // The UI Designer document stays disabled until a gameplay module
    // explicitly references it; only the separate test marker opts it in.
    if(!ValidateScreenDocument(doc,meta.projectId,error)||
        !WriteScreenDocument(target.generic_u8string(),doc,error)){
        std::cerr<<"Invalid authorable inventory Screen: "<<error<<"\n";
        return 7;
    }
    ScreenDocument proof;
    if(!ReadScreenDocument(target.generic_u8string(),meta.projectId,proof,error)||
        proof.widgets.size()!=doc.widgets.size()){
        std::cerr<<"Authored inventory screen round-trip failed: "<<error<<"\n";
        return 8;
    }
    const auto marker=root/fs::u8path("Content/UI/InventoryG4B3B2Grid.rinvui");
    if(fs::exists(marker,ec))return 9;
    {
        std::ofstream out(marker,std::ios::binary|std::ios::trunc);
        out<<"RENEGADE_INVENTORY_UI_PREVIEW 1\n";
        if(!out.good())return 10;
    }
    std::cout<<"Created editable, disabled-by-default native Screen: "
        <<target.generic_u8string()<<"\n"
        <<"Widgets: "<<proof.widgets.size()
        <<", slot buttons: "<<proof.focusOrder.size()<<"\n";
    return 0;
}
