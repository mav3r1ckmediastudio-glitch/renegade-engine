#include "renegade/bridge/InventoryPickupAuthoringService.h"
#include "renegade/bridge/InventoryService.h"
#include "renegade/bridge/IdentityService.h"
#include <cmath>
#include <utility>

namespace renegade::bridge {
bool ValidateInventoryPickupConfig(const InventoryPickupConfig& config, std::string& error) {
    if (config.quantity == 0 || config.quantity > 1000000 ||
        !std::isfinite(config.interactDistance) ||
        config.interactDistance < 0.5f || config.interactDistance > 20.0f ||
        static_cast<unsigned>(config.method) > 1) {
        error = "Pickup quantity, interaction method or distance is invalid.";
        return false;
    }
    if (config.canCollect || !config.itemId.empty()) {
        InventoryService scratch;
        if (scratch.RegisterItem({config.itemId, "Pickup Item", 1, 0.0, {}}) !=
            InventoryResult::Success) {
            error = "Choose an existing inventory item before enabling collection.";
            return false;
        }
    }
    error.clear();
    return true;
}

bool ReadInventoryPickupConfig(const wi::scene::Scene& scene,
        wi::ecs::Entity selected, InventoryPickupConfig& result, std::string& error) {
    const auto entity = ResolveSceneComponentAuthoringRoot(scene, selected);
    InventoryPickupConfig candidate;
    const auto* metadata = scene.metadatas.GetComponent(entity);
    if (metadata != nullptr) {
        if (metadata->bool_values.has(PickupEnabledKey))
            candidate.canCollect = metadata->bool_values.get(PickupEnabledKey);
        if (metadata->string_values.has(PickupItemIdKey))
            candidate.itemId = metadata->string_values.get(PickupItemIdKey);
        if (metadata->int_values.has(PickupQuantityKey)) {
            const int value = metadata->int_values.get(PickupQuantityKey);
            if (value <= 0) {
                error = "Saved pickup quantity must be positive.";
                return false;
            }
            candidate.quantity = static_cast<std::uint32_t>(value);
        }
        if (metadata->int_values.has(PickupMethodKey)) {
            const int value = metadata->int_values.get(PickupMethodKey);
            if (value < 0 || value > 1) {
                error = "Saved pickup interaction method is unsupported.";
                return false;
            }
            candidate.method = static_cast<InventoryPickupMethod>(value);
        }
        if (metadata->bool_values.has(PickupRemoveKey))
            candidate.removeWhenCollected = metadata->bool_values.get(PickupRemoveKey);
        if (metadata->float_values.has(PickupDistanceKey))
            candidate.interactDistance = metadata->float_values.get(PickupDistanceKey);
    }
    if (!ValidateInventoryPickupConfig(candidate, error)) return false;
    result = std::move(candidate);
    error.clear();
    return true;
}

SetInventoryPickupConfigCommand::SetInventoryPickupConfigCommand(
        wi::scene::Scene& scene, wi::ecs::Entity selected, InventoryPickupConfig desired)
        : scene_(&scene),
          entity_(ResolveSceneComponentAuthoringRoot(scene, selected)),
          after_(std::move(desired)) {
    if (const auto* metadata = scene.metadatas.GetComponent(entity_)) {
        before_ = *metadata;
        hadMetadata_ = true;
    }
}
bool SetInventoryPickupConfigCommand::Execute() {
    if (!scene_ || entity_ == wi::ecs::INVALID_ENTITY) return false;
    std::string error;
    if (!ValidateInventoryPickupConfig(after_, error)) return false;
    InventoryPickupConfig current;
    if (!ReadInventoryPickupConfig(*scene_, entity_, current, error)) return false;
    // A scene object's persistent ID is the identity used by Save Game to
    // retire this particular world pickup, not the shared catalogue item ID.
    // Legacy pickup scenes may have settings but no persistent ID yet.
    const auto existingId = PersistentEntityId(*scene_, entity_);
    if (!existingId.empty() && !IsValidStableId(existingId)) return false;
    const bool needsId = after_.canCollect && existingId.empty();
    if (!needsId && current.canCollect == after_.canCollect &&
        current.itemId == after_.itemId &&
        current.quantity == after_.quantity &&
        current.method == after_.method &&
        current.removeWhenCollected == after_.removeWhenCollected &&
        current.interactDistance == after_.interactDistance)
        return false;
    auto* metadata = scene_->metadatas.GetComponent(entity_);
    if (!metadata) metadata = &scene_->metadatas.Create(entity_);
    if (after_.canCollect) metadata->preset = wi::scene::MetadataComponent::Preset::Pickup;
    else if (metadata->preset == wi::scene::MetadataComponent::Preset::Pickup)
        metadata->preset = wi::scene::MetadataComponent::Preset::Custom;
    metadata->bool_values.set(PickupEnabledKey, after_.canCollect);
    metadata->string_values.set(PickupItemIdKey, after_.itemId);
    metadata->int_values.set(PickupQuantityKey, static_cast<int>(after_.quantity));
    metadata->int_values.set(PickupMethodKey, static_cast<int>(after_.method));
    metadata->bool_values.set(PickupRemoveKey, after_.removeWhenCollected);
    metadata->float_values.set(PickupDistanceKey, after_.interactDistance);
    if (needsId) {
        std::string identityError;
        const bool assigned = generatedPersistentId_.empty()
            ? AssignNewPersistentEntityId(*scene_,entity_,identityError)
            : AssignPersistentEntityId(*scene_,entity_,
                                       generatedPersistentId_,identityError);
        if (!assigned) {
            if (!hadMetadata_) scene_->metadatas.Remove(entity_);
            else *metadata = before_;
            return false;
        }
        generatedPersistentId_ = PersistentEntityId(*scene_,entity_);
    }
    executed_ = true;
    return true;
}
void SetInventoryPickupConfigCommand::Undo() {
    if (!executed_ || !scene_ || entity_ == wi::ecs::INVALID_ENTITY) return;
    if (!hadMetadata_) scene_->metadatas.Remove(entity_);
    else {
        auto* metadata = scene_->metadatas.GetComponent(entity_);
        if (!metadata) metadata = &scene_->metadatas.Create(entity_);
        *metadata = before_;
    }
}
}
