#include "RuntimeBootstrap.h"

#include "renegade/bridge/ResourceAssetRuntimeService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ReusableAssetRuntimeService.h"

#include <sstream>
#include <utility>

namespace renegade::runtime
{
    bool RefreshRuntimeReusableAssets(
        bridge::SceneService& scenes,
        RuntimeBootstrapResult& result,
        std::string& error,
        bridge::MaterialTextureResourceLoader authoringTextureLoader)
    {
        // Authored/Test Level WISCENEs persist governed texture stable IDs,
        // not live Wicked Resource handles. Rehydrate those bindings from the
        // explicit project root before gameplay starts. Test Level snapshots
        // carry the registry plus only the referenced governed texture products.
        if (!result.packageRelativeLaunch)
        {
            const auto restored = bridge::RestoreMaterialTextureBindings(
                scenes.GetScene(),
                result.project.rootPath,
                result.project.projectId,
                std::move(authoringTextureLoader));
            if (!restored.succeeded)
            {
                error = "Authored Runtime governed material restore failed: " +
                    restored.error;
                return false;
            }
            error.clear();
            return true;
        }

        bridge::PackagedReusableAssetRefreshResult refresh;
        if (!bridge::RefreshPackagedReusableAssetInstances(
                scenes.GetScene(),
                result.packageRootPath,
                result.project.projectId,
                refresh,
                error))
        {
            return false;
        }

        // Story Flow can load multiple Level scenes during one Runtime session.
        // Keep cumulative evidence so a reusable asset refreshed in an earlier
        // Level remains visible in the final bootstrap log even when later
        // Levels contain no reusable instances.
        result.reusableAssetInstancesDiscovered +=
            refresh.discoveredInstanceCount;
        result.reusableAssetInstancesRefreshed +=
            refresh.refreshedInstanceCount;
        result.reusableAssetRefreshTrace.reserve(
            result.reusableAssetRefreshTrace.size() + refresh.records.size());
        for (const auto& record : refresh.records)
        {
            std::ostringstream trace;
            trace << "asset_id=" << record.assetId
                  << " payload_hash=" << record.payloadHash
                  << " package_path=" << record.packagedAssetPath;
            result.reusableAssetRefreshTrace.push_back(trace.str());
        }

        bridge::PackagedMaterialTextureRefreshResult resourceRefresh;
        if (!bridge::RefreshPackagedMaterialTextureAssets(
                scenes.GetScene(),
                result.packageRootPath,
                result.project.projectId,
                resourceRefresh,
                error))
        {
            error = "Packaged governed resource refresh failed: " + error;
            return false;
        }

        // Reuse the existing deterministic Runtime trace transport rather than
        // inventing a second bootstrap log format. LP07 model records retain
        // their original asset_id= prefix; LP08 resource records are explicitly
        // typed so acceptance can distinguish them while remaining additive.
        result.reusableAssetRefreshTrace.reserve(
            result.reusableAssetRefreshTrace.size() +
            resourceRefresh.records.size());
        for (const auto& record : resourceRefresh.records)
        {
            std::ostringstream trace;
            trace << "resource_asset_id=" << record.assetId
                  << " payload_hash=" << record.payloadHash
                  << " package_path=" << record.packagedAssetPath;
            result.reusableAssetRefreshTrace.push_back(trace.str());
        }

        result.entityCount = scenes.EntityCount();
        error.clear();
        return true;
    }
}
