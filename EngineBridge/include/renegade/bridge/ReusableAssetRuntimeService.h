#pragma once

#include "renegade/bridge/IdentityService.h"

#include <cstddef>
#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::bridge
{
    struct PackagedReusableAssetRefreshRecord
    {
        StableId assetId;
        std::string packagedAssetPath;
        std::string payloadHash;
        wi::ecs::Entity instanceRoot = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity payloadRoot = wi::ecs::INVALID_ENTITY;
    };

    struct PackagedReusableAssetRefreshResult
    {
        std::size_t discoveredInstanceCount = 0;
        std::size_t refreshedInstanceCount = 0;
        std::vector<PackagedReusableAssetRefreshRecord> records;
    };

    struct PreparedPackagedReusableAsset
    {
        StableId assetId;
        std::string packagedAssetPath;
        std::string payloadHash;
        wi::allocator::shared_ptr<wi::scene::Scene> scene;

        [[nodiscard]] bool IsReady() const noexcept
        {
            return IsValidStableId(assetId) && !packagedAssetPath.empty() &&
                !payloadHash.empty() && scene.IsValid();
        }
    };

    // Resolves one governed reusable product directly from the package content
    // manifest. This is the transient-view-model/package seam: it shares the
    // exact same validation as scene-instance replacement without requiring a
    // persisted reusable-instance wrapper in the level scene.
    [[nodiscard]] bool PreparePackagedReusableAsset(
        const std::string& packageRoot,
        const StableId& projectId,
        const StableId& assetId,
        PreparedPackagedReusableAsset& prepared,
        std::string& error);

    // Package-relative Runtime acceptance boundary. A saved scene carries only
    // stable reusable asset identity plus its authored instance wrapper. This
    // resolves the current governed .rasset from LP06's content manifest and
    // replaces the child payload from packaged bytes. It never consults or
    // converts SourceAssets/FBX/GLTF input.
    [[nodiscard]] bool RefreshPackagedReusableAssetInstances(
        wi::scene::Scene& scene,
        const std::string& packageRoot,
        const StableId& projectId,
        PackagedReusableAssetRefreshResult& result,
        std::string& error);
}
