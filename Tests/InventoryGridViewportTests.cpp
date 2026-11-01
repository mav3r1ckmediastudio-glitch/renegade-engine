#include "renegade/bridge/InventoryGridViewport.h"
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
using namespace renegade::bridge;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL: "<<#x<<" line "<<__LINE__<<"\n";std::abort();}}while(false)
int main() {
    InventoryGridLayout cfg;
    std::string error;
    CHECK(ValidateInventoryGridLayout(cfg,error));
    InventoryGridViewport v;
    CHECK(ResolveInventoryGridViewport(cfg,100,0,v,error));
    CHECK(v.containerCapacity==100 && v.visibleCellCapacity==25);
    CHECK(v.totalRows==20 && v.maximumTopRow==15);
    CHECK(v.topRow==0 && v.firstContainerSlot==0);
    CHECK(v.numberOfContainerSlotsVisible==25);
    CHECK(v.scrollbarNeeded && v.scrollbarFraction==0);
    CHECK(std::fabs(v.scrollbarThumbFraction-.25)<1e-9);
    CHECK(InventoryGridRowAfterWheel(v,2)==2);
    CHECK(InventoryGridRowAfterWheel(v,INT_MAX)==15);
    CHECK(InventoryGridRowAfterWheel(v,INT_MIN)==0);
    CHECK(InventoryGridRowAtScrollbar(v,.5)==8);
    CHECK(InventoryGridRowAtScrollbar(v,1.0)==15);
    CHECK(InventoryGridRowAtScrollbar(v,-1.0)==0);
    CHECK(InventoryGridRowAtScrollbar(v,2.0)==15);
    CHECK(InventoryGridRowAtScrollbar(v,
        std::numeric_limits<double>::quiet_NaN())==0);
    CHECK(ResolveInventoryGridViewport(cfg,100,UINT_MAX,v,error));
    CHECK(v.topRow==15 && v.firstContainerSlot==75);
    CHECK(v.numberOfContainerSlotsVisible==25);
    CHECK(InventoryGridRowAfterWheel(v,-3)==12);
    CHECK(ResolveInventoryGridViewport(cfg,27,100,v,error));
    CHECK(v.totalRows==6 && v.maximumTopRow==1 && v.topRow==1);
    CHECK(v.firstContainerSlot==5 && v.numberOfContainerSlotsVisible==22);
    CHECK(ResolveInventoryGridViewport(cfg,8,6,v,error));
    CHECK(v.totalRows==2 && v.maximumTopRow==0 && v.topRow==0);
    CHECK(!v.scrollbarNeeded && v.numberOfContainerSlotsVisible==8);
    CHECK(ResolveInventoryGridViewport(cfg,0,5,v,error));
    CHECK(v.numberOfContainerSlotsVisible==0 && v.totalRows==0);
    CHECK(v.firstContainerSlot==0 && !v.scrollbarNeeded);
    cfg.showScrollbar=false;
    CHECK(ResolveInventoryGridViewport(cfg,100,5,v,error));
    CHECK(!v.scrollbarNeeded && v.topRow==5 && v.firstContainerSlot==25);
    cfg.showScrollbar=true;cfg.columns=8;cfg.visibleRows=3;
    cfg.slotWidth=84;cfg.slotHeight=58;cfg.squareSlots=false;
    cfg.iconFit=InventoryIconFit::Cover;
    CHECK(ResolveInventoryGridViewport(cfg,100,3,v,error));
    CHECK(v.visibleCellCapacity==24 && v.firstContainerSlot==24);
    cfg.squareSlots=true;
    CHECK(!ValidateInventoryGridLayout(cfg,error)); // Reject mismatched square dims
    cfg.slotHeight=84;
    CHECK(ValidateInventoryGridLayout(cfg,error));
    cfg.iconPadding=42;
    CHECK(!ValidateInventoryGridLayout(cfg,error));
    cfg.iconPadding=5;cfg.columns=0;
    CHECK(!ValidateInventoryGridLayout(cfg,error));
    cfg.columns=8;cfg.visibleRows=0;
    CHECK(!ValidateInventoryGridLayout(cfg,error));
    cfg.visibleRows=3;cfg.horizontalGap=-1;
    CHECK(!ValidateInventoryGridLayout(cfg,error));
    cfg.horizontalGap=6;
    cfg.slotHeight=std::numeric_limits<float>::quiet_NaN();
    CHECK(!ValidateInventoryGridLayout(cfg,error));
    cfg.slotHeight=84;
    cfg.iconFit=static_cast<InventoryIconFit>(100);
    CHECK(!ValidateInventoryGridLayout(cfg,error));
    cfg.iconFit=InventoryIconFit::Contain;
    CHECK(ResolveInventoryGridViewport(cfg,UINT_MAX,UINT_MAX,v,error));
    CHECK(v.topRow<=v.maximumTopRow && v.firstContainerSlot<=UINT_MAX);
    std::cout<<"G4-B3C pure creator-controlled inventory grid viewport PASS\n";
}
