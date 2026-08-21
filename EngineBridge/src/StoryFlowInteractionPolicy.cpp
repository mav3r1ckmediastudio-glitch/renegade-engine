#include "renegade/bridge/StoryFlowInteractionPolicy.h"

#include <cmath>

namespace renegade::bridge
{
    bool IsStoryFlowNodeActivatable(const FlowNodeKind kind) noexcept
    {
        return StoryFlowActivationTargetForKind(kind) !=
            StoryFlowActivationTarget::None;
    }

    StoryFlowActivationTarget StoryFlowActivationTargetForKind(
        const FlowNodeKind kind) noexcept
    {
        switch (kind)
        {
        case FlowNodeKind::Level:
            return StoryFlowActivationTarget::LevelEditor;
        case FlowNodeKind::Screen:
            return StoryFlowActivationTarget::ScreenEditor;
        default:
            return StoryFlowActivationTarget::None;
        }
    }

    bool ShouldActivateStoryFlowNodeClick(
        const FlowNodeKind kind,
        const bool sameNodeAsPreviousClick,
        const float secondsSincePreviousClick,
        const float pointerDistanceFromPreviousClick) noexcept
    {
        return IsStoryFlowNodeActivatable(kind) &&
            sameNodeAsPreviousClick &&
            std::isfinite(secondsSincePreviousClick) &&
            secondsSincePreviousClick >= 0.0f &&
            secondsSincePreviousClick <= StoryFlowDoubleClickSeconds &&
            std::isfinite(pointerDistanceFromPreviousClick) &&
            pointerDistanceFromPreviousClick >= 0.0f &&
            pointerDistanceFromPreviousClick <= StoryFlowDoubleClickDistance;
    }
}
