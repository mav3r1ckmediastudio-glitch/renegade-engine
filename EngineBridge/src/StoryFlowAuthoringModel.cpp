#include "renegade/bridge/StoryFlowAuthoringModel.h"

#include <algorithm>
#include <deque>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        int NodeKindRank(const FlowNodeKind kind) noexcept
        {
            switch (kind)
            {
            case FlowNodeKind::GameStart:
                return 0;
            case FlowNodeKind::Level:
                return 1;
            case FlowNodeKind::CompleteGame:
                return 2;
            case FlowNodeKind::ReturnToMainMenu:
                return 3;
            case FlowNodeKind::Quit:
                return 4;
            default:
                return 5;
            }
        }

        bool RouteLess(const FlowRoute* left, const FlowRoute* right)
        {
            return std::tie(
                left->priority,
                left->outcome,
                left->destinationNodeId,
                left->id) <
                std::tie(
                    right->priority,
                    right->outcome,
                    right->destinationNodeId,
                    right->id);
        }
    }

    bool StoryFlowAuthoringModel::Load(
        FlowDocument document,
        const StableId& expectedProjectId,
        std::string& error)
    {
        Clear();
        if (!ValidateFlowDocument(document, expectedProjectId, error))
        {
            return false;
        }

        document_ = std::move(document);

        std::unordered_map<StableId, const FlowNode*> flowNodes;
        flowNodes.reserve(document_.nodes.size());
        for (const auto& node : document_.nodes)
        {
            flowNodes.emplace(node.id, &node);
        }

        std::unordered_map<StableId, std::vector<const FlowRoute*>> outgoing;
        outgoing.reserve(document_.nodes.size());
        for (const auto& route : document_.routes)
        {
            outgoing[route.sourceNodeId].push_back(&route);
        }
        for (auto& item : outgoing)
        {
            std::sort(item.second.begin(), item.second.end(), RouteLess);
        }

        constexpr std::size_t Unreached =
            std::numeric_limits<std::size_t>::max();
        std::unordered_map<StableId, std::size_t> depthByNode;
        depthByNode.reserve(document_.nodes.size());
        for (const auto& node : document_.nodes)
        {
            depthByNode.emplace(node.id, Unreached);
        }

        std::deque<StableId> pending;
        depthByNode[document_.startNodeId] = 0;
        pending.push_back(document_.startNodeId);
        std::size_t maximumReachableDepth = 0;
        while (!pending.empty())
        {
            const StableId current = std::move(pending.front());
            pending.pop_front();
            const std::size_t currentDepth = depthByNode[current];
            maximumReachableDepth = std::max(maximumReachableDepth, currentDepth);

            const auto routes = outgoing.find(current);
            if (routes == outgoing.end())
            {
                continue;
            }
            for (const auto* route : routes->second)
            {
                auto destination = depthByNode.find(route->destinationNodeId);
                if (destination == depthByNode.end() ||
                    destination->second != Unreached)
                {
                    continue;
                }
                destination->second = currentDepth + 1;
                pending.push_back(route->destinationNodeId);
            }
        }

        std::vector<const FlowNode*> orderedNodes;
        orderedNodes.reserve(document_.nodes.size());
        for (const auto& node : document_.nodes)
        {
            orderedNodes.push_back(&node);
        }
        std::sort(
            orderedNodes.begin(),
            orderedNodes.end(),
            [&](const FlowNode* left, const FlowNode* right)
            {
                const std::size_t leftDepth = depthByNode[left->id];
                const std::size_t rightDepth = depthByNode[right->id];
                const bool leftReachable = leftDepth != Unreached;
                const bool rightReachable = rightDepth != Unreached;
                const std::size_t leftColumn = leftReachable
                    ? leftDepth
                    : maximumReachableDepth + 1;
                const std::size_t rightColumn = rightReachable
                    ? rightDepth
                    : maximumReachableDepth + 1;
                return std::tuple{
                           !leftReachable,
                           leftColumn,
                           NodeKindRank(left->kind),
                           left->name,
                           left->id} <
                    std::tuple{
                           !rightReachable,
                           rightColumn,
                           NodeKindRank(right->kind),
                           right->name,
                           right->id};
            });

        std::unordered_map<std::size_t, std::size_t> nextRowByColumn;
        nodes_.reserve(orderedNodes.size());
        for (const auto* node : orderedNodes)
        {
            const std::size_t depth = depthByNode[node->id];
            const bool reachable = depth != Unreached;
            const std::size_t column = reachable
                ? depth
                : maximumReachableDepth + 1;

            StoryFlowNodeView view;
            view.id = node->id;
            view.kind = node->kind;
            view.name = node->name;
            view.sceneAssetId = node->sceneAssetId;
            view.scenePathHint = node->scenePathHint;
            view.reachableFromStart = reachable;
            view.presentationColumn = column;
            view.presentationRow = nextRowByColumn[column]++;
            nodeIndexById_.emplace(view.id, nodes_.size());
            nodes_.push_back(std::move(view));

            if (!reachable)
            {
                diagnostics_.push_back({
                    StoryFlowDiagnosticSeverity::Warning,
                    "flow.unreachable_node",
                    "Story Flow node '" + node->name +
                        "' is not reachable from Game Start.",
                    node->id,
                    {},
                });
            }
        }

        std::vector<const FlowRoute*> orderedRoutes;
        orderedRoutes.reserve(document_.routes.size());
        for (const auto& route : document_.routes)
        {
            orderedRoutes.push_back(&route);
        }
        std::sort(
            orderedRoutes.begin(),
            orderedRoutes.end(),
            [&](const FlowRoute* left, const FlowRoute* right)
            {
                const std::size_t leftSource = nodeIndexById_.at(left->sourceNodeId);
                const std::size_t rightSource = nodeIndexById_.at(right->sourceNodeId);
                const std::size_t leftDestination =
                    nodeIndexById_.at(left->destinationNodeId);
                const std::size_t rightDestination =
                    nodeIndexById_.at(right->destinationNodeId);
                return std::tie(
                           leftSource,
                           left->priority,
                           left->outcome,
                           leftDestination,
                           left->id) <
                    std::tie(
                           rightSource,
                           right->priority,
                           right->outcome,
                           rightDestination,
                           right->id);
            });

        routes_.reserve(orderedRoutes.size());
        for (const auto* route : orderedRoutes)
        {
            StoryFlowRouteView view;
            view.id = route->id;
            view.sourceNodeId = route->sourceNodeId;
            view.outcome = route->outcome;
            view.destinationNodeId = route->destinationNodeId;
            view.destinationEntry = route->destinationEntry;
            view.priority = route->priority;
            view.conditionCount = route->conditions.size();
            routeIndexById_.emplace(view.id, routes_.size());
            routes_.push_back(std::move(view));

            nodes_[nodeIndexById_.at(route->sourceNodeId)]
                .outgoingRouteIds.push_back(route->id);
            nodes_[nodeIndexById_.at(route->destinationNodeId)]
                .incomingRouteIds.push_back(route->id);
        }

        loaded_ = true;
        error.clear();
        return true;
    }

    void StoryFlowAuthoringModel::Clear() noexcept
    {
        document_ = {};
        nodes_.clear();
        routes_.clear();
        diagnostics_.clear();
        nodeIndexById_.clear();
        routeIndexById_.clear();
        loaded_ = false;
    }

    const StoryFlowNodeView* StoryFlowAuthoringModel::FindNode(
        const StableId& id) const noexcept
    {
        const auto found = nodeIndexById_.find(id);
        return found == nodeIndexById_.end()
            ? nullptr
            : &nodes_[found->second];
    }

    const StoryFlowRouteView* StoryFlowAuthoringModel::FindRoute(
        const StableId& id) const noexcept
    {
        const auto found = routeIndexById_.find(id);
        return found == routeIndexById_.end()
            ? nullptr
            : &routes_[found->second];
    }
}
