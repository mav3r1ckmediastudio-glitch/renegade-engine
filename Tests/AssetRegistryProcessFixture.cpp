#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/ProjectService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    using namespace renegade::bridge;
    namespace fs = std::filesystem;

    constexpr const char* ProjectId =
        "11111111-1111-4111-8111-111111111111";
    constexpr const char* OriginalSource = "Content/Source/source.asset";
    constexpr const char* MovedSource = "Content/Source/Moved/source.asset";
    constexpr const char* Product = "Content/Scenes/Main.wiscene";

    class ProofProvider final : public IDependencyProvider
    {
    public:
        explicit ProofProvider(std::string sourcePath)
            : sourcePath_(std::move(sourcePath)) {}

        const char* Name() const noexcept override { return "lc01-packaged-proof"; }
        std::uint32_t Version() const noexcept override { return 1; }
        bool Supports(DependencyClass) const noexcept override { return true; }

        bool Discover(
            const DependencyProviderContext& context,
            const DependencyCandidateSink& emit,
            const DependencyDiagnosticSink&,
            std::string& error) const override
        {
            if (context.source == nullptr)
            {
                error = "LC01 proof provider requires a source node.";
                return false;
            }
            if (context.source->projectRelativePath == "LC01Proof.renegade")
            {
                emit({sourcePath_, DependencyClass::ImportedContent,
                    DependencyRequirement::Required, "proof.source", false});
                emit({Product, DependencyClass::Scene,
                    DependencyRequirement::Required, "proof.product", false});
            }
            error.clear();
            return true;
        }

    private:
        std::string sourcePath_;
    };

    const AssetRecord* FindPath(const AssetRegistry& registry, const std::string& path)
    {
        const auto found = std::find_if(registry.records.begin(), registry.records.end(),
            [&path](const AssetRecord& record)
            { return record.projectRelativePath == path; });
        return found == registry.records.end() ? nullptr : &*found;
    }

    bool BuildGraph(
        const fs::path& root,
        const std::string& sourcePath,
        DependencyGraph& graph,
        std::string& error)
    {
        ProofProvider provider(sourcePath);
        DependencyCollector collector(root.generic_u8string());
        if (!collector.RegisterProvider(provider, error) ||
            !collector.AddRoot({"LC01Proof.renegade",
                DependencyClass::ProjectDocument,
                DependencyRequirement::Required,
                "lc01.packaged.root"}, error) ||
            !collector.DiscoverTransitiveDependencies(error))
            return false;
        graph = collector.Graph();
        return true;
    }

    bool WriteEvidence(
        const fs::path& outputPath,
        const AssetRegistry& registry,
        std::string& error)
    {
        std::string json;
        if (!SerializeAssetRegistry(registry, json, error))
            return false;
        fs::create_directories(outputPath.parent_path());
        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        output.write(json.data(), static_cast<std::streamsize>(json.size()));
        if (!output)
        {
            error = "Could not write LC01 packaged proof evidence.";
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    using namespace renegade::bridge;
    if (argc != 4)
    {
        std::cerr << "Usage: RenegadeAssetRegistryProcessFixture "
                     "<init|update|move-reopen|verify-reopen> <project-root> <evidence-json>\n";
        return 2;
    }

    const std::string mode = argv[1];
    const fs::path root = fs::absolute(fs::u8path(argv[2])).lexically_normal();
    const fs::path evidence = fs::absolute(fs::u8path(argv[3])).lexically_normal();
    const fs::path descriptor = root / "LC01Proof.renegade";
    if (!fs::is_regular_file(descriptor) ||
        !fs::is_regular_file(root / Product))
    {
        std::cerr << "LC01 packaged proof fixture is incomplete.\n";
        return 3;
    }
    const bool originalExists = fs::is_regular_file(root / OriginalSource);
    const bool movedExists = fs::is_regular_file(root / MovedSource);
    if (originalExists == movedExists)
    {
        std::cerr << "LC01 proof requires exactly one source location.\n";
        return 4;
    }
    const std::string sourcePath = originalExists ? OriginalSource : MovedSource;

    ProjectService projects;
    projects.Initialize((evidence.parent_path() /
        ("editor-state-" + mode + ".ini")).generic_u8string());
    if (!projects.OpenProject(descriptor.generic_u8string()) ||
        projects.CurrentProject().projectId != ProjectId)
    {
        std::cerr << "Project Open failed: " << projects.LastError() << '\n';
        return 5;
    }

    DependencyGraph graph;
    std::string error;
    if (!BuildGraph(root, sourcePath, graph, error))
    {
        std::cerr << error << '\n';
        return 6;
    }

    AssetRegistryRefresh refresh;
    AssetRegistry previous;
    if (mode == "init")
    {
        std::vector<StableId> ids = {
            "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
            "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
            "cccccccc-cccc-4ccc-8ccc-cccccccccccc",
        };
        std::size_t index = 0;
        if (!originalExists || !RefreshAssetRegistry(ProjectId, graph, nullptr,
                refresh, error, [&ids, &index] { return ids.at(index++); }))
        {
            std::cerr << (error.empty() ? "Initial source location is wrong." : error) << '\n';
            return 7;
        }
        const AssetRecord* source = FindPath(refresh.registry, OriginalSource);
        const AssetRecord* product = FindPath(refresh.registry, Product);
        if (source == nullptr || product == nullptr)
            return 8;
        ImportedProductRecord imported;
        imported.sourceAssetId = source->assetId;
        imported.productAssetId = product->assetId;
        imported.importer = "lc01.packaged-proof";
        imported.importerVersion = 1;
        imported.settingsSchema = "lc01.packaged-proof.settings";
        imported.settingsVersion = 1;
        imported.settingsJson = "{\"mode\":\"proof\"}";
        imported.sourceContentHashAtImport = source->contentHash;
        imported.productContentHashAtImport = product->contentHash;
        if (!SetImportedProductRecords(refresh.registry, {imported}, error))
        {
            std::cerr << error << '\n';
            return 9;
        }
    }
    else
    {
        if (!ReadAssetRegistry(root.generic_u8string(), ProjectId, previous, error) ||
            !RefreshAssetRegistry(ProjectId, graph, &previous, refresh, error,
                [] { return StableId{}; }))
        {
            std::cerr << error << '\n';
            return 10;
        }
        if (refresh.registry.importedProducts.size() != 1)
            return 11;
        const auto& imported = refresh.registry.importedProducts.front();
        const AssetRecord* source = FindPath(refresh.registry, sourcePath);
        ImportedProductStatus status;
        if (source == nullptr || source->assetId != imported.sourceAssetId ||
            !GetImportedProductStatus(refresh.registry, imported, status, error) ||
            !status.sourceAvailable || !status.sourceChanged ||
            !status.productAvailable || status.productChanged)
            return 12;

        if (mode == "update")
        {
            if (!originalExists || refresh.changedAssetIds !=
                    std::vector<StableId>{source->assetId} ||
                !refresh.recoveredAssetIds.empty())
                return 13;
        }
        else if (mode == "move-reopen")
        {
            if (!movedExists || refresh.recoveredAssetIds !=
                    std::vector<StableId>{source->assetId} ||
                !refresh.addedAssetIds.empty() || !refresh.removedAssetIds.empty())
                return 14;
        }
        else if (mode == "verify-reopen")
        {
            if (!movedExists || !refresh.addedAssetIds.empty() ||
                !refresh.changedAssetIds.empty() ||
                !refresh.removedAssetIds.empty() ||
                !refresh.recoveredAssetIds.empty())
                return 15;
        }
        else
        {
            std::cerr << "Unknown LC01 proof mode.\n";
            return 16;
        }
    }

    const auto write = WriteAssetRegistry(root.generic_u8string(), refresh.registry);
    if (!write.success)
    {
        std::cerr << write.message << '\n';
        return 17;
    }
    if (!WriteEvidence(evidence, refresh.registry, error))
    {
        std::cerr << error << '\n';
        return 18;
    }
    std::cout << "LC01_GATE5_PASS mode=" << mode
              << " assets=" << refresh.registry.records.size()
              << " provenance=" << refresh.registry.importedProducts.size()
              << "\n";
    return 0;
}
