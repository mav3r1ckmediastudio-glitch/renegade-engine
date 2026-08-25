#pragma once

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/ProjectService.h"

#include <cstddef>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // Project-side preparation for the owner-facing Windows build. Gate 10
    // closes the old LP06 Level-only smoke assumption: the expected trace and
    // smoke outcomes now come from one deterministic path through the actual
    // authored Story Flow, including Screen and Level destinations.
    struct WindowsGameBuildProjectState
    {
        DependencyGraph dependencyGraph;
        AssetRegistry assetRegistry;
        std::vector<std::string> expectedFlowTrace;
        std::vector<std::string> smokeOutcomes;

        // Retained as compatibility/evidence for the accepted LP06 regressions.
        // Runtime smoke execution no longer uses this count to invent repeated
        // "level.complete" outcomes.
        std::size_t levelCompletionCount = 0;
    };

    // Refreshes/persists LC01 identity metadata only; creator content is never
    // imported, reimported, moved or rewritten. Existing import provenance is
    // retained so BuildService can fail closed when a source/product snapshot
    // is stale. Gate 10 derives a bounded, condition-compatible path from the
    // authoritative Story Flow to Complete Game and records the exact authored
    // outcomes that the packaged Runtime smoke must reproduce. A legacy
    // project-level startup Screen is no longer required: Screen dependencies
    // are discovered transitively from Story Flow.
    [[nodiscard]] bool PrepareWindowsGameBuildProjectState(
        const ProjectMetadata& project,
        WindowsGameBuildProjectState& state,
        std::string& error);
}
