#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/CreatorAssetActionPolicy.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "90000000-0000-4000-8000-000000000001";
    constexpr const char* HealthySourceId =
        "10000000-0000-4000-8000-000000000001";
    constexpr const char* HealthyProductId =
        "20000000-0000-4000-8000-000000000001";
    constexpr const char* RecoverableId =
        "30000000-0000-4000-8000-000000000001";
    constexpr const char* UnresolvedId =
        "30000000-0000-4000-8000-000000000002";

    constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    int failures = 0;

    void Require(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void WriteText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::string HashText(const std::string& text)
    {
        std::uint64_t hash = FnvOffset;
        for (const unsigned char byte : text)
        {
            hash ^= byte;
            hash *= FnvPrime;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    AssetRecord MakeRecord(
        const StableId& assetId,
        const std::string& nodeId,
        const std::string& path,
        const std::string& hash)
    {
        AssetRecord record;
        record.assetId = assetId;
        record.dependencyNodeId = nodeId;
        record.projectRelativePath = path;
        record.dependencyClass = DependencyClass::ImportedContent;
        record.requirement = DependencyRequirement::Required;
        record.applicability = "windows-x64";
        record.provider = "asset-browser-tombstone-recovery-test";
        record.providerVersion = 1;
        record.contentHash = hash;
        record.sourceAvailable = true;
        return record;
    }

    MissingAssetRecord MakeMissing(
        const StableId& assetId,
        const std::string& path,
        const std::string& hash)
    {
        MissingAssetRecord missing;
        missing.assetId = assetId;
        missing.lastKnownPath = path;
        missing.dependencyClass = DependencyClass::ImportedContent;
        missing.requirement = DependencyRequirement::Required;
        missing.applicability = "windows-x64";
        missing.provider = "asset-browser-tombstone-recovery-test";
        missing.providerVersion = 1;
        missing.contentHash = hash;
        return missing;
    }

    const AssetCatalogueEntry* FindEntry(
        const AssetCatalogue& catalogue,
        const StableId& assetId)
    {
        const auto found = std::find_if(
            catalogue.entries.begin(), catalogue.entries.end(),
            [&assetId](const AssetCatalogueEntry& entry)
            { return entry.assetId == assetId; });
        return found == catalogue.entries.end() ? nullptr : &*found;
    }

    bool ContainsRecord(const AssetRegistry& registry, const StableId& assetId)
    {
        return std::any_of(
            registry.records.begin(), registry.records.end(),
            [&assetId](const AssetRecord& record)
            { return record.assetId == assetId; });
    }

    bool ContainsMissing(const AssetRegistry& registry, const StableId& assetId)
    {
        return std::any_of(
            registry.missingAssets.begin(), registry.missingAssets.end(),
            [&assetId](const MissingAssetRecord& missing)
            { return missing.assetId == assetId; });
    }
}

int main()
{
    using namespace renegade::bridge;

    const auto unique = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-asset-browser-tombstone-recovery-" +
            std::to_string(unique));

    const std::string healthySourceBytes = "healthy-source-fbx";
    const std::string healthyProductBytes = "healthy-product-rasset";
    const std::string recoverableBytes = "recoverable-rasset";
    const std::string unresolvedBytes = "changed-rasset-on-disk";

    WriteText(root / "SourceAssets/Models/healthy/healthy.fbx",
        healthySourceBytes);
    WriteText(root / "Content/Models/healthy.rasset",
        healthyProductBytes);
    WriteText(root / "Content/Models/recoverable.rasset",
        recoverableBytes);
    WriteText(root / "Content/Models/unresolved.rasset",
        unresolvedBytes);

    AssetRegistry registry;
    registry.projectId = ProjectId;
    registry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
    registry.records = {
        MakeRecord(
            HealthySourceId,
            "test.healthy.source",
            "SourceAssets/Models/healthy/healthy.fbx",
            HashText(healthySourceBytes)),
        MakeRecord(
            HealthyProductId,
            "test.healthy.product",
            "Content/Models/healthy.rasset",
            HashText(healthyProductBytes)),
    };

    ImportedProductRecord imported;
    imported.sourceAssetId = HealthySourceId;
    imported.productAssetId = HealthyProductId;
    imported.importer = "wicked.ufbx";
    imported.importerVersion = 1;
    imported.settingsSchema = "renegade-model-import";
    imported.settingsVersion = 1;
    imported.settingsJson = "{}";
    imported.sourceContentHashAtImport = HashText(healthySourceBytes);
    imported.productContentHashAtImport = HashText(healthyProductBytes);
    registry.importedProducts.push_back(imported);

    registry.missingAssets.push_back(MakeMissing(
        RecoverableId,
        "Content/Models/recoverable.rasset",
        HashText(recoverableBytes)));
    registry.missingAssets.push_back(MakeMissing(
        UnresolvedId,
        "Content/Models/unresolved.rasset",
        HashText("the-old-unresolved-bytes")));

    std::string error;
    Require(ValidateAssetRegistry(registry, error),
        "fixture registry is invalid");

    AssetRegistryPersistenceOptions writeOptions;
    writeOptions.transactionId = "asset-browser-tombstone-recovery-fixture";
    const auto written = WriteAssetRegistry(
        root.generic_u8string(), registry, std::move(writeOptions));
    Require(written.success && written.committed,
        "fixture registry did not persist");

    CreatorAssetWorkflowService workflow;
    AssetCatalogue catalogue;
    error.clear();
    Require(workflow.BuildCatalogueSnapshot(
            root.generic_u8string(), ProjectId, catalogue, error),
        "snapshot did not self-heal/isolate tombstone collisions");

    const auto* healthy = FindEntry(catalogue, HealthyProductId);
    Require(healthy != nullptr,
        "healthy imported model disappeared from repaired catalogue");
    Require(healthy != nullptr && CanPlaceCreatorModelAsset(*healthy),
        "healthy imported model is not placeable after unrelated tombstone collision");

    const auto* recovered = FindEntry(catalogue, RecoverableId);
    Require(recovered != nullptr && recovered->registered &&
            (recovered->state == AssetCatalogueState::Current ||
             recovered->state == AssetCatalogueState::Moved),
        "exact tombstone/file match was not recovered into the catalogue");

    const auto* unresolved = FindEntry(catalogue, UnresolvedId);
    Require(unresolved != nullptr && unresolved->registered &&
            unresolved->state == AssetCatalogueState::Invalid &&
            !unresolved->productAvailable,
        "changed tombstone collision was not quarantined as unavailable/Invalid");

    AssetRegistry persisted;
    error.clear();
    Require(ReadAssetRegistry(
            root.generic_u8string(), ProjectId, persisted, error),
        "repaired registry did not reopen");
    Require(ContainsRecord(persisted, RecoverableId) &&
            !ContainsMissing(persisted, RecoverableId),
        "exact recovery was not persisted by LC01");
    Require(!ContainsRecord(persisted, UnresolvedId) &&
            ContainsMissing(persisted, UnresolvedId),
        "quarantine incorrectly rebound stable identity to changed bytes");

    AssetCatalogue secondCatalogue;
    error.clear();
    Require(workflow.BuildCatalogueSnapshot(
            root.generic_u8string(), ProjectId, secondCatalogue, error),
        "second snapshot was re-bricked by unresolved tombstone collision");
    const auto* secondHealthy = FindEntry(secondCatalogue, HealthyProductId);
    Require(secondHealthy != nullptr && CanPlaceCreatorModelAsset(*secondHealthy),
        "healthy imported model stopped being placeable on repeated snapshot");

    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);

    if (failures != 0)
        return 1;
    std::cout << "ASSET_BROWSER_TOMBSTONE_RECOVERY_PASS\n";
    return 0;
}
