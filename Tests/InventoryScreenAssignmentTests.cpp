#include "renegade/bridge/InventoryScreenAssignmentService.h"
#include "renegade/bridge/InventoryScreenAdapter.h"
#include "renegade/bridge/IdentityService.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
namespace fs=std::filesystem;
using namespace renegade::bridge;
#define CHECK(x) do {if(!(x)){std::cerr<<"G5E Inventory Screen assignment line "<<__LINE__<<": "<<#x<<"\n";return 1;}} while(false)
int main() {
    const auto project=GenerateStableId();
    const auto root=fs::temp_directory_path()/("renegade-g5e-screen-"+project);
    std::error_code ec;
    fs::create_directories(root/"Content"/"UI",ec);
    CHECK(!ec);
    fs::create_directories(root/"Content"/"Gameplay",ec);
    CHECK(!ec);
    struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{root};
    const std::string relative="Content/UI/G5Inventory.renegade-screen";
    auto screen=MakeInventoryIconGridScreen(project,relative);
    CHECK(screen.usage==ScreenUsage::Overlay && !screen.enabledInGame);
    const auto file=root/fs::u8path(relative);
    std::string error;
    CHECK(WriteScreenDocument(file.generic_u8string(),screen,error));
    const auto assignment=root/fs::u8path(InventoryScreenAssignmentPath);
    const auto writeAssignment=[&](const std::string& id,const std::string& path,
                                   const std::string& mode) {
        std::ofstream out(assignment,std::ios::binary|std::ios::trunc);
        out<<"RENEGADE_INVENTORY_SCREEN 1\n"<<id<<"\n"<<path<<"\n"<<mode<<"\n";
        return bool(out);
    };
    InventoryScreenAssignment result;
    CHECK(!ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(writeAssignment(screen.envelope.documentId,relative,"grid"));
    CHECK(ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(result.screenPath==relative && result.screenId==screen.envelope.documentId && !result.paged);
    CHECK(!ReadInventoryScreenAssignment(root.generic_u8string(),GenerateStableId(),result,error));
    CHECK(writeAssignment(GenerateStableId(),relative,"grid"));
    CHECK(!ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(writeAssignment(screen.envelope.documentId,"Content/UI/../UI/G5Inventory.renegade-screen","grid"));
    CHECK(!ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(writeAssignment(screen.envelope.documentId,relative,"bogus"));
    CHECK(!ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(writeAssignment(screen.envelope.documentId,"Content/UI/Missing.renegade-screen","grid"));
    CHECK(!ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(writeAssignment(screen.envelope.documentId,relative,"paged"));
    CHECK(ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(result.paged);
    // This is the same journaled project writer called by the Studio picker.
    CHECK(SetInventoryScreenAssignment(root.generic_u8string(),project,
        screen.envelope.documentId,relative,error));
    CHECK(ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(result.screenId==screen.envelope.documentId && !result.paged);
    CHECK(!SetInventoryScreenAssignment(root.generic_u8string(),project,
        GenerateStableId(),relative,error));
    CHECK(ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(result.screenId==screen.envelope.documentId);
    CHECK(!SetInventoryScreenAssignment(root.generic_u8string(),project,
        screen.envelope.documentId,"Content/UI/../../outside.renegade-screen",error));
    CHECK(ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    CHECK(SetInventoryScreenAssignment(root.generic_u8string(),project,{}, {},error));
    CHECK(!fs::exists(assignment));
    CHECK(SetInventoryScreenAssignment(root.generic_u8string(),project,{}, {},error));
    CHECK(SetInventoryScreenAssignment(root.generic_u8string(),project,
        screen.envelope.documentId,relative,error));
    // Enabled Story Flow Screens are never implicitly borrowed as an Inventory
    // module: the authored Screen remains disabled; Runtime presents a copy.
    screen.enabledInGame=true;
    CHECK(WriteScreenDocument(file.generic_u8string(),screen,error));
    CHECK(!ReadInventoryScreenAssignment(root.generic_u8string(),project,result,error));
    std::cout<<"PASS: G5E explicit Inventory Screen binding, identity, containment, usage, invalid input\n";
    return 0;
}
