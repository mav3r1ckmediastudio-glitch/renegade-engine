#include "renegade/bridge/ResourceAssetService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    constexpr const char* ProjectId =
        "74444444-4444-4444-8444-444444444444";

    int failures = 0;

    void Require(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL // " << message << '\n';
        }
    }

    void WriteBytes(
        const fs::path& path,
        const std::vector<std::uint8_t>& bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!bytes.empty())
        {
            stream.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
    }

    void WriteText(const fs::path& path, const std::string& text)
    {
        WriteBytes(
            path,
            std::vector<std::uint8_t>(text.begin(), text.end()));
    }

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    std::string ReadText(const fs::path& path)
    {
        const auto bytes = ReadBytes(path);
        return std::string(bytes.begin(), bytes.end());
    }
}

int main()
{
    using namespace renegade::bridge;

    const fs::path root = fs::temp_directory_path() /
        "renegade-lp08-gate2-product-rollback";
    const fs::path source = root / "SourceAssets/Audio/rollback.wav";
    const fs::path product = root / "Content/Audio/rollback.rasset";
    const fs::path registryPath = root / AssetRegistryDocumentName;
    const fs::path metadataPath = root / ResourceAssetMetadataDocumentName;

    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    fs::create_directories(root / "SourceAssets/Audio", cleanupError);
    fs::create_directories(root / "Content/Audio", cleanupError);
    fs::create_directories(root / "Intermediate/Transactions", cleanupError);
    Require(!cleanupError, "could not create rollback proof project");

    const std::vector<std::uint8_t> wav = {
        'R','I','F','F',0,0,0,0,'W','A','V','E'};
    WriteBytes(source, wav);

    AssetRegistry initialRegistry;
    initialRegistry.projectId = ProjectId;
    initialRegistry.schemaVersion = AssetRegistry::CurrentSchemaVersion;
    std::string registryJson;
    std::string error;
    Require(
        SerializeAssetRegistry(initialRegistry, registryJson, error),
        "could not serialize rollback proof registry: " + error);
    WriteText(registryPath, registryJson);

    const std::string registryBefore = ReadText(registryPath);
    Require(!registryBefore.empty(), "rollback proof registry is empty");
    Require(!fs::exists(product), "rollback proof product already exists");
    Require(!fs::exists(metadataPath),
        "rollback proof metadata unexpectedly exists before import");

    bool observedProductAfterReplace = false;
    ResourceAssetImportOptions options;
    options.transactionId = "lp08-gate2-product-rollback";
    options.operationHook = [&observedProductAfterReplace, product](
        const ProjectDocumentTransactionStage stage,
        const std::size_t,
        const std::string& path,
        std::string& hookError)
    {
        if (stage != ProjectDocumentTransactionStage::AfterReplace)
        {
            return ProjectDocumentTransactionHookAction::Continue;
        }

        std::error_code existsError;
        if (!fs::is_regular_file(product, existsError) || existsError)
        {
            return ProjectDocumentTransactionHookAction::Continue;
        }

        std::error_code equivalentError;
        const bool isProduct = fs::equivalent(
            fs::u8path(path), product, equivalentError);
        if (equivalentError || !isProduct)
        {
            return ProjectDocumentTransactionHookAction::Continue;
        }

        observedProductAfterReplace = true;
        hookError =
            "intentional Gate 2 failure after governed product replacement";
        return ProjectDocumentTransactionHookAction::Fail;
    };

    ResourceAssetImportRequest request;
    request.projectRoot = root.generic_u8string();
    request.projectId = ProjectId;
    request.sourceProjectRelativePath =
        "SourceAssets/Audio/rollback.wav";
    request.assetProjectRelativePath =
        "Content/Audio/rollback.rasset";
    request.expectedFormat = ResourceSourceFormat::Wav;

    ResourceAssetService service;
    const auto result = service.ImportResourceAsset(
        request,
        std::move(options));

    Require(!result.succeeded,
        "fault-injected import unexpectedly succeeded");
    Require(observedProductAfterReplace,
        "test did not observe the actual .rasset after replacement");
    Require(result.transaction.rolledBack,
        "post-product failure did not report transaction rollback");
    Require(!result.transaction.recoveryRequired,
        "ordinary post-product failure unexpectedly requires recovery");
    Require(!fs::exists(product),
        "rollback left the newly replaced .rasset product on disk");
    Require(ReadText(registryPath) == registryBefore,
        "rollback did not restore exact previous registry bytes");
    Require(!fs::exists(metadataPath),
        "rollback left resource metadata from the failed first import");
    Require(ReadBytes(source) == wav,
        "rollback proof modified retained source bytes");

    AssetRegistry reopenedRegistry;
    Require(
        ReadAssetRegistry(
            root.generic_u8string(),
            ProjectId,
            reopenedRegistry,
            error),
        "restored registry did not reopen: " + error);
    Require(reopenedRegistry.records.empty() &&
            reopenedRegistry.importedProducts.empty(),
        "restored registry contains identity/provenance from failed import");

    fs::remove_all(root, cleanupError);
    if (failures != 0)
    {
        std::cerr << "LP08 GATE 2 PRODUCT ROLLBACK FAIL // "
                  << failures << " checks failed\n";
        return 1;
    }

    std::cout <<
        "LP08 GATE 2 PRODUCT ROLLBACK PASS // product_observed_then_removed\n";
    return 0;
}
