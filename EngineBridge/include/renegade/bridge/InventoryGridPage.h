#pragma once
// G4-B3C1: bounded, scoped read-only inventory projection for a creator grid.
// Widget positions are display cells, NOT authoritative item placement slots.
#include "renegade/bridge/InventoryGameplayGateway.h"
#include "renegade/bridge/InventoryGridViewport.h"
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge {
struct InventoryGridCell {
    std::uint32_t absoluteSlot=0; // Index within the container's packed stacks.
    std::string instanceId;       // Empty if no stack at this displayed index.
    std::string definitionId;
    std::uint32_t quantity=0;
    bool Occupied() const noexcept {return !instanceId.empty() && quantity>0;}
};
struct InventoryGridPage {
    std::string containerId;
    std::uint64_t revision=0;
    std::uint32_t usedSlots=0;
    InventoryGridViewport viewport;
    std::vector<InventoryGridCell> cells;
};
// Respect the same gateway's read permissions; never retain its mutable
// container snapshot. Release checks after the next gameplay revision.
InventoryResult ReadInventoryGridPage(const InventoryGameplayGateway& gateway,
    const std::string& containerId,const InventoryGridLayout& layout,
    std::uint32_t topRow,InventoryGridPage& out,
    std::string& error);

// A Screen button carries only a visible cell index, never an authority
// token or the item's instance ID. The live page provides that mapping.
std::string InventoryGridCellActionId(std::uint32_t visibleCellIndex);
bool ParseInventoryGridCellActionId(const std::string& action,
                                    std::uint32_t& visibleCellIndex) noexcept;

// Captures a source stack at a specific visible absolute position and
// revision. The requested quantity is frozen for the gesture.
struct InventoryPagedTransfer {
    std::string sourceContainerId,instanceId;
    std::uint32_t sourceAbsoluteSlot=0;
    std::uint32_t quantity=0;
    std::uint64_t revision=0;
    bool valid=false;
};
bool BeginInventoryPagedTransfer(const InventoryGridPage& source,
    std::uint32_t visibleCellIndex,bool fullStack,
    InventoryPagedTransfer& gesture) noexcept;
// Resolves a creator-authored Screen's action through its currently
// visible read-only page, then applies the same permission-scoped command.
InventoryResult ExecuteInventoryGridCellAction(
    const std::string& actionId,
    const InventoryGridPage& source,
    const InventoryGridPage& destination,
    std::uint32_t destinationVisibleCell,
    bool fullStack,InventoryGameplayGateway& gateway);

// The drop target chooses the destination CONTAINER, not an exact slot
// position: the existing compact-stacks InventoryService controls placement.
InventoryResult CommitInventoryPagedTransfer(
    const InventoryPagedTransfer& gesture,
    const InventoryGridPage& source,
    const InventoryGridPage& destination,std::uint32_t targetVisibleCellIndex,
    InventoryGameplayGateway& gateway);
} // namespace renegade::bridge
