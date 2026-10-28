#include "renegade/bridge/InventoryItemCatalogueService.h"
#include <filesystem>
#include <iostream>
#include <string>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "InventoryItemCatalogueTests FAILED line " << __LINE__ \
              << ": " << #condition << std::endl; return 1; \
} } while (false)

int main() {
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    const auto project = GenerateStableId();
    const auto root = fs::temp_directory_path() /
        ("renegade-item-catalogue-" + GenerateStableId());
    const auto path = root / "Content" / "Gameplay" / "Inventory.renegade-items";
    fs::create_directories(path.parent_path());
    std::string error;
    InventoryItemCatalogue document;
    document.envelope = CreateDocumentEnvelope(
        project, InventoryItemCatalogueDocumentType,
        "Content/Gameplay/Inventory.renegade-items", "G5A");
    document.defaultMaximumStack = 20;
    InventoryAuthoringItem potion;
    potion.id = "health-potion";
    potion.name = "Small Health Potion";
    potion.kind = InventoryItemKind::Consumable;
    potion.effect = InventoryItemEffect::RestoreHealth;
    potion.healthRestored = 50;
    potion.maximumStack = 10;
    potion.weightPerUnit = 0.2;
    potion.iconProjectRelativePath = "Content/UI/Images/healing-potion.png";
    CHECK(UpsertInventoryAuthoringItem(document, potion, error));
    InventoryAuthoringItem sword;
    sword.id = "long-sword";
    sword.name = "Long Sword";
    sword.kind = InventoryItemKind::Equipment;
    sword.maximumStack = 1;
    sword.equipmentAssetId = "equipment.sword";
    CHECK(UpsertInventoryAuthoringItem(document, sword, error));
    CHECK(document.items.size() == 2);
    CHECK(WriteInventoryItemCatalogue(path.generic_u8string(), document, error));
    InventoryItemCatalogue loaded;
    CHECK(ReadInventoryItemCatalogue(path.generic_u8string(), project, loaded, error));
    CHECK(loaded.envelope.documentId == document.envelope.documentId);
    CHECK(loaded.items.size() == 2);
    const auto* recovered = FindInventoryAuthoringItem(loaded, "health-potion");
    CHECK(recovered && recovered->healthRestored == 50);
    CHECK(recovered->maximumStack == 10);
    CHECK(recovered->iconProjectRelativePath == potion.iconProjectRelativePath);
    CHECK(MakeRuntimeInventoryDefinition(*recovered).maximumStack == 10);
    const auto originalIcon = recovered->iconProjectRelativePath;
    potion.name = "Greater Health Potion";
    potion.healthRestored = 100;
    CHECK(UpsertInventoryAuthoringItem(loaded, potion, error));
    CHECK(loaded.items.size() == 2);
    CHECK(WriteInventoryItemCatalogue(path.generic_u8string(), loaded, error));
    InventoryItemCatalogue reopening;
    CHECK(ReadInventoryItemCatalogue(path.generic_u8string(), project, reopening, error));
    CHECK(FindInventoryAuthoringItem(reopening, "health-potion")->healthRestored == 100);
    CHECK(FindInventoryAuthoringItem(reopening, "long-sword")->maximumStack == 1);

    // Atomic in-memory failure, and no damaged project document on invalid write.
    const auto goodName = FindInventoryAuthoringItem(reopening, "health-potion")->name;
    potion.iconProjectRelativePath = "../outside.png";
    CHECK(!UpsertInventoryAuthoringItem(reopening, potion, error));
    CHECK(FindInventoryAuthoringItem(reopening, "health-potion")->name == goodName);
    potion.iconProjectRelativePath = originalIcon;
    potion.maximumStack = 0;
    CHECK(!UpsertInventoryAuthoringItem(reopening, potion, error));
    potion.maximumStack = 10;
    potion.effect = InventoryItemEffect::None;
    CHECK(!UpsertInventoryAuthoringItem(reopening, potion, error));
    potion.effect = InventoryItemEffect::RestoreHealth;
    potion.kind = InventoryItemKind::KeyItem;
    CHECK(!UpsertInventoryAuthoringItem(reopening, potion, error));
    CHECK(reopening.items.size() == 2);
    potion.kind = InventoryItemKind::Consumable;
    reopening.items.push_back(potion);
    CHECK(!WriteInventoryItemCatalogue(path.generic_u8string(), reopening, error));
    InventoryItemCatalogue intact;
    CHECK(ReadInventoryItemCatalogue(path.generic_u8string(), project, intact, error));
    CHECK(intact.items.size() == 2);
    CHECK(FindInventoryAuthoringItem(intact, "health-potion")->healthRestored == 100);
    CHECK(!ReadInventoryItemCatalogue(path.generic_u8string(), GenerateStableId(), reopening, error));
    CHECK(!ReadInventoryItemCatalogue((root / "nonexistent.renegade-items").generic_u8string(),
                                   project, reopening, error));
    std::error_code ec;
    fs::remove_all(root, ec);
    std::cout << "PASS: G5A item catalogue roundtrip, validation, stable IDs and atomic failure\n";
    return 0;
}
