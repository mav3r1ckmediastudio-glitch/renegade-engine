#include "renegade/bridge/ResourceAssetDependencyService.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ResourceAssetService.h"
#include "renegade/bridge/SceneDocumentService.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        const AssetRecord* FindAssetById(
            const AssetRegistry& registry,
            const StableId& assetId)
        {
            const auto found = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&assetId](const AssetRecord& record)
                { return record.assetId == assetId; });
            return found == registry.records.end() ? nullptr : &*found;
        }

        const ImportedProductRecord* FindImportedProduct(
            const AssetRegistry& registry,
            const StableId& productId)
        {
            const auto found = std::find_if(
                registry.importedProducts.begin(),
                registry.importedProducts.end(),
                [&productId](const ImportedProductRecord& record)
                { return record.productAssetId == productId; });
            return found == registry.importedProducts.end() ? nullptr : &*found;
        }
    }

    ResourceAssetDependencyProvider::ResourceAssetDependencyProvider(
        StableId projectId)
        : projectId_(std::move(projectId))
    {
    }

    const char* ResourceAssetDependencyProvider::Name() const noexcept
    {
        // This deliberately matches the durable provider identity created by
        // LP08 first import. LC01 refresh must not relabel a reached product.
        return "lp08.rasset";
    }

    std::uint32_t ResourceAssetDependencyProvider::Version() const noexcept
    {
        return 1;
    }

    bool ResourceAssetDependencyProvider::Supports(
        const DependencyClass dependencyClass) const noexcept
    {
        return dependencyClass == DependencyClass::Scene;
    }

    bool ResourceAssetDependencyProvider::Discover(
        const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink&,
        std::string& error) const
    {
        if (context.source == nullptr || context.projectRoot.empty() ||
            !IsValidStableId(projectId_))
        {
            error =
                "Resource asset dependency provider requires a scene source, project root and valid project ID.";
            return false;
        }

        const auto resolved = ResolveDependencyPath(
            context.projectRoot, context.source->projectRelativePath);
        if (!resolved.accepted || !resolved.exists)
        {
            error = resolved.accepted
                ? "Resource asset dependency scene is unavailable."
                : resolved.error;
            return false;
        }

        auto prepared = PrepareWickedSceneOpen(resolved.absolutePath);
        if (!prepared.IsReady() || prepared.ReadOnlyScene() == nullptr)
        {
            error = prepared.Error().empty()
                ? "Resource asset dependency scene could not be inspected."
                : prepared.Error();
            return false;
        }

        std::vector<MaterialTextureBindingRecord> bindings;
        if (!InspectMaterialTextureBindings(
                *prepared.ReadOnlyScene(), bindings, error))
        {
            error =
                "Resource asset dependency scene metadata is invalid: " + error;
            return false;
        }
        if (bindings.empty())
        {
            error.clear();
            return true;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(
                context.projectRoot, projectId_, registry, error))
        {
            error =
                "Resource asset dependency provider could not read LC01: " + error;
            return false;
        }

        std::set<StableId> emittedAssets;
        for (const auto& binding : bindings)
        {
            if (!emittedAssets.insert(binding.baseColorTextureAssetId).second)
                continue;

            const AssetRecord* product =
                FindAssetById(registry, binding.baseColorTextureAssetId);
            const ImportedProductRecord* provenance =
                FindImportedProduct(registry, binding.baseColorTextureAssetId);
            if (product == nullptr || provenance == nullptr ||
                product->dependencyClass != DependencyClass::Texture ||
                product->requirement != DependencyRequirement::Required ||
                !product->sourceAvailable ||
                product->provider != "lp08.rasset" ||
                product->providerVersion != 1 ||
                fs::u8path(product->projectRelativePath).extension() !=
                    ResourceAssetExtension ||
                provenance->importer != "wicked.resourcemanager" ||
                provenance->importerVersion != 1)
            {
                error =
                    "Governed material texture stable ID does not resolve to an available authoritative LP08 .rasset product in LC01: " +
                    binding.baseColorTextureAssetId;
                return false;
            }

            DependencyCandidate candidate;
            candidate.declaredPath = product->projectRelativePath;
            candidate.dependencyClass = DependencyClass::Texture;
            candidate.requirement = DependencyRequirement::Required;
            candidate.provenance =
                "lp08.material_texture_binding:" +
                binding.baseColorTextureAssetId;
            emit(candidate);
        }

        error.clear();
        return true;
    }
}
