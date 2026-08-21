#pragma once

#include "renegade/bridge/StoryFlowAuthoringModel.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace renegade::bridge
{
    struct StoryFlowJourneyCard
    {
        StableId nodeId;
        std::size_t trackIndex = 0;
        std::size_t sequenceIndex = 0;
        std::size_t columnIndex = 0;
        bool reachableFromStart = false;
    };

    struct StoryFlowJourneyTrack
    {
        std::size_t index = 0;
        bool mainTrack = false;
        bool detached = false;
        std::size_t startColumn = 0;
        StableId sourceRouteId;
        std::vector<StableId> cardNodeIds;
    };

    struct StoryFlowJourneyExit
    {
        StableId routeId;
        StableId sourceNodeId;
        StableId destinationNodeId;
        std::size_t sourceTrackIndex = 0;
        std::size_t destinationTrackIndex = 0;
        bool primaryContinuation = false;
    };

    // Deterministic, UI-independent Journey projection over the one
    // authoritative StoryFlowAuthoringModel. It never owns or mutates Flow
    // semantics. Every semantic node appears as exactly one card; merges and
    // loops reference that existing card through exits.
    class StoryFlowJourneyModel final
    {
    public:
        [[nodiscard]] bool Build(
            const StoryFlowAuthoringModel& model,
            std::string& error);
        void Clear() noexcept;

        [[nodiscard]] bool IsLoaded() const noexcept { return loaded_; }
        [[nodiscard]] const std::vector<StoryFlowJourneyTrack>& Tracks() const noexcept
        {
            return tracks_;
        }
        [[nodiscard]] const std::vector<StoryFlowJourneyCard>& Cards() const noexcept
        {
            return cards_;
        }
        [[nodiscard]] const std::vector<StoryFlowJourneyExit>& Exits() const noexcept
        {
            return exits_;
        }
        [[nodiscard]] const StoryFlowJourneyCard* FindCard(
            const StableId& nodeId) const noexcept;

    private:
        std::vector<StoryFlowJourneyTrack> tracks_;
        std::vector<StoryFlowJourneyCard> cards_;
        std::vector<StoryFlowJourneyExit> exits_;
        std::unordered_map<StableId, std::size_t> cardIndexByNodeId_;
        bool loaded_ = false;
    };
}
