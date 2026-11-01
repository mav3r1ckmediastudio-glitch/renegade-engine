#pragma once
// G4-B3C0: pure, creator-configured inventory viewport model.
// This is NOT a second item store, a Screen renderer, or a widget factory.
#include <cstdint>
#include <string>

namespace renegade::bridge {

enum class InventoryIconFit { Contain, Cover, Stretch };

// Display settings only. They never change an InventoryContainer's capacity
// or any InventoryService transaction. Individual slot styling remains in the
// authorable Screen widget/theme, NOT in this model.
struct InventoryGridLayout {
    std::uint32_t columns=5;
    std::uint32_t visibleRows=5;
    float slotWidth=72.0f;
    float slotHeight=72.0f;
    float horizontalGap=6.0f;
    float verticalGap=6.0f;
    float padding=8.0f;
    float iconPadding=7.0f;
    bool squareSlots=true;
    bool showScrollbar=true;
    InventoryIconFit iconFit=InventoryIconFit::Contain;
};

struct InventoryGridViewport {
    std::uint32_t containerCapacity=0;
    std::uint32_t totalRows=0;
    std::uint32_t maximumTopRow=0;
    std::uint32_t topRow=0;
    std::uint32_t visibleCellCapacity=0;
    std::uint32_t firstContainerSlot=0;
    std::uint32_t numberOfContainerSlotsVisible=0;
    bool scrollbarNeeded=false;
    double scrollbarFraction=0.0;
    double scrollbarThumbFraction=1.0;
};

// Explicit errors protect authoring and Runtime equally. An absent/empty
// container has an empty viewport and never constructs fictitious items.
bool ValidateInventoryGridLayout(const InventoryGridLayout& layout,
                                 std::string& error) noexcept;
bool ResolveInventoryGridViewport(const InventoryGridLayout& layout,
                                  std::uint32_t containerCapacity,
                                  std::uint32_t requestedTopRow,
                                  InventoryGridViewport& result,
                                  std::string& error) noexcept;
// These helpers never mutate items or alter capacity; callers use an actual
// visible absolute slot index to bind their authorised presentation snapshot.
std::uint32_t InventoryGridRowAfterWheel(const InventoryGridViewport& viewport,
                                         int wheelRows) noexcept;
std::uint32_t InventoryGridRowAtScrollbar(const InventoryGridViewport& viewport,
                                          double fraction) noexcept;
} // namespace renegade::bridge
