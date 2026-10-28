#pragma once
#include <string>
namespace renegade::bridge {
// Opt-in creator-test-only projection: copy validated source Scene inventory
// companions into the disposable Test Level scene. Does not mutate source
// authoring bytes, invent a save game, or enable inventory for other projects.
bool SnapshotInventoryCompanions(const std::string& projectRoot,
                                 const std::string& sourceScenePath,
                                 const std::string& targetScenePath,
                                 std::string& error);
} // namespace renegade::bridge
