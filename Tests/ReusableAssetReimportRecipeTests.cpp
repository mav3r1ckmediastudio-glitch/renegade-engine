#include "renegade/bridge/ReusableAssetService.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr const char* ProjectId = "88888888-8888-4888-8888-888888888888";
    constexpr const char* SourceId = "99999999-9999-4999-8999-999999999999";
    constexpr const char* ProductId = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    constexpr std::uint64_t FnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    bool Require(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "LP07 GATE 4 RECIPE TEST FAIL // " << message << '\n';
        return false;
    }

    std::string HashBytes(const std::vector<std::uint8_t>& bytes)
    {
        std::uint64_t hash = FnvOffset;
        for (const auto value : bytes)
        {
            hash ^= value;
            hash *= FnvPrime;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
               << std::setw(16) << hash;
        return stream.str();
    }

    bool WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        if (!bytes.empty())
        {
            output.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        return static_cast<bool>(output);
    }

    bool ReadBytes(const fs::path& path, std::vector<std::uint8_t>& bytes)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;
        bytes.assign(std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
        return input.good() || input.eof();
    }
}

int main()
{
    using namespace renegade::bridge;

    const fs::path root = fs::temp_directory_path() /
        "renegade-lp07-gate4-recipe";
    const fs::path sourcePath = root / "SourceAssets" / "Models" / "fixture.fbx";
    const fs::path productPath = root / "Content" / "Models" / "fixture.rasset";

    std::error_code ec;
    fs::remove_all(root, ec);
    ec.clear();
    fs::create_directories(sourcePath.parent_path(), ec);
    fs::create_directories(productPath.parent_path(), ec);
    fs::create_directories(root / "Intermediate" / "Transactions", ec);
    if (!Require(!ec, "could not create project fixture"))
        return 1;

    const std::vector<std::uint8_t> sourceBytes = {
        'n', 'o', 't', '-', 'a', '-', 'r', 'e', 'a', 'l', '-', 'f', 'b', 'x'
    };
    if (!Require(WriteBytes(sourcePath, sourceBytes),
            "could not write source fixture"))
        return 1;

    ReusableAssetService service;
    ReusableModelImportRequest unsupportedImport;
    unsupportedImport.projectRoot = root.generic_u8string();
    unsupportedImport.projectId = ProjectId;
    unsupportedImport.sourceProjectRelativePath = "SourceAssets/Models/fixture.fbx";
    unsupportedImport.assetProjectRelativePath = "Content/Models/rejected.rasset";
    unsupportedImport.settingsJson = "{\"unsupported\":true}";
    const auto rejectedImport = service.ImportModelAsset(unsupportedImport);
    if (!Require(!rejectedImport.succeeded &&
            rejectedImport.error.find("unsupported key: unsupported") !=
                std::string::npos,
            "new import did not reject an unknown creator-option key before conversion: " +
                rejectedImport.error) ||
        !Require(!fs::exists(root / "Content" / "Models" / "rejected.rasset") &&
            !fs::exists(root / AssetRegistryDocumentName),
            "rejected new import created persistent product/registry state"))
        return 1;

    ReusableModelAssetDocument asset;
    asset.manifest.projectId = ProjectId;
    asset.manifest.assetId = ProductId;
    asset.manifest.sourceAssetId = SourceId;
    asset.manifest.sourceFormat = "fbx";
    asset.manifest.importer = "wicked.ufbx";
    asset.manifest.importerVersion = 1;
    asset.manifest.settingsSchema = ReusableModelImportSettingsSchema;
    asset.manifest.settingsVersion = 1;
    asset.manifest.settingsJson =
        "{\"options\":{\"unsupported\":true},\"source_format\":\"fbx\"}";
    asset.payload = {'W', 'I', 'S', 'C', 'E', 'N', 'E', 1};
    asset.manifest.payloadHash = HashBytes(asset.payload);

    std::string error;
    std::vector<std::uint8_t> productBytes;
    if (!Require(SerializeReusableModelAssetDocument(
            asset, productBytes, error),
            "could not create stored version-1 recipe fixture: " + error) ||
        !Require(WriteBytes(productPath, productBytes),
            "could not write product fixture"))
        return 1;

    AssetRegistry registry;
    registry.projectId = ProjectId;

    AssetRecord source;
    source.assetId = SourceId;
    source.dependencyNodeId = std::string("lp07.source:") + SourceId;
    source.projectRelativePath = "SourceAssets/Models/fixture.fbx";
    source.dependencyClass = DependencyClass::ImportedContent;
    source.requirement = DependencyRequirement::EditorOnly;
    source.provider = "lp07.source_asset";
    source.providerVersion = 1;
    source.contentHash = HashBytes(sourceBytes);
    registry.records.push_back(source);

    AssetRecord product;
    product.assetId = ProductId;
    product.dependencyNodeId = std::string("lp07.rasset:") + ProductId;
    product.projectRelativePath = "Content/Models/fixture.rasset";
    product.dependencyClass = DependencyClass::ImportedContent;
    product.requirement = DependencyRequirement::Required;
    product.provider = "lp07.rasset";
    product.providerVersion = 1;
    product.contentHash = HashBytes(productBytes);
    registry.records.push_back(product);

    ImportedProductRecord provenance;
    provenance.sourceAssetId = SourceId;
    provenance.productAssetId = ProductId;
    provenance.importer = "wicked.ufbx";
    provenance.importerVersion = 1;
    provenance.settingsSchema = ReusableModelImportSettingsSchema;
    provenance.settingsVersion = 1;
    provenance.settingsJson = asset.manifest.settingsJson;
    provenance.sourceContentHashAtImport = source.contentHash;
    provenance.productContentHashAtImport = product.contentHash;

    if (!Require(SetImportedProductRecords(
            registry, {provenance}, error),
            "could not create imported-product provenance: " + error))
        return 1;

    AssetRegistryPersistenceOptions persistence;
    persistence.transactionId = "lp07-gate4-recipe-setup";
    const auto written = WriteAssetRegistry(
        root.generic_u8string(), registry, std::move(persistence));
    if (!Require(written.success && written.committed,
            "could not persist registry fixture: " + written.message))
        return 1;

    std::vector<std::uint8_t> productBefore;
    std::vector<std::uint8_t> registryBefore;
    if (!Require(ReadBytes(productPath, productBefore) &&
            ReadBytes(root / AssetRegistryDocumentName, registryBefore),
            "could not capture authoritative bytes before rejection"))
        return 1;

    ReusableModelReimportRequest request;
    request.projectRoot = root.generic_u8string();
    request.projectId = ProjectId;
    request.assetId = ProductId;
    const auto result = service.ReimportModelAsset(request);

    std::vector<std::uint8_t> productAfter;
    std::vector<std::uint8_t> registryAfter;
    const bool passed =
        Require(!result.succeeded,
            "stored recipe with an unknown creator-option key unexpectedly reimported") &&
        Require(result.error.find("unsupported key: unsupported") != std::string::npos,
            "stored creator recipe did not reject the unknown key at the recipe boundary: " +
                result.error) &&
        Require(ReadBytes(productPath, productAfter) &&
            ReadBytes(root / AssetRegistryDocumentName, registryAfter),
            "could not reopen authoritative bytes after rejection") &&
        Require(productAfter == productBefore && registryAfter == registryBefore,
            "recipe rejection mutated authoritative product/registry bytes");

    fs::remove_all(root, ec);
    if (!passed)
        return 1;

    std::cout << "LP07 GATE 4 RECIPE FAIL-CLOSED PASS\n";
    return 0;
}
