#include "RuntimeBootstrap.h"

#include "renegade/bridge/ReusableAssetRuntimeService.h"

#include <sstream>

namespace renegade::runtime
{
    bool RefreshRuntimeReusableAssets(
        bridge::SceneService& scenes,
        RuntimeBootstrapResult& result,
        std::string& error)
    {
        result.reusableAssetInstancesDiscovered = 0;
        result.reusableAssetInstancesRefreshed = 0;
        result.reusableAssetRefreshTrace.clear();

        // Studio Test Level and other explicit --project launches continue to
        // consume their already-authored scene exactly as before. Gate 6 only
        // changes the integrity-validated LP06 package-relative launch path.
        if (!result.packageRelativeLaunch)
        {
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

        result.reusableAssetInstancesDiscovered =
            refresh.discoveredInstanceCount;
        result.reusableAssetInstancesRefreshed =
            refresh.refreshedInstanceCount;
        result.reusableAssetRefreshTrace.reserve(refresh.records.size());
        for (const auto& record : refresh.records)
        {
            std::ostringstream trace;
            trace << "asset_id=" << record.assetId
                  << " payload_hash=" << record.payloadHash
                  << " package_path=" << record.packagedAssetPath;
            result.reusableAssetRefreshTrace.push_back(trace.str());
        }

        result.entityCount = scenes.EntityCount();
        error.clear();
        return true;
    }
}
