#include "renegade/bridge/InventoryTestLevelSnapshot.h"
#include "renegade/bridge/InventoryService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
namespace fs=std::filesystem;
using namespace renegade::bridge;
#define CHECK(c) do { if(!(c)){std::cerr<<"failed: "<<#c<<" line "<<__LINE__<<"\n"; std::abort();}}while(false)
static void Write(const fs::path& path, const std::string& value) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    CHECK(out.good());
    out<<value;
    CHECK(out.good());
}
int main() {
    const auto project=fs::temp_directory_path()/"renegade-inv-g3b-testlevel";
    std::error_code ec;fs::remove_all(project,ec);
    fs::create_directories(project/"Content"/"Scenes",ec);CHECK(!ec);
    fs::create_directories(project/"Intermediate"/"TestLevel"/"Content"/"Scenes",ec);CHECK(!ec);
    const auto original=project/"Content"/"Scenes"/"Main.wiscene";
    const auto shadow=project/"Intermediate"/"TestLevel"/"Content"/"Scenes"/"TestLevel.wiscene";
    const auto source=fs::u8path(original.generic_u8string()+".rinventory");
    const auto access=fs::u8path(original.generic_u8string()+".rinventory-access");
    const auto result=fs::u8path(shadow.generic_u8string()+".rinventory");
    const auto resultAccess=fs::u8path(shadow.generic_u8string()+".rinventory-access");
    Write(original,"Wicked authored Scene");Write(shadow,"Disposable Test Level Scene");
    std::string error;
    CHECK(SnapshotInventoryCompanions(project.generic_u8string(),
                                     original.generic_u8string(),shadow.generic_u8string(),error));
    CHECK(!fs::exists(result) && error.empty());
    InventoryService initial;
    CHECK(initial.RegisterItem({"herb","Herb",12,0.1,{}})==InventoryResult::Success);
    CHECK(initial.CreateContainer("player",3,3.0)==InventoryResult::Success);
    CHECK(initial.Add("player","herb",5)==InventoryResult::Success);
    const auto raw=initial.SerializeSnapshot();
    Write(source,raw);
    Write(access,"RENEGADE_INVENTORY_ACCESS 1 0\n");
    CHECK(SnapshotInventoryCompanions(project.generic_u8string(),
                                     original.generic_u8string(),shadow.generic_u8string(),error));
    InventoryService copied;
    CHECK(copied.LoadSnapshot(result.generic_u8string())==InventoryResult::Success);
    CHECK(copied.SerializeSnapshot()==raw);
    CHECK(fs::file_size(resultAccess)>0);
    CHECK(SnapshotInventoryCompanions(project.generic_u8string(),
                                     original.generic_u8string(),shadow.generic_u8string(),error)==false);
    CHECK(!error.empty()); // no clobbering of previously staged files
    fs::remove(result,ec); fs::remove(resultAccess,ec);
    // Test Level must reject a policy that Build Game rejects.
    Write(access,
        "RENEGADE_INVENTORY_ACCESS 1 2\n"
        "CONTAINER \"user\" \"-\" \"player\" 1 1 1\n"
        "CONTAINER \"user\" \"-\" \"player\" 1 0 0\n");
    CHECK(!SnapshotInventoryCompanions(project.generic_u8string(),
                                      original.generic_u8string(),shadow.generic_u8string(),error));
    CHECK(!fs::exists(result) && !fs::exists(resultAccess));
    Write(access,"RENEGADE_INVENTORY_ACCESS 1 0\n");
    Write(source,"corrupt");
    CHECK(!SnapshotInventoryCompanions(project.generic_u8string(),
                                      original.generic_u8string(),shadow.generic_u8string(),error));
    CHECK(!fs::exists(result)); // never stage a broken snapshot
    fs::remove(source,ec);
    CHECK(!SnapshotInventoryCompanions(project.generic_u8string(),
                                      original.generic_u8string(),shadow.generic_u8string(),error));
    CHECK(!fs::exists(result)); // policy without item data fails closed
    fs::remove_all(project,ec);
    std::cout<<"G3B Test Level inventory projection PASS\n";
}
