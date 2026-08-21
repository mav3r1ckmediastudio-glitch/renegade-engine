#pragma once

#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"

#include <string>

namespace renegade::bridge
{
    enum class StoryFlowLevelCreateCode
    {
        Success,
        InvalidRequest,
        Conflict,
        RenderFailed,
        TransactionFailed,
        VerificationFailed,
    };

    [[nodiscard]] const char* StoryFlowLevelCreateCodeName(
        StoryFlowLevelCreateCode code) noexcept;

    // Gate 4A multi-document creation request. The Flow snapshot is the
    // authoritative editor candidate to persist alongside the new Level. This
    // deliberately means Add New Level is a persistence boundary: any pending
    // Flow edits in the supplied snapshot are committed in the same transaction
    // as the WISCENE and its stable-ID sidecar rather than leaving an orphaned
    // Scene if the application exits before a later Flow save.
    struct StoryFlowNewLevelRequest
    {
        std::string projectRoot;
        StableId projectId;
        std::string flowPath;
        FlowDocument flow;
        std::string levelName;
        std::string scenePathHint;

        // Optional deterministic seams for automated rollback/recovery proof.
        // Production callers normally leave transactionId empty and provide no
        // hook.
        std::string transactionId;
        ProjectDocumentTransactionHook transactionHook;
    };

    struct StoryFlowNewLevelResult
    {
        bool succeeded = false;
        StoryFlowLevelCreateCode code =
            StoryFlowLevelCreateCode::InvalidRequest;
        std::string message;

        StableId levelNodeId;
        StableId sceneDocumentId;
        std::string scenePathHint;
        std::string scenePath;
        FlowDocument committedFlow;

        // Preserved verbatim so callers/tests can surface deterministic LF02
        // transaction evidence or a retained recovery journal when required.
        ProjectDocumentTransactionResult transaction;
    };

    // Gate 4A backend seam for governed Level creation. It creates a blank
    // Wicked WISCENE, a .wiscene.rmeta Scene envelope and the updated Story
    // Flow as one LF02 project transaction. No UI or render-path state lives
    // here; Gate 4B/4C bind this contract into Story Flow and Level Editor
    // lifecycle presentation.
    class StoryFlowLevelLifecycleService final
    {
    public:
        [[nodiscard]] StoryFlowNewLevelResult CreateNewLevel(
            const StoryFlowNewLevelRequest& request) const;
    };
}
