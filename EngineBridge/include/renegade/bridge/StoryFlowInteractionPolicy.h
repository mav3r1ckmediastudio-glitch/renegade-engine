#pragma once

#include "renegade/bridge/FlowService.h"

namespace renegade::bridge
{
    enum class StoryFlowActivationTarget
    {
        None,
        LevelEditor,
        ScreenEditor,
    };

    inline constexpr float StoryFlowDoubleClickSeconds = 0.35f;
    inline constexpr float StoryFlowDoubleClickDistance = 8.0f;

    [[nodiscard]] bool IsStoryFlowNodeActivatable(FlowNodeKind kind) noexcept;
    [[nodiscard]] StoryFlowActivationTarget StoryFlowActivationTargetForKind(
        FlowNodeKind kind) noexcept;

    [[nodiscard]] bool ShouldActivateStoryFlowNodeClick(
        FlowNodeKind kind,
        bool sameNodeAsPreviousClick,
        float secondsSincePreviousClick,
        float pointerDistanceFromPreviousClick) noexcept;
}
