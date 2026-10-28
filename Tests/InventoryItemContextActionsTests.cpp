#include "renegade/bridge/InventoryItemContextActions.h"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"G5H context actions: "<<#x<<" line "<<__LINE__<<"\n";return 1;}}while(false)
using namespace renegade::bridge;
int main() {
    InventoryService inventory;
    CHECK(inventory.RegisterItem({"potion","Potion",10,0.1,{}})==InventoryResult::Success);
    CHECK(inventory.RegisterItem({"quest","Key",1,0,{}})==InventoryResult::Success);
    CHECK(inventory.RegisterItem({"wood","Wood",20,0,{}})==InventoryResult::Success);
    CHECK(inventory.CreateContainer("player",5)==InventoryResult::Success);
    CHECK(inventory.CreateContainer("chest",5)==InventoryResult::Success);
    CHECK(inventory.Add("player","potion",3)==InventoryResult::Success);
    CHECK(inventory.Add("player","quest",1)==InventoryResult::Success);
    CHECK(inventory.Add("chest","wood",4)==InventoryResult::Success);
    InventoryItemCatalogue catalogue;
    InventoryAuthoringItem potion;
    potion.id="potion";potion.name="Potion";potion.kind=InventoryItemKind::Consumable;
    potion.effect=InventoryItemEffect::RestoreHealth;potion.healthRestored=50;
    catalogue.items.push_back(potion);
    InventoryAuthoringItem key;
    key.id="quest";key.name="Quest Key";key.kind=InventoryItemKind::KeyItem;
    catalogue.items.push_back(key);
    InventoryAuthoringItem wood;
    wood.id="wood";wood.name="Wood";wood.kind=InventoryItemKind::Material;
    catalogue.items.push_back(wood);
    const auto potionId=inventory.FindContainer("player")->stacks[0].instanceId;
    const auto keyId=inventory.FindContainer("player")->stacks[1].instanceId;
    const auto woodId=inventory.FindContainer("chest")->stacks[0].instanceId;
    const auto player=DescribeInventoryItemActions(inventory,catalogue,"player",potionId,false);
    CHECK(player.options.size()==3 && player.options[0].enabled);
    CHECK(!player.options[1].enabled && !player.options[2].enabled);
    const auto ready=DescribeInventoryItemActions(inventory,catalogue,"player",potionId,true);
    CHECK(ready.options[1].enabled && ready.options[2].enabled && ready.quantity==3);
    const auto protectedItem=DescribeInventoryItemActions(inventory,catalogue,"player",keyId,true);
    CHECK(protectedItem.options.size()==3 && !protectedItem.options[0].enabled);
    CHECK(!protectedItem.options[1].enabled && !protectedItem.options[2].enabled);
    const auto chest=DescribeInventoryItemActions(inventory,catalogue,"chest",woodId,true);
    CHECK(chest.options.size()==2 && chest.options[0].enabled && chest.options[1].enabled);
    CHECK(DescribeInventoryItemActions(inventory,catalogue,"player",woodId,true).options.empty());
    CHECK(DescribeInventoryItemActions(inventory,catalogue,"other",potionId,true).options.empty());
    CHECK(DescribeInventoryItemActions(inventory,catalogue,"player","invalid",true).options.empty());
    CHECK(inventory.FindContainer("player")->stacks[0].quantity==3);
    CHECK(inventory.FindContainer("chest")->stacks[0].quantity==4);
    std::cout<<"PASS: G5H backpack/chest item action offers are scoped, nonmutating, drop-gated and respect quest items\n";
    return 0;
}
