#pragma once

#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ResourceAssetService.h"

#include <cstddef>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct PackagedResourceAsset
    {
        StableId projectId;
        StableId assetId;
        StableId sourceAssetId;
        std::string packagedAssetPath;
        ResourceClass resourceClass = ResourceClass::Unknown;
        ResourceSourceFormat sourceFormat = ResourceSourceFormat::Unknown;
        std::string payloadHash;
        std::vector<std::uint8_t> payload;
    };

    struct PackagedMaterialTextureRefreshRecord
    {
        StableId assetId;
        std::string packagedAssetPath;
        std::string payloadHash;
        wi::ecs::Entity materialEntity = wi::ecs::INVALID_ENTITY;
    };

    struct PackagedMaterialTextureRefreshResult
    {
        std::size_t discoveredBindingCount = 0;
        std::size_t refreshedBindingCount = 0;
        std::vector<PackagedMaterialTextureRefreshRecord> records;
    };

    // Generic LP08 package-relative resolver. Stable product identity comes
    // from LP06's content-manifest.json and the governed .rasset itself must
    // agree with that package/project identity. No AssetRegistry or retained
    // SourceAssets input is consulted at Runtime.
    [[nodiscard]] bool PreparePackagedResourceAsset(
        const std::string& packageRoot,
        const StableId& projectId,
        const StableId& assetId,
        PackagedResourceAsset& prepared,
        std::string& error);

    // Rehydrates saved material stable-ID bindings from packaged current
    // governed texture products. All distinct payloads are resolved/decoded
    // before any material is changed, so a required missing/corrupt product
    // fails closed without partially replacing authored bindings.
    [[nodiscard]] bool RefreshPackagedMaterialTextureAssets(
        wi::scene::Scene& scene,
        const std::string& packageRoot,
        const StableId& projectId,
        PackagedMaterialTextureRefreshResult& result,
        std::string& error,
        MaterialTextureResourceLoader loader = {});
}
