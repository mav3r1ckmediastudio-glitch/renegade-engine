#include "renegade/bridge/InventorySelectionController.h"
#include <cstdlib>
#include <iostream>
using namespace renegade::bridge;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<"\n";std::abort();}}while(false)
int main() {
    InventoryService inventory;
    CHECK(inventory.RegisterItem({"potion","Potion",10,0.2,{}})==InventoryResult::Success);
    CHECK(inventory.RegisterItem({"key","Key",1,0.1,{}})==InventoryResult::Success);
    CHECK(inventory.CreateContainer("player",4,2.0)==InventoryResult::Success);
    CHECK(inventory.CreateContainer("chest",4,2.0)==InventoryResult::Success);
    CHECK(inventory.Add("player","potion",3)==InventoryResult::Success);
    CHECK(inventory.Add("player","key",1)==InventoryResult::Success);
    CHECK(inventory.Add("chest","potion",2)==InventoryResult::Success);
    const auto query=[&](){return BuildInventoryOverlayView(inventory,
        {{"player","Player"},{"chest","Chest"}});};
    InventoryGameplayGateway ui(inventory);
    InventorySelectionController selection;
    auto view=query();
    selection.Reconcile(view);
    CHECK(selection.SelectedPanel()==0 && selection.SelectedRow()==0);
    CHECK(selection.SelectedInstanceId()==view.panels[0].rows[0].instanceId);
    const auto first=selection.SelectedInstanceId();
    CHECK(selection.Move(view,1) && selection.SelectedRow()==1);
    CHECK(selection.Move(view,1) && selection.SelectedRow()==1); // clamp
    CHECK(selection.Move(view,-1) && selection.SelectedInstanceId()==first);
    CHECK(!selection.Select(view,10,0) && selection.SelectedInstanceId()==first);
    const auto original=inventory.SerializeSnapshot();
    CHECK(selection.TransferSelected(view,ui)==InventoryResult::PermissionDenied);
    CHECK(inventory.SerializeSnapshot()==original);
    ui.Grant("player",{true,true,true});
    ui.Grant("chest",{true,true,true});
    CHECK(selection.TransferSelected(view,ui)==InventoryResult::Success);
    view=query();
    selection.Reconcile(view);
    CHECK(selection.SelectedPanel()==0 && selection.SelectedInstanceId()==first);
    CHECK(view.panels[0].rows[0].quantity==2 && view.panels[1].rows[0].quantity==3);
    CHECK(selection.SwitchContainer(view) && selection.SelectedPanel()==1);
    CHECK(selection.TransferSelected(view,ui)==InventoryResult::Success);
    view=query();
    selection.Reconcile(view);
    CHECK(view.panels[0].rows[0].quantity==3 && view.panels[1].rows[0].quantity==2);
    // External Lua transaction can remove the selected stack: selection
    // reconciles to another legitimate row rather than keeping stale pointer.
    const auto chestId=view.panels[1].rows[0].instanceId;
    CHECK(inventory.Transfer("chest","player",chestId,2)==InventoryResult::Success);
    view=query();
    selection.Reconcile(view);
    CHECK(selection.SelectedPanel()==0); // chest emptied, fallback to player
    CHECK(selection.SwitchContainer(view)==false);
    // Only a windowed view is allowed to dispatch a selected item.
    InventoryOverlayView absent;
    CHECK(selection.TransferSelected(absent,ui)==InventoryResult::InvalidArgument);
    selection.Reset();
    CHECK(selection.SelectedPanel()==-1 && selection.SelectedRow()==-1);
    // Source/target cannot be the same even if a caller gives duplicate panels.
    auto duplicate=query();
    duplicate.panels[1]=duplicate.panels[0];
    selection.Reconcile(duplicate);
    CHECK(selection.TransferSelected(duplicate,ui)==InventoryResult::InvalidArgument);
    std::cout<<"G4B selection and scoped transfer controller PASS\n";
}