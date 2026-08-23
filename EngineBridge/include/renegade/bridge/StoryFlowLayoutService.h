#pragma once

#include "renegade/bridge/StoryFlowAuthoringModel.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* StoryFlowLayoutFormat =
        "renegade-story-flow-layout";
    inline constexpr const char* StoryFlowLayoutExtension =
        ".renegade-flow-layout";

    struct StoryFlowNodeLayout
    {
        StableId nodeId;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct StoryFlowCanvasLayout
    {
        float panX = 0.0f;
        float panY = 0.0f;
        float zoom = 1.0f;
    };

    enum class StoryFlowViewMode
    {
        Journey,
        Graph,
    };

    struct StoryFlowJourneyCardLayout
    {
        StableId nodeId;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        // Gate 9B presentation-only media. This is always a project-relative
        // managed thumbnail path and never participates in Runtime semantics.
        std::string thumbnailPathHint;
    };

    struct StoryFlowLayoutDocument
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 3;

        std::string formatIdentifier = StoryFlowLayoutFormat;
        std::uint32_t schemaVersion = CurrentSchemaVersion;
        StableId projectId;
        StableId flowDocumentId;
        StoryFlowViewMode activeView = StoryFlowViewMode::Journey;
        // `canvas` remains the Graph canvas for source compatibility with the
        // accepted Gate 1-5 presentation code and schema-v1 migration.
        StoryFlowCanvasLayout canvas;
        StoryFlowCanvasLayout journeyCanvas;
        std::vector<StoryFlowNodeLayout> nodes;
        std::vector<StoryFlowJourneyCardLayout> journeyCards;
    };

    [[nodiscard]] const char* StoryFlowViewModeName(
        StoryFlowViewMode mode) noexcept;

    [[nodiscard]] std::string ResolveStoryFlowLayoutPath(
        const std::string& projectRoot,
        const StableId& flowDocumentId);

    [[nodiscard]] StoryFlowLayoutDocument BuildDeterministicStoryFlowLayout(
        const StoryFlowAuthoringModel& model,
        const StableId& projectId,
        const StableId& flowDocumentId);

    // Reconcile saved presentation state against the currently authoritative
    // semantic Flow. Unknown/stale node layout records are discarded and
    // missing nodes receive deterministic defaults. This function never
    // modifies the StoryFlowAuthoringModel or its FlowDocument.
    [[nodiscard]] bool ReconcileStoryFlowLayout(
        const StoryFlowAuthoringModel& model,
        const StableId& projectId,
        const StableId& flowDocumentId,
        StoryFlowLayoutDocument& layout,
        std::string& error);

    [[nodiscard]] bool ValidateStoryFlowLayout(
        const StoryFlowLayoutDocument& layout,
        const StableId& expectedProjectId,
        const StableId& expectedFlowDocumentId,
        std::string& error);

    [[nodiscard]] bool WriteStoryFlowLayout(
        const std::string& filePath,
        const StoryFlowLayoutDocument& layout,
        std::string& error);

    [[nodiscard]] bool ReadStoryFlowLayout(
        const std::string& filePath,
        const StableId& expectedProjectId,
        const StableId& expectedFlowDocumentId,
        StoryFlowLayoutDocument& layout,
        std::string& error);
}
