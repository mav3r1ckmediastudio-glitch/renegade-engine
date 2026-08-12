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

        result.entityCount = scenes.EntityCount();
        error.clear();
        return true;
    }
}
