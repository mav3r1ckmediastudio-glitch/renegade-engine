#include "InventoryPointerGesture.h"
#include <cstdlib>
#include <iostream>
using renegade::runtime::InventoryPointerGesture;
using Kind=InventoryPointerGesture::Kind;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<"\n";std::abort();}}while(false)
int main(){
    InventoryPointerGesture g;
    // First drag transfers a single item, rearm at next mouse-down,
    // second and third drags operate on the same UI cell.
    for(int i=0;i<3;++i){
        const auto start=g.Step(true,false,100,100,"inventory.slot.0.0");
        CHECK(start.kind==Kind::Began&&!start.fullStack);
        CHECK(g.Step(true,false,200,150,"inventory.slot.1.0").kind==Kind::None);
        const auto drop=g.Step(false,false,400,180,"inventory.slot.1.0");
        CHECK(drop.kind==Kind::DragReleased && !drop.fullStack);
        CHECK(drop.sourceAction=="inventory.slot.0.0");
        CHECK(g.ConsumeNativeMouseClick("inventory.slot.0.0"));
        CHECK(!g.ConsumeNativeMouseClick("inventory.slot.0.0"));
    }
    // Shift captured on press, not when leaving slot or releasing mouse.
    auto start=g.Step(true,true,100,100,"inventory.slot.0.0");
    CHECK(start.kind==Kind::Began && start.fullStack);
    g.Step(true,false,400,100,"inventory.slot.1.0");
    auto end=g.Step(false,false,400,100,"inventory.slot.1.0");
    CHECK(end.kind==Kind::DragReleased && end.fullStack);
    // Holding Shift only at release must not upgrade one to a stack.
    CHECK(g.Step(true,false,100,100,"inventory.slot.0.0").kind==Kind::Began);
    g.Step(true,true,300,150,"inventory.slot.1.0");
    CHECK(!g.Step(false,true,300,150,"inventory.slot.1.0").fullStack);
    // A normal click stays under the existing native button action path.
    CHECK(g.Step(true,false,100,100,"inventory.slot.0.0").kind==Kind::Began);
    const auto ordinaryClick=g.Step(false,false,102,102,"inventory.slot.0.0");
    CHECK(ordinaryClick.kind==Kind::ClickReleased);
    CHECK(InventoryPointerGesture::ConfirmedSlotClick(ordinaryClick,"inventory.slot.0.0"));
    CHECK(!InventoryPointerGesture::ConfirmedSlotClick(ordinaryClick,"inventory.slot.1.0"));
    CHECK(!g.ConsumeNativeMouseClick("inventory.slot.0.0"));
    // The mouse can start on an icon overlay because hit-testing uses
    // the actual SLOT rectangle, not Wicked's focus/priority state.
    CHECK(g.Step(true,false,120,120,"inventory.slot.1.0").kind==Kind::Began);
    g.Step(true,false,130,130,"inventory.slot.1.1");
    CHECK(g.Step(false,false,120,120,"inventory.slot.1.0").kind==Kind::DragReleased);
    CHECK(g.ConsumeNativeMouseClick("inventory.slot.1.0"));
    // An outside press never spontaneously captures an item.
    CHECK(g.Step(true,true,900,900,"").kind==Kind::None);
    CHECK(g.Step(false,true,100,100,"inventory.slot.0.0").kind==Kind::None);
    g.Reset();
    CHECK(g.Step(false,false,100,100,"inventory.slot.0.0").kind==Kind::None);
    // Clicks on an authored icon above a slot still route to the one
    // underlying slot action. A released drag never activates it as a click.
    CHECK(g.Step(true,false,210,210,"inventory.paged.0.slot.0").kind==Kind::Began);
    auto iconClick=g.Step(false,false,212,210,"inventory.paged.0.slot.0");
    CHECK(InventoryPointerGesture::ConfirmedSlotClick(iconClick,"inventory.paged.0.slot.0"));
    CHECK(g.Step(true,false,210,210,"inventory.paged.0.slot.0").kind==Kind::Began);
    const auto releasedOutside=g.Step(false,false,800,800,"");
    CHECK(!InventoryPointerGesture::ConfirmedSlotClick(releasedOutside,""));
    std::cout<<"G4B3B3 repeat drags Shift on press no double click PASS\n";
}
