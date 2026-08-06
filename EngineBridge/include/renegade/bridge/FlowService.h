#pragma once

#include "renegade/bridge/IdentityService.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace renegade::bridge
{
    inline constexpr const char* StoryFlowDocumentType = "story-flow";
    inline constexpr const char* GameStartOutcome = "renegade.flow.start";
    inline constexpr const char* SceneDocumentType = "scene";

    enum class FlowNodeKind
    {
        GameStart,
        Level,
        CompleteGame,
        ReturnToMainMenu,
        Quit,
    };

    enum class FlowConditionOperator
    {
        Equals,
        NotEquals,
        Exists,
        Missing,
    };

    struct FlowCondition
    {
        std::string key;
        FlowConditionOperator operation = FlowConditionOperator::Equals;
        std::string value;
    };

    struct FlowNode
    {
        StableId id;
        FlowNodeKind kind = FlowNodeKind::Level;
        std::string name;

        // Level nodes use a stable asset identity as authority. The path is a
        // current project-relative resolution hint and is never a route
        // destination or durable identity by itself.
        StableId sceneAssetId;
        std::string scenePathHint;
    };

    struct FlowRoute
    {
        StableId id;
        StableId sourceNodeId;
        std::string outcome;
        StableId destinationNodeId;
        std::string destinationEntry;
        int priority = 0;
        std::vector<FlowCondition> conditions;
    };

    struct FlowDocument
    {
        DocumentEnvelope envelope;
        StableId startNodeId;
        std::vector<FlowNode> nodes;
        std::vector<FlowRoute> routes;
    };

    [[nodiscard]] const char* FlowNodeKindName(FlowNodeKind kind) noexcept;
    [[nodiscard]] const char* FlowConditionOperatorName(
        FlowConditionOperator operation) noexcept;

    [[nodiscard]] bool ValidateFlowDocument(
        const FlowDocument& document,
        const StableId& expectedProjectId,
        std::string& error);
    [[nodiscard]] bool WriteFlowDocument(
        const std::string& filePath,
        const FlowDocument& document,
        std::string& error);
    [[nodiscard]] bool ReadFlowDocument(
        const std::string& filePath,
        const StableId& expectedProjectId,
        FlowDocument& document,
        std::string& error);

    // Bounded LP02 document-reference resolution. Stable IDs are authoritative;
    // path hints are tried first for speed, then a deterministic project-local
    // scan repairs a stale hint after rename/move. The later Asset Registry may
    // replace the linear scan without changing the serialized reference.
    [[nodiscard]] bool ResolveStoryFlowDocumentPath(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        const StableId& documentId,
        const std::string& pathHint,
        std::string& resolvedPath,
        std::string& error);
    [[nodiscard]] bool ResolveSceneDocumentPath(
        const std::string& projectRoot,
        const StableId& expectedProjectId,
        const StableId& documentId,
        const std::string& pathHint,
        std::string& resolvedPath,
        std::string& error);

    enum class FlowStepCode
    {
        Success,
        NotStarted,
        InvalidOutcome,
        MissingRoute,
        AmbiguousRoute,
    };

    enum class FlowTerminalAction
    {
        None,
        CompleteGame,
        ReturnToMainMenu,
        Quit,
    };

    [[nodiscard]] const char* FlowStepCodeName(FlowStepCode code) noexcept;
    [[nodiscard]] const char* FlowTerminalActionName(
        FlowTerminalAction action) noexcept;

    struct FlowStepResult
    {
        bool succeeded = false;
        FlowStepCode code = FlowStepCode::NotStarted;
        std::string message;
        StableId previousNodeId;
        StableId currentNodeId;
        std::string currentNodeName;
        FlowNodeKind currentNodeKind = FlowNodeKind::GameStart;
        StableId routeId;
        std::string outcome;
        StableId sceneAssetId;
        std::string scenePathHint;
        std::string destinationEntry;
        FlowTerminalAction terminalAction = FlowTerminalAction::None;
    };

    class FlowInterpreter
    {
    public:
        [[nodiscard]] bool Initialize(
            FlowDocument document,
            std::string& error);

        [[nodiscard]] bool SetState(
            std::string key,
            std::string value,
            std::string& error);
        void RemoveState(const std::string& key);
        void ClearState() noexcept;

        [[nodiscard]] FlowStepResult Start();
        [[nodiscard]] FlowStepResult EmitOutcome(const std::string& outcome);

        [[nodiscard]] const FlowDocument& Document() const noexcept;
        [[nodiscard]] const FlowNode* CurrentNode() const noexcept;
        [[nodiscard]] bool HasStarted() const noexcept;

    private:
        [[nodiscard]] const FlowNode* FindNode(const StableId& id) const noexcept;
        [[nodiscard]] bool ConditionsMatch(const FlowRoute& route) const;
        [[nodiscard]] FlowStepResult EnterNode(
            const FlowNode& node,
            const FlowRoute* route,
            std::string outcome,
            StableId previousNodeId);

        FlowDocument document_;
        std::unordered_map<std::string, std::string> state_;
        StableId currentNodeId_;
        bool initialized_ = false;
        bool started_ = false;
    };
}
