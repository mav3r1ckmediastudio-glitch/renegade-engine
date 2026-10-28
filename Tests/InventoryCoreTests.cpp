#include "renegade/bridge/InventoryService.h"
#include <cstdlib>
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "CHECK failed: " << #condition << " at line " << __LINE__ << "\n"; \
    std::abort(); } } while (false)
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace renegade::bridge;
namespace {
void Expect(InventoryResult actual, InventoryResult expected, const char* label) {
    if (actual != expected) {
        std::cerr << label << " failed: got " << static_cast<int>(actual)
                  << ", expected " << static_cast<int>(expected) << "\n";
        std::abort();
    }
}
std::uint32_t Quantity(const InventoryContainer* c, const std::string& definition) {
    std::uint32_t n = 0;
    for (const auto& x : c->stacks) if (x.definitionId == definition) n += x.quantity;
    return n;
}
std::string Instance(const InventoryContainer* c, const std::string& definition) {
    for (const auto& x : c->stacks) if (x.definitionId == definition) return x.instanceId;
    return {};
}
}
int main() {
    // Creator edits may change shared stack limits without losing saved items.
    // Increasing the limit is safe; decreasing below a live stack is rejected
    // without a partial change, and the accepted definition persists.
    {
        InventoryService editable;
        Expect(editable.RegisterItem({"potion", "Potion", 3, 0.2, {}}),
               InventoryResult::Success, "editable stack item");
        Expect(editable.CreateContainer("player", 10), InventoryResult::Success,
               "editable player");
        Expect(editable.Add("player", "potion", 3), InventoryResult::Success,
               "editable stock");
        const auto before = editable.SerializeSnapshot();
        const auto revision = editable.Revision();
        const auto changes = editable.LatestChangeSequence();
        Expect(editable.UpdateMaximumStack("potion", 2),
               InventoryResult::CapacityExceeded, "reject live oversize stack");
        Expect(editable.UpdateMaximumStack("potion", 0),
               InventoryResult::InvalidArgument, "reject zero stack maximum");
        Expect(editable.UpdateMaximumStack("unknown", 5),
               InventoryResult::NotFound, "reject missing definition");
        CHECK(editable.SerializeSnapshot() == before);
        CHECK(editable.Revision() == revision);
        CHECK(editable.LatestChangeSequence() == changes);
        Expect(editable.UpdateMaximumStack("potion", 10),
               InventoryResult::Success, "increase shared stack maximum");
        CHECK(editable.FindDefinition("potion")->maximumStack == 10);
        CHECK(editable.Revision() == revision + 1);
        const auto updated = editable.SerializeSnapshot();
        InventoryService reopened;
        Expect(reopened.RestoreSnapshot(updated), InventoryResult::Success,
               "new maximum survives save payload");
        CHECK(reopened.FindDefinition("potion")->maximumStack == 10);
        CHECK(Quantity(reopened.FindContainer("player"), "potion") == 3);
        Expect(editable.UpdateMaximumStack("potion", 3),
               InventoryResult::Success, "shrink after stock permits");
        CHECK(editable.FindDefinition("potion")->maximumStack == 3);
        CHECK(Quantity(editable.FindContainer("player"), "potion") == 3);
        const auto revisionBeforeEdit=editable.Revision();
        const auto snapshotBeforeEdit=editable.SerializeSnapshot();
        // Shared authoring edits change the real name/weight/stack rules,
        // without destroying any already-owned potion stack.
        Expect(editable.UpdateItemDefinition({"potion","Greater Potion",2,0.2,{}}),
               InventoryResult::CapacityExceeded, "reject definition that invalidates stacks");
        CHECK(editable.SerializeSnapshot()==snapshotBeforeEdit);
        CHECK(editable.Revision()==revisionBeforeEdit);
        Expect(editable.UpdateItemDefinition({"potion","Greater Potion",10,0.3,{}}),
               InventoryResult::Success, "update full item definition");
        CHECK(editable.FindDefinition("potion")->name=="Greater Potion");
        CHECK(editable.FindDefinition("potion")->maximumStack==10);
        CHECK(editable.FindDefinition("potion")->weightPerUnit==0.3);
        CHECK(Quantity(editable.FindContainer("player"),"potion")==3);
        InventoryService newSession;
        Expect(newSession.RestoreSnapshot(editable.SerializeSnapshot()),
               InventoryResult::Success,"restore edited shared definition");
        CHECK(newSession.FindDefinition("potion")->name=="Greater Potion");
    }

    InventoryService inventory;
    CHECK(inventory.FindContainer("player") == nullptr);
    // Nothing allocates a player inventory or HUD implicitly.
    CHECK(inventory.SerializeSnapshot().find("\n0\n0\n") != std::string::npos);

    Expect(inventory.RegisterItem({"potion","Healing potion",10,0.2,{}}),
           InventoryResult::Success,"register potion");
    Expect(inventory.RegisterItem({"apple","Apple",8,0.1,{}}),
           InventoryResult::Success,"register apple");
    Expect(inventory.RegisterItem({"sword","Sword",1,1.0,"equipment.sword"}),
           InventoryResult::Success,"register sword");
    Expect(inventory.RegisterItem({"potion","Duplicate",5,0.0,{}}),
           InventoryResult::DuplicateId,"duplicate definition");
    Expect(inventory.RegisterItem({"bad","Bad",0,0.0,{}}),
           InventoryResult::InvalidArgument,"zero maximum stack");
    Expect(inventory.CreateContainer("player",2,2.0),InventoryResult::Success,"player");
    Expect(inventory.CreateContainer("chest",0,0.0),InventoryResult::Success,"chest");
    Expect(inventory.CreateContainer("armoury"),InventoryResult::Success,"armoury");
    Expect(inventory.CreateContainer("player"),InventoryResult::DuplicateId,"duplicate container");

    Expect(inventory.Add("player","potion",5),InventoryResult::Success,"add potions");
    Expect(inventory.Add("player","sword",1),InventoryResult::Success,"add sword");
    const auto potionId = Instance(inventory.FindContainer("player"),"potion");
    const auto swordId = Instance(inventory.FindContainer("player"),"sword");
    CHECK(!potionId.empty() && !swordId.empty() && swordId != potionId);
    const auto beforeFailure = inventory.SerializeSnapshot();
    const auto revisionBeforeFailure = inventory.Revision();
    Expect(inventory.Add("player","apple",1),InventoryResult::CapacityExceeded,
           "blocked overweight or slot full");
    Expect(inventory.Add("player","potion",1),InventoryResult::CapacityExceeded,
           "blocked by weight despite matching stack");
    Expect(inventory.Transfer("player","chest",potionId,7),
           InventoryResult::InsufficientQuantity,"reject excess transfer");
    Expect(inventory.Transfer("player","player",potionId,1),
           InventoryResult::InvalidArgument,"reject self transfer");
    CHECK(inventory.SerializeSnapshot() == beforeFailure);
    CHECK(inventory.Revision() == revisionBeforeFailure);

    Expect(inventory.Transfer("player","chest",potionId,2),
           InventoryResult::Success,"partial transfer");
    CHECK(Quantity(inventory.FindContainer("player"),"potion") == 3);
    CHECK(Quantity(inventory.FindContainer("chest"),"potion") == 2);
    CHECK(Instance(inventory.FindContainer("player"),"potion") == potionId);
    CHECK(Instance(inventory.FindContainer("chest"),"potion") != potionId);
    Expect(inventory.Add("chest","potion",7),InventoryResult::Success,"stack merge");
    CHECK(Quantity(inventory.FindContainer("chest"),"potion") == 9);
    CHECK(inventory.FindContainer("chest")->stacks.size() == 1);
    Expect(inventory.Transfer("player","armoury",swordId,1),
           InventoryResult::Success,"whole unique transfer");
    CHECK(Instance(inventory.FindContainer("armoury"),"sword") == swordId);
    CHECK(Quantity(inventory.FindContainer("player"),"sword") == 0);
    Expect(inventory.Remove("chest",Instance(inventory.FindContainer("chest"),"potion"),8),
           InventoryResult::Success,"remove partial");
    CHECK(Quantity(inventory.FindContainer("chest"),"potion") == 1);
    Expect(inventory.Remove("chest",Instance(inventory.FindContainer("chest"),"potion"),1),
           InventoryResult::Success,"remove last");
    CHECK(inventory.FindContainer("chest")->stacks.empty());

    // Fill the second player slot. Transferring another full apple stack
    // then requires a third slot and must fail atomically.
    Expect(inventory.Add("player","apple",1),InventoryResult::Success,"fill player slots");
    // Failed transfers must never partially reduce the source.
    Expect(inventory.Add("chest","apple",8),InventoryResult::Success,"add apples");
    const auto snapshotBeforeTransfer = inventory.SerializeSnapshot();
    Expect(inventory.Transfer("chest","player",
               Instance(inventory.FindContainer("chest"),"apple"),8),
           InventoryResult::CapacityExceeded,"atomic failed transfer");
    CHECK(inventory.SerializeSnapshot() == snapshotBeforeTransfer);

    InventoryService restored;
    Expect(restored.RestoreSnapshot(snapshotBeforeTransfer),
           InventoryResult::Success,"snapshot restore");
    CHECK(restored.SerializeSnapshot() == snapshotBeforeTransfer);
    CHECK(Quantity(restored.FindContainer("player"),"potion") == 3);
    CHECK(Quantity(restored.FindContainer("chest"),"apple") == 8);
    const auto goodSnapshot = restored.SerializeSnapshot();
    Expect(restored.RestoreSnapshot("RENEGADE_INVENTORY 999 1 0\n0\n0\n"),
           InventoryResult::InvalidSnapshot,"future version");
    Expect(restored.RestoreSnapshot(goodSnapshot+"CORRUPTION"),
           InventoryResult::InvalidSnapshot,"trailing corruption");
    CHECK(restored.SerializeSnapshot() == goodSnapshot);

    // Real disk round-trip and safe replacement backup.
    const auto path = std::filesystem::temp_directory_path() /
                      "renegade-inventory-core-g1-acceptance.snapshot";
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string()+".bak", ec);
    Expect(restored.SaveSnapshot(path.string()),InventoryResult::Success,"save");
    InventoryService disk;
    Expect(disk.LoadSnapshot(path.string()),InventoryResult::Success,"load");
    CHECK(disk.SerializeSnapshot() == restored.SerializeSnapshot());
    Expect(disk.CreateContainer("new-container"),InventoryResult::Success,"new after load");
    Expect(disk.Add("new-container","potion",1),InventoryResult::Success,
           "new unique identity after restore");
    Expect(disk.SaveSnapshot(path.string()),InventoryResult::Success,"replace save");
    CHECK(std::filesystem::exists(path.string()+".bak"));
    InventoryService prior;
    Expect(prior.LoadSnapshot(path.string()+".bak"),InventoryResult::Success,"old backup");
    CHECK(prior.SerializeSnapshot() == restored.SerializeSnapshot());
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string()+".bak", ec);
    std::cout << "Inventory Core G1: PASS (atomic transactions, capacity, "
                 "stable instances, versions, file round-trip and backup)\n";
    return 0;
}
