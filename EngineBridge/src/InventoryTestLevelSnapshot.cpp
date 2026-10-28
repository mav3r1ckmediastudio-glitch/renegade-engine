#include "renegade/bridge/InventoryTestLevelSnapshot.h"
#include "renegade/bridge/InventoryAccessPolicy.h"
#include "renegade/bridge/InventoryService.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace renegade::bridge {
namespace {
namespace fs = std::filesystem;
constexpr std::uintmax_t MaxSnapshotBytes = 16u*1024u*1024u;
constexpr std::uintmax_t MaxAccessBytes = 1024u*1024u;
bool InRoot(const fs::path& root, const fs::path& path,
            fs::path& canonical, std::string& error) {
    std::error_code ec;
    canonical = fs::weakly_canonical(path, ec);
    if (ec) { error = "Inventory companion path cannot be canonicalised."; return false; }
    const auto relative = fs::relative(canonical, root, ec);
    if (ec || relative.empty() || relative.is_absolute()) {
        error = "Inventory companion is outside project root."; return false;
    }
    for (const auto& part : relative)
        if (part == "..") {
            error = "Inventory companion escapes project root."; return false;
        }
    return true;
}
bool Copy(const fs::path& source, const fs::path& dest,
          std::uintmax_t maxSize, std::string& error) {
    std::error_code ec;
    if (!fs::is_regular_file(source, ec) || ec ||
        fs::file_size(source, ec) > maxSize || ec) {
        error = "Inventory companion is not a bounded regular file.";
        return false;
    }
    // destination is already in a disposable Test Level directory.
    fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) { error = "Inventory companion projection failed: " + ec.message(); return false; }
    return true;
}
} // namespace

bool SnapshotInventoryCompanions(const std::string& projectRoot,
                                 const std::string& sourceScenePath,
                                 const std::string& targetScenePath,
                                 std::string& error) {
    error.clear();
    if (projectRoot.empty() || sourceScenePath.empty() || targetScenePath.empty()) {
        error = "Inventory snapshot requires project and Scene paths."; return false;
    }
    std::error_code ec;
    const auto root = fs::weakly_canonical(fs::u8path(projectRoot), ec);
    if (ec || !fs::is_directory(root)) {
        error = "Invalid inventory project root."; return false;
    }
    fs::path sourceScene, source, access;
    if (!InRoot(root, fs::u8path(sourceScenePath), sourceScene, error) ||
        !InRoot(root, fs::u8path(sourceScene.generic_u8string()+".rinventory"), source, error) ||
        !InRoot(root, fs::u8path(sourceScene.generic_u8string()+".rinventory-access"), access, error))
        return false;
    const bool hasSnapshot=fs::exists(source,ec);
    if (ec) { error="Cannot inspect authored inventory companion."; return false; }
    const bool hasAccess=fs::exists(access,ec);
    if (ec) { error="Cannot inspect inventory permissions companion."; return false; }
    if (!hasSnapshot && !hasAccess) return true;
    if (!hasSnapshot) {
        error = "Inventory access permissions have no authoritative inventory snapshot.";
        return false;
    }
    if (!fs::is_regular_file(source,ec) || ec ||
        fs::file_size(source,ec)>MaxSnapshotBytes || ec) {
        error="Inventory snapshot is missing or oversized."; return false;
    }
    InventoryService verify;
    if (verify.LoadSnapshot(source.generic_u8string()) != InventoryResult::Success) {
        error="Inventory snapshot failed validation."; return false;
    }
    if (hasAccess) {
        std::vector<InventoryAccessGrant> grants;
        if (!ReadInventoryAccessPolicy(access.generic_u8string(), verify, grants, error))
            return false;
    }
    const auto target = fs::u8path(targetScenePath);
    if (!fs::is_regular_file(target,ec) || ec) {
        error = "Test Level Scene must exist before inventory is projected."; return false;
    }
    // Test Level session directory is a fresh disposable staging root. Never
    // allow the output to collide with an authored source companion.
    const auto output = fs::u8path(targetScenePath+".rinventory");
    const auto outputAccess = fs::u8path(targetScenePath+".rinventory-access");
    if (fs::exists(output,ec) || fs::exists(outputAccess,ec) || ec) {
        error = "Test Level inventory output already exists."; return false;
    }
    if (!Copy(source,output,MaxSnapshotBytes,error)) return false;
    if (hasAccess && !Copy(access,outputAccess,MaxAccessBytes,error)) {
        std::error_code ignored;
        fs::remove(output,ignored);
        return false;
    }
    error.clear();
    return true;
}
} // namespace renegade::bridge
