#pragma once

#include "renegade/bridge/StoryFlowScreenReferenceService.h"

#include <string>
#include <utility>
#include <vector>

namespace renegade::studio
{
    // Gate 5 stops at this boundary. Gate 8's visual Screen Editor consumes
    // this stable, fully-resolved handoff rather than re-resolving a path or
    // inventing editor-only Screen semantics.
    struct StoryFlowScreenEditorHandoff
    {
        bool ready = false;
        bridge::StableId screenNodeId;
        bridge::StableId screenDocumentId;
        std::string resolvedPath;
        std::string resolvedPathHint;
        bool pathHintMoved = false;
        std::vector<std::string> actionIds;
    };

    [[nodiscard]] inline bool BuildStoryFlowScreenEditorHandoff(
        const bridge::StoryFlowScreenResolution& resolution,
        StoryFlowScreenEditorHandoff& handoff,
        std::string& error)
    {
        handoff = {};
        if (!resolution.succeeded)
        {
            error = resolution.message.empty()
                ? "The Screen could not be resolved for editor handoff."
                : resolution.message;
            return false;
        }
        if (!bridge::IsValidStableId(resolution.screenNodeId) ||
            !bridge::IsValidStableId(resolution.screenDocumentId) ||
            resolution.resolvedPath.empty())
        {
            error = "The resolved Screen is missing stable editor-handoff identity.";
            return false;
        }

        handoff.ready = true;
        handoff.screenNodeId = resolution.screenNodeId;
        handoff.screenDocumentId = resolution.screenDocumentId;
        handoff.resolvedPath = resolution.resolvedPath;
        handoff.resolvedPathHint = resolution.resolvedPathHint;
        handoff.pathHintMoved = resolution.pathHintMoved;
        handoff.actionIds = resolution.actionIds;
        error.clear();
        return true;
    }
}
