#pragma once

#include <algorithm>

namespace renegade::studio
{
    struct JourneyUiRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;

        [[nodiscard]] float Right() const noexcept { return x + width; }
        [[nodiscard]] float Bottom() const noexcept { return y + height; }
    };

    struct JourneyShellLayout
    {
        JourneyUiRect topBar;
        JourneyUiRect navigationRail;
        JourneyUiRect workspace;
        JourneyUiRect journeyCanvas;
        JourneyUiRect inspector;
        JourneyUiRect workspaceTitle;
        JourneyUiRect canvasNavigation;
        JourneyUiRect storyOverview;
    };

    // Gate 9A's responsive shell contract is deliberately UI-toolkit neutral.
    // The Journey canvas may pan and semantically zoom inside journeyCanvas;
    // every other rectangle remains fixed in screen space.
    [[nodiscard]] inline JourneyShellLayout ComputeJourneyShellLayout(
        const float requestedWidth,
        const float requestedHeight) noexcept
    {
        const float width = std::max(1.0f, requestedWidth);
        const float height = std::max(1.0f, requestedHeight);
        constexpr float topBarHeight = 70.0f;
        constexpr float railWidth = 96.0f;
        constexpr float titleHeight = 112.0f;

        const float inspectorWidth = std::clamp(
            width * 0.185f, 280.0f, 336.0f);
        const float inspectorX = std::max(
            railWidth + 1.0f, width - inspectorWidth);

        JourneyShellLayout layout;
        layout.topBar = {0.0f, 0.0f, width, topBarHeight};
        layout.navigationRail = {
            0.0f, topBarHeight, railWidth,
            std::max(1.0f, height - topBarHeight)};
        layout.workspace = {
            railWidth, topBarHeight,
            std::max(1.0f, inspectorX - railWidth),
            std::max(1.0f, height - topBarHeight)};
        layout.inspector = {
            inspectorX, topBarHeight, inspectorWidth,
            std::max(1.0f, height - topBarHeight)};
        layout.workspaceTitle = {
            layout.workspace.x,
            layout.workspace.y,
            layout.workspace.width,
            std::min(titleHeight, layout.workspace.height)};
        layout.journeyCanvas = {
            layout.workspace.x,
            layout.workspace.y + layout.workspaceTitle.height,
            layout.workspace.width,
            std::max(1.0f, layout.workspace.height - layout.workspaceTitle.height)};

        const float navigationWidth = std::min(
            390.0f, std::max(326.0f, layout.journeyCanvas.width * 0.34f));
        layout.canvasNavigation = {
            layout.journeyCanvas.x + 18.0f,
            std::max(layout.journeyCanvas.y + 12.0f, height - 64.0f),
            navigationWidth,
            46.0f};

        const float overviewWidth = std::clamp(
            layout.journeyCanvas.width * 0.27f, 238.0f, 340.0f);
        const float overviewHeight = std::clamp(
            height * 0.096f, 78.0f, 104.0f);
        layout.storyOverview = {
            layout.journeyCanvas.Right() - overviewWidth - 18.0f,
            std::max(layout.journeyCanvas.y + 12.0f,
                height - overviewHeight - 18.0f),
            overviewWidth,
            overviewHeight};
        return layout;
    }
}
