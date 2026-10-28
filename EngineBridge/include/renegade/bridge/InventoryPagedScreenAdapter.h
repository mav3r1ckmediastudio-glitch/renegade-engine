#pragma once
#include "renegade/bridge/InventoryGridPage.h"
#include "renegade/bridge/ScreenService.h"
#include <cstddef>
#include <string>
namespace renegade::bridge {
// Two panels use six native widgets per visible cell pair plus sixteen fixed widgets.
inline constexpr std::uint32_t InventoryPagedMaximumVisibleCells=40;
bool FitInventoryPagedGridSlots(InventoryGridLayout& layout,std::string& error);
// Separate creator-authored Screen template. It owns NO inventory data.
ScreenDocument MakeInventoryPagedGridScreen(const StableId& projectId,
    const std::string& path,const InventoryGridLayout& layout);
bool ParseInventoryPagedGridSlotAction(const std::string& action,
    std::size_t& panel,std::uint32_t& visibleIndex) noexcept;
bool ParseInventoryPagedGridScrollAction(const std::string& action,
    std::size_t& panel,int& direction) noexcept;
// Strict, shared local-action semantics, including legacy Screens without local_only.
bool IsInventoryPagedLocalAction(const std::string& action) noexcept;
// Derives UI settings from the existing native widgets; never stores item data.
bool ReadInventoryPagedGridLayout(const ScreenDocument& screen,
    InventoryGridLayout& layout,std::string& error);
// Only changes the transient Screen projection, never the saved Screen file.
// Both panels must represent the same authoritative inventory revision.
bool ProjectPagedInventoryIntoScreen(const ScreenDocument& authored,
    const InventoryGridPage& player,const InventoryGridPage& chest,
    ScreenDocument& projected,std::string& error);
} // namespace renegade::bridge
