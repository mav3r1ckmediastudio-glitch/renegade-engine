#pragma once
#include "renegade/bridge/InventoryOverlayView.h"
#include "renegade/bridge/InventorySelectionController.h"
#include "renegade/bridge/ScreenService.h"
#include <cstddef>
#include <string>

namespace renegade::bridge {

// G4-B2A: original, editable *standard Screen* starter, not a new UI editor.
// Slot actions are stable explicit identities, not gameplay state in the
// authored document. The normal Screen Renderer can draw its projected copy.
// Optional authorable four-by-two graphical slot screen, still using ordinary
// ScreenService widgets and the same inventory actions/gateway.
ScreenDocument MakeInventoryIconGridScreen(
    const StableId& projectId,const std::string& projectRelativePath);

// Safe built-in starter art mapping, independent of gameplay definitions.
// Creators can replace individual files or provide their own icon provider.
std::string InventoryStarterIconForDefinition(const std::string& definitionId);

ScreenDocument MakeInventoryStarterScreen(
    const StableId& projectId,
    const std::string& projectRelativePath,
    std::size_t rowsPerContainer = 8);

// Pure view projection: preserve authored layout, focus order, widget IDs,
// styles, local actions and optional artwork. Live inventory can only change
// slot button text and enabled state on a transient copy.
bool ProjectInventoryIntoScreen(
    const ScreenDocument& authored,
    const InventoryOverlayView& view,
    ScreenDocument& projected,
    std::string& error);

// Parse a bounded native slot action without trusting the caller's string.
bool ParseInventoryScreenSlotAction(const std::string& actionId,
    std::size_t& panel,std::size_t& row) noexcept;

// Exact one-item command issued only for a recognised, currently visible slot
// action. The caller still supplies the per-gameplay-session scoped gateway.
// Works with mouse activation or keyboard/gamepad focus from existing Screens.
InventoryResult ExecuteInventoryScreenSlotAction(
    const std::string& actionId,
    const InventoryOverlayView& view,
    InventorySelectionController& selection,
    InventoryGameplayGateway& gateway);

} // namespace renegade::bridge
