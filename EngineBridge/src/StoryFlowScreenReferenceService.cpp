#include "renegade/bridge/StoryFlowScreenReferenceService.h"

#include "renegade/bridge/ScreenService.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    const FlowNode* FindNode(const FlowDocument& document, const StableId& nodeId)
    {
        const auto found = std::find_if(document.nodes.begin(), document.nodes.end(),
            [&nodeId](const FlowNode& node) { return node.id == nodeId; });
        return found == document.nodes.end() ? nullptr : &*found;
    }

    bool IsInsideRoot(const fs::path& root, const fs::path& candidate)
    {
        const fs::path relative = candidate.lexically_relative(root);
        if (relative.empty() || relative.is_absolute() ||
            relative.has_root_name() || relative.has_root_directory())
        {
            return false;
        }
        return std::none_of(relative.begin(), relative.end(),
            [](const fs::path& part) { return part == ".."; });
    }

    StoryFlowScreenReferenceCode ClassifyResolutionFailure(const std::string& error)
    {
        if (error.find("different project") != std::string::npos)
            return StoryFlowScreenReferenceCode::WrongProject;
        if (error.find("wrong document type") != std::string::npos)
            return StoryFlowScreenReferenceCode::WrongDocumentType;
        if (error.find("ambiguous") != std::string::npos)
            return StoryFlowScreenReferenceCode::Conflict;
        return StoryFlowScreenReferenceCode::Missing;
    }

    template <typename Value>
    void AddUnique(std::vector<Value>& values, const Value& value)
    {
        if (std::find(values.begin(), values.end(), value) == values.end())
            values.push_back(value);
    }

    std::string Join(const std::vector<std::string>& values)
    {
        std::ostringstream message;
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index != 0) message << ", ";
            message << values[index];
        }
        return message.str();
    }
}

namespace renegade::bridge
{
    const char* StoryFlowScreenReferenceCodeName(
        const StoryFlowScreenReferenceCode code) noexcept
    {
        switch (code)
        {
        case StoryFlowScreenReferenceCode::Success: return "success";
        case StoryFlowScreenReferenceCode::InvalidRequest: return "invalid_request";
        case StoryFlowScreenReferenceCode::Missing: return "missing";
        case StoryFlowScreenReferenceCode::Conflict: return "conflict";
        case StoryFlowScreenReferenceCode::WrongProject: return "wrong_project";
        case StoryFlowScreenReferenceCode::WrongDocumentType: return "wrong_document_type";
        case StoryFlowScreenReferenceCode::InvalidScreen: return "invalid_screen";
        default: return "unknown";
        }
    }

    StoryFlowScreenResolution StoryFlowScreenReferenceService::ResolveScreen(
        const std::string& projectRoot,
        const StableId& projectId,
        const FlowDocument& flow,
        const StableId& screenNodeId) const
    {
        StoryFlowScreenResolution result;
        result.screenNodeId = screenNodeId;

        if (!IsValidStableId(projectId) || projectRoot.empty() ||
            !IsValidStableId(screenNodeId))
        {
            result.message =
                "Screen resolution requires an active project and stable Screen node ID.";
            return result;
        }

        std::string validationError;
        if (!ValidateFlowDocument(flow, projectId, validationError))
        {
            result.message = "Story Flow is invalid: " + validationError;
            return result;
        }

        const FlowNode* screenNode = FindNode(flow, screenNodeId);
        if (!screenNode || screenNode->kind != FlowNodeKind::Screen)
        {
            result.message = "The selected Story Flow node is not a Screen.";
            return result;
        }

        result.screenDocumentId = screenNode->screenDocumentId;
        result.requestedPathHint = screenNode->screenPathHint;

        std::string resolved;
        std::string error;
        if (!ResolveRuntimeScreenDocumentPath(
                projectRoot,
                projectId,
                screenNode->screenDocumentId,
                screenNode->screenPathHint,
                resolved,
                error))
        {
            result.code = ClassifyResolutionFailure(error);
            result.message = error;
            return result;
        }

        std::error_code rootError;
        std::error_code pathError;
        const fs::path root =
            fs::weakly_canonical(fs::u8path(projectRoot), rootError);
        const fs::path path =
            fs::weakly_canonical(fs::u8path(resolved), pathError);
        if (rootError || pathError || !IsInsideRoot(root, path))
        {
            result.code = StoryFlowScreenReferenceCode::Conflict;
            result.message = "Resolved Screen escaped the active project root.";
            return result;
        }

        ScreenDocument screen;
        if (!ReadScreenDocument(path.generic_u8string(), projectId, screen, error))
        {
            result.code = StoryFlowScreenReferenceCode::InvalidScreen;
            result.message = "Resolved Runtime Screen is invalid: " + error;
            return result;
        }
        if (screen.envelope.documentId != screenNode->screenDocumentId)
        {
            result.code = StoryFlowScreenReferenceCode::Conflict;
            result.message =
                "Resolved Runtime Screen changed stable document identity.";
            return result;
        }

        result.resolvedPath = path.generic_u8string();
        result.resolvedPathHint = path.lexically_relative(root).generic_u8string();
        result.pathHintMoved =
            fs::u8path(result.requestedPathHint).lexically_normal() !=
            fs::u8path(result.resolvedPathHint).lexically_normal();

        result.actionIds.reserve(screen.actions.size());
        for (const auto& action : screen.actions)
            result.actionIds.push_back(action.id);

        result.succeeded = true;
        result.code = StoryFlowScreenReferenceCode::Success;
        result.message = result.pathHintMoved
            ? "Screen resolved by stable document identity after a move; the stored path hint is stale."
            : "Screen resolved by stable document identity.";
        return result;
    }

