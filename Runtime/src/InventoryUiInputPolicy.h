#pragma once
namespace renegade::runtime {
// Inventory-only UI interaction may remain active while ordinary movement
// and weapon input are blocked by modal Screen composition.
inline bool InventoryInteractHasFocus(bool gameplayFrame,bool overlayOpen,
                                     bool presenterLoaded,bool paused,
                                     bool anotherTextInputActive) noexcept {
    return gameplayFrame && overlayOpen && presenterLoaded &&
           !paused && !anotherTextInputActive;
}
// Generic over the engine action-frame type: cannot import Wicked or create a
// second input system. The opened inventory owns E/Interact as well as mouse
// and arrow keys; do not also send E into world scripts or pickups. Preserve
// Pause, while discarding world interaction, movement and combat controls.
template <typename ActionFrame>
ActionFrame GateWorldInputForInventory(ActionFrame input,bool overlayOpen) noexcept {
    if(!overlayOpen)return input;
    ActionFrame result{};
    result.pausePressed=input.pausePressed;
    return result;
}
} // namespace renegade::runtime
