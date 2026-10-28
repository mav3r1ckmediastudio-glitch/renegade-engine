#include "renegade/bridge/InventoryDependencyProvider.h"
#include "renegade/bridge/InventoryService.h"
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <vector>
namespace fs = std::filesystem;
using namespace renegade::bridge;
#define CHECK(value) do { if(!(value)) { std::cerr << "FAIL: " << #value << " at line " << __LINE__ << "\n"; std::abort(); } } while(false)
static void Write(const fs::path& name,const std::string& value) {
    std::ofstream out(name,std::ios::binary|std::ios::trunc);
    CHECK(out.good());
    out.write(value.data(),static_cast<std::streamsize>(value.size()));
    CHECK(out.good());
}
int main(){
    const auto root=fs::temp_directory_path()/"renegade-inv-package-discovery-g3c";
    std::error_code ec;
    fs::remove_all(root,ec);
    fs::create_directories(root/"Content"/"Scenes",ec);CHECK(!ec);
    const auto scene=root/"Content"/"Scenes"/"Main.wiscene";
    const auto other=root/"Content"/"Scenes"/"Unused.wiscene";
    Write(scene,"scene fixture");Write(other,"scene fixture");
    const auto initial=fs::u8path(scene.generic_u8string()+".rinventory");
    const auto permissions=fs::u8path(scene.generic_u8string()+".rinventory-access");
    const auto unused=fs::u8path(other.generic_u8string()+".rinventory");
    InventoryDependencyProvider provider;
    CHECK(provider.Supports(DependencyClass::Scene));
    CHECK(!provider.Supports(DependencyClass::Script));
    CHECK(std::string(provider.Name())=="inventory-scene-companions");
    DependencyNode node;
    node.projectRelativePath="Content/Scenes/Main.wiscene";
    node.dependencyClass=DependencyClass::Scene;
    const DependencyProviderContext context{root.generic_u8string(),&node};
    std::vector<DependencyCandidate> discovered;
    std::vector<DependencyProviderDiagnostic> diagnostics;
    std::string error;
    const auto discover=[&]() {
        discovered.clear();diagnostics.clear();
        return provider.Discover(context,
            [&](const DependencyCandidate& item){discovered.push_back(item);},
            [&](const DependencyProviderDiagnostic& diag){diagnostics.push_back(diag);},
            error);
    };
    // Inventory-free projects get no extra data dependencies.
    CHECK(discover() && discovered.empty() && diagnostics.empty());
    Write(permissions,"RENEGADE_INVENTORY_ACCESS 1 0\n");
    CHECK(!discover() && discovered.empty());
    CHECK(!error.empty()); // cannot package policy without data
    fs::remove(permissions,ec);
    InventoryService data;
    CHECK(data.RegisterItem({"potion","Potion",10,0.1,{}})==InventoryResult::Success);
    CHECK(data.CreateContainer("player",16,20.0)==InventoryResult::Success);
    CHECK(data.Add("player","potion",4)==InventoryResult::Success);
    Write(initial,data.SerializeSnapshot());
    CHECK(discover() && discovered.size()==1);
    CHECK(discovered[0].declaredPath=="Content/Scenes/Main.wiscene.rinventory");
    CHECK(discovered[0].dependencyClass==DependencyClass::Data);
    CHECK(discovered[0].requirement==DependencyRequirement::Required);
    CHECK(discovered[0].provenance=="inventory.scene.initial_state");
    Write(permissions,"RENEGADE_INVENTORY_ACCESS 1 0\n");
    CHECK(discover() && discovered.size()==2);
    CHECK(discovered[1].declaredPath=="Content/Scenes/Main.wiscene.rinventory-access");
    CHECK(discovered[1].dependencyClass==DependencyClass::Data);
    CHECK(discovered[1].requirement==DependencyRequirement::Required);
    Write(permissions,
        "RENEGADE_INVENTORY_ACCESS 1 1\n"
        "CONTAINER \"creator\" \"-\" \"missing\" 1 1 1\n");
    CHECK(!discover() && discovered.empty()); // invalid scoped owner/container
    Write(permissions,
        "RENEGADE_INVENTORY_ACCESS 1 2\n"
        "CONTAINER \"creator\" \"-\" \"player\" 1 1 1\n"
        "CONTAINER \"creator\" \"-\" \"player\" 1 0 0\n");
    CHECK(!discover() && discovered.empty()); // duplicate grants fail Build Game
    Write(permissions,
        "RENEGADE_INVENTORY_ACCESS 1 3\n"
        "CONTAINER \"creator\" \"-\" \"player\" 1 1 1\n"
        "DEFINE \"creator\" \"-\"\n"
        "CREATE \"creator\" \"-\" \"script:creator:\"\n");
    CHECK(discover() && discovered.size()==2);
    // Inventory on unrelated, unreachable Scenes does not enter this scene's closure.
    Write(unused,data.SerializeSnapshot());
    CHECK(discover() && discovered.size()==2);
    for(const auto& item:discovered)
        CHECK(item.declaredPath.find("Unused")==std::string::npos);
    // Tampered state must fail during Build Game discovery.
    Write(initial,"damaged");
    CHECK(!discover() && discovered.empty());
    Write(initial,data.SerializeSnapshot());
    fs::remove(permissions,ec);
    CHECK(discover() && discovered.size()==1);
    // G5: shared definitions and unique icon files accompany the reachable
    // scene; a missing icon or incompatible updated stack blocks packaging.
    fs::create_directories(root/"Content"/"Gameplay",ec);CHECK(!ec);
    fs::create_directories(root/"Content"/"UI"/"Icons",ec);CHECK(!ec);
    InventoryItemCatalogue catalogue;
    const auto projectId=GenerateStableId();
    catalogue.envelope=CreateDocumentEnvelope(projectId,
        InventoryItemCatalogueDocumentType,
        "Content/Gameplay/Inventory.renegade-items","G5");
    InventoryAuthoringItem potion;
    potion.id="potion";potion.name="Potion";potion.maximumStack=10;
    potion.iconProjectRelativePath="Content/UI/Icons/potion.png";
    CHECK(UpsertInventoryAuthoringItem(catalogue,potion,error));
    const auto catalogPath=root/"Content"/"Gameplay"/"Inventory.renegade-items";
    const auto imagePath=root/"Content"/"UI"/"Icons"/"potion.png";
    Write(imagePath,"PNG icon test asset");
    CHECK(WriteInventoryItemCatalogue(catalogPath.generic_u8string(),catalogue,error));
    CHECK(discover() && discovered.size()==3);
    CHECK(discovered[1].declaredPath=="Content/Gameplay/Inventory.renegade-items");
    CHECK(discovered[1].dependencyClass==DependencyClass::Data);
    CHECK(discovered[2].declaredPath=="Content/UI/Icons/potion.png");
    CHECK(discovered[2].dependencyClass==DependencyClass::Texture);
    fs::remove(imagePath,ec);
    CHECK(!discover() && discovered.empty());
    Write(imagePath,"PNG icon test asset");
    catalogue.items.front().maximumStack=2;
    CHECK(WriteInventoryItemCatalogue(catalogPath.generic_u8string(),catalogue,error));
    CHECK(!discover() && discovered.empty());
    catalogue.items.front().maximumStack=10;
    CHECK(WriteInventoryItemCatalogue(catalogPath.generic_u8string(),catalogue,error));
    CHECK(discover() && discovered.size()==3);
    fs::remove(catalogPath,ec);
    CHECK(discover() && discovered.size()==1);
    fs::remove(initial,ec);
    CHECK(discover() && discovered.empty());
    fs::remove_all(root,ec);
    std::cout<<"G3C automatic optional Build Game inventory dependencies PASS\n";
    return 0;
}
