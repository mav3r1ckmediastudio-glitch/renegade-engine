#pragma once

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/ProjectService.h"

#include <cstddef>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // LP06 Gate 5 project-side preparation for the owner-facing Windows build.
    // This binds the accepted LP05 dependency closure and LC01 stable registry
    // to the same deterministic Story Flow route that the packaged Runtime
    // must reproduce during Gate 4 verification.
    struct WindowsGameBuildProjectState
    {
        DependencyGraph dependencyGraph;
        AssetRegistry assetRegistry;
        std::vector<std::string> expectedFlowTrace;
        std::size_t levelCompletionCount = 0;
    };

    // Refreshes/persists LC01 identity metadata only; creator content is never
    // imported, reimported, moved or rewritten. Existing import provenance is
    // retained so BuildService can fail closed when a source/product snapshot
    // is stale. The current LP06 smoke route advances Level nodes only through
    // the established "level.complete" outcome and must terminate at
    // CompleteGame rather than guessing an alternative gameplay route.
    [[nodiscard]] bool PrepareWindowsGameBuildProjectState(
        const ProjectMetadata& project,
        WindowsGameBuildProjectState& state,
        std::string& error);
}
