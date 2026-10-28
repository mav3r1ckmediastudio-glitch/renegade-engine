#include "renegade/bridge/InventoryOverlayView.h"
#include <cstdlib>
#include <iostream>
using namespace renegade::bridge;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<#x<<" at "<<__LINE__<<"\n";std::abort();}}while(false)
int main(){
    InventoryService service;
    const std::vector<std::pair<std::string,std::string>> wanted={
        {"player","BACKPACK"},{"chest","CHEST"}};
    auto blank=BuildInventoryOverlayView(service,wanted);
    CHECK(blank.panels.empty()); // disabled game never creates a container
    CHECK(service.RegisterItem({"potion","Healing Potion",10,0.2,{}})==InventoryResult::Success);
    CHECK(service.RegisterItem({"key","Bronze Key",1,0.5,{}})==InventoryResult::Success);
    CHECK(service.CreateContainer("player",12,8)==InventoryResult::Success);
    CHECK(service.CreateContainer("chest",16,9)==InventoryResult::Success);
    CHECK(service.Add("player","potion",5)==InventoryResult::Success);
    CHECK(service.Add("player","key",1)==InventoryResult::Success);
    CHECK(service.Add("chest","potion",2)==InventoryResult::Success);
    const auto revision=service.Revision();
    auto view=BuildInventoryOverlayView(service,wanted);
    CHECK(service.Revision()==revision && view.revision==revision);
    CHECK(view.panels.size()==2 && view.panels[0].title=="BACKPACK");
    CHECK(view.panels[0].usedSlots==2 && view.panels[0].maxSlots==12);
    CHECK(view.panels[0].rows[0].itemName=="Healing Potion");
    CHECK(view.panels[0].rows[0].quantity==5);
    CHECK(view.panels[0].usedWeight>1.49 && view.panels[0].usedWeight<1.51);
    CHECK(view.panels[1].rows.size()==1 && view.panels[1].rows[0].quantity==2);
    const auto instance=view.panels[0].rows[0].instanceId;
    CHECK(service.Transfer("player","chest",instance,1)==InventoryResult::Success);
    view=BuildInventoryOverlayView(service,wanted);
    CHECK(view.revision>revision);
    CHECK(view.panels[0].rows[0].quantity==4);
    CHECK(view.panels[1].rows[0].quantity==3);
    auto bounded=BuildInventoryOverlayView(service,wanted,1);
    CHECK(bounded.panels[0].truncated && bounded.panels[0].rows.size()==1);
    CHECK(bounded.panels[0].usedWeight>1.29 && bounded.panels[0].usedWeight<1.31);
    // An invalid request doesn't grant a new container.
    auto missing=BuildInventoryOverlayView(service,{{"secret","SECRET"}});
    CHECK(missing.panels.empty() && service.FindContainer("secret")==nullptr);
    std::cout<<"G4 inventory view is revisioned read-only and live: PASS\n";
}
