#include "renegade/bridge/InventoryDependencyProvider.h"
#include "renegade/bridge/InventoryAccessPolicy.h"
#include "renegade/bridge/InventoryService.h"
#include "renegade/bridge/InventoryItemCatalogueService.h"
#include "renegade/bridge/InventoryScreenAssignmentService.h"
#include "renegade/bridge/IdentityService.h"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace renegade::bridge {
namespace {
namespace fs = std::filesystem;
constexpr std::uintmax_t MaximumSnapshotBytes = 16u * 1024u * 1024u;
constexpr std::uintmax_t MaximumAccessBytes = 1024u * 1024u;
constexpr std::uintmax_t MaximumCatalogueBytes = 1024u * 1024u;
constexpr std::uintmax_t MaximumIconBytes = 8u * 1024u * 1024u;

bool ContainedFile(const fs::path& root, const fs::path& candidate,
                   fs::path& canonical, std::string& error) {
    std::error_code ec;
    canonical = fs::weakly_canonical(candidate, ec);
    if (ec) { error = "Inventory companion path cannot be resolved."; return false; }
    const auto rel = fs::relative(canonical, root, ec);
    if (ec || rel.empty() || rel.is_absolute()) {
        error = "Inventory companion is outside the project root.";
        return false;
    }
    for (const auto& part : rel)
        if (part == "..") {
            error = "Inventory companion escapes project root.";
            return false;
        }
    return true;
}
bool ValidateRegularFile(const fs::path& path, std::uintmax_t maxBytes,
                         std::string& error) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) {
        error = "Inventory companion is not a regular file.";
        return false;
    }
    const auto size = fs::file_size(path, ec);
    if (ec || size > maxBytes || size == 0) {
        error = "Inventory companion is empty, oversized or unreadable.";
        return false;
    }
    return true;
}
bool Exists(const fs::path& path, bool& present, std::string& error) {
    std::error_code ec;
    present = fs::exists(path, ec);
    if (ec) {
        error = "Inventory companion existence check failed.";
        return false;
    }
    return true;
}
} // namespace

