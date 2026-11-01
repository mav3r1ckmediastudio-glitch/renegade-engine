#pragma once
// G5B scene authoring contract. A Wicked PICKUP preset alone is not an
// inventory pickup. Only explicit opt-in with a valid item ID may collect.
#include "renegade/bridge/SceneComponentService.h"
#include <cstdint>
#include <string>

namespace renegade::bridge {
enum class InventoryPickupMethod : std::uint8_t { PressE, OnTouch };
struct InventoryPickupConfig {
    bool canCollect = false;
    std::string itemId;
    std::uint32_t quantity = 1;
    InventoryPickupMethod method = InventoryPickupMethod::PressE;
    bool removeWhenCollected = true;
    float interactDistance = 2.5f;
};
// Metadata keys are stable saved scene data, not a parallel world item store.
inline constexpr const char* PickupEnabledKey = "renegade.inventory.pickup.enabled";
inline constexpr const char* PickupItemIdKey = "renegade.inventory.pickup.item";
inline constexpr const char* PickupQuantityKey = "renegade.inventory.pickup.quantity";
inline constexpr const char* PickupMethodKey = "renegade.inventory.pickup.method";
inline constexpr const char* PickupRemoveKey = "renegade.inventory.pickup.remove";
inline constexpr const char* PickupDistanceKey = "renegade.inventory.pickup.distance";

[[nodiscard]] bool ValidateInventoryPickupConfig(const InventoryPickupConfig&,
                                                  std::string& error);
[[nodiscard]] bool ReadInventoryPickupConfig(const wi::scene::Scene& scene,
        wi::ecs::Entity selected, InventoryPickupConfig& result, std::string& error);
// Only called from the command boundary, never directly from UI controls.
class SetInventoryPickupConfigCommand final : public ICommand {
public:
    SetInventoryPickupConfigCommand(wi::scene::Scene& scene,
        wi::ecs::Entity selected, InventoryPickupConfig desired);
    bool Execute() override;
    void Undo() override;
private:
    wi::scene::Scene* scene_;
    wi::ecs::Entity entity_;
    InventoryPickupConfig after_;
    wi::scene::MetadataComponent before_;
    bool hadMetadata_ = false;
    bool executed_ = false;
    std::string generatedPersistentId_;
};
}
