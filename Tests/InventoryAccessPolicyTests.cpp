#include "renegade/bridge/InventoryAccessPolicy.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace renegade::bridge;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL: " << #x << " line " << __LINE__ << "\n"; std::abort(); } } while(false)

int main() {
    InventoryService core;
    CHECK(core.RegisterItem({"potion", "Potion", 10, 0.1, {}}) == InventoryResult::Success);
    CHECK(core.CreateContainer("player") == InventoryResult::Success);
    CHECK(core.CreateContainer("chest") == InventoryResult::Success);
    const std::string valid =
        "RENEGADE_INVENTORY_ACCESS 1 5\n"
        "CONTAINER \"picker\" \"-\" \"player\" 1 1 1\n"
        "CONTAINER \"picker\" \"-\" \"chest\" 1 1 1\n"
        "DEFINE \"picker\" \"-\"\n"
        "CREATE \"picker\" \"-\" \"script:picker:\"\n"
        "CONTAINER \"viewer\" \"entity-1\" \"chest\" 1 0 0\n";
    std::vector<InventoryAccessGrant> grants;
    std::string error;
    CHECK(ParseInventoryAccessPolicy(valid,core,grants,error));
    CHECK(error.empty() && grants.size()==5);
    CHECK(grants[0].kind==InventoryGrantKind::Container);
    CHECK(grants[0].ownerEntityId.empty());
    CHECK(grants[0].permissions.read && grants[0].permissions.deposit &&
          grants[0].permissions.withdraw);
    CHECK(grants[2].kind==InventoryGrantKind::DefineItems);
    CHECK(grants[3].kind==InventoryGrantKind::CreateContainer);
    CHECK(grants[3].argument=="script:picker:");
    CHECK(grants[4].ownerEntityId=="entity-1");
    const auto reject=[&](const std::string& doc) {
        grants.push_back({}); // failures must clear stale partial output
        CHECK(!ParseInventoryAccessPolicy(doc,core,grants,error));
        CHECK(!error.empty() && grants.empty());
    };
    reject("RENEGADE_INVENTORY_ACCESS 2 0\n");
    reject("RENEGADE_INVENTORY_ACCESS 1 1\n"
           "CONTAINER \"x\" \"-\" \"missing\" 1 1 1\n");
    reject("RENEGADE_INVENTORY_ACCESS 1 1\n"
           "CONTAINER \"x\" \"-\" \"player\" 0 1 0\n");
    reject("RENEGADE_INVENTORY_ACCESS 1 2\n"
           "CONTAINER \"x\" \"-\" \"player\" 1 1 1\n"
           "CONTAINER \"x\" \"-\" \"player\" 1 0 0\n");
    reject("RENEGADE_INVENTORY_ACCESS 1 2\n"
           "DEFINE \"x\" \"-\"\n"
           "DEFINE \"x\" \"-\"\n");
    reject("RENEGADE_INVENTORY_ACCESS 1 2\n"
           "CREATE \"x\" \"-\" \"script:first:\"\n"
           "CREATE \"x\" \"-\" \"script:second:\"\n");
    reject("RENEGADE_INVENTORY_ACCESS 1 2\n"
           "CONTAINER \"x\" \"-\" \"player\" 1 1 1\n"
           "CONTAINER \"x\" \"different-owner\" \"chest\" 1 0 0\n");
    reject("RENEGADE_INVENTORY_ACCESS 1 1\n"
           "CONTAINER \"x\" \"-\" \"player\" 1 1 1\n"
           "EXTRA\n");
    reject("RENEGADE_INVENTORY_ACCESS 1 1\n"
           "CREATE \"x\" \"-\" \"..\" payload\n");
    CHECK(ParseInventoryAccessPolicy(
        "RENEGADE_INVENTORY_ACCESS 1 0\n",core,grants,error));
    CHECK(grants.empty());
    std::cout << "G3D shared inventory access-policy validator PASS\n";
}
