#pragma once
// G3-A: Explicit, optional Runtime inventory owner for one Scene.
// No inventory is constructed when the level has no companion.
#include "renegade/bridge/InventoryGameplayGateway.h"

#include <map>
#include <memory>
#include <string>

namespace renegade::runtime {

class RuntimeInventorySession final {
public:
    // Source is <validated-scene-path>.rinventory and an optional
    // <validated-scene-path>.rinventory-access authoring policy. Both must
    // remain within the project root. No automatic Player inventory.
    bool OpenForScene(const std::string& projectRoot, const std::string& scenePath,
                      std::string& error);
    void Reset() noexcept;
    bool Enabled() const noexcept { return static_cast<bool>(inventory_); }
    bridge::InventoryService* Inventory() noexcept { return inventory_.get(); }
    const bridge::InventoryService* Inventory() const noexcept { return inventory_.get(); }

    // Save Game integration seam. This emits/consumes payload bytes only;
    // the future governed save-slot owner supplies project/scene identity,
    // disk transactions, player selection and recovery. Never overwrite the
    // authored <scene>.rinventory companion with live gameplay progress.
    bool CaptureRuntimeState(std::string& out, std::string& error) const;
    bool RestoreRuntimeState(const std::string& bytes, std::string& error);

    bridge::InventoryGameplayGateway* GatewayForScript(
        const std::string& scriptInstanceId,
        const std::string& ownerEntityId) noexcept;

private:
    struct Entry {
        std::string ownerEntityId;
        std::unique_ptr<bridge::InventoryGameplayGateway> gateway;
    };
    std::unique_ptr<bridge::InventoryService> inventory_;
    std::map<std::string, Entry> scriptAccess_;
};

} // namespace renegade::runtime
