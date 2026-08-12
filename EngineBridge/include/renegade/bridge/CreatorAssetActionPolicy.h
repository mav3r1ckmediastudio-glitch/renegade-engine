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

    inline bool CanReimportCreatorModelAsset(
        const AssetCatalogueEntry& entry) noexcept
    {
        return entry.registered && IsValidStableId(entry.assetId) &&
            entry.importedProduct &&
            IsCreatorModelSourceFormat(entry.sourceFormat);
    }

    // Gate 4 resource reimport always requires an active retained source. An
    // active Current/Stale/Moved product can be replaced after last-good
    // validation; a Missing product can be regenerated when LC01 retains its
    // tombstone and the source is still available. Invalid products remain
    // blocked because reimport must not overwrite untrusted on-disk bytes.
    inline bool CanReimportCreatorResourceAsset(
        const AssetCatalogueEntry& entry) noexcept
    {
        const bool activeProductState = entry.productAvailable &&
            (entry.state == AssetCatalogueState::Current ||
             entry.state == AssetCatalogueState::Stale ||
             entry.state == AssetCatalogueState::Moved);
        const bool recoverableMissingProduct = !entry.productAvailable &&
            entry.state == AssetCatalogueState::Missing;
        return entry.registered && IsValidStableId(entry.assetId) &&
            entry.importedProduct && entry.sourceAvailable &&
            entry.importer == "wicked.resourcemanager" &&
            IsCreatorGovernedResourceClass(entry.dependencyClass) &&
            (activeProductState || recoverableMissingProduct);
    }
}
