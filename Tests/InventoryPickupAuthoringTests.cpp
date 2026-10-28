#include "renegade/bridge/InventoryPickupAuthoringService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/IdentityService.h"
#include <iostream>
#include <memory>
#include <string>

#define CHECK(v) do { if (!(v)) { std::cerr << "Pickup authoring line " << __LINE__ \
    << ": " << #v << std::endl; return 1; } } while (false)

int main() {
    using namespace renegade::bridge;
    wi::scene::Scene scene;
    const auto root = scene.Entity_CreateTransform("Potion Bottle");
    const auto child = scene.Entity_CreateTransform("Bottle Mesh");
    scene.Component_Attach(child, root, true);
    auto& existing = scene.metadatas.Create(root);
    existing.string_values.set(ReusableAssetInstanceIdMetadataKey, "asset-potion-001");
    existing.string_values.set("creator.original.description", "Glass vial");
    existing.preset = wi::scene::MetadataComponent::Preset::Custom;

    InventoryPickupConfig before;
    std::string error;
    CHECK(ReadInventoryPickupConfig(scene, child, before, error));
    CHECK(!before.canCollect);
    CHECK(before.quantity == 1);

    InventoryPickupConfig config;
    config.canCollect = true;
    config.itemId = "health-potion";
    config.quantity = 3;
    config.method = InventoryPickupMethod::PressE;
    config.removeWhenCollected = true;
    config.interactDistance = 2.75f;
    CommandService history;
    CHECK(history.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        scene, child, config)));
    const auto* meta = scene.metadatas.GetComponent(root);
    CHECK(meta && meta->preset == wi::scene::MetadataComponent::Preset::Pickup);
    const auto persistentId = PersistentEntityId(scene,root);
    CHECK(IsValidStableId(persistentId));
    CHECK(meta->string_values.get("creator.original.description") == "Glass vial");
    CHECK(scene.metadatas.GetComponent(child) == nullptr);
    InventoryPickupConfig loaded;
    CHECK(ReadInventoryPickupConfig(scene, root, loaded, error));
    CHECK(loaded.canCollect && loaded.itemId == "health-potion");
    CHECK(loaded.quantity == 3 && loaded.method == InventoryPickupMethod::PressE);
    CHECK(loaded.interactDistance == 2.75f);
    // The settings must survive the existing native WISCENE serializer,
    // not merely this in-memory command instance.
    {
        wi::Archive archive;
        scene.Serialize(archive);
        archive.SetReadModeAndResetPos(true);
        wi::scene::Scene reopened;
        reopened.Serialize(archive);
        const auto reopenedRoot = reopened.Entity_FindByName("Potion Bottle");
        CHECK(reopenedRoot != wi::ecs::INVALID_ENTITY);
        InventoryPickupConfig fromScene;
        CHECK(ReadInventoryPickupConfig(reopened, reopenedRoot, fromScene, error));
        CHECK(fromScene.canCollect && fromScene.quantity == 3);
        CHECK(fromScene.itemId == "health-potion");
        CHECK(fromScene.method == InventoryPickupMethod::PressE);
        CHECK(PersistentEntityId(reopened,reopenedRoot)==persistentId);
    }
    CHECK(history.Undo());
    CHECK(ReadInventoryPickupConfig(scene, root, loaded, error));
    CHECK(!loaded.canCollect);
    CHECK(PersistentEntityId(scene,root).empty());
    CHECK(scene.metadatas.GetComponent(root)->string_values.get(
        "creator.original.description") == "Glass vial");
    CHECK(history.Redo());
    CHECK(PersistentEntityId(scene,root)==persistentId);
    CHECK(ReadInventoryPickupConfig(scene, child, loaded, error));
    CHECK(loaded.canCollect && loaded.quantity == 3);

    const auto previous = loaded;
    auto invalid = config;
    invalid.quantity = 0;
    CHECK(!history.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        scene, root, invalid)));
    invalid = config;
    invalid.itemId.clear();
    CHECK(!history.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        scene, root, invalid)));
    invalid = config;
    invalid.interactDistance = 999.0f;
    CHECK(!history.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        scene, root, invalid)));
    CHECK(ReadInventoryPickupConfig(scene, root, loaded, error));
    CHECK(loaded.itemId == previous.itemId && loaded.quantity == previous.quantity);

    // Re-applying unchanged settings on a legacy G5 scene must upgrade its
    // missing persistent identity without changing pickup gameplay fields.
    const auto legacy = scene.Entity_CreateTransform("Legacy Potion");
    auto& legacyMetadata=scene.metadatas.Create(legacy);
    legacyMetadata.bool_values.set(PickupEnabledKey,true);
    legacyMetadata.string_values.set(PickupItemIdKey,config.itemId);
    legacyMetadata.int_values.set(PickupQuantityKey,static_cast<int>(config.quantity));
    legacyMetadata.int_values.set(PickupMethodKey,static_cast<int>(config.method));
    legacyMetadata.bool_values.set(PickupRemoveKey,config.removeWhenCollected);
    legacyMetadata.float_values.set(PickupDistanceKey,config.interactDistance);
    CHECK(PersistentEntityId(scene,legacy).empty());
    CHECK(history.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        scene,legacy,config)));
    CHECK(IsValidStableId(PersistentEntityId(scene,legacy)));
    CHECK(history.Undo());
    CHECK(PersistentEntityId(scene,legacy).empty());
    CHECK(history.Redo());
    CHECK(IsValidStableId(PersistentEntityId(scene,legacy)));

    const auto basic = scene.Entity_CreateTransform("Unrelated Rock");
    auto& semanticOnly = scene.metadatas.Create(basic);
    semanticOnly.preset = wi::scene::MetadataComponent::Preset::Pickup;
    CHECK(ReadInventoryPickupConfig(scene, basic, loaded, error));
    CHECK(!loaded.canCollect); // Legacy semantic tag cannot grant items.
    config.method = InventoryPickupMethod::OnTouch;
    config.removeWhenCollected = false;
    CHECK(history.Execute(std::make_unique<SetInventoryPickupConfigCommand>(
        scene, basic, config)));
    CHECK(ReadInventoryPickupConfig(scene, basic, loaded, error));
    CHECK(loaded.canCollect && loaded.method == InventoryPickupMethod::OnTouch);
    CHECK(!loaded.removeWhenCollected);
    CHECK(history.Undo());
    CHECK(ReadInventoryPickupConfig(scene, basic, loaded, error));
    CHECK(!loaded.canCollect);
    CHECK(scene.metadatas.GetComponent(basic)->preset ==
        wi::scene::MetadataComponent::Preset::Pickup);

    std::cout << "PASS: G5B authored pickup metadata and native Undo/Redo" << std::endl;
    return 0;
}
