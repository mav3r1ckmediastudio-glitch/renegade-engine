#include "RuntimeInventorySession.h"
#include "renegade/bridge/InventoryAccessPolicy.h"

#include <filesystem>
#include <memory>
#include <utility>

namespace renegade::runtime {
namespace {
namespace fs = std::filesystem;
bool Contained(const fs::path& canonicalRoot, const fs::path& candidate,
               fs::path& resolved, std::string& error) {
    std::error_code ec;
    resolved = fs::weakly_canonical(candidate, ec);
    if (ec) { error = "Could not resolve inventory companion path."; return false; }
    const auto relative = fs::relative(resolved, canonicalRoot, ec);
    if (ec || relative.empty() || relative.is_absolute()) {
        error = "Inventory companion is outside the project root."; return false;
    }
    for (const auto& part : relative)
        if (part == "..") {
            error = "Inventory companion escapes the project root."; return false;
        }
    return true;
}

} // namespace

void RuntimeInventorySession::Reset() noexcept {
    // Caller must stop creator scripts before resetting live Lua gateways.
    scriptAccess_.clear();
    inventory_.reset();
}

bool RuntimeInventorySession::CaptureRuntimeState(
    std::string& out, std::string& error) const {
    out.clear();
    if (!inventory_) {
        error = "No active inventory to include in a game save.";
        return false;
    }
    const auto payload = inventory_->SerializeSnapshot();
    if (payload.empty() || payload.size() > 16u * 1024u * 1024u) {
        error = "Inventory gameplay state exceeds its save budget.";
        return false;
    }
    out = payload;
    error.clear();
    return true;
}

bool RuntimeInventorySession::RestoreRuntimeState(
    const std::string& bytes, std::string& error) {
    if (!inventory_) {
        error = "No active inventory to restore.";
        return false;
    }
    if (bytes.empty() || bytes.size() > 16u * 1024u * 1024u ||
        inventory_->RestoreSnapshot(bytes) != bridge::InventoryResult::Success) {
        error = "Inventory game-save data is corrupt or incompatible.";
        return false;
    }
    // In-place restore keeps existing per-script capability gateway pointers
    // valid; a Restored journal event asks observers to reread their views.
    error.clear();
    return true;
}

bridge::InventoryGameplayGateway* RuntimeInventorySession::GatewayForScript(
    const std::string& scriptInstanceId, const std::string& ownerEntityId) noexcept {
    const auto it = scriptAccess_.find(scriptInstanceId);
    if (it == scriptAccess_.end() || it->second.ownerEntityId != ownerEntityId)
        return nullptr;
    return it->second.gateway.get();
}

bool RuntimeInventorySession::OpenForScene(
    const std::string& projectRoot, const std::string& scenePath,
    std::string& error) {
    Reset();
    if (projectRoot.empty() || scenePath.empty()) {
        error = "Inventory scene/project path is empty.";
        return false;
    }
    std::error_code ec;
    const fs::path root = fs::weakly_canonical(fs::u8path(projectRoot), ec);
    if (ec || !fs::is_directory(root)) {
        error = "Inventory project root is invalid."; return false;
    }
    fs::path scene;
    if (!Contained(root, fs::u8path(scenePath), scene, error)) return false;
    if (!fs::is_regular_file(scene, ec) || ec) {
        error = "Inventory scene file is missing."; return false;
    }
    fs::path snapshot, access;
    if (!Contained(root, fs::u8path(scene.generic_u8string() + ".rinventory"),
                   snapshot, error) ||
        !Contained(root, fs::u8path(scene.generic_u8string() + ".rinventory-access"),
                   access, error))
        return false;

    const bool snapshotExists = fs::exists(snapshot, ec);
    if (ec) { error = "Inventory snapshot cannot be inspected."; return false; }
    const bool accessExists = fs::exists(access, ec);
    if (ec) { error = "Inventory access policy cannot be inspected."; return false; }
    if (!snapshotExists && !accessExists) {
        error.clear(); return true; // Explicitly disabled, zero allocations.
    }
    if (!snapshotExists || !fs::is_regular_file(snapshot, ec) || ec ||
        (accessExists && (!fs::is_regular_file(access, ec) || ec))) {
        error = "Inventory companion requires a regular inventory snapshot.";
        return false;
    }

    auto incoming = std::make_unique<bridge::InventoryService>();
    if (incoming->LoadSnapshot(snapshot.generic_u8string()) !=
        bridge::InventoryResult::Success) {
        error = "Inventory snapshot is malformed or unsupported.";
        return false;
    }

    std::map<std::string, Entry> grants;
    if (accessExists) {
        std::vector<bridge::InventoryAccessGrant> accepted;
        if (!bridge::ReadInventoryAccessPolicy(
                access.generic_u8string(), *incoming, accepted, error))
            return false;

        for (const auto& grant : accepted) {
            auto it = grants.find(grant.scriptInstanceId);
            if (it == grants.end()) {
                Entry entry;
                entry.ownerEntityId = grant.ownerEntityId;
                entry.gateway =
                    std::make_unique<bridge::InventoryGameplayGateway>(*incoming);
                it = grants.emplace(grant.scriptInstanceId, std::move(entry)).first;
            }
            if (grant.kind == bridge::InventoryGrantKind::Container)
                it->second.gateway->Grant(grant.argument, grant.permissions);
            else if (grant.kind == bridge::InventoryGrantKind::CreateContainer)
                it->second.gateway->AllowContainerCreation(grant.argument);
            else if (grant.kind == bridge::InventoryGrantKind::DefineItems)
                it->second.gateway->AllowItemDefinitions(true);
        }
    }
    inventory_ = std::move(incoming);
    scriptAccess_ = std::move(grants);
    error.clear();
    return true;
}

} // namespace renegade::runtime
