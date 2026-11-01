#pragma once
// G5C: successful potion use is an authorised inventory withdrawal followed
// by an update to the existing native combat Player health authority.
// No shadow health values, no effect from merely selecting an item.
#include "RuntimeCombatService.h"
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/InventoryGameplayGateway.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace renegade::runtime {
enum class InventoryItemUseResult {
    Used, MissingItem, NotConsumable, NotHealing, NoHealthAuthority,
    AtFullHealth, PlayerDead, PermissionDenied, InventoryChanged
};
inline InventoryItemUseResult UseHealthPotion(
        const bridge::InventoryService& inventory,
        bridge::InventoryGameplayGateway& gateway,
        const bridge::InventoryItemCatalogue& catalogue,
        const std::string& playerStackId,
        RuntimeCombatState& combat) {
    if (playerStackId.empty()) return InventoryItemUseResult::MissingItem;
    const auto* player=inventory.FindContainer("player");
    if (!player) return InventoryItemUseResult::MissingItem;
    const auto stack=std::find_if(player->stacks.begin(),player->stacks.end(),
        [&](const bridge::InventoryItemStack& s) {
            return s.instanceId==playerStackId && s.quantity>0;
        });
    if (stack==player->stacks.end()) return InventoryItemUseResult::MissingItem;
    const auto* item=bridge::FindInventoryAuthoringItem(
        catalogue,stack->definitionId);
    if (!item || item->kind!=bridge::InventoryItemKind::Consumable)
        return InventoryItemUseResult::NotConsumable;
    if (item->effect!=bridge::InventoryItemEffect::RestoreHealth ||
        item->healthRestored==0)
        return InventoryItemUseResult::NotHealing;
    // Preserve authoritative native health; do not create a synthetic 100HP
    // player if the combat state is invalid or has no health to heal.
    if (!std::isfinite(combat.playerHealth) ||
        !std::isfinite(combat.playerMaxHealth) ||
        combat.playerMaxHealth<=0 || combat.playerHealth<0)
        return InventoryItemUseResult::NoHealthAuthority;
    if (combat.playerDead || combat.playerHealth<=0)
        return InventoryItemUseResult::PlayerDead;
    if (combat.playerHealth>=combat.playerMaxHealth)
        return InventoryItemUseResult::AtFullHealth;
    const double healed=std::min(static_cast<double>(combat.playerMaxHealth),
        static_cast<double>(combat.playerHealth)+item->healthRestored);
    // The gateway enforces player-container withdrawal permission. No
    // health is awarded if another system removed the stack or denied use.
    const auto removed=gateway.Remove("player",playerStackId,1);
    if (removed!=bridge::InventoryResult::Success)
        return removed==bridge::InventoryResult::PermissionDenied
            ? InventoryItemUseResult::PermissionDenied
            : InventoryItemUseResult::InventoryChanged;
    combat.playerHealth=static_cast<float>(healed);
    return InventoryItemUseResult::Used;
}
inline std::string InventoryUseStatus(InventoryItemUseResult result) {
    switch (result) {
    case InventoryItemUseResult::Used: return "HEALTH RESTORED - USED ONE POTION";
    case InventoryItemUseResult::AtFullHealth: return "HEALTH FULL - POTION KEPT";
    case InventoryItemUseResult::PlayerDead: return "CANNOT USE WHILE DEAD";
    case InventoryItemUseResult::NotConsumable: return "THIS ITEM CANNOT BE CONSUMED";
    case InventoryItemUseResult::NotHealing: return "THIS ITEM HAS NO HEALTH EFFECT";
    case InventoryItemUseResult::NoHealthAuthority: return "PLAYER HEALTH UNAVAILABLE";
    case InventoryItemUseResult::PermissionDenied: return "ITEM USE ACCESS DENIED";
    case InventoryItemUseResult::InventoryChanged: return "ITEM NO LONGER AVAILABLE";
    default: return "SELECT A POTION IN YOUR BACKPACK";
    }
}
} // namespace renegade::runtime
