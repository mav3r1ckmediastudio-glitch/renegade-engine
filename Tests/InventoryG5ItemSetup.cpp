#include "InventoryG5DisposableProject.h"
// Create only safe G5 shared item data in a disposable cloned proof project.
// Does not change scene files or any authored user project.
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/InventoryService.h"
#include <filesystem>
#include <iostream>
#include <string>
namespace fs=std::filesystem;
using namespace renegade::bridge;
int main(int argc,char** argv) {
 if(argc!=2) return 2;
 std::error_code ec;
 const auto root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
 if(ec||root.filename()!="G5Play"||
    !IsDisposableG5Project(root,"G5Play")||
    !fs::is_regular_file(root/"ScreenProject.renegade"))
    {std::cerr<<"Refusing non-disposable project"<<std::endl;return 3;}
 const auto cataloguePath=root/"Content"/"Gameplay"/"Inventory.renegade-items";
 if(fs::exists(cataloguePath)) {std::cerr<<"Refusing to replace existing catalogue"<<std::endl;return 4;}
 InventoryItemCatalogue doc;
 doc.envelope=CreateDocumentEnvelope(
   "61111111-1111-4111-8111-111111111111",
   InventoryItemCatalogueDocumentType,
   "Content/Gameplay/Inventory.renegade-items",
   "G5 gameplay test item");
 InventoryAuthoringItem potion;
 potion.id="health-potion";potion.name="Health Potion";
 potion.kind=InventoryItemKind::Consumable;
 potion.effect=InventoryItemEffect::RestoreHealth;
 potion.healthRestored=50;
 potion.maximumStack=10;
 potion.weightPerUnit=0.2;
 potion.iconProjectRelativePath="Content/UI/InventoryIcons/potion.png";
 std::string error;
 if(!fs::is_regular_file(root/fs::u8path(potion.iconProjectRelativePath))||
    !UpsertInventoryAuthoringItem(doc,potion,error))
    {std::cerr<<"No icon or invalid item: "<<error<<std::endl;return 5;}
 fs::create_directories(cataloguePath.parent_path(),ec);
 if(ec||!WriteInventoryItemCatalogue(cataloguePath.generic_u8string(),doc,error))
    {std::cerr<<"Catalogue write failed: "<<error<<std::endl;return 6;}
 InventoryItemCatalogue test;
 if(!ReadInventoryItemCatalogue(cataloguePath.generic_u8string(),
     doc.envelope.projectId,test,error)||test.items.size()!=1)
    {std::cerr<<"Catalogue reopen failed: "<<error<<std::endl;return 7;}
 InventoryService inventory;
 if(inventory.RegisterItem(MakeRuntimeInventoryDefinition(potion))!=InventoryResult::Success||
    inventory.CreateContainer("player",12)!=InventoryResult::Success||
    inventory.CreateContainer("chest",12)!=InventoryResult::Success||
    inventory.Add("chest","health-potion",2)!=InventoryResult::Success)
    {std::cerr<<"Inventory bootstrap failed"<<std::endl;return 8;}
 for(const char* name:{"LevelOne","LevelTwo"}) {
    auto path=root/"Content"/"Scenes"/(std::string(name)+".wiscene.rinventory");
    if(!fs::is_regular_file(path)) return 9;
    fs::copy_file(path,fs::path(path.string()+".before-G5"),
                  fs::copy_options::none,ec);
    if(ec||inventory.SaveSnapshot(path.generic_u8string())!=InventoryResult::Success)
       {std::cerr<<"Inventory snapshot write failed"<<std::endl;return 10;}
 }
 std::cout<<"G5 item catalogue READY, potion heals 50, max stack 10; player 0, chest 2"<<std::endl;
 return 0;
}
