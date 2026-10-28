#include "renegade/bridge/InventorySelectionController.h"
#include <algorithm>
#include <utility>

namespace renegade::bridge {
void InventorySelectionController::Reset() noexcept {
    selectedPanel_=-1; selectedRow_=-1;
    containerId_.clear(); instanceId_.clear();
}

bool InventorySelectionController::Select(const InventoryOverlayView& view,
                                         std::size_t panel, std::size_t row) {
    if (panel>=view.panels.size() || row>=view.panels[panel].rows.size())
        return false;
    selectedPanel_=static_cast<int>(panel);
    selectedRow_=static_cast<int>(row);
    containerId_=view.panels[panel].containerId;
    instanceId_=view.panels[panel].rows[row].instanceId;
    return true;
}

void InventorySelectionController::Reconcile(const InventoryOverlayView& view) {
    // Keep selection tied to the authoritative stack instance ID rather than
    // its index, which can change when Lua transfers and stacks coalesce.
    if (!containerId_.empty() && !instanceId_.empty()) {
        for (std::size_t p=0;p<view.panels.size();++p) {
            if (view.panels[p].containerId!=containerId_) continue;
            for (std::size_t i=0;i<view.panels[p].rows.size();++i) {
                if (view.panels[p].rows[i].instanceId==instanceId_) {
                    (void)Select(view,p,i);return;
                }
            }
        }
    }
    // If a stack was consumed, prefer another item in its old panel.
    if (!containerId_.empty()) {
        for (std::size_t p=0;p<view.panels.size();++p)
            if (view.panels[p].containerId==containerId_ &&
                !view.panels[p].rows.empty()) {
                const auto row=selectedRow_<0?0:std::min(
                    static_cast<std::size_t>(selectedRow_),view.panels[p].rows.size()-1);
                (void)Select(view,p,row);return;
            }
    }
    // Otherwise select the first item in any accessible container.
    for (std::size_t p=0;p<view.panels.size();++p)
        if (!view.panels[p].rows.empty()) {
            (void)Select(view,p,0);return;
        }
    Reset();
}

bool InventorySelectionController::Move(const InventoryOverlayView& view,int direction) {
    if (selectedPanel_<0 ||
        static_cast<std::size_t>(selectedPanel_)>=view.panels.size()) {
        Reconcile(view);return selectedPanel_>=0;
    }
    const auto p=static_cast<std::size_t>(selectedPanel_);
    const auto rows=view.panels[p].rows.size();
    if (rows==0) { Reconcile(view); return selectedPanel_>=0; }
    const int last=static_cast<int>(rows)-1;
    return Select(view,p,static_cast<std::size_t>(std::clamp(
        selectedRow_ + (direction>0?1:-1),0,last)));
}

bool InventorySelectionController::SwitchContainer(const InventoryOverlayView& view) {
    if (view.panels.size()!=2) return false;
    const auto other=selectedPanel_==0?1u:0u;
    if (view.panels[other].rows.empty()) return false;
    const auto row=selectedRow_<0?0u:static_cast<std::size_t>(selectedRow_);
    return Select(view,other,std::min(row,view.panels[other].rows.size()-1));
}

InventoryResult InventorySelectionController::TransferSelected(
    const InventoryOverlayView& view,InventoryGameplayGateway& gateway,
    std::uint32_t count) {
    if (count==0 || view.panels.size()!=2 || selectedPanel_<0 ||
        selectedRow_<0 || selectedPanel_>1)
        return InventoryResult::InvalidArgument;
    const auto& source=view.panels[static_cast<std::size_t>(selectedPanel_)];
    const auto& target=view.panels[static_cast<std::size_t>(1-selectedPanel_)];
    if (source.containerId==target.containerId || instanceId_.empty() ||
        source.containerId!=containerId_) return InventoryResult::InvalidArgument;
    // Never dispatch using a stale or mismatched selected item identity.
    const auto it=std::find_if(source.rows.begin(),source.rows.end(),
        [this](const InventorySlotView& row){return row.instanceId==instanceId_;});
    if (it==source.rows.end()) return InventoryResult::NotFound;
    if (it->quantity<count) return InventoryResult::InsufficientQuantity;
    return gateway.Transfer(source.containerId,target.containerId,instanceId_,count);
}
} // namespace renegade::bridge
