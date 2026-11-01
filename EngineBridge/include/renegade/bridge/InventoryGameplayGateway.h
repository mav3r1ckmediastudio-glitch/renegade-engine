#pragma once

#include "renegade/bridge/InventoryService.h"

#include <map>
#include <string>
#include <utility>

namespace renegade::bridge {

// Scoped access is assigned by trusted Runtime code when binding creator Lua
// or UI to a live gameplay context. No raw InventoryService pointer is exposed
// to untrusted creator scripts. C++ engine plugins can use the base service.
struct InventoryAccess {
    bool read = false;
    bool deposit = false;
    bool withdraw = false;
};

// A single authoritative service may have many context-scoped gateways.
// The gateway owns no items and never caches mutable inventory state.
class InventoryGameplayGateway final {
public:
    explicit InventoryGameplayGateway(InventoryService& service) noexcept
        : service_(service) {}

    void Grant(const std::string& containerId, InventoryAccess access) {
        if (containerId.empty()) return;
        permissions_[containerId] = access;
    }
    void Revoke(const std::string& containerId) { permissions_.erase(containerId); }
    void ClearGrants() noexcept { permissions_.clear(); }

    // Trusted host may explicitly grant definition and scoped container creation.
    // Untrusted Lua cannot grant these permissions to itself.
    void AllowItemDefinitions(bool enabled) noexcept { canDefineItems_ = enabled; }
    void AllowContainerCreation(std::string requiredPrefix) {
        creationPrefix_ = std::move(requiredPrefix);
    }
    InventoryResult RegisterItem(InventoryItemDefinition definition) {
        if (!canDefineItems_) return InventoryResult::PermissionDenied;
        return service_.RegisterItem(std::move(definition));
    }
    InventoryResult CreateContainer(const std::string& id,
                                    std::uint32_t maxSlots = 0,
                                    double maxWeight = 0.0) {
        if (creationPrefix_.empty() || id.rfind(creationPrefix_, 0) != 0 ||
            id.size() <= creationPrefix_.size())
            return InventoryResult::PermissionDenied;
        const auto result = service_.CreateContainer(id, maxSlots, maxWeight);
        if (result == InventoryResult::Success) Grant(id, {true, true, true});
        return result;
    }

    InventoryResult ReadContainer(const std::string& containerId,
                                  InventoryContainer& result) const {
        if (!Allowed(containerId, &InventoryAccess::read))
            return InventoryResult::PermissionDenied;
        const auto* found = service_.FindContainer(containerId);
        if (!found) return InventoryResult::NotFound;
        result = *found; // snapshot copy; does not outlive authoritative references
        return InventoryResult::Success;
    }

    InventoryResult Add(const std::string& containerId,
                        const std::string& definitionId,
                        std::uint32_t quantity) {
        if (!Allowed(containerId, &InventoryAccess::deposit))
            return InventoryResult::PermissionDenied;
        return service_.Add(containerId, definitionId, quantity);
    }

    InventoryResult Remove(const std::string& containerId,
                           const std::string& instanceId,
                           std::uint32_t quantity) {
        if (!Allowed(containerId, &InventoryAccess::withdraw))
            return InventoryResult::PermissionDenied;
        return service_.Remove(containerId, instanceId, quantity);
    }

    InventoryResult Transfer(const std::string& sourceId,
                             const std::string& targetId,
                             const std::string& instanceId,
                             std::uint32_t quantity) {
        if (!Allowed(sourceId, &InventoryAccess::withdraw) ||
            !Allowed(targetId, &InventoryAccess::deposit))
            return InventoryResult::PermissionDenied;
        return service_.Transfer(sourceId, targetId, instanceId, quantity);
    }

    std::uint64_t Revision() const noexcept { return service_.Revision(); }
    std::uint64_t LatestChangeSequence() const noexcept {
        return service_.LatestChangeSequence();
    }

    // Event journal reveals only changes on explicitly readable containers.
    // If an underlying change affected another container, redact that ID;
    // private-only events are omitted and the sequence may contain gaps.
    // Callers must always be prepared to refresh from the authoritative view.
    bool ChangesSince(std::uint64_t cursor, std::vector<InventoryChange>& result) const {
        std::vector<InventoryChange> all;
        if (!service_.ChangesSince(cursor, all)) {
            result.clear();
            return false;
        }
        result.clear();
        for (auto event : all) {
            const bool seeSource = Allowed(event.sourceContainerId, &InventoryAccess::read);
            const bool seeTarget = Allowed(event.targetContainerId, &InventoryAccess::read);
            if (event.kind == InventoryChangeKind::Restored) {
                result.push_back(std::move(event));
                continue;
            }
            if (!seeSource && !seeTarget) continue;
            if (!seeSource) event.sourceContainerId.clear();
            if (!seeTarget) event.targetContainerId.clear();
            result.push_back(std::move(event));
        }
        return true;
    }

private:
    bool Allowed(const std::string& id, bool InventoryAccess::* field) const {
        const auto it = permissions_.find(id);
        return it != permissions_.end() && it->second.*field;
    }

    InventoryService& service_;
    std::map<std::string, InventoryAccess> permissions_;
    bool canDefineItems_ = false;
    std::string creationPrefix_;
};

} // namespace renegade::bridge