bool InventoryDependencyProvider::Discover(
    const DependencyProviderContext& context,
    const DependencyCandidateSink& emit,
    const DependencyDiagnosticSink&,
    std::string& error) const {
    error.clear();
    if (context.source == nullptr ||
        context.source->dependencyClass != DependencyClass::Scene ||
        context.source->projectRelativePath.empty() ||
        context.projectRoot.empty()) {
        error = "Inventory discovery requires a reachable project Scene node.";
        return false;
    }
    std::error_code ec;
    const auto root = fs::weakly_canonical(fs::u8path(context.projectRoot), ec);
    if (ec || !fs::is_directory(root)) {
        error = "Inventory discovery requires a valid project root.";
        return false;
    }
    const std::string sceneHint = context.source->projectRelativePath;
    // Use paths from the registered Scene node, not invented paths from
    // user-supplied Lua, and enforce the same root containment as Runtime.
    fs::path scene, snapshot, access;
    const auto scenePath = root / fs::u8path(sceneHint);
    if (!ContainedFile(root, scenePath, scene, error) ||
        !ContainedFile(root, fs::u8path(scene.generic_u8string()+".rinventory"),
                       snapshot, error) ||
        !ContainedFile(root, fs::u8path(scene.generic_u8string()+".rinventory-access"),
                       access, error))
        return false;
    bool hasSnapshot=false, hasAccess=false;
    if (!Exists(snapshot, hasSnapshot, error) || !Exists(access, hasAccess, error))
        return false;
    if (!hasSnapshot && !hasAccess)
        return true; // This game has no inventory at all.
    if (!hasSnapshot) {
        error = "Inventory permissions exist without a Scene inventory snapshot.";
        return false;
    }
    if (!ValidateRegularFile(snapshot, MaximumSnapshotBytes, error))
        return false;
    InventoryService data;
    if (data.LoadSnapshot(snapshot.generic_u8string()) != InventoryResult::Success) {
        error = "Invalid versioned Scene inventory snapshot; Build Game stopped.";
        return false;
    }
    if (hasAccess) {
        std::vector<InventoryAccessGrant> grants;
        if (!ValidateRegularFile(access, MaximumAccessBytes, error) ||
            !ReadInventoryAccessPolicy(
                access.generic_u8string(), data, grants, error))
            return false;
    }

    // G5: project-owned item definitions and their custom artwork are part of
    // the same reachable Scene closure. Validate all files BEFORE emitting
    // any candidates so a broken icon cannot create a partial package.
    std::vector<DependencyCandidate> itemAssets;
    fs::path cataloguePath;
    const std::string catalogueRelative="Content/Gameplay/Inventory.renegade-items";
    StableId catalogueProjectId;
    if (!ContainedFile(root, root / fs::u8path(catalogueRelative),
                       cataloguePath, error))
        return false;
    bool hasCatalogue=false;
    if (!Exists(cataloguePath,hasCatalogue,error)) return false;
    if (hasCatalogue) {
        if (!ValidateRegularFile(cataloguePath,MaximumCatalogueBytes,error))
            return false;
        DocumentEnvelope envelope;
        if (!ReadDocumentEnvelope(cataloguePath.generic_u8string(),envelope,error))
            return false;
        catalogueProjectId=envelope.projectId;
        InventoryItemCatalogue catalogue;
        if (!ReadInventoryItemCatalogue(cataloguePath.generic_u8string(),
                                        envelope.projectId,catalogue,error))
            return false;
        // G5 must not lower an existing demo stack limit, violate capacity or
        // ship item definitions the Runtime will reject on startup.
        InventoryService verified;
        if (verified.RestoreSnapshot(data.SerializeSnapshot())!=InventoryResult::Success) {
            error="Build Game cannot validate inventory snapshot for item catalogue.";
            return false;
        }
        std::set<std::string> seenIcons;
        for (const auto& authored:catalogue.items) {
            const auto def=MakeRuntimeInventoryDefinition(authored);
            const auto result=verified.FindDefinition(def.id)
                ? verified.UpdateItemDefinition(def)
                : verified.RegisterItem(def);
            if (result!=InventoryResult::Success) {
                error="Inventory item catalogue conflicts with authored inventory: "+def.name;
                return false;
            }
            if (authored.iconProjectRelativePath.empty() ||
                !seenIcons.insert(authored.iconProjectRelativePath).second) continue;
            fs::path iconPath;
            if (!ContainedFile(root,root / fs::u8path(authored.iconProjectRelativePath),
                               iconPath,error) ||
                !ValidateRegularFile(iconPath,MaximumIconBytes,error)) {
                error="Inventory icon cannot be packaged: "+
                      authored.iconProjectRelativePath+"; "+error;
                return false;
            }
            DependencyCandidate image;
            image.dependencyClass=DependencyClass::Texture;
            image.requirement=DependencyRequirement::Required;
            image.declaredPath=authored.iconProjectRelativePath;
            image.provenance="inventory.item.icon";
            itemAssets.push_back(std::move(image));
        }
        DependencyCandidate authoredFile;
        authoredFile.dependencyClass=DependencyClass::Data;
        authoredFile.requirement=DependencyRequirement::Required;
        authoredFile.declaredPath=catalogueRelative;
        authoredFile.provenance="inventory.item.catalogue";
        itemAssets.insert(itemAssets.begin(),std::move(authoredFile));
    }
    // A project can explicitly choose an editable Inventory Overlay Screen.
    // An old developer-only preview marker is NOT a production dependency.
    // Validate its identity/usage/action semantics before including it and
    // let RuntimeScreenDependencyProvider pull its referenced artwork.
    const auto assignmentFile=root/fs::u8path(InventoryScreenAssignmentPath);
    bool hasAssignment=false;
    if(!Exists(assignmentFile,hasAssignment,error))return false;
    if(hasAssignment) {
        if(!IsValidStableId(catalogueProjectId)) {
            error="An inventory Screen assignment requires a shared item catalogue.";
            return false;
        }
        InventoryScreenAssignment binding;
        if(!ReadInventoryScreenAssignment(root.generic_u8string(),
                catalogueProjectId,binding,error))
            return false;
        DependencyCandidate assignment;
        assignment.dependencyClass=DependencyClass::Data;
        assignment.requirement=DependencyRequirement::Required;
        assignment.declaredPath=InventoryScreenAssignmentPath;
        assignment.provenance="inventory.screen.assignment";
        itemAssets.push_back(std::move(assignment));
        DependencyCandidate screen;
        screen.dependencyClass=DependencyClass::RuntimeScreenDocument;
        screen.requirement=DependencyRequirement::Required;
        screen.declaredPath=binding.screenPath;
        screen.provenance="inventory.screen.authoring";
        itemAssets.push_back(std::move(screen));
    }
    // Emit the Scene-adjacent *declared* relative paths; source content hashes
    // and stable Registry records remain the collector's responsibility.
    DependencyCandidate item;
    item.dependencyClass = DependencyClass::Data;
    item.requirement = DependencyRequirement::Required;
    item.declaredPath = sceneHint + ".rinventory";
    item.provenance = "inventory.scene.initial_state";
    emit(item);
    if (hasAccess) {
        item.declaredPath = sceneHint + ".rinventory-access";
        item.provenance = "inventory.scene.script_permissions";
        emit(item);
    }
    for (const auto& asset:itemAssets) emit(asset);
    return true;
}
} // namespace renegade::bridge
