#pragma once

#include "renegade/bridge/FlowService.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace renegade::bridge
{
    enum class StoryFlowDiagnosticSeverity
    {
        Info,
        Warning,
        Error,
    };

    struct StoryFlowDiagnostic
    {
        StoryFlowDiagnosticSeverity severity = StoryFlowDiagnosticSeverity::Info;
        std::string code;
        std::string message;
        StableId nodeId;
        StableId routeId;
    };

    struct StoryFlowNodeView
    {
        StableId id;
        FlowNodeKind kind = FlowNodeKind::Level;
        std::string name;
        StableId sceneAssetId;
        std::string scenePathHint;
        StableId screenDocumentId;
        std::string screenPathHint;
        bool reachableFromStart = false;
        std::size_t presentationColumn = 0;
        std::size_t presentationRow = 0;
        std::vector<StableId> incomingRouteIds;
        std::vector<StableId> outgoingRouteIds;
    };

    struct StoryFlowRouteView
    {
        StableId id;
        StableId sourceNodeId;
        std::string outcome;
        StableId destinationNodeId;
        std::string destinationEntry;
        int priority = 0;
        std::size_t conditionCount = 0;
    };

    // Shared authoring boundary. It owns a validated FlowDocument and derives
    // deterministic presentation metadata without duplicating Runtime semantics.
    // Journey View and Graph View must consume this same model.
    class StoryFlowAuthoringModel
    {
    public:
        [[nodiscard]] bool Load(
            FlowDocument document,
            const StableId& expectedProjectId,
            std::string& error);

        void Clear() noexcept;

        [[nodiscard]] bool IsLoaded() const noexcept
        {
            return loaded_;
        }

        [[nodiscard]] const FlowDocument& Document() const noexcept
        {
            return document_;
        }

        [[nodiscard]] const std::vector<StoryFlowNodeView>& Nodes() const noexcept
        {
            return nodes_;
        }

        [[nodiscard]] const std::vector<StoryFlowRouteView>& Routes() const noexcept
        {
            return routes_;
        }

        [[nodiscard]] const std::vector<StoryFlowDiagnostic>& Diagnostics() const noexcept
        {
            return diagnostics_;
        }

        [[nodiscard]] const StoryFlowNodeView* FindNode(
            const StableId& id) const noexcept;
        [[nodiscard]] const StoryFlowRouteView* FindRoute(
            const StableId& id) const noexcept;

        [[nodiscard]] const StableId& GameStartNodeId() const noexcept
        {
            return document_.startNodeId;
        }

    private:
        FlowDocument document_;
        std::vector<StoryFlowNodeView> nodes_;
        std::vector<StoryFlowRouteView> routes_;
        std::vector<StoryFlowDiagnostic> diagnostics_;
        std::unordered_map<StableId, std::size_t> nodeIndexById_;
        std::unordered_map<StableId, std::size_t> routeIndexById_;
        bool loaded_ = false;
    };
}