    StoryFlowScreenOutcomeAudit StoryFlowScreenReferenceService::AuditScreenOutcomes(
        const std::string& projectRoot,
        const StableId& projectId,
        const FlowDocument& flow,
        const StableId& screenNodeId) const
    {
        StoryFlowScreenOutcomeAudit audit;
        audit.screenNodeId = screenNodeId;

        const auto resolution = ResolveScreen(
            projectRoot, projectId, flow, screenNodeId);
        audit.resolutionCode = resolution.code;
        audit.screenDocumentId = resolution.screenDocumentId;
        audit.actionIds = resolution.actionIds;
        if (!resolution.succeeded)
        {
            audit.message = resolution.message;
            return audit;
        }

        ScreenDocument screen;
        std::string error;
        if (!ReadScreenDocument(
                resolution.resolvedPath, projectId, screen, error))
        {
            audit.resolutionCode = StoryFlowScreenReferenceCode::InvalidScreen;
            audit.message = "Resolved Runtime Screen is invalid: " + error;
            return audit;
        }

        std::unordered_set<std::string> declaredActions;
        declaredActions.reserve(screen.actions.size());
        for (const auto& action : screen.actions)
            declaredActions.insert(action.id);

        for (const auto& widget : screen.widgets)
        {
            if (widget.kind == ScreenWidgetKind::Button)
                AddUnique(audit.buttonActionIds, widget.actionId);
        }

        for (const auto& route : flow.routes)
        {
            if (route.sourceNodeId != screenNodeId) continue;
            if (declaredActions.count(route.outcome) == 0)
            {
                audit.invalidRouteIds.push_back(route.id);
                audit.invalidRouteOutcomes.push_back(route.outcome);
                continue;
            }
            AddUnique(audit.routedActionIds, route.outcome);
        }

        for (const auto& actionId : audit.buttonActionIds)
        {
            if (std::find(
                    audit.routedActionIds.begin(),
                    audit.routedActionIds.end(),
                    actionId) == audit.routedActionIds.end())
            {
                audit.unroutedButtonActionIds.push_back(actionId);
            }
        }

        if (!audit.invalidRouteOutcomes.empty() ||
            !audit.unroutedButtonActionIds.empty())
        {
            std::ostringstream message;
            if (!audit.invalidRouteOutcomes.empty())
            {
                message << "Story Flow routes use Screen outcomes that are no longer authored: "
                        << Join(audit.invalidRouteOutcomes) << '.';
            }
            if (!audit.unroutedButtonActionIds.empty())
            {
                if (message.tellp() > 0) message << ' ';
                message << "Button actions have no Story Flow destination: "
                        << Join(audit.unroutedButtonActionIds) << '.';
            }
            audit.message = message.str();
            return audit;
        }

        audit.succeeded = true;
        audit.resolutionCode = StoryFlowScreenReferenceCode::Success;
        audit.message =
            "Screen Button actions and Story Flow outcomes are synchronized.";
        return audit;
    }
}
