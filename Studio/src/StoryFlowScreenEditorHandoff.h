#pragma once

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScreenLibraryService.h"
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

    // Shared assets deliberately retain an unresolved context until the creator chooses.
    inline std::vector<bridge::StableId> BindLibraryFlowContext(
        const bridge::FlowDocument& flow, StoryFlowScreenEditorHandoff& handoff)
    {
        std::vector<bridge::StableId> contexts;
        for(const auto& node:flow.nodes)
            if(node.kind==bridge::FlowNodeKind::Screen && node.screenDocumentId==handoff.screenDocumentId)
                contexts.push_back(node.id);
        handoff.screenNodeId=contexts.size()==1?contexts.front():bridge::StableId{};
        return contexts;
    }
    // Standalone project Screens and HUDs are editable without inventing a
    // StoryFlow destination node. Explicit routing remains a separate command.
    [[nodiscard]] inline bool BuildStandaloneScreenEditorHandoff(
        const std::string& projectRoot,const bridge::StableId& projectId,
        const bridge::StableId& documentId,const std::string& pathHint,
        StoryFlowScreenEditorHandoff& handoff,std::string& error)
    {
        bridge::ScreenDocument document;std::string path,hint;
        if(!bridge::ResolveProjectScreenLibraryDocument(projectRoot,projectId,documentId,pathHint,
                document,path,hint,error))return false;
        StoryFlowScreenEditorHandoff candidate;candidate.ready=true;
        candidate.screenDocumentId=documentId;candidate.resolvedPath=std::move(path);
        candidate.resolvedPathHint=std::move(hint);candidate.pathHintMoved=candidate.resolvedPathHint!=pathHint;
        for(const auto& action:document.actions)candidate.actionIds.push_back(action.id);
        handoff=std::move(candidate);error.clear();return true;
    }

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
