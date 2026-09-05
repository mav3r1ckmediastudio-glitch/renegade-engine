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

    // Gate 8E cross-document parity result. Screen actions remain symbolic
    // Screen-owned identities; Story Flow remains the sole authority for where
    // those outcomes route. The audit never mutates either document.
    struct StoryFlowScreenOutcomeAudit
    {
        bool succeeded = false;
        StoryFlowScreenReferenceCode resolutionCode =
            StoryFlowScreenReferenceCode::InvalidRequest;
        std::string message;

        StableId screenNodeId;
        StableId screenDocumentId;
        std::vector<std::string> actionIds;
        std::vector<std::string> buttonActionIds;
        std::vector<std::string> routedActionIds;
        std::vector<StableId> invalidRouteIds;
        std::vector<std::string> invalidRouteOutcomes;
        std::vector<std::string> unroutedButtonActionIds;
    };

    // Stable Screen resolution/action boundary. Story Flow remains stable-ID
    // authoritative; the stored path is only a repairable hint. Successful
    // resolution exposes the Runtime Screen's authored action IDs so
    // Graph/Journey authoring can present only real Screen outcomes.
    class StoryFlowScreenReferenceService final
    {
    public:
        [[nodiscard]] StoryFlowScreenResolution ResolveScreen(
            const std::string& projectRoot,
            const StableId& projectId,
            const FlowDocument& flow,
            const StableId& screenNodeId) const;

        // Gate 8E validates the relationship without changing it:
        // - every outgoing Screen route must use a declared Screen action;
        // - every action actually referenced by a Button must have at least one
        //   Story Flow route;
        // - multiple conditional/prioritized routes for one valid action stay
        //   legal and are left to FlowInterpreter's existing route semantics.
        [[nodiscard]] StoryFlowScreenOutcomeAudit AuditScreenOutcomes(
            const std::string& projectRoot,
            const StableId& projectId,
            const FlowDocument& flow,
            const StableId& screenNodeId) const;
    };
}
