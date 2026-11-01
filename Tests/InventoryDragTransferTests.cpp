#include "renegade/bridge/InventoryDragTransfer.h"
#include "InventoryPointerGesture.h"
#include <cstdlib>
#include <iostream>
using namespace renegade::bridge;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<"\n";std::abort();}}while(false)
int main(){
    InventoryService inventory;
    CHECK(inventory.RegisterItem({"potion","Health Potion",10,0.2,{}})==InventoryResult::Success);
    CHECK(inventory.CreateContainer("player",4,10)==InventoryResult::Success);
    CHECK(inventory.CreateContainer("chest",4,10)==InventoryResult::Success);
    CHECK(inventory.Add("player","potion",3)==InventoryResult::Success);
    CHECK(inventory.Add("chest","potion",2)==InventoryResult::Success);
    const auto view=[&]{return BuildInventoryOverlayView(inventory,
        {{"player","Player"},{"chest","Chest"}},8);};
    InventoryGameplayGateway scope(inventory);
    InventoryDragTransfer drag;
    auto current=view();
    CHECK(!drag.Begin(current,"inventory.slot.0.8")); // no stack here
    CHECK(drag.Begin(current,"inventory.slot.0.0"));
    CHECK(drag.Quantity()==1); // default must never move a full stack
    const auto original=inventory.SerializeSnapshot();
    CHECK(drag.Drop(current,"inventory.slot.1.0",scope)==InventoryResult::PermissionDenied);
    CHECK(inventory.SerializeSnapshot()==original && !drag.Active());
    scope.Grant("player",{true,true,true});
    scope.Grant("chest",{true,true,true});
    CHECK(drag.Begin(current,"inventory.slot.0.0"));
    CHECK(drag.Drop(current,"inventory.slot.0.0",scope)==InventoryResult::InvalidArgument);
    CHECK(original==inventory.SerializeSnapshot());
    CHECK(drag.Begin(current,"inventory.slot.0.0"));
    CHECK(drag.Quantity()==1);
    CHECK(drag.Drop(current,"inventory.slot.1.3",scope)==InventoryResult::Success);
    current=view();
    CHECK(current.panels[0].rows[0].quantity==2);
    CHECK(current.panels[1].rows[0].quantity==3);
    // Capturing the modifier at mouse-down requests the full stack.
    CHECK(drag.Begin(current,"inventory.slot.0.0",true));
    CHECK(drag.Quantity()==2);
    CHECK(drag.Drop(current,"inventory.slot.1.0",scope)==InventoryResult::Success);
    current=view();
    CHECK(current.panels[0].rows.empty());
    CHECK(current.panels[1].rows[0].quantity==5);
    // Default works in reverse and does not require holding shift.
    CHECK(drag.Begin(current,"inventory.slot.1.0"));
    CHECK(drag.Quantity()==1);
    CHECK(drag.Drop(current,"inventory.slot.0.0",scope)==InventoryResult::Success);
    current=view();
    CHECK(current.panels[0].rows[0].quantity==1);
    CHECK(current.panels[1].rows[0].quantity==4);
    // Lua changes gameplay between pointer down and release: fail closed.
    CHECK(drag.Begin(current,"inventory.slot.1.0",true));
    CHECK(drag.Quantity()==4);
    CHECK(inventory.Transfer("chest","player",
        current.panels[1].rows[0].instanceId,1)==InventoryResult::Success);
    const auto afterLua=inventory.SerializeSnapshot();
    CHECK(drag.Drop(view(),"inventory.slot.0.0",scope)==InventoryResult::InvalidArgument);
    CHECK(afterLua==inventory.SerializeSnapshot() && !drag.Active());
    current=view();
    CHECK(current.panels[0].rows[0].quantity==2);
    CHECK(current.panels[1].rows[0].quantity==3);
    CHECK(drag.Begin(current,"inventory.slot.1.0",true));
    CHECK(drag.Quantity()==3);
    CHECK(drag.Drop(current,"inventory.slot.0.1",scope)==InventoryResult::Success);
    current=view();
    CHECK(current.panels[0].rows[0].quantity==5);
    CHECK(current.panels[1].rows.empty());
    CHECK(!drag.Active() && drag.Quantity()==0);
    CHECK(drag.Drop(current,"inventory.slot.0.0",scope)==InventoryResult::InvalidArgument);
    CHECK(!drag.Begin(current,"inventory.slot.1.0"));
    CHECK(drag.Begin(current,"inventory.slot.0.0",true));
    drag.Cancel();
    CHECK(drag.Quantity()==0);
    CHECK(drag.Drop(view(),"inventory.slot.1.0",scope)==InventoryResult::InvalidArgument);
    // Real service + repeated physical gestures through the same input
    // policy. This was missing from the previous single-gesture tests.
    {
        InventoryService repeat;
        CHECK(repeat.RegisterItem({"potion","Health Potion",10,0.2,{}})==InventoryResult::Success);
        CHECK(repeat.CreateContainer("player",8,10)==InventoryResult::Success);
        CHECK(repeat.CreateContainer("chest",8,10)==InventoryResult::Success);
        CHECK(repeat.Add("player","potion",5)==InventoryResult::Success);
        InventoryGameplayGateway allowed(repeat);
        allowed.Grant("player",{true,true,true});
        allowed.Grant("chest",{true,true,true});
        renegade::runtime::InventoryPointerGesture pointer;
        InventoryDragTransfer controller;
        const auto frame=[&] {return BuildInventoryOverlayView(repeat,
            {{"player","Player"},{"chest","Chest"}},8);};
        for(int i=0;i<3;i++) {
            const auto start=pointer.Step(true,false,100,100,"inventory.slot.0.0");
            CHECK(start.kind==renegade::runtime::InventoryPointerGesture::Kind::Began);
            CHECK(controller.Begin(frame(),start.sourceAction,start.fullStack));
            pointer.Step(true,false,470,100,"inventory.slot.1.0");
            const auto end=pointer.Step(false,false,470,100,"inventory.slot.1.0");
            CHECK(end.kind==renegade::runtime::InventoryPointerGesture::Kind::DragReleased);
            CHECK(controller.Drop(frame(),"inventory.slot.1.0",allowed)==InventoryResult::Success);
            CHECK(pointer.ConsumeNativeMouseClick("inventory.slot.0.0"));
            CHECK(frame().panels[0].rows[0].quantity==4-static_cast<unsigned>(i));
            CHECK(frame().panels[1].rows[0].quantity==1+static_cast<unsigned>(i));
        }
        const auto start=pointer.Step(true,true,470,100,"inventory.slot.1.0");
        CHECK(start.kind==renegade::runtime::InventoryPointerGesture::Kind::Began);
        CHECK(controller.Begin(frame(),start.sourceAction,start.fullStack));
        CHECK(controller.Quantity()==3);
        pointer.Step(true,false,100,100,"inventory.slot.0.0"); // Shift released
        const auto end=pointer.Step(false,false,100,100,"inventory.slot.0.0");
        CHECK(end.kind==renegade::runtime::InventoryPointerGesture::Kind::DragReleased);
        CHECK(controller.Drop(frame(),"inventory.slot.0.0",allowed)==InventoryResult::Success);
        CHECK(frame().panels[0].rows[0].quantity==5);
        CHECK(frame().panels[1].rows.empty());
    }
    std::cout<<"Inventory repeated physical gestures and Shift full-stack PASS\n";
}
