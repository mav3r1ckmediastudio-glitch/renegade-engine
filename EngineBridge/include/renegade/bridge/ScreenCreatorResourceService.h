#pragma once

#include "renegade/bridge/AssetBrowserService.h"
#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/ResourceAssetService.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class ScreenCreatorResourceKind
    {
        Image,
        Font,
    };

    struct ScreenCreatorResourceChoice
    {
        // Stable LP08 product identity is creator-selection authority. The
        // source path remains the schema-v2 renderer-compatible path hint until
        // Screen persistence itself moves to stable resource references.
        StableId assetId;
        std::string projectRelativePath;
        std::string productProjectRelativePath;
        AssetType assetType = AssetType::Unknown;
    };

    struct ScreenCreatorResourceCatalogue
    {
        bool succeeded = false;
        std::vector<ScreenCreatorResourceChoice> choices;
        std::string error;
    };

    // Creator-facing projection over accepted LC01/LP08 governance. No raw
    // filesystem scan and no arbitrary path picker is used: every choice must
    // resolve through imported-product provenance to a current authoritative
    // .rasset plus retained source inside the active project.
    [[nodiscard]] inline ScreenCreatorResourceCatalogue
    EnumerateScreenCreatorResources(
        const std::string& projectRoot,
        const StableId& projectId,
        const ScreenCreatorResourceKind kind)
    {
        ScreenCreatorResourceCatalogue result;
        if (projectRoot.empty() || !IsValidStableId(projectId))
        {
            result.error =
                "Screen resource catalogue requires an active project and valid project ID.";
            return result;
        }

        AssetRegistry registry;
        if (!ReadAssetRegistry(projectRoot, projectId, registry, result.error))
        {
            result.error = "Screen resource catalogue could not read LC01: " +
                result.error;
            return result;
        }

        ResourceAssetMetadataDocument metadata;
        if (!ReadResourceAssetMetadata(
                projectRoot, projectId, metadata, result.error))
        {
            result.error =
                "Screen resource catalogue could not read LP08 metadata: " +
                result.error;
            return result;
        }

        const ResourceClass requestedClass =
            kind == ScreenCreatorResourceKind::Image
                ? ResourceClass::Texture : ResourceClass::Font;
        const AssetType requestedType =
            ResourceClassAssetType(requestedClass);

        const auto findAsset = [&registry](const StableId& id)
            -> const AssetRecord*
        {
            const auto found = std::find_if(
                registry.records.begin(), registry.records.end(),
                [&id](const AssetRecord& record)
                {
                    return record.assetId == id;
                });
            return found == registry.records.end() ? nullptr : &*found;
        };
        const auto findMetadata = [&metadata](const StableId& id)
            -> const ResourceAssetMetadataRecord*
        {
            const auto found = std::find_if(
                metadata.records.begin(), metadata.records.end(),
                [&id](const ResourceAssetMetadataRecord& record)
                {
                    return record.assetId == id;
                });
            return found == metadata.records.end() ? nullptr : &*found;
        };

        for (const auto& provenance : registry.importedProducts)
        {
            const auto* product = findAsset(provenance.productAssetId);
            const auto* source = findAsset(provenance.sourceAssetId);
            const auto* productMetadata = findMetadata(
                provenance.productAssetId);
            if (product == nullptr || source == nullptr ||
                productMetadata == nullptr ||
                productMetadata->resourceClass != requestedClass ||
                !product->sourceAvailable || !source->sourceAvailable ||
                product->provider != "lp08.rasset" ||
                provenance.importer != "wicked.resourcemanager" ||
                std::filesystem::u8path(product->projectRelativePath).extension() !=
                    ResourceAssetExtension ||
                AssetBrowserService::Classify(source->projectRelativePath) !=
                    requestedType)
            {
                continue;
            }

            ScreenCreatorResourceChoice choice;
            choice.assetId = product->assetId;
            choice.projectRelativePath = std::filesystem::u8path(
                source->projectRelativePath).lexically_normal().generic_u8string();
            choice.productProjectRelativePath = std::filesystem::u8path(
                product->projectRelativePath).lexically_normal().generic_u8string();
            choice.assetType = requestedType;
            result.choices.push_back(std::move(choice));
        }

        std::sort(
            result.choices.begin(), result.choices.end(),
            [](const ScreenCreatorResourceChoice& left,
               const ScreenCreatorResourceChoice& right)
            {
                if (left.projectRelativePath != right.projectRelativePath)
                    return left.projectRelativePath < right.projectRelativePath;
                return left.assetId < right.assetId;
            });
        result.choices.erase(
            std::unique(
                result.choices.begin(), result.choices.end(),
                [](const ScreenCreatorResourceChoice& left,
                   const ScreenCreatorResourceChoice& right)
                {
                    return left.assetId == right.assetId;
                }),
            result.choices.end());
        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
