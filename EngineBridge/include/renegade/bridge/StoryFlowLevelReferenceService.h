#pragma once

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <string>

namespace renegade::bridge
{
    enum class StoryFlowLevelReferenceCode
    {
        Success,
        InvalidRequest,
        Missing,
        Conflict,
        WrongProject,
        WrongDocumentType,
        InvalidScene,
        TransactionFailed,
        VerificationFailed,
    };

    [[nodiscard]] const char* StoryFlowLevelReferenceCodeName(
        StoryFlowLevelReferenceCode code) noexcept;

    struct StoryFlowExistingLevelRequest
    {
        std::string projectRoot;
        StableId projectId;
        std::string flowPath;
        FlowDocument flow;
        std::string levelName;
        std::string scenePath;
        std::string transactionId;
        ProjectDocumentTransactionHook transactionHook;
    };

    struct StoryFlowExistingLevelResult
    {
        bool succeeded = false;
        StoryFlowLevelReferenceCode code =
            StoryFlowLevelReferenceCode::InvalidRequest;
        std::string message;
        StableId levelNodeId;
        StableId sceneDocumentId;
        std::string scenePath;
        std::string scenePathHint;
        bool createdMetadata = false;
        FlowDocument committedFlow;
        ProjectDocumentTransactionResult transaction;
    };

    struct StoryFlowLevelResolution
    {
        bool succeeded = false;
        StoryFlowLevelReferenceCode code =
            StoryFlowLevelReferenceCode::InvalidRequest;
        std::string message;
        StableId levelNodeId;
        StableId sceneDocumentId;
        std::string requestedPathHint;
        std::string resolvedPath;
        std::string resolvedPathHint;
        bool pathHintMoved = false;
    };

    // Gate 4B adoption/resolution boundary. Existing WISCENE files are never
    // trusted by path alone: a valid Scene document identity is either adopted
    // or created, then the Story Flow reference is committed transactionally.
    // Resolution always treats the Scene document ID as authority and reports a
    // moved hint without silently changing runtime semantics.
    class StoryFlowLevelReferenceService final
    {
    public:
        [[nodiscard]] StoryFlowExistingLevelResult AddExistingLevel(
            const StoryFlowExistingLevelRequest& request) const;

        [[nodiscard]] StoryFlowLevelResolution ResolveLevel(
            const std::string& projectRoot,
            const StableId& projectId,
            const FlowDocument& flow,
            const StableId& levelNodeId) const;
    };
}
