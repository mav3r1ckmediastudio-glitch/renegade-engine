#include "renegade/bridge/InventoryGridPage.h"
#include <algorithm>
#include <charconv>
#include <system_error>
#include <cstdint>
#include <limits>
#include <utility>

namespace renegade::bridge {
InventoryResult ReadInventoryGridPage(
    const InventoryGameplayGateway& gateway,const std::string& containerId,
    const InventoryGridLayout& layout,std::uint32_t requestedTopRow,
    InventoryGridPage& out,std::string& error) {
    out={}; // Never leave stale readable data after denied or invalid query.
    InventoryGridViewport test;
    if(!ValidateInventoryGridLayout(layout,error) || containerId.empty())
        return InventoryResult::InvalidArgument;
    InventoryContainer snap;
    const auto read=gateway.ReadContainer(containerId,snap);
    if(read!=InventoryResult::Success) {
        error="Inventory grid container is inaccessible or missing.";
        return read;
    }
    // For an unbounded container, display one spare row at its end.
    // This is *display space*, not a new limit on gameplay storage.
    std::uint32_t capacity=snap.maximumSlots;
    if(capacity==0) {
        const auto used=static_cast<std::uint64_t>(snap.stacks.size());
        const auto spare=static_cast<std::uint64_t>(layout.columns);
        capacity=static_cast<std::uint32_t>(
            std::min<std::uint64_t>(
                std::numeric_limits<std::uint32_t>::max(),
                std::max<std::uint64_t>(layout.columns*layout.visibleRows,
                                        used+spare)));
    }
    if(!ResolveInventoryGridViewport(layout,capacity,requestedTopRow,test,error))
        return InventoryResult::InvalidArgument;
    InventoryGridPage candidate;
    candidate.containerId=snap.id;
    candidate.revision=gateway.Revision();
    candidate.usedSlots=static_cast<std::uint32_t>(snap.stacks.size());
    candidate.viewport=test;
    candidate.cells.reserve(test.numberOfContainerSlotsVisible);
    for(std::uint32_t i=0;i<test.numberOfContainerSlotsVisible;++i) {
        const auto absolute=test.firstContainerSlot+i;
        InventoryGridCell cell;
        cell.absoluteSlot=absolute;
        if(absolute<snap.stacks.size()) {
            const auto& stack=snap.stacks[absolute];
            cell.instanceId=stack.instanceId;
            cell.definitionId=stack.definitionId;
            cell.quantity=stack.quantity;
        }
        candidate.cells.push_back(std::move(cell));
    }
    out=std::move(candidate);
    error.clear();
    return InventoryResult::Success;
}
std::string InventoryGridCellActionId(const std::uint32_t index) {
    return "inventory.grid.cell."+std::to_string(index);
}
bool ParseInventoryGridCellActionId(
    const std::string& action,std::uint32_t& visibleIndex) noexcept {
    visibleIndex=0;
    constexpr char prefix[]="inventory.grid.cell.";
    if(action.compare(0,sizeof(prefix)-1,prefix)!=0)return false;
    const char* first=action.data()+sizeof(prefix)-1;
    const char* last=action.data()+action.size();
    if(first==last || *first=='+' || *first=='-' ||
       (*first=='0' && first+1!=last))return false;
    std::uint32_t parsed=0;
    const auto parse=std::from_chars(first,last,parsed,10);
    if(parse.ec!=std::errc{}||parse.ptr!=last || parsed>=1024)return false;
    visibleIndex=parsed;return true;
}
InventoryResult ExecuteInventoryGridCellAction(
    const std::string& action,const InventoryGridPage& source,
    const InventoryGridPage& dest,const std::uint32_t targetVisibleCell,
    const bool wholeStack,InventoryGameplayGateway& gateway) {
    std::uint32_t index=0;
    if(!ParseInventoryGridCellActionId(action,index))
        return InventoryResult::InvalidArgument;
    InventoryPagedTransfer gesture;
    if(!BeginInventoryPagedTransfer(source,index,wholeStack,gesture))
        return InventoryResult::NotFound;
    return CommitInventoryPagedTransfer(
        gesture,source,dest,targetVisibleCell,gateway);
}

bool BeginInventoryPagedTransfer(
    const InventoryGridPage& page,const std::uint32_t index,
    const bool fullStack,InventoryPagedTransfer& gesture) noexcept {
    gesture={};
    if(index>=page.cells.size() || page.containerId.empty())return false;
    const auto& cell=page.cells[index];
    if(!cell.Occupied())return false;
    gesture.sourceContainerId=page.containerId;
    gesture.sourceAbsoluteSlot=cell.absoluteSlot;
    gesture.instanceId=cell.instanceId;
    gesture.quantity=fullStack?cell.quantity:1;
    gesture.revision=page.revision;
    gesture.valid=true;
    return true;
}
InventoryResult CommitInventoryPagedTransfer(
    const InventoryPagedTransfer& gesture,
    const InventoryGridPage& source,
    const InventoryGridPage& destination,
    const std::uint32_t targetVisibleCell,
    InventoryGameplayGateway& gateway) {
    if(!gesture.valid || gesture.quantity==0 ||
       source.containerId.empty() || destination.containerId.empty() ||
       source.containerId==destination.containerId ||
       source.containerId!=gesture.sourceContainerId ||
       source.revision!=gesture.revision ||
       destination.revision!=gesture.revision ||
       gateway.Revision()!=gesture.revision ||
       targetVisibleCell>=destination.cells.size())
        return InventoryResult::InvalidArgument;
    // Validate the source cell after scrolling: a stale or differently
    // populated viewport cannot move an unintended item.
    const auto it=std::find_if(source.cells.begin(),source.cells.end(),
        [&](const InventoryGridCell& cell) {
            return cell.absoluteSlot==gesture.sourceAbsoluteSlot &&
                   cell.instanceId==gesture.instanceId &&
                   cell.quantity>=gesture.quantity;
        });
    if(it==source.cells.end())return InventoryResult::NotFound;
    const auto& target=destination.cells[targetVisibleCell];
    if(target.absoluteSlot<destination.viewport.firstContainerSlot ||
       target.absoluteSlot>=destination.viewport.containerCapacity)
        return InventoryResult::InvalidArgument;
    // The page isn't a capability token. Re-check the authoritative
    // container against the captured position and identity, with live
    // scoped read rights, before asking the gateway to make a transaction.
    InventoryContainer current;
    const auto read=gateway.ReadContainer(gesture.sourceContainerId,current);
    if(read!=InventoryResult::Success)return read;
    if(gesture.sourceAbsoluteSlot>=current.stacks.size() ||
       current.stacks[gesture.sourceAbsoluteSlot].instanceId!=gesture.instanceId ||
       current.stacks[gesture.sourceAbsoluteSlot].quantity<gesture.quantity)
        return InventoryResult::NotFound;
    return gateway.Transfer(gesture.sourceContainerId,
        destination.containerId,gesture.instanceId,gesture.quantity);
}
} // namespace renegade::bridge
