#pragma once
// INV-G1: UI-independent, opt-in gameplay inventory core.
// No Runtime/Studio/Wicked dependencies. A project without inventories does not
// construct this service or execute its code.
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace renegade::bridge {

struct InventoryItemDefinition {
    std::string id;
    std::string name;
    std::uint32_t maximumStack = 1;
    double weightPerUnit = 0.0; // Optional; zero means weightless.
    std::string equipmentAssetId; // Optional reference; no second equipment authority.
};

struct InventoryItemStack {
    std::string instanceId; // Stable across moves/reloads when moved intact.
    std::string definitionId;
    std::uint32_t quantity = 0;
};

struct InventoryContainer {
    std::string id;
    std::uint32_t maximumSlots = 0; // Zero = unlimited.
    double maximumWeight = 0.0;     // Zero = no weight limit.
    std::vector<InventoryItemStack> stacks;
};

enum class InventoryResult {
    Success,
    InvalidArgument,
    NotFound,
    DuplicateId,
    InsufficientQuantity,
    CapacityExceeded,
    PermissionDenied,
    InvalidSnapshot
};

enum class InventoryChangeKind {
    DefinitionRegistered, ContainerCreated, Added, Removed, Transferred, Restored,
    DefinitionUpdated
};

// A bounded, replayable event journal. Consumers supply their own cursor;
// multiple HUDs/Lua observers can watch without stealing one another's events.
struct InventoryChange {
    std::uint64_t sequence = 0;
    InventoryChangeKind kind = InventoryChangeKind::Added;
    std::uint64_t dataRevision = 0;
    std::string sourceContainerId;
    std::string targetContainerId;
    std::string definitionId;
    std::uint32_t quantity = 0;
};

class InventoryService final {
public:
    static constexpr std::size_t MaximumChangeHistory = 1024;
    std::uint64_t LatestChangeSequence() const noexcept { return lastChangeSequence_; }
    // Returns true only if the complete journal since cursor is available.
    // If false, discard incremental changes and reread the authoritative state.
    bool ChangesSince(std::uint64_t cursor, std::vector<InventoryChange>& out) const;
    // Registering definitions and creating containers are explicit, optional operations.
    InventoryResult RegisterItem(InventoryItemDefinition definition);
    // Edit a shared item definition safely. Reject a lower limit while any
    // existing stack exceeds it; never truncate items or mutate on failure.
    InventoryResult UpdateMaximumStack(const std::string& definitionId,
                                       std::uint32_t maximumStack);
    // Update a shared item's name, weight and stack policy together. All
    // existing world/container quantities must still satisfy the new rules.
    InventoryResult UpdateItemDefinition(InventoryItemDefinition definition);
    InventoryResult CreateContainer(std::string id,
                                    std::uint32_t maximumSlots = 0,
                                    double maximumWeight = 0.0);

    // Atomic all-or-nothing gameplay transactions; no partial acceptance.
    InventoryResult Add(const std::string& containerId,
                        const std::string& definitionId, std::uint32_t quantity);
    InventoryResult Remove(const std::string& containerId,
                           const std::string& instanceId, std::uint32_t quantity);
    InventoryResult Transfer(const std::string& sourceId, const std::string& targetId,
                             const std::string& instanceId, std::uint32_t quantity);

    const InventoryItemDefinition* FindDefinition(const std::string& id) const;
    const InventoryContainer* FindContainer(const std::string& id) const;
    std::uint64_t Revision() const noexcept { return revision_; }

    // Versioned deterministic snapshots. Deserialization validates before commit
    // and preserves the current state unchanged on malformed input.
    std::string SerializeSnapshot() const;
    InventoryResult RestoreSnapshot(const std::string& bytes);
    InventoryResult SaveSnapshot(const std::string& path) const;
    InventoryResult LoadSnapshot(const std::string& path);

private:
    using Definitions = std::map<std::string, InventoryItemDefinition>;
    using Containers = std::map<std::string, InventoryContainer>;
    static bool ValidText(const std::string& value);
    static bool Validate(const Definitions& definitions, const Containers& containers);
    static InventoryResult Insert(InventoryContainer& container,
                                  const Definitions& definitions,
                                  const InventoryItemDefinition& definition,
                                  std::uint32_t quantity, std::uint64_t& nextInstance,
                                  const std::string& preservedInstance = {});
    void RecordChange(InventoryChangeKind kind, std::string source,
                      std::string target, std::string definition, std::uint32_t quantity);
    Definitions definitions_;
    Containers containers_;
    std::deque<InventoryChange> changes_;
    std::uint64_t lastChangeSequence_ = 0;
    std::uint64_t nextInstance_ = 1;
    std::uint64_t revision_ = 0;
};

} // namespace renegade::bridge
