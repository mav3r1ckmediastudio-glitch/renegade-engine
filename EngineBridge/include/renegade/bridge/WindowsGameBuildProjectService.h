#pragma once

#include "renegade/bridge/AssetRegistryService.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ProjectService.h"

#include <cstddef>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // Renegade-owned files that live beside Studio rather than inside a
    // creator project. The exact source file is admitted only when the same
    // declaration is also supplied to the standalone package as hashed
    // Runtime support at destinationPath. This is deliberately an exact-file
    // allowlist; arbitrary dependencies outside the project remain fatal.
    struct WindowsGameBundledResource
    {
        std::string logicalName;
        std::string sourcePath;
        std::string destinationPath;
    };

    struct StoryFlowRuntimeRoute
    {
        std::vector<std::string> trace;
        std::vector<std::string> outcomes;
        std::size_t levelCompletionCount = 0;
    };

    // Shared Runtime/build readiness authority. Studio validates the live
    // in-memory Flow through this function; Build Windows Game validates the
    // saved Flow through the same deterministic traversal before packaging.
    [[nodiscard]] bool ResolveStoryFlowRuntimeRoute(
        const FlowDocument& document,
        StoryFlowRuntimeRoute& route,
        std::string& error);

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
        std::vector<WindowsGameBundledResource> bundledResources;
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

    [[nodiscard]] bool PrepareWindowsGameBuildProjectState(
        const ProjectMetadata& project,
        const std::vector<WindowsGameBundledResource>& bundledResources,
        WindowsGameBuildProjectState& state,
        std::string& error);
}
