#include "../Runtime/src/RuntimeInventorySession.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>

using namespace renegade;
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL: "<<#x<<" at "<<__LINE__<<"\n";std::abort();}}while(false)
static void Write(const fs::path& path,const std::string& bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    CHECK(out.good());
    out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    CHECK(out.good());
}
int main() {
    const auto root=fs::temp_directory_path() / "renegade-inventory-g3-session-fixture";
    std::error_code ec;
    fs::remove_all(root,ec);
    fs::create_directories(root/"Content"/"Scenes",ec);
    CHECK(!ec);
    const auto scene=root/"Content"/"Scenes"/"Starter.wiscene";
    const auto snapshot=fs::u8path(scene.generic_u8string()+".rinventory");
    const auto policy=fs::u8path(scene.generic_u8string()+".rinventory-access");
    Write(scene,"scene fixture");
    runtime::RuntimeInventorySession session;
    std::string error;
    CHECK(session.OpenForScene(root.generic_u8string(),scene.generic_u8string(),error));
    CHECK(error.empty() && !session.Enabled());
    CHECK(session.GatewayForScript("creator1","")==nullptr);
    bridge::InventoryService initial;
    CHECK(initial.RegisterItem({"potion","Potion",10,0.2,{}})==bridge::InventoryResult::Success);
    CHECK(initial.CreateContainer("player",12,10.0)==bridge::InventoryResult::Success);
    CHECK(initial.CreateContainer("chest",12,50.0)==bridge::InventoryResult::Success);
    CHECK(initial.Add("player","potion",5)==bridge::InventoryResult::Success);
    Write(snapshot,initial.SerializeSnapshot());
    CHECK(session.OpenForScene(root.generic_u8string(),scene.generic_u8string(),error));
    CHECK(session.Enabled() && session.GatewayForScript("creator1","")==nullptr);
    Write(policy,
        "RENEGADE_INVENTORY_ACCESS 1 5\n"
        "CONTAINER \"creator1\" \"-\" \"player\" 1 1 1\n"
        "CONTAINER \"creator1\" \"-\" \"chest\" 1 1 1\n"
        "CONTAINER \"observer\" \"entity-101\" \"chest\" 1 0 0\n"
        "DEFINE \"creator1\" \"-\"\n"
        "CREATE \"creator1\" \"-\" \"script:creator1:\"\n");
    CHECK(session.OpenForScene(root.generic_u8string(),scene.generic_u8string(),error));
    auto* creator=session.GatewayForScript("creator1","");
    CHECK(creator);
    CHECK(creator->RegisterItem({"herb","Herb",20,0.01,{}})==bridge::InventoryResult::Success);
    CHECK(creator->CreateContainer("script:creator1:stash",12,10.0)==bridge::InventoryResult::Success);
    CHECK(creator->CreateContainer("other-script:stash",12,10.0)==bridge::InventoryResult::PermissionDenied);
    CHECK(session.GatewayForScript("creator1","wrong-owner")==nullptr);
    CHECK(session.GatewayForScript("observer","")==nullptr);
    auto* observer=session.GatewayForScript("observer","entity-101");
    CHECK(observer);
    bridge::InventoryContainer player;
    CHECK(creator->ReadContainer("player",player)==bridge::InventoryResult::Success);
    CHECK(player.stacks.size()==1 && player.stacks[0].quantity==5);
    const auto original=player.stacks[0].instanceId;
    CHECK(creator->Transfer("player","chest",original,2)==bridge::InventoryResult::Success);
    CHECK(creator->ReadContainer("player",player)==bridge::InventoryResult::Success);
    CHECK(player.stacks[0].quantity==3);
    CHECK(observer->ReadContainer("player",player)==bridge::InventoryResult::PermissionDenied);
    CHECK(observer->Add("chest","potion",1)==bridge::InventoryResult::PermissionDenied);
    CHECK(observer->ReadContainer("chest",player)==bridge::InventoryResult::Success);
    CHECK(player.stacks[0].quantity==2);
    // Runtime mutations do not mutate authoring snapshots in place.
    bridge::InventoryService onDisk;
    CHECK(onDisk.LoadSnapshot(snapshot.generic_u8string())==bridge::InventoryResult::Success);
    CHECK(onDisk.FindContainer("player")->stacks[0].quantity==5);

    // The future Save Game owner can capture live state without overwriting
    // authored Scene data, then restore it into a fresh Scene Runtime session.
    std::string gameSave;
    CHECK(session.CaptureRuntimeState(gameSave,error));
    CHECK(!gameSave.empty() && error.empty());
    const auto liveState = session.Inventory()->SerializeSnapshot();
    const auto cursor = session.Inventory()->LatestChangeSequence();
    CHECK(!session.RestoreRuntimeState("corrupt game save",error));
    CHECK(session.Inventory()->SerializeSnapshot()==liveState);
    CHECK(session.Inventory()->LatestChangeSequence()==cursor);
    session.Reset();
    CHECK(!session.Enabled() && session.GatewayForScript("creator1","")==nullptr);
    CHECK(!session.CaptureRuntimeState(gameSave,error));
    CHECK(gameSave.empty());
    CHECK(session.OpenForScene(root.generic_u8string(),scene.generic_u8string(),error));
    CHECK(session.Inventory()->FindContainer("player")->stacks[0].quantity==5);
    CHECK(session.RestoreRuntimeState(liveState,error));
    CHECK(session.Inventory()->FindContainer("player")->stacks[0].quantity==3);
    CHECK(session.Inventory()->FindContainer("chest")->stacks[0].quantity==2);
    CHECK(session.GatewayForScript("creator1","") != nullptr);
    // Runtime state restore is never mistaken for an authored Scene save.
    bridge::InventoryService authoredAgain;
    CHECK(authoredAgain.LoadSnapshot(snapshot.generic_u8string())==bridge::InventoryResult::Success);
    CHECK(authoredAgain.FindContainer("player")->stacks[0].quantity==5);
    session.Reset();
    // Runtime must reject duplicate grants exactly as Build Game does.
    Write(policy,
        "RENEGADE_INVENTORY_ACCESS 1 2\n"
        "CONTAINER \"creator1\" \"-\" \"player\" 1 1 1\n"
        "CONTAINER \"creator1\" \"-\" \"player\" 1 0 0\n");
    CHECK(!session.OpenForScene(root.generic_u8string(),scene.generic_u8string(),error));
    CHECK(!session.Enabled() && !error.empty());
    // Reject malformed access and leave no state allocated.
    Write(policy,
        "RENEGADE_INVENTORY_ACCESS 1 1\n"
        "CONTAINER \"creator1\" \"-\" \"missing-container\" 1 1 1\n");
    CHECK(!session.OpenForScene(root.generic_u8string(),scene.generic_u8string(),error));
    CHECK(!session.Enabled() && !error.empty());
    // No authored inventory means no Runtime inventory subsystem.
    fs::remove(snapshot,ec);
    fs::remove(policy,ec);
    CHECK(session.OpenForScene(root.generic_u8string(),scene.generic_u8string(),error));
    CHECK(!session.Enabled());
    fs::remove_all(root,ec);
    std::cout<<"Inventory G3 Runtime opt-in companion session PASS\n";
}
