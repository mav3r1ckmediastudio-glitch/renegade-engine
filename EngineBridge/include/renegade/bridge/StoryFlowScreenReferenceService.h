#pragma once

#include "renegade/bridge/FlowService.h"

#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class StoryFlowScreenReferenceCode
    {
        Success,
        InvalidRequest,
        Missing,
        Conflict,
        WrongProject,
        WrongDocumentType,
        InvalidScreen,
    };

    [[nodiscard]] const char* StoryFlowScreenReferenceCodeName(
        StoryFlowScreenReferenceCode code) noexcept;

    struct StoryFlowScreenResolution
    {
        bool succeeded = false;
        StoryFlowScreenReferenceCode code =
            StoryFlowScreenReferenceCode::InvalidRequest;
        std::string message;

        StableId screenNodeId;
        StableId screenDocumentId;
        std::string requestedPathHint;
        std::string resolvedPath;
        std::string resolvedPathHint;
        bool pathHintMoved = false;
        std::vector<std::string> actionIds;
    };

    // Gate 5B stable Screen resolution/action boundary. Story Flow remains
    // stable-ID authoritative; the stored path is only a repairable hint.
    // Successful resolution also exposes the Runtime Screen's authored action
    // IDs so Graph/Journey authoring can present only real Screen outcomes.
    class StoryFlowScreenReferenceService final
    {
    public:
        [[nodiscard]] StoryFlowScreenResolution ResolveScreen(
            const std::string& projectRoot,
            const StableId& projectId,
            const FlowDocument& flow,
            const StableId& screenNodeId) const;
    };
}
