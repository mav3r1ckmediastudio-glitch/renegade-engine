#include "renegade/bridge/InventoryGridPage.h"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace renegade::bridge;
#define CHECK(v) do{if(!(v)){std::cerr<<"FAIL "<<#v<<" line "<<__LINE__<<"\n";std::abort();}}while(false)
int main(){
    InventoryService store;
    CHECK(store.CreateContainer("backpack",100,0)==InventoryResult::Success);
    CHECK(store.CreateContainer("chest",100,0)==InventoryResult::Success);
    for(int i=0;i<100;++i) {
        const auto id="thing-"+std::to_string(i);
        CHECK(store.RegisterItem({id,id,5,0,{}})==InventoryResult::Success);
        CHECK(store.Add("backpack",id,1)==InventoryResult::Success);
    }
    InventoryGameplayGateway gateway(store);
    InventoryGridLayout layout;
    std::string err;
    InventoryGridPage page;
    CHECK(ReadInventoryGridPage(gateway,"backpack",layout,15,page,err)==
          InventoryResult::PermissionDenied);
    CHECK(page.cells.empty()); // denied read must not leak prior contents
    gateway.Grant("backpack",{true,false,false});
    gateway.Grant("chest",{true,false,false});
    CHECK(ReadInventoryGridPage(gateway,"backpack",layout,0,page,err)==InventoryResult::Success);
    CHECK(page.usedSlots==100&&page.viewport.containerCapacity==100);
    CHECK(page.cells.size()==25 && page.cells[0].absoluteSlot==0);
    CHECK(page.cells[0].definitionId=="thing-0");
    CHECK(page.cells[24].definitionId=="thing-24");
    CHECK(page.viewport.maximumTopRow==15);
    CHECK(ReadInventoryGridPage(gateway,"backpack",layout,4,page,err)==InventoryResult::Success);
    CHECK(page.cells[0].absoluteSlot==20);
    CHECK(page.cells[0].definitionId=="thing-20");
    CHECK(page.cells[24].definitionId=="thing-44");
    CHECK(ReadInventoryGridPage(gateway,"backpack",layout,999,page,err)==InventoryResult::Success);
    CHECK(page.viewport.topRow==15 && page.cells.size()==25);
    CHECK(page.cells[0].absoluteSlot==75 && page.cells[0].definitionId=="thing-75");
    CHECK(page.cells[24].absoluteSlot==99 && page.cells[24].definitionId=="thing-99");
    std::uint32_t visible=999;
    CHECK(InventoryGridCellActionId(24)=="inventory.grid.cell.24");
    CHECK(ParseInventoryGridCellActionId("inventory.grid.cell.24",visible));
    CHECK(visible==24);
    for(const char* invalid:{"inventory.grid.cell.", "inventory.grid.cell.-1",
        "inventory.grid.cell.01", "inventory.grid.cell.1024",
        "inventory.grid.cell.4294967295", "inventory.grid.cell.3anything",
        "inventory.slot.0.1", "inventory.grid.cell.+3"}) {
        CHECK(!ParseInventoryGridCellActionId(invalid,visible));
    }
    InventoryPagedTransfer drag;
    CHECK(BeginInventoryPagedTransfer(page,0,false,drag));
    CHECK(drag.valid && drag.quantity==1);
    InventoryGridPage dest;
    CHECK(ReadInventoryGridPage(gateway,"chest",layout,0,dest,err)==InventoryResult::Success);
    CHECK(dest.cells.size()==25 && !dest.cells[0].Occupied());
    const auto oldSnapshot=store.SerializeSnapshot();
    CHECK(CommitInventoryPagedTransfer(drag,page,dest,0,gateway)==
          InventoryResult::PermissionDenied);
    CHECK(store.SerializeSnapshot()==oldSnapshot);
    CHECK(ExecuteInventoryGridCellAction("inventory.grid.cell.0",
        page,dest,0,false,gateway)==InventoryResult::PermissionDenied);
    CHECK(store.SerializeSnapshot()==oldSnapshot);
    // Scrolling the source invalidates target identity, even at the same
    // authoritative revision. Wrong on-screen cell is never actionable.
    InventoryGridPage scrolled;
    CHECK(ReadInventoryGridPage(gateway,"backpack",layout,0,scrolled,err)==InventoryResult::Success);
    CHECK(CommitInventoryPagedTransfer(drag,scrolled,dest,0,gateway)==
          InventoryResult::NotFound);
    CHECK(store.SerializeSnapshot()==oldSnapshot);
    gateway.Grant("backpack",{true,false,true});
    gateway.Grant("chest",{true,true,false});
    CHECK(CommitInventoryPagedTransfer(drag,page,dest,0,gateway)==InventoryResult::Success);
    CHECK(store.FindContainer("backpack")->stacks.size()==99);
    CHECK(store.FindContainer("chest")->stacks.size()==1);
    CHECK(store.FindContainer("chest")->stacks[0].definitionId=="thing-75");
    CHECK(ReadInventoryGridPage(gateway,"backpack",layout,15,page,err)==InventoryResult::Success);
    CHECK(ReadInventoryGridPage(gateway,"chest",layout,0,dest,err)==InventoryResult::Success);
    CHECK(ExecuteInventoryGridCellAction("inventory.grid.cell.0",
          page,dest,0,false,gateway)==InventoryResult::Success);
    CHECK(store.FindContainer("chest")->stacks.size()==2);
    CHECK(store.FindContainer("chest")->stacks[1].definitionId=="thing-76");
    CHECK(CommitInventoryPagedTransfer(drag,page,dest,0,gateway)==
          InventoryResult::InvalidArgument); // stale revision/double-drop
    CHECK(ReadInventoryGridPage(gateway,"backpack",layout,15,page,err)==InventoryResult::Success);
    CHECK(page.viewport.topRow==15);
    CHECK(page.cells[0].definitionId=="thing-77"); // vector compresses
    CHECK(!BeginInventoryPagedTransfer(page,100,false,drag));
    CHECK(!drag.valid);
    CHECK(ReadInventoryGridPage(gateway,"chest",layout,0,dest,err)==InventoryResult::Success);
    CHECK(!BeginInventoryPagedTransfer(dest,3,true,drag));
    CHECK(BeginInventoryPagedTransfer(dest,0,true,drag));
    CHECK(drag.quantity==1);
    CHECK(CommitInventoryPagedTransfer(drag,dest,page,0,gateway)==
          InventoryResult::PermissionDenied); // reverse grants unavailable
    gateway.Grant("backpack",{true,true,true});
    gateway.Grant("chest",{true,true,true});
    CHECK(CommitInventoryPagedTransfer(drag,dest,page,0,gateway)==InventoryResult::Success);
    CHECK(store.FindContainer("backpack")->stacks.size()==99);
    const auto remaining=store.FindContainer("chest")->stacks[0].instanceId;
    CHECK(gateway.Transfer("chest","backpack",remaining,1)==InventoryResult::Success);
    CHECK(store.FindContainer("backpack")->stacks.size()==100);
    CHECK(store.FindContainer("chest")->stacks.empty());
    // Whole stack transfer from another container is still atomic.
    CHECK(store.RegisterItem({"potions","Potions",10,0,{}})==InventoryResult::Success);
    CHECK(store.Add("chest","potions",7)==InventoryResult::Success);
    CHECK(ReadInventoryGridPage(gateway,"chest",layout,0,dest,err)==InventoryResult::Success);
    CHECK(BeginInventoryPagedTransfer(dest,0,true,drag)&&drag.quantity==7);
    CHECK(ReadInventoryGridPage(gateway,"backpack",layout,0,page,err)==InventoryResult::Success);
    CHECK(CommitInventoryPagedTransfer(drag,dest,page,0,gateway)==
          InventoryResult::CapacityExceeded); // backpack 100/100; no partial move
    CHECK(store.FindContainer("chest")->stacks.back().quantity==7);
    // Unbounded containers gain one extra view row, not an item cap.
    CHECK(store.CreateContainer("unlimited",0,0)==InventoryResult::Success);
    gateway.Grant("unlimited",{true,true,true});
    CHECK(ReadInventoryGridPage(gateway,"unlimited",layout,17,page,err)==InventoryResult::Success);
    CHECK(page.viewport.containerCapacity==25 && page.viewport.topRow==0);
    CHECK(page.cells.size()==25 && !page.cells[24].Occupied());
    layout.columns=0;
    CHECK(ReadInventoryGridPage(gateway,"unlimited",layout,0,page,err)==
          InventoryResult::InvalidArgument);
    CHECK(page.cells.empty());
    std::cout<<"G4B3C1 permission-scoped 100-slot paged inventory and stable transfers PASS\n";
}
