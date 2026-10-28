#pragma once
// G5H-A: bounded context action presentation policy, NOT an action executor.
// Inventory gameplay gateway and a future world-drop authority must revalidate
// the precise live stack and scoped permissions before any state mutation.
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/InventoryService.h"
#include <algorithm>
#include <string>
#include <vector>
namespace renegade::bridge {
enum class InventoryItemContextAction {
    Use, DropOne, DropStack, TakeOne, TakeStack
};
struct InventoryItemContextOption {
    InventoryItemContextAction action=InventoryItemContextAction::Use;
    std::string title;
    bool enabled=false;
    std::string explanation;
};
struct InventoryItemContextMenu {
    std::string sourceContainerId;
    std::string sourceInstanceId;
    std::string displayName;
    std::uint32_t quantity=0;
    std::vector<InventoryItemContextOption> options;
};
// Supported behaviors are intentionally narrow. A future Item Use handler can
// add Equip or scripted effects without creating a second inventory authority.
inline InventoryItemContextMenu DescribeInventoryItemActions(
        const InventoryService& inventory,
        const InventoryItemCatalogue& catalogue,
        const std::string& sourceContainerId,
        const std::string& instanceId,
        bool worldDropReady) {
    InventoryItemContextMenu result;
    if((sourceContainerId!="player" && sourceContainerId!="chest") ||
       instanceId.empty())return result;
    const auto* container=inventory.FindContainer(sourceContainerId);
    if(!container)return result;
    const auto found=std::find_if(container->stacks.begin(),container->stacks.end(),
        [&](const InventoryItemStack& s) {
            return s.instanceId==instanceId && s.quantity>0;
        });
    if(found==container->stacks.end())return result;
    const auto* item=FindInventoryAuthoringItem(catalogue,found->definitionId);
    if(!item || item->id!=found->definitionId)return result;
    result.sourceContainerId=sourceContainerId;
    result.sourceInstanceId=instanceId;
    result.quantity=found->quantity;
    result.displayName=item->name;
    if(sourceContainerId=="chest") {
        result.options.push_back({InventoryItemContextAction::TakeOne,"Take One",true,{}});
        result.options.push_back({InventoryItemContextAction::TakeStack,"Take Stack",
            found->quantity>1,found->quantity>1?"":"Only one item in this stack"});
        return result;
    }
    const bool usable=item->kind==InventoryItemKind::Consumable &&
        item->effect==InventoryItemEffect::RestoreHealth && item->healthRestored>0;
    result.options.push_back({InventoryItemContextAction::Use,"Use",usable,
        usable?"":"No supported Use effect on this item"});
    const bool restricted=item->kind==InventoryItemKind::KeyItem;
    const bool canDrop=!restricted && worldDropReady;
    const std::string reason=restricted?"Key items cannot be dropped":
        worldDropReady?"":"World drop is not available";
    result.options.push_back({InventoryItemContextAction::DropOne,"Drop One",
        canDrop,canDrop?"":reason});
    result.options.push_back({InventoryItemContextAction::DropStack,"Drop Stack",
        canDrop && found->quantity>1,
        !canDrop?reason:(found->quantity>1?"":"Only one item in this stack")});
    return result;
}
} // namespace renegade::bridge
