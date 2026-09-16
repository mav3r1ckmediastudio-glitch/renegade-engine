#pragma once

#include <cstddef>
#include <string>

#include <WickedEngine.h>

namespace renegade::bridge
{
    struct TestLevelNavigationPreparation
    {
        bool reusedCache = false;
        bool rebuiltCache = false;
        std::size_t gridCount = 0;
        std::string signature;
        std::string cacheDirectory;
    };

    // Prepares native Wicked navigation for a disposable TestGame scene.
    // Persistent cache bytes live under <project>/Intermediate/NavigationCache;
    // the supplied scene is the disposable TestLevel scene, never the live
    // authoring Scene. Existing authored grid identities are retained.
    [[nodiscard]] bool PrepareTestLevelNavigationCache(
        const std::string& projectRoot,
        const std::string& sourceSceneIdentity,
        wi::scene::Scene& scene,
        TestLevelNavigationPreparation& result,
        std::string& error);
}
