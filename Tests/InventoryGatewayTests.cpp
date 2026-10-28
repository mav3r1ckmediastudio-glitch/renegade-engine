#include "renegade/bridge/InventoryGameplayGateway.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace renegade::bridge;
#define REQUIRE(x) do { if (!(x)) {std::cerr << "FAIL: " << #x << " at " << __LINE__ << "\n"; std::abort();}} while(false)
int main() {
    InventoryService inventory;
    std::vector<InventoryChange> changes;
    REQUIRE(inventory.ChangesSince(0, changes));
    REQUIRE(changes.empty());
    REQUIRE(inventory.RegisterItem({"potion","Potion",20,0.1,{}})==InventoryResult::Success);
    REQUIRE(inventory.CreateContainer("player",0,0)==InventoryResult::Success);
    REQUIRE(inventory.CreateContainer("chest",0,0)==InventoryResult::Success);
    REQUIRE(inventory.CreateContainer("private",0,0)==InventoryResult::Success);
    REQUIRE(inventory.Add("private","potion",4)==InventoryResult::Success);
    const auto setupCursor = inventory.LatestChangeSequence();
    InventoryGameplayGateway player(inventory);
    player.Grant("player",{true,true,true});
    player.Grant("chest",{true,true,true});
    InventoryGameplayGateway guest(inventory);
    guest.Grant("chest",{true,false,false});
    InventoryContainer copy;
    REQUIRE(guest.ReadContainer("player",copy)==InventoryResult::PermissionDenied);
    REQUIRE(guest.Add("chest","potion",2)==InventoryResult::PermissionDenied);
    REQUIRE(guest.Transfer("chest","player","anything",1)==InventoryResult::PermissionDenied);
    REQUIRE(player.Add("player","potion",5)==InventoryResult::Success);
    REQUIRE(player.ReadContainer("player",copy)==InventoryResult::Success);
    REQUIRE(copy.stacks.size()==1 && copy.stacks.front().quantity==5);
    const auto instance = copy.stacks.front().instanceId;
    REQUIRE(player.Transfer("player","chest",instance,2)==InventoryResult::Success);
    REQUIRE(player.ReadContainer("player",copy)==InventoryResult::Success);
    REQUIRE(copy.stacks.front().quantity==3);
    REQUIRE(player.ChangesSince(setupCursor,changes));
    REQUIRE(changes.size()==2);
    REQUIRE(changes[0].kind==InventoryChangeKind::Added);
    REQUIRE(changes[1].kind==InventoryChangeKind::Transferred);
    REQUIRE(changes[1].sourceContainerId=="player");
    REQUIRE(changes[1].targetContainerId=="chest");
    // A second subscriber gets the same events without consuming the first.
    REQUIRE(guest.ChangesSince(setupCursor,changes));
    REQUIRE(changes.size()==1);
    REQUIRE(changes[0].sourceContainerId.empty());
    REQUIRE(changes[0].targetContainerId=="chest");
    // An ungranted container never appears in this subscriber's journal.
    const auto cursor = inventory.LatestChangeSequence();
    REQUIRE(inventory.Add("private","potion",1)==InventoryResult::Success);
    REQUIRE(guest.ChangesSince(cursor,changes) && changes.empty());
    player.Revoke("chest");
    REQUIRE(player.Transfer("player","chest",instance,1)==InventoryResult::PermissionDenied);
    player.ClearGrants();
    REQUIRE(player.ReadContainer("player",copy)==InventoryResult::PermissionDenied);
    // Bounded history drops old events with an explicit resynchronisation signal.
    const auto veryOldCursor = inventory.LatestChangeSequence();
    for (int i=0;i<1100;++i)
        REQUIRE(inventory.Add("private","potion",1)==InventoryResult::Success);
    REQUIRE(!inventory.ChangesSince(veryOldCursor,changes));
    REQUIRE(changes.empty());
    auto newest = inventory.LatestChangeSequence();
    REQUIRE(inventory.ChangesSince(newest,changes) && changes.empty());
    // A successful restore announces that old snapshots need re-querying.
    const auto snapshot = inventory.SerializeSnapshot();
    REQUIRE(inventory.RestoreSnapshot(snapshot)==InventoryResult::Success);
    REQUIRE(inventory.ChangesSince(newest,changes));
    REQUIRE(changes.size()==1 && changes[0].kind==InventoryChangeKind::Restored);
    // An invalid restore emits no event and does not advance state.
    newest = inventory.LatestChangeSequence();
    REQUIRE(inventory.RestoreSnapshot("garbage")==InventoryResult::InvalidSnapshot);
    REQUIRE(inventory.LatestChangeSequence()==newest);
    std::cout << "Inventory G2 C++ gateway and event journal PASS\n";
}
