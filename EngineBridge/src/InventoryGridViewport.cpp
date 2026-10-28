#include "renegade/bridge/InventoryGridViewport.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace renegade::bridge {
namespace {
bool finite(float value) noexcept {return std::isfinite(value);}
}
bool ValidateInventoryGridLayout(const InventoryGridLayout& c,
                                 std::string& error) noexcept {
    if(c.columns==0 || c.columns>32 || c.visibleRows==0 ||
       c.visibleRows>32 || c.columns*c.visibleRows>1024) {
        error="Inventory grid must have 1..32 columns and rows.";
        return false;
    }
    if(!finite(c.slotWidth)||!finite(c.slotHeight)||
       c.slotWidth<8||c.slotHeight<8||
       c.slotWidth>2048||c.slotHeight>2048||
       !finite(c.horizontalGap)||!finite(c.verticalGap)||
       !finite(c.padding)||!finite(c.iconPadding)||
       c.horizontalGap<0||c.verticalGap<0||
       c.padding<0||c.iconPadding<0||
       c.horizontalGap>512||c.verticalGap>512||
       c.padding>512||c.iconPadding>512) {
        error="Inventory grid dimensions, spacing or padding are invalid.";
        return false;
    }
    if(c.squareSlots && std::fabs(c.slotWidth-c.slotHeight)>0.001f){
        error="Square inventory slots must have matching height and width.";
        return false;
    }
    if(c.iconPadding*2>=std::min(c.slotWidth,c.slotHeight)) {
        error="Inventory grid icon padding leaves no visible image space.";
        return false;
    }
    if(c.iconFit!=InventoryIconFit::Contain &&
       c.iconFit!=InventoryIconFit::Cover &&
       c.iconFit!=InventoryIconFit::Stretch){
        error="Invalid inventory icon scaling mode.";
        return false;
    }
    error.clear();return true;
}

bool ResolveInventoryGridViewport(const InventoryGridLayout& config,
        std::uint32_t capacity,std::uint32_t requestedTopRow,
        InventoryGridViewport& result,std::string& error) noexcept {
    if(!ValidateInventoryGridLayout(config,error))return false;
    InventoryGridViewport next;
    next.containerCapacity=capacity;
    next.visibleCellCapacity=config.columns*config.visibleRows;
    // widen before adding columns to avoid overflow near UINT32_MAX.
    const std::uint64_t rows=
        (static_cast<std::uint64_t>(capacity)+config.columns-1)/config.columns;
    next.totalRows=static_cast<std::uint32_t>(rows);
    next.maximumTopRow=rows>config.visibleRows?
        static_cast<std::uint32_t>(rows-config.visibleRows):0;
    next.topRow=std::min(requestedTopRow,next.maximumTopRow);
    next.firstContainerSlot=next.topRow*config.columns;
    if(next.firstContainerSlot<capacity)
        next.numberOfContainerSlotsVisible=
            std::min(next.visibleCellCapacity,capacity-next.firstContainerSlot);
    next.scrollbarNeeded=config.showScrollbar && next.maximumTopRow>0;
    if(next.maximumTopRow>0)
        next.scrollbarFraction=static_cast<double>(next.topRow)/
                               next.maximumTopRow;
    if(next.totalRows>0)
        next.scrollbarThumbFraction=std::min(1.0,
            static_cast<double>(config.visibleRows)/next.totalRows);
    result=next;error.clear();return true;
}
std::uint32_t InventoryGridRowAfterWheel(const InventoryGridViewport& v,
                                         int wheelRows) noexcept {
    // Signed/widened subtraction also handles INT_MIN safely.
    const auto desired=static_cast<std::int64_t>(v.topRow)+wheelRows;
    if(desired<=0)return 0;
    if(static_cast<std::uint64_t>(desired)>=v.maximumTopRow)
        return v.maximumTopRow;
    return static_cast<std::uint32_t>(desired);
}
std::uint32_t InventoryGridRowAtScrollbar(const InventoryGridViewport& v,
                                          double fraction) noexcept {
    if(!std::isfinite(fraction)||v.maximumTopRow==0)return v.topRow;
    fraction=std::clamp(fraction,0.0,1.0);
    return static_cast<std::uint32_t>(
        std::round(fraction*static_cast<double>(v.maximumTopRow)));
}
} // namespace renegade::bridge
