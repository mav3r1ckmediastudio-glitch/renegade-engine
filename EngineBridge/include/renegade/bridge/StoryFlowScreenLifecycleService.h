#pragma once

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class StoryFlowScreenTemplate
    {
        Custom,
        Title,
        Loading,
        Options,
        Pause,
        Failure,
        Victory,
        SaveLoad,
        Credits,
    };

    [[nodiscard]] const char* StoryFlowScreenTemplateName(
        StoryFlowScreenTemplate value) noexcept;

    enum class StoryFlowScreenCreateCode
    {
        Success,
        InvalidRequest,
        Conflict,
        RenderFailed,
        TransactionFailed,
        VerificationFailed,
    };

    [[nodiscard]] const char* StoryFlowScreenCreateCodeName(
        StoryFlowScreenCreateCode code) noexcept;

    // Gate 5A governed Screen creation request. The supplied Flow snapshot is
    // committed atomically with the new runtime-screen document, so Add Screen
    // cannot leave either an orphan Screen or a dangling Flow relationship.
    struct StoryFlowNewScreenRequest
    {
        std::string projectRoot;
        StableId projectId;
        std::string flowPath;
        FlowDocument flow;
        std::string screenName;
        std::string screenPathHint;
        StoryFlowScreenTemplate screenTemplate = StoryFlowScreenTemplate::Custom;

        // Optional deterministic seams for rollback/recovery proof.
        std::string transactionId;
        ProjectDocumentTransactionHook transactionHook;
    };

    struct StoryFlowNewScreenResult
    {
        bool succeeded = false;
        StoryFlowScreenCreateCode code = StoryFlowScreenCreateCode::InvalidRequest;
        std::string message;

        StableId screenNodeId;
        StableId screenDocumentId;
        std::string screenPathHint;
        std::string screenPath;
        std::vector<std::string> actionIds;
        FlowDocument committedFlow;
        ProjectDocumentTransactionResult transaction;
    };

    // Gate 5A backend seam. Templates are one-time creator scaffolds over the
    // existing LP03 runtime-screen document contract; they are not separate
    // document types or runtime architectures.
    class StoryFlowScreenLifecycleService final
    {
    public:
        [[nodiscard]] StoryFlowNewScreenResult CreateNewScreen(
            const StoryFlowNewScreenRequest& request) const;
    };
}
