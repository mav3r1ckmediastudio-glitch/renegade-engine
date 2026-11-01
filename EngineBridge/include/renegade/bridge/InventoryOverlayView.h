#pragma once
#include "renegade/bridge/InventoryService.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace renegade::bridge {
// One frame of presentation data, never an inventory authority.
// Rendering and scripted gameplay share the same InventoryService source.
struct InventorySlotView {
    std::string instanceId;
    std::string itemName;
    std::uint32_t quantity=0;
    std::uint32_t maxStack=0;
    std::string definitionId; // Presentation-only icon lookup; no item authority.
};
struct InventoryContainerView {
    std::string containerId;
    std::string title;
    std::uint32_t usedSlots=0;
    std::uint32_t maxSlots=0;
    double usedWeight=0;
    double maxWeight=0;
    std::vector<InventorySlotView> rows;
    bool truncated=false;
};
struct InventoryOverlayView {
    std::uint64_t revision=0;
    std::vector<InventoryContainerView> panels;
};
// Explicitly choose the containers and titles to present. Missing containers
// are omitted, not constructed. Bound item labels/rows for safe UI rendering.
// Results use no Wicked/renderer/UI Designer code and mutate no gameplay state.
InventoryOverlayView BuildInventoryOverlayView(
    const InventoryService& inventory,
    const std::vector<std::pair<std::string,std::string>>& requestedContainers,
    std::size_t maximumRowsPerContainer=8);
} // namespace renegade::bridge
