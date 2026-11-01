#pragma once

#include "renegade/bridge/InventoryGameplayGateway.h"

#include <string>
#include <vector>

namespace renegade::bridge {

enum class InventoryGrantKind {
    Container, DefineItems, CreateContainer
};

struct InventoryAccessGrant {
    InventoryGrantKind kind = InventoryGrantKind::Container;
    std::string scriptInstanceId;
    std::string ownerEntityId; // Empty means level-scoped script.
    std::string argument;      // Container ID or creation prefix.
    InventoryAccess permissions;
};

// One shared, versioned validator for Runtime, Test Level and Build Game.
// Validation and the result vector are atomic: failures clear the output.
// Do not trust parsed grants to authenticate the invoking script instance;
// the Runtime must still match StableId and entity owner ID per script.
bool ParseInventoryAccessPolicy(const std::string& bytes,
                                const InventoryService& inventory,
                                std::vector<InventoryAccessGrant>& result,
                                std::string& error);

// Reads only bounded regular files, then calls the same validator.
bool ReadInventoryAccessPolicy(const std::string& path,
                               const InventoryService& inventory,
                               std::vector<InventoryAccessGrant>& result,
                               std::string& error);
} // namespace renegade::bridge
