#pragma once
#include "renegade/bridge/InventoryScreenAdapter.h"
#include <cstddef>
#include <cstdint>
#include <string>
namespace renegade::bridge {
// A pointer gesture borrows a stable stack ID; it never takes item ownership.
// A drop is a single scoped InventoryService transaction, all-or-nothing.
class InventoryDragTransfer final {
public:
    // Default: a single item. Modifier at mouse-down requests a full stack.
    bool Begin(const InventoryOverlayView& view,const std::string& sourceAction,
               bool wholeStack=false);
    InventoryResult Drop(const InventoryOverlayView& view,
                         const std::string& destinationAction,
                         InventoryGameplayGateway& gateway);
    void Cancel() noexcept;
    bool Active() const noexcept {return active_;}
    const std::string& SourceInstanceId() const noexcept {return instanceId_;}
    std::uint32_t Quantity() const noexcept {return active_?count_:0;}
private:
    bool active_=false;
    std::uint64_t revision_=0;
    std::size_t sourcePanel_=0;
    std::uint32_t count_=0;
    std::string sourceContainer_,instanceId_;
};
} // namespace renegade::bridge
