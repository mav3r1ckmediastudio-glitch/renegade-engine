#include "renegade/bridge/InventoryDragTransfer.h"
namespace renegade::bridge {
bool InventoryDragTransfer::Begin(const InventoryOverlayView& view,
                                  const std::string& sourceAction,
                                  const bool wholeStack) {
    Cancel();
    std::size_t panel=0,row=0;
    if(!ParseInventoryScreenSlotAction(sourceAction,panel,row) ||
       panel>=view.panels.size() || row>=view.panels[panel].rows.size())
        return false;
    const auto& item=view.panels[panel].rows[row];
    if(item.quantity==0 || item.instanceId.empty())return false;
    revision_=view.revision;sourcePanel_=panel;
    sourceContainer_=view.panels[panel].containerId;
    instanceId_=item.instanceId;
    count_=wholeStack?item.quantity:1;
    active_=true;
    return true;
}
InventoryResult InventoryDragTransfer::Drop(
    const InventoryOverlayView& view,const std::string& targetAction,
    InventoryGameplayGateway& gateway) {
    if(!active_)return InventoryResult::InvalidArgument;
    const auto revision=revision_;
    const auto from=sourcePanel_;
    const auto source=sourceContainer_;
    const auto instance=instanceId_;
    const auto count=count_;
    Cancel(); // exactly one result per gesture, even after a failure.
    std::size_t to=0,row=0;
    if(!ParseInventoryScreenSlotAction(targetAction,to,row) ||
       to>=view.panels.size() || view.panels.size()!=2 ||
       from>=view.panels.size() || to==from ||
       view.panels[to].containerId==source ||
       view.panels[from].containerId!=source ||
       view.revision!=revision || row>=24)
        return InventoryResult::InvalidArgument;
    // Revalidate the original stack rather than trusting a stale row index.
    const auto* original=&view.panels[from].rows;
    bool found=false;
    for(const auto& stack:*original)
        if(stack.instanceId==instance && stack.quantity>=count) {
            found=true;break;
        }
    if(!found)return InventoryResult::NotFound;
    return gateway.Transfer(source,view.panels[to].containerId,instance,count);
}
void InventoryDragTransfer::Cancel() noexcept {
    active_=false;revision_=0;sourcePanel_=0;count_=0;
    sourceContainer_.clear();instanceId_.clear();
}
} // namespace renegade::bridge
