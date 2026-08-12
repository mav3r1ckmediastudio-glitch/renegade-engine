#include "renegade/bridge/ReusableAssetDependencyService.h"

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/SceneDocumentService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        std::string LowerExtension(const std::string& path)
        {
            std::string extension =
                fs::u8path(path).extension().generic_u8string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](const unsigned char value)
                {
                    return static_cast<char>(std::tolower(value));
                });
            return extension;
        }

        const AssetRecord* FindAssetById(
            const AssetRegistry& registry,
            const StableId& assetId)
        {
            const auto found = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&assetId](const AssetRecord& record)
                {
                    return record.assetId == assetId;
                });
            return found == registry.records.end() ? nullptr : &*found;
        }

        const AssetRecord* FindAssetByPath(
            const AssetRegistry& registry,
            const std::string& projectRelativePath)
        {
            const fs::path wanted =
                fs::u8path(projectRelativePath).lexically_normal();
            const auto found = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&wanted](const AssetRecord& record)
                {
                    return fs::u8path(record.projectRelativePath)
                        .lexically_normal() == wanted;
                });
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
                {
                    return record.productAssetId == productId;
                });
            return found == registry.importedProducts.end() ? nullptr : &*found;
        }

        DependencyCandidate ProjectRelativeCandidate(
            const std::string& projectRoot,
            DependencyCandidate candidate)
        {
            const fs::path declared = fs::u8path(candidate.declaredPath);
            if (!declared.is_absolute())
                return candidate;

            std::error_code ec;
            const fs::path root = fs::weakly_canonical(
                fs::u8path(projectRoot), ec);
            if (ec)
                return candidate;

            const fs::path relative =
                declared.lexically_normal().lexically_relative(root);
            if (!relative.empty())
                candidate.declaredPath = relative.generic_u8string();
            return candidate;
        }

        bool ReadRegistryForProvider(
            const DependencyProviderContext& context,
            const StableId& projectId,
            AssetRegistry& registry,
            std::string& error)
        {
            if (!IsValidStableId(projectId))
            {
                error =
                    "Reusable asset dependency provider requires a valid project ID.";
                return false;
            }
            if (!ReadAssetRegistry(
                    context.projectRoot, projectId, registry, error))
            {
                error =
                    "Reusable asset dependency provider could not read LC01: " + error;
                return false;
            }
            return true;
        }
    }

    ReusableAssetDependencyProvider::ReusableAssetDependencyProvider(
        StableId projectId)
        : projectId_(std::move(projectId))
    {
    }

    const char* ReusableAssetDependencyProvider::Name() const noexcept
    {
        // Gate 3 established this provider identity on the durable product
        // record. Reaching that same product from a saved scene must not cause
        // a build-time LC01 refresh to relabel it and invalidate Gate 4's
        // source/product relationship guard.
        return "lp07.rasset";
    }

    std::uint32_t ReusableAssetDependencyProvider::Version() const noexcept
    {
        return 1;
    }

    bool ReusableAssetDependencyProvider::Supports(
        const DependencyClass dependencyClass) const noexcept
    {
        return dependencyClass == DependencyClass::Scene ||
            dependencyClass == DependencyClass::ImportedContent;
    }

    bool ReusableAssetDependencyProvider::Discover(
        const DependencyProviderContext& context,
        const DependencyCandidateSink& emit,
        const DependencyDiagnosticSink&,
        std::string& error) const
    {
        if (context.source == nullptr || context.projectRoot.empty())
        {
            error =
                "Reusable asset dependency provider requires a source node and project root.";
            return false;
        }

        if (context.source->dependencyClass == DependencyClass::Scene)
        {
            const auto resolved = ResolveDependencyPath(
                context.projectRoot, context.source->projectRelativePath);
            if (!resolved.accepted || !resolved.exists)
            {
                error = resolved.accepted
                    ? "Reusable asset dependency scene is unavailable."
                    : resolved.error;
                return false;
            }

            auto prepared = PrepareWickedSceneOpen(resolved.absolutePath);
            if (!prepared.IsReady() || prepared.ReadOnlyScene() == nullptr)
            {
                error = prepared.Error().empty()
                    ? "Reusable asset dependency scene could not be inspected."
                    : prepared.Error();
                return false;
            }

            std::vector<ReusableAssetInstanceRecord> instances;
            if (!InspectReusableAssetInstances(
                    *prepared.ReadOnlyScene(), instances, error))
            {
                error =
                    "Reusable asset dependency scene metadata is invalid: " + error;
                return false;
            }
            if (instances.empty())
            {
                error.clear();
                return true;
            }

            AssetRegistry registry;
            if (!ReadRegistryForProvider(
                    context, projectId_, registry, error))
                return false;

            for (const auto& instance : instances)
            {
                const AssetRecord* product =
                    FindAssetById(registry, instance.assetId);
                const ImportedProductRecord* provenance =
                    FindImportedProduct(registry, instance.assetId);
                if (product == nullptr || provenance == nullptr ||
                    product->dependencyClass != DependencyClass::ImportedContent ||
                    !product->sourceAvailable ||
                    LowerExtension(product->projectRelativePath) !=
                        ReusableAssetExtension)
                {
                    error =
                        "Reusable asset scene instance does not resolve to an available governed .rasset product in LC01: " +
                        instance.assetId;
                    return false;
                }

                DependencyCandidate candidate;
                candidate.declaredPath = product->projectRelativePath;
                candidate.dependencyClass = DependencyClass::ImportedContent;
                candidate.requirement = DependencyRequirement::Required;
                candidate.provenance =
                    "lp07.reusable_asset_instance:" + instance.assetId;
                emit(candidate);
            }

            error.clear();
            return true;
        }

        // ImportedContent is broader than reusable assets. Be completely inert
        // for FBX, GLTF, GLB and every other imported source handled elsewhere.
        if (LowerExtension(context.source->projectRelativePath) !=
            ReusableAssetExtension)
        {
            error.clear();
            return true;
        }

        AssetRegistry registry;
        if (!ReadRegistryForProvider(context, projectId_, registry, error))
            return false;
        const AssetRecord* product = FindAssetByPath(
            registry, context.source->projectRelativePath);
        if (product == nullptr ||
            product->dependencyClass != DependencyClass::ImportedContent ||
            FindImportedProduct(registry, product->assetId) == nullptr)
        {
            error =
                "Reachable .rasset is not an authoritative LC01 imported product.";
            return false;
        }

        ReusableAssetService reusableAssets;
        ReusableModelPlacementRequest request;
        request.projectRoot = context.projectRoot;
        request.projectId = projectId_;
        request.assetId = product->assetId;
        auto prepared = reusableAssets.PrepareModelAssetPlacement(request);
        if (!prepared.IsReady() || prepared.PeekScene() == nullptr)
        {
            error = prepared.Result().error.empty()
                ? "Reachable .rasset payload could not be inspected."
                : prepared.Result().error;
            return false;
        }

        WisceneDependencyDocument payloadDependencies;
        InspectWisceneDependencies(
            *prepared.PeekScene(), payloadDependencies);
        for (auto reference : payloadDependencies.references)
        {
            reference = ProjectRelativeCandidate(
                context.projectRoot, std::move(reference));
            reference.provenance =
                "lp07.rasset_payload." + reference.provenance;
            emit(reference);
        }

        error.clear();
        return true;
    }
}
