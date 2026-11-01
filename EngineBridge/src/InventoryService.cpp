#include "renegade/bridge/InventoryService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <utility>

namespace renegade::bridge {
namespace {
constexpr std::size_t MaxDefinitions = 10000, MaxContainers = 10000;
constexpr std::size_t MaxStacksPerContainer = 100000;
constexpr std::size_t MaxSnapshotBytes = 16u * 1024u * 1024u;
bool IsSafeWeight(double value) { return std::isfinite(value) && value >= 0.0 && value <= 1e12; }
double TotalWeight(const InventoryContainer& container,
                   const std::map<std::string, InventoryItemDefinition>& definitions) {
    double total = 0.0;
    for (const auto& stack : container.stacks) {
        const auto found = definitions.find(stack.definitionId);
        if (found == definitions.end()) return std::numeric_limits<double>::infinity();
        total += static_cast<double>(stack.quantity) * found->second.weightPerUnit;
    }
    return total;
}
bool ValidId(const std::string& s) {
    if (s.empty() || s.size() > 256) return false;
    for (unsigned char c : s) if (c < 33 || c > 126 || c == '"' || c == '\\') return false;
    return true;
}
} // namespace

bool InventoryService::ChangesSince(std::uint64_t cursor,
                                    std::vector<InventoryChange>& out) const {
    out.clear();
    if (cursor > lastChangeSequence_ ||
        (!changes_.empty() && cursor < changes_.front().sequence - 1))
        return false;
    for (const auto& event : changes_)
        if (event.sequence > cursor) out.push_back(event);
    return true;
}

void InventoryService::RecordChange(InventoryChangeKind kind,
        std::string source, std::string target, std::string definition,
        std::uint32_t quantity) {
    if (lastChangeSequence_ == std::numeric_limits<std::uint64_t>::max()) {
        changes_.clear();
        lastChangeSequence_ = 0; // Epoch exhaustion: consumers must resynchronise.
    }
    changes_.push_back({++lastChangeSequence_, kind, revision_,
                        std::move(source), std::move(target),
                        std::move(definition), quantity});
    if (changes_.size() > MaximumChangeHistory) changes_.pop_front();
}

bool InventoryService::ValidText(const std::string& value) {
    if (value.empty() || value.size() > 256) return false;
    for (unsigned char c : value) if (c == 0 || c == 10 || c == 13 || c < 32) return false;
    return true;
}

const InventoryItemDefinition* InventoryService::FindDefinition(const std::string& id) const {
    const auto it = definitions_.find(id);
    return it == definitions_.end() ? nullptr : &it->second;
}
const InventoryContainer* InventoryService::FindContainer(const std::string& id) const {
    const auto it = containers_.find(id);
    return it == containers_.end() ? nullptr : &it->second;
}

InventoryResult InventoryService::RegisterItem(InventoryItemDefinition definition) {
    if (!ValidId(definition.id) || !ValidText(definition.name) ||
        !IsSafeWeight(definition.weightPerUnit) ||
        definition.maximumStack == 0 ||
        (!definition.equipmentAssetId.empty() && !ValidId(definition.equipmentAssetId)))
        return InventoryResult::InvalidArgument;
    if (definitions_.size() >= MaxDefinitions) return InventoryResult::CapacityExceeded;
    if (definitions_.count(definition.id)) return InventoryResult::DuplicateId;
    const auto id = definition.id;
    definitions_.emplace(id, std::move(definition));
    ++revision_;
    RecordChange(InventoryChangeKind::DefinitionRegistered, {}, {}, id, 0);
    return InventoryResult::Success;
}

InventoryResult InventoryService::UpdateMaximumStack(
        const std::string& definitionId, const std::uint32_t maximumStack) {
    if (!ValidId(definitionId) || maximumStack == 0)
        return InventoryResult::InvalidArgument;
    auto found = definitions_.find(definitionId);
    if (found == definitions_.end()) return InventoryResult::NotFound;
    if (found->second.maximumStack == maximumStack) return InventoryResult::Success;
    // Existing stacks are authoritative. Never shrink them behind the
    // creator's back; the editor can ask the developer to split them first.
    for (const auto& [containerId, container] : containers_)
        for (const auto& stack : container.stacks)
            if (stack.definitionId == definitionId && stack.quantity > maximumStack)
                return InventoryResult::CapacityExceeded;
    found->second.maximumStack = maximumStack;
    ++revision_;
    RecordChange(InventoryChangeKind::DefinitionUpdated, {}, {}, definitionId, 0);
    return InventoryResult::Success;
}

InventoryResult InventoryService::UpdateItemDefinition(InventoryItemDefinition definition) {
    if (!ValidId(definition.id) || !ValidText(definition.name) ||
        !IsSafeWeight(definition.weightPerUnit) || definition.maximumStack == 0 ||
        (!definition.equipmentAssetId.empty() && !ValidId(definition.equipmentAssetId)))
        return InventoryResult::InvalidArgument;
    auto it = definitions_.find(definition.id);
    if (it == definitions_.end()) return InventoryResult::NotFound;
    const auto prior = it->second;
    if (prior.name == definition.name && prior.maximumStack == definition.maximumStack &&
        prior.weightPerUnit == definition.weightPerUnit &&
        prior.equipmentAssetId == definition.equipmentAssetId)
        return InventoryResult::Success;
    it->second = std::move(definition);
    if (!Validate(definitions_, containers_)) {
        it->second = prior;
        return InventoryResult::CapacityExceeded;
    }
    ++revision_;
    RecordChange(InventoryChangeKind::DefinitionUpdated, {}, {}, it->first, 0);
    return InventoryResult::Success;
}

InventoryResult InventoryService::CreateContainer(std::string id,
        std::uint32_t maximumSlots, double maximumWeight) {
    if (!ValidId(id) || !IsSafeWeight(maximumWeight) ||
        maximumSlots > MaxStacksPerContainer) return InventoryResult::InvalidArgument;
    if (containers_.size() >= MaxContainers) return InventoryResult::CapacityExceeded;
    if (containers_.count(id)) return InventoryResult::DuplicateId;
    InventoryContainer c;
    c.id = std::move(id);
    c.maximumSlots = maximumSlots;
    c.maximumWeight = maximumWeight;
    const auto containerId = c.id;
    containers_.emplace(containerId, std::move(c));
    ++revision_;
    RecordChange(InventoryChangeKind::ContainerCreated, {}, containerId, {}, 0);
    return InventoryResult::Success;
}

InventoryResult InventoryService::Insert(InventoryContainer& c, const Definitions& definitions,
        const InventoryItemDefinition& def, std::uint32_t count,
        std::uint64_t& nextInstance, const std::string& preservedInstance) {
    if (!count) return InventoryResult::InvalidArgument;
    const double existingWeight = TotalWeight(c, definitions);
    const double incomingWeight = static_cast<double>(count) * def.weightPerUnit;
    if (!std::isfinite(existingWeight + incomingWeight) ||
        (c.maximumWeight > 0.0 && existingWeight + incomingWeight > c.maximumWeight + 1e-8))
        return InventoryResult::CapacityExceeded;
    std::uint64_t remainder = count;
    // Fill existing compatible stacks before creating new ones.
    for (auto& stack : c.stacks) {
        if (stack.definitionId != def.id || stack.quantity >= def.maximumStack) continue;
        const auto free = static_cast<std::uint64_t>(def.maximumStack) - stack.quantity;
        const auto add = std::min(remainder, free);
        stack.quantity += static_cast<std::uint32_t>(add);
        remainder -= add;
        if (!remainder) break;
    }
    bool preserved = false;
    while (remainder) {
        if (c.stacks.size() >= MaxStacksPerContainer ||
            (c.maximumSlots && c.stacks.size() >= c.maximumSlots) ||
            nextInstance >= std::numeric_limits<std::uint64_t>::max() - 1)
            return InventoryResult::CapacityExceeded;
        const auto part = static_cast<std::uint32_t>(
            std::min(remainder, static_cast<std::uint64_t>(def.maximumStack)));
        auto id = !preserved && !preservedInstance.empty()
            ? preservedInstance : "stack-" + std::to_string(nextInstance++);
        preserved = true;
        c.stacks.push_back({std::move(id), def.id, part});
        remainder -= part;
    }
    return InventoryResult::Success;
}

InventoryResult InventoryService::Add(const std::string& containerId,
        const std::string& definitionId, std::uint32_t quantity) {
    if (!quantity) return InventoryResult::InvalidArgument;
    const auto c = containers_.find(containerId);
    const auto def = definitions_.find(definitionId);
    if (c == containers_.end() || def == definitions_.end()) return InventoryResult::NotFound;
    auto draft = c->second;
    auto counter = nextInstance_;
    const auto result = Insert(draft, definitions_, def->second, quantity, counter);
    if (result != InventoryResult::Success) return result;
    if (!Validate(definitions_, Containers{{containerId, draft}}))
        return InventoryResult::InvalidArgument;
    c->second = std::move(draft);
    nextInstance_ = counter;
    ++revision_;
    RecordChange(InventoryChangeKind::Added, {}, containerId, definitionId, quantity);
    return InventoryResult::Success;
}

InventoryResult InventoryService::Remove(const std::string& containerId,
        const std::string& instanceId, std::uint32_t quantity) {
    if (!quantity) return InventoryResult::InvalidArgument;
    const auto c = containers_.find(containerId);
    if (c == containers_.end()) return InventoryResult::NotFound;
    auto& stacks = c->second.stacks;
    const auto it = std::find_if(stacks.begin(), stacks.end(),
        [&](const InventoryItemStack& s) { return s.instanceId == instanceId; });
    if (it == stacks.end()) return InventoryResult::NotFound;
    if (quantity > it->quantity) return InventoryResult::InsufficientQuantity;
    const auto definitionId = it->definitionId;
    if (quantity == it->quantity) stacks.erase(it);
    else it->quantity -= quantity;
    ++revision_;
    RecordChange(InventoryChangeKind::Removed, containerId, {}, definitionId, quantity);
    return InventoryResult::Success;
}

InventoryResult InventoryService::Transfer(const std::string& sourceId,
        const std::string& targetId, const std::string& instanceId,
        std::uint32_t quantity) {
    if (!quantity || sourceId == targetId) return InventoryResult::InvalidArgument;
    const auto src = containers_.find(sourceId), dst = containers_.find(targetId);
    if (src == containers_.end() || dst == containers_.end()) return InventoryResult::NotFound;
    auto source = src->second, target = dst->second;
    const auto it = std::find_if(source.stacks.begin(), source.stacks.end(),
        [&](const InventoryItemStack& s) { return s.instanceId == instanceId; });
    if (it == source.stacks.end()) return InventoryResult::NotFound;
    if (quantity > it->quantity) return InventoryResult::InsufficientQuantity;
    const auto def = definitions_.find(it->definitionId);
    if (def == definitions_.end()) return InventoryResult::InvalidSnapshot;
    const bool whole = quantity == it->quantity;
    if (whole) source.stacks.erase(it);
    else it->quantity -= quantity;
    auto counter = nextInstance_;
    const auto result = Insert(target, definitions_, def->second,
                                quantity, counter, whole ? instanceId : std::string{});
    if (result != InventoryResult::Success) return result;
    if (!Validate(definitions_, Containers{{sourceId, source}, {targetId, target}}))
        return InventoryResult::InvalidSnapshot;
    src->second = std::move(source);
    dst->second = std::move(target);
    nextInstance_ = counter;
    ++revision_;
    RecordChange(InventoryChangeKind::Transferred, sourceId, targetId, def->second.id, quantity);
    return InventoryResult::Success;
}

bool InventoryService::Validate(const Definitions& defs, const Containers& containers) {
    if (defs.size() > MaxDefinitions || containers.size() > MaxContainers) return false;
    std::set<std::string> identities;
    for (const auto& entry : defs) {
        const auto& d = entry.second;
        if (entry.first != d.id || !ValidId(d.id) || !ValidText(d.name) ||
            !IsSafeWeight(d.weightPerUnit) || d.maximumStack == 0 ||
            (!d.equipmentAssetId.empty() && !ValidId(d.equipmentAssetId))) return false;
    }
    for (const auto& entry : containers) {
        const auto& c = entry.second;
        if (entry.first != c.id || !ValidId(c.id) ||
            !IsSafeWeight(c.maximumWeight) || c.maximumSlots > MaxStacksPerContainer ||
            c.stacks.size() > MaxStacksPerContainer ||
            (c.maximumSlots && c.stacks.size() > c.maximumSlots)) return false;
        for (const auto& s : c.stacks) {
            const auto def = defs.find(s.definitionId);
            if (!ValidId(s.instanceId) || !identities.insert(s.instanceId).second ||
                def == defs.end() || s.quantity == 0 || s.quantity > def->second.maximumStack)
                return false;
        }
        const double weight = TotalWeight(c, defs);
        if (!std::isfinite(weight) ||
            (c.maximumWeight && weight > c.maximumWeight + 1e-8)) return false;
    }
    return true;
}

std::string InventoryService::SerializeSnapshot() const {
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::setprecision(17);
    s << "RENEGADE_INVENTORY 1 " << nextInstance_ << " " << revision_ << "\n";
    s << definitions_.size() << "\n";
    for (const auto& e : definitions_) {
        const auto& d = e.second;
        s << std::quoted(d.id) << " " << std::quoted(d.name) << " "
          << d.maximumStack << " " << d.weightPerUnit << " "
          << std::quoted(d.equipmentAssetId) << "\n";
    }
    s << containers_.size() << "\n";
    for (const auto& e : containers_) {
        const auto& c = e.second;
        s << std::quoted(c.id) << " " << c.maximumSlots << " "
          << c.maximumWeight << " " << c.stacks.size() << "\n";
        for (const auto& item : c.stacks)
            s << std::quoted(item.instanceId) << " " << std::quoted(item.definitionId)
              << " " << item.quantity << "\n";
    }
    return s.str();
}

InventoryResult InventoryService::RestoreSnapshot(const std::string& bytes) {
    if (bytes.empty() || bytes.size() > MaxSnapshotBytes)
        return InventoryResult::InvalidSnapshot;
    std::istringstream s(bytes);
    s.imbue(std::locale::classic());
    std::string magic;
    unsigned version = 0;
    std::uint64_t counter = 0, revision = 0;
    if (!(s >> magic >> version >> counter >> revision) ||
        magic != "RENEGADE_INVENTORY" || version != 1 ||
        counter == 0 || counter >= std::numeric_limits<std::uint64_t>::max() - 1)
        return InventoryResult::InvalidSnapshot;
    Definitions defs;
    Containers containers;
    std::size_t count = 0;
    if (!(s >> count) || count > MaxDefinitions) return InventoryResult::InvalidSnapshot;
    for (std::size_t i = 0; i < count; ++i) {
        InventoryItemDefinition x;
        if (!(s >> std::quoted(x.id) >> std::quoted(x.name) >>
              x.maximumStack >> x.weightPerUnit >> std::quoted(x.equipmentAssetId)) ||
            !defs.emplace(x.id, x).second) return InventoryResult::InvalidSnapshot;
    }
    if (!(s >> count) || count > MaxContainers) return InventoryResult::InvalidSnapshot;
    for (std::size_t i = 0; i < count; ++i) {
        InventoryContainer c;
        std::size_t stackCount = 0;
        if (!(s >> std::quoted(c.id) >> c.maximumSlots >> c.maximumWeight >> stackCount) ||
            stackCount > MaxStacksPerContainer) return InventoryResult::InvalidSnapshot;
        for (std::size_t j = 0; j < stackCount; ++j) {
            InventoryItemStack x;
            if (!(s >> std::quoted(x.instanceId) >> std::quoted(x.definitionId) >> x.quantity))
                return InventoryResult::InvalidSnapshot;
            // Ensure generated IDs cannot collide after reloading.
            if (x.instanceId.rfind("stack-", 0) == 0) {
                const auto suffix = x.instanceId.substr(6);
                if (suffix.empty() || suffix.size() > 20 ||
                    !std::all_of(suffix.begin(), suffix.end(),
                                 [](char ch){return ch >= '0' && ch <= '9';}))
                    return InventoryResult::InvalidSnapshot;
                try {
                    if (std::stoull(suffix) >= counter) return InventoryResult::InvalidSnapshot;
                } catch (...) { return InventoryResult::InvalidSnapshot; }
            }
            c.stacks.push_back(std::move(x));
        }
        if (!containers.emplace(c.id, std::move(c)).second)
            return InventoryResult::InvalidSnapshot;
    }
    s >> std::ws;
    if (!s.eof() || !Validate(defs, containers)) return InventoryResult::InvalidSnapshot;
    definitions_.swap(defs);
    containers_.swap(containers);
    nextInstance_ = counter;
    revision_ = revision;
    RecordChange(InventoryChangeKind::Restored, {}, {}, {}, 0);
    return InventoryResult::Success;
}

InventoryResult InventoryService::SaveSnapshot(const std::string& path) const {
    if (path.empty() || path.size() > 2048) return InventoryResult::InvalidArgument;
    namespace fs = std::filesystem;
    const fs::path dest(path), temp(path + ".tmp"), backup(path + ".bak");
    std::error_code ec;
    // Never erase an existing save until its complete replacement is on disk.
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        const auto payload = SerializeSnapshot();
        if (!out || payload.size() > MaxSnapshotBytes) return InventoryResult::InvalidSnapshot;
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        out.flush();
        if (!out.good()) return InventoryResult::InvalidSnapshot;
    }
    const bool previous = fs::exists(dest, ec);
    if (ec) return InventoryResult::InvalidSnapshot;
    if (previous) {
        fs::remove(backup, ec);
        if (ec) return InventoryResult::InvalidSnapshot;
        fs::rename(dest, backup, ec);
        if (ec) return InventoryResult::InvalidSnapshot;
    }
    fs::rename(temp, dest, ec);
    if (ec) {
        if (previous) { std::error_code rollback; fs::rename(backup, dest, rollback); }
        return InventoryResult::InvalidSnapshot;
    }
    return InventoryResult::Success;
}

InventoryResult InventoryService::LoadSnapshot(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return InventoryResult::NotFound;
    std::string bytes;
    char buffer[4096];
    while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
        bytes.append(buffer, static_cast<std::size_t>(in.gcount()));
        if (bytes.size() > MaxSnapshotBytes) return InventoryResult::InvalidSnapshot;
    }
    if (!in.eof()) return InventoryResult::InvalidSnapshot;
    return RestoreSnapshot(bytes);
}
} // namespace renegade::bridge
