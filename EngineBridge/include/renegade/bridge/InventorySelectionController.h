#pragma once
#include "renegade/bridge/InventoryGameplayGateway.h"
#include "renegade/bridge/InventoryOverlayView.h"
#include <cstddef>
#include <string>

namespace renegade::bridge {

// Presentation-only selection for the two-container G4 prototype. Both native
// Runtime and a future UI Designer inventory binding can use this logic.
// It owns no item stacks and cannot grant itself container permissions.
class InventorySelectionController final {
public:
    void Reset() noexcept;
    void Reconcile(const InventoryOverlayView& view);
    bool Select(const InventoryOverlayView& view, std::size_t panel, std::size_t row);
    bool Move(const InventoryOverlayView& view, int direction);
    bool SwitchContainer(const InventoryOverlayView& view);
    InventoryResult TransferSelected(const InventoryOverlayView& view,
                                    InventoryGameplayGateway& gateway,
                                    std::uint32_t count = 1);

    int SelectedPanel() const noexcept { return selectedPanel_; }
    int SelectedRow() const noexcept { return selectedRow_; }
    const std::string& SelectedInstanceId() const noexcept { return instanceId_; }
private:
    int selectedPanel_=-1;
    int selectedRow_=-1;
    std::string containerId_;
    std::string instanceId_;
};
} // namespace renegade::bridge
