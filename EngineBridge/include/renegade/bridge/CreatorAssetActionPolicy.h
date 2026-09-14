#pragma once

#include "renegade/bridge/AssetCatalogueService.h"

#include <string>

namespace renegade::bridge
{
    inline bool IsCreatorModelSourceFormat(
        const std::string& value) noexcept
    {
        const auto lower = [](const unsigned char character) noexcept
        {
            return character >= 'A' && character <= 'Z'
                ? static_cast<char>(character + ('a' - 'A'))
                : static_cast<char>(character);
        };
        if (value.size() == 3)
        {
            const char a = lower(static_cast<unsigned char>(value[0]));
            const char b = lower(static_cast<unsigned char>(value[1]));
            const char c = lower(static_cast<unsigned char>(value[2]));
            return (a == 'f' && b == 'b' && c == 'x') ||
                (a == 'g' && b == 'l' && c == 'b');
        }
        return value.size() == 4 &&
            lower(static_cast<unsigned char>(value[0])) == 'g' &&
            lower(static_cast<unsigned char>(value[1])) == 'l' &&
            lower(static_cast<unsigned char>(value[2])) == 't' &&
            lower(static_cast<unsigned char>(value[3])) == 'f';
    }

    inline bool IsCreatorGovernedResourceClass(
        const DependencyClass dependencyClass) noexcept
    {
        return dependencyClass == DependencyClass::Texture ||
            dependencyClass == DependencyClass::Audio ||
            dependencyClass == DependencyClass::Script ||
            dependencyClass == DependencyClass::Video ||
            dependencyClass == DependencyClass::Font;
    }

    // Placement requires a live imported model product. Reimport deliberately
    // does not: a missing governed product is one of the states reimport must
    // be able to recover from.
    inline bool CanPlaceCreatorModelAsset(
        const AssetCatalogueEntry& entry) noexcept
    {
        return entry.registered && IsValidStableId(entry.assetId) &&
            entry.importedProduct && entry.productAvailable &&
            IsCreatorModelSourceFormat(entry.sourceFormat);
    }

    inline bool IsCreatorCharacterPrefabPath(
        const std::string& projectRelativePath) noexcept
    {
        constexpr const char* suffix = ".rcharprefab";
        if (projectRelativePath.size() < 16 ||
            projectRelativePath.rfind("Content/Prefabs/", 0) != 0)
        {
            return false;
        }
        return projectRelativePath.size() >= 13 &&
            projectRelativePath.compare(
                projectRelativePath.size() - 13, 13, suffix) == 0;
    }

    // CW-05 Character Prefabs are generated project assets rather than imported
    // model products. They are placeable only while their registered prefab
    // document is available/current; the placement service resolves their
    // stable base Character Asset dependency before any Scene mutation occurs.
    inline bool CanPlaceCreatorCharacterPrefabAsset(
        const AssetCatalogueEntry& entry) noexcept
    {
        return entry.registered && IsValidStableId(entry.assetId) &&
            entry.type == AssetType::Prefab && entry.productAvailable &&
            (entry.state == AssetCatalogueState::Current ||
             entry.state == AssetCatalogueState::Moved) &&
            IsCreatorCharacterPrefabPath(entry.projectRelativePath);
    }

    inline bool CanPlaceCreatorSceneAsset(
        const AssetCatalogueEntry& entry) noexcept
    {
        return CanPlaceCreatorModelAsset(entry) ||
            CanPlaceCreatorCharacterPrefabAsset(entry);
    }

    inline bool CanReimportCreatorModelAsset(
        const AssetCatalogueEntry& entry) noexcept
    {
        return entry.registered && IsValidStableId(entry.assetId) &&
            entry.importedProduct &&
            IsCreatorModelSourceFormat(entry.sourceFormat);
    }

    // Gate 4 resource reimport requires a live retained source. Creator
    // catalogue refresh projects that source state even when the governed
    // product itself is represented only by an LC01 tombstone, so Studio can
    // truthfully offer missing-product recovery without relying on backend
    // failure as its action policy. Invalid active products remain blocked.
    inline bool CanReimportCreatorResourceAsset(
        const AssetCatalogueEntry& entry) noexcept
    {
        const bool activeProductState = entry.sourceAvailable &&
            entry.productAvailable &&
            (entry.state == AssetCatalogueState::Current ||
             entry.state == AssetCatalogueState::Stale ||
             entry.state == AssetCatalogueState::Moved);
        const bool recoverableMissingProduct = entry.sourceAvailable &&
            !entry.productAvailable &&
            entry.state == AssetCatalogueState::Missing;
        return entry.registered && IsValidStableId(entry.assetId) &&
            entry.importedProduct &&
            entry.importer == "wicked.resourcemanager" &&
            IsCreatorGovernedResourceClass(entry.dependencyClass) &&
            (activeProductState || recoverableMissingProduct);
    }
}
