#include "RuntimeInventoryItemUse.h"
#include <iostream>

#define CHECK(x) do { if(!(x)) { std::cerr<<"G5C use test line "<<__LINE__\
<<": "<<#x<<std::endl; return 1; } } while(false)
using namespace renegade;
int main() {
    bridge::InventoryService inventory;
    CHECK(inventory.RegisterItem({"potion","Health Potion",10,.2,{}})
        ==bridge::InventoryResult::Success);
    CHECK(inventory.RegisterItem({"sword","Sword",1,1.0,{}})
        ==bridge::InventoryResult::Success);
    CHECK(inventory.CreateContainer("player",3)==bridge::InventoryResult::Success);
    CHECK(inventory.CreateContainer("chest",3)==bridge::InventoryResult::Success);
    CHECK(inventory.Add("player","potion",3)==bridge::InventoryResult::Success);
    CHECK(inventory.Add("player","sword",1)==bridge::InventoryResult::Success);
    CHECK(inventory.Add("chest","potion",2)==bridge::InventoryResult::Success);
    bridge::InventoryItemCatalogue catalogue;
    bridge::InventoryAuthoringItem potion;
    potion.id="potion";potion.name="Health Potion";
    potion.kind=bridge::InventoryItemKind::Consumable;
    potion.effect=bridge::InventoryItemEffect::RestoreHealth;
    potion.healthRestored=50;potion.maximumStack=10;
    catalogue.items.push_back(potion);
    const auto stack=inventory.FindContainer("player")->stacks[0].instanceId;
    const auto sword=inventory.FindContainer("player")->stacks[1].instanceId;
    const auto chest=inventory.FindContainer("chest")->stacks[0].instanceId;
    bridge::InventoryGameplayGateway granted(inventory),denied(inventory);
    granted.Grant("player",{true,true,true});
    denied.Grant("player",{true,true,false});
    runtime::RuntimeCombatState health;
    health.playerHealth=70;health.playerMaxHealth=100;
    const auto before=inventory.Revision();
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,stack,health)
        ==runtime::InventoryItemUseResult::Used);
    CHECK(health.playerHealth==100);
    CHECK(inventory.FindContainer("player")->stacks[0].quantity==2);
    CHECK(inventory.Revision()==before+1);
    const auto fullRevision=inventory.Revision();
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,stack,health)
        ==runtime::InventoryItemUseResult::AtFullHealth);
    CHECK(inventory.Revision()==fullRevision);
    CHECK(inventory.FindContainer("player")->stacks[0].quantity==2);
    health.playerHealth=10;
    CHECK(runtime::UseHealthPotion(inventory,denied,catalogue,stack,health)
        ==runtime::InventoryItemUseResult::PermissionDenied);
    CHECK(health.playerHealth==10 && inventory.Revision()==fullRevision);
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,chest,health)
        ==runtime::InventoryItemUseResult::MissingItem);
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,sword,health)
        ==runtime::InventoryItemUseResult::NotConsumable);
    CHECK(health.playerHealth==10 && inventory.Revision()==fullRevision);
    health.playerDead=true;
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,stack,health)
        ==runtime::InventoryItemUseResult::PlayerDead);
    health.playerDead=false;health.playerHealth=0;
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,stack,health)
        ==runtime::InventoryItemUseResult::PlayerDead);
    health.playerHealth=1;health.playerMaxHealth=100;
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,stack,health)
        ==runtime::InventoryItemUseResult::Used);
    CHECK(health.playerHealth==51);
    CHECK(inventory.FindContainer("player")->stacks[0].quantity==1);
    potion.healthRestored=1000000;
    catalogue.items[0]=potion;
    health.playerHealth=2;
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,stack,health)
        ==runtime::InventoryItemUseResult::Used);
    CHECK(health.playerHealth==100); // clamped, no overflow
    CHECK(inventory.FindContainer("player")->stacks[0].definitionId=="sword");
    const auto finalRevision=inventory.Revision();
    CHECK(runtime::UseHealthPotion(inventory,granted,catalogue,stack,health)
        ==runtime::InventoryItemUseResult::MissingItem);
    CHECK(inventory.Revision()==finalRevision);
    // G5E: apply genuine native combat damage, then consume a potion through
    // the same scoped gameplay gateway used by the Runtime Inventory Screen.
    // This is NOT a test-only assignment to playerHealth.
    catalogue.items[0].healthRestored=50;
    bridge::InventoryService damagedInventory;
    CHECK(damagedInventory.RegisterItem({"potion","Health Potion",10,.2,{}})
        ==bridge::InventoryResult::Success);
    CHECK(damagedInventory.CreateContainer("player",3)==bridge::InventoryResult::Success);
    CHECK(damagedInventory.Add("player","potion",2)==bridge::InventoryResult::Success);
    bridge::InventoryGameplayGateway damagedGateway(damagedInventory);
    damagedGateway.Grant("player",{true,true,true});
    runtime::RuntimeCombatState damagedCombat;
    wi::scene::Scene scene;
    bool killed=false;
    CHECK(runtime::ApplyCombatDamage(scene,damagedCombat,
        runtime::RuntimePlayerKnowledgeId,73.0f,killed));
    CHECK(!killed && damagedCombat.damageEvents==1 &&
          damagedCombat.playerHealth==27.0f);
    const auto damagedStack=damagedInventory.FindContainer("player")->stacks[0].instanceId;
    CHECK(runtime::UseHealthPotion(damagedInventory,damagedGateway,
        catalogue,damagedStack,damagedCombat)==runtime::InventoryItemUseResult::Used);
    CHECK(damagedCombat.playerHealth==77.0f);
    CHECK(damagedInventory.FindContainer("player")->stacks[0].quantity==1);
    CHECK(runtime::UseHealthPotion(damagedInventory,damagedGateway,
        catalogue,damagedStack,damagedCombat)==runtime::InventoryItemUseResult::Used);
    CHECK(damagedCombat.playerHealth==100.0f);
    CHECK(damagedInventory.FindContainer("player")->stacks.empty());
    std::cout<<"PASS: native player-health potion use after actual combat damage, permission, rollback and cap"<<std::endl;
    return 0;
}
