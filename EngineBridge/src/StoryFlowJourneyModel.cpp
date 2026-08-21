#include "renegade/bridge/StoryFlowJourneyModel.h"

#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace renegade::bridge
{
    bool StoryFlowJourneyModel::Build(
        const StoryFlowAuthoringModel& model,
        std::string& error)
    {
        Clear();
        if (!model.IsLoaded())
        {
            error = "Journey View requires a loaded Story Flow authoring model.";
            return false;
        }

        struct PendingTrack
        {
            StableId seedNodeId;
            StableId sourceRouteId;
            bool mainTrack = false;
            bool detached = false;
            std::size_t startColumn = 0;
        };

        std::deque<PendingTrack> pending;
        std::unordered_set<StableId> assigned;
        std::unordered_set<StableId> queued;
        std::unordered_map<StableId, StableId> primaryRouteBySource;
        pending.push_back({model.GameStartNodeId(), {}, true, false, 0});
        queued.insert(model.GameStartNodeId());

        const auto enqueueBranch = [&pending, &queued, &assigned](
            const StableId& destinationNodeId,
            const StableId& routeId,
            const std::size_t startColumn,
            const bool detached)
        {
            if (assigned.find(destinationNodeId) != assigned.end() ||
                !queued.insert(destinationNodeId).second)
            {
                return;
            }
            pending.push_back({
                destinationNodeId, routeId, false, detached, startColumn});
        };

        const auto buildPendingTrack = [&](PendingTrack next)
        {
            if (assigned.find(next.seedNodeId) != assigned.end()) return;

            StoryFlowJourneyTrack track;
            track.index = tracks_.size();
            track.mainTrack = next.mainTrack;
            track.detached = next.detached;
            track.startColumn = next.startColumn;
            track.sourceRouteId = std::move(next.sourceRouteId);

            StableId currentNodeId = std::move(next.seedNodeId);
            while (!currentNodeId.empty() &&
                assigned.find(currentNodeId) == assigned.end())
            {
                const StoryFlowNodeView* node = model.FindNode(currentNodeId);
                if (!node) break;

                assigned.insert(currentNodeId);
                queued.insert(currentNodeId);
                StoryFlowJourneyCard card;
                card.nodeId = currentNodeId;
                card.trackIndex = track.index;
                card.sequenceIndex = track.cardNodeIds.size();
                card.columnIndex = track.startColumn + card.sequenceIndex;
                card.reachableFromStart = node->reachableFromStart;
                cardIndexByNodeId_.emplace(card.nodeId, cards_.size());
                cards_.push_back(card);
                track.cardNodeIds.push_back(currentNodeId);

                if (node->outgoingRouteIds.empty()) break;

                const StoryFlowRouteView* primary =
                    model.FindRoute(node->outgoingRouteIds.front());
                if (primary)
                    primaryRouteBySource[node->id] = primary->id;

                for (std::size_t i = 1; i < node->outgoingRouteIds.size(); ++i)
                {
                    const StoryFlowRouteView* branch =
                        model.FindRoute(node->outgoingRouteIds[i]);
                    if (branch)
                        enqueueBranch(
                            branch->destinationNodeId,
                            branch->id,
                            card.columnIndex + 1,
                            track.detached || !card.reachableFromStart);
                }

                if (!primary ||
                    assigned.find(primary->destinationNodeId) != assigned.end())
                {
                    break;
                }
                currentNodeId = primary->destinationNodeId;
            }

            if (!track.cardNodeIds.empty())
                tracks_.push_back(std::move(track));
        };

        while (!pending.empty())
        {
            PendingTrack next = std::move(pending.front());
            pending.pop_front();
            buildPendingTrack(std::move(next));
        }

        // Keep valid but unreachable content visible and authorable. Model node
        // order is already deterministic, so detached track order is stable.
        for (const auto& node : model.Nodes())
        {
            if (assigned.find(node.id) != assigned.end()) continue;
            buildPendingTrack({node.id, {}, false, true, 0});
            while (!pending.empty())
            {
                PendingTrack next = std::move(pending.front());
                pending.pop_front();
                buildPendingTrack(std::move(next));
            }
        }

        exits_.reserve(model.Routes().size());
        for (const auto& route : model.Routes())
        {
            const StoryFlowJourneyCard* source = FindCard(route.sourceNodeId);
            const StoryFlowJourneyCard* destination = FindCard(route.destinationNodeId);
            if (!source || !destination)
            {
                Clear();
                error = "Journey projection omitted a valid Story Flow route endpoint.";
                return false;
            }
            const auto primary = primaryRouteBySource.find(route.sourceNodeId);
            exits_.push_back({
                route.id,
                route.sourceNodeId,
                route.destinationNodeId,
                source->trackIndex,
                destination->trackIndex,
                primary != primaryRouteBySource.end() && primary->second == route.id,
            });
        }

        if (cards_.size() != model.Nodes().size())
        {
            Clear();
            error = "Journey projection did not represent every Story Flow node exactly once.";
            return false;
        }

        loaded_ = true;
        error.clear();
        return true;
    }

    void StoryFlowJourneyModel::Clear() noexcept
    {
        tracks_.clear();
        cards_.clear();
        exits_.clear();
        cardIndexByNodeId_.clear();
        loaded_ = false;
    }

    const StoryFlowJourneyCard* StoryFlowJourneyModel::FindCard(
        const StableId& nodeId) const noexcept
    {
        const auto found = cardIndexByNodeId_.find(nodeId);
        return found == cardIndexByNodeId_.end()
            ? nullptr
            : &cards_[found->second];
    }
}
