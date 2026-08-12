#pragma once

#include "renegade/bridge/AssetCatalogueService.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace renegade::bridge
{
    inline bool IsCreatorModelSourceFormat(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value == "fbx" || value == "gltf" || value == "glb";
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
}
