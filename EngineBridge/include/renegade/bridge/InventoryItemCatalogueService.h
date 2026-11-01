#pragma once
// G5A: Creator-owned, optional item catalogue. Independent from Runtime stacks
// and ordinary Screen layout. A project without items does not need this file.
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/InventoryService.h"
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge {
inline constexpr const char* InventoryItemCatalogueDocumentType = "inventory-item-catalogue";

enum class InventoryItemKind : std::uint8_t {
    Consumable, Equipment, Material, KeyItem
};
enum class InventoryItemEffect : std::uint8_t {
    None, RestoreHealth
};

struct InventoryAuthoringItem {
    // Stable logical item identifier, not the name of a scene entity or image.
    std::string id;
    std::string name;
    InventoryItemKind kind = InventoryItemKind::Material;
    InventoryItemEffect effect = InventoryItemEffect::None;
    std::uint32_t healthRestored = 0;
    std::uint32_t maximumStack = 20;
    double weightPerUnit = 0;
    std::string iconProjectRelativePath;
    std::string equipmentAssetId;
};
struct InventoryItemCatalogue {
    DocumentEnvelope envelope;
    std::uint32_t schemaVersion = 1;
    std::uint32_t defaultMaximumStack = 20;
    std::vector<InventoryAuthoringItem> items;
};

[[nodiscard]] bool ValidateInventoryItemCatalogue(
    const InventoryItemCatalogue& catalogue,
    const StableId& expectedProjectId, std::string& error);
[[nodiscard]] bool ReadInventoryItemCatalogue(
    const std::string& filePath, const StableId& expectedProjectId,
    InventoryItemCatalogue& result, std::string& error);
[[nodiscard]] bool WriteInventoryItemCatalogue(
    const std::string& filePath, const InventoryItemCatalogue& catalogue,
    std::string& error);
// An atomic authoring edit. Does not mutate the caller on failure; replaces an
// item with the same ID or inserts a new definition after full validation.
[[nodiscard]] bool UpsertInventoryAuthoringItem(
    InventoryItemCatalogue& catalogue,
    InventoryAuthoringItem item, std::string& error);
[[nodiscard]] const InventoryAuthoringItem* FindInventoryAuthoringItem(
    const InventoryItemCatalogue& catalogue, const std::string& id) noexcept;
// Runtime definitions are derived from the catalogue, not copied into the
// Screen template. Existing InventoryService is still the stack authority.
[[nodiscard]] InventoryItemDefinition MakeRuntimeInventoryDefinition(
    const InventoryAuthoringItem& item);
}
