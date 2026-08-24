#pragma once

#include "RenegadeStoryFlowVisualTheme.h"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Presentation-only Journey lane. Gate 9E intentionally removes the heavy
    // boxed-track look: the approved concept is an open map/canvas where branch
    // structure is carried by cards and coloured route paths.
    class RenegadeStoryFlowJourneyLane final : public wi::gui::Widget
    {
    public:
        void Create(const std::size_t trackIndex)
        {
            trackIndex_ = trackIndex;
            SetName("Journey lane " + std::to_string(trackIndex_));
            SetShadowRadius(0.0f);
        }

        void SetBounds(const XMFLOAT4& bounds)
        {
            SetPos(XMFLOAT2(bounds.x, bounds.y));
            SetSize(XMFLOAT2(
                std::max(1.0f, bounds.z),
                std::max(1.0f, bounds.w)));
        }

        void SetPresentation(
            const std::size_t visibleIndex,
            const bool mainTrack,
            const bool detached,
            std::string title)
        {
            visibleIndex_ = visibleIndex;
            mainTrack_ = mainTrack;
            detached_ = detached;
            title_ = std::move(title);
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;
            ApplyScissor(canvas, scissorRect, cmd);
            const auto& theme = StoryFlowVisualTheme::Get();

            const wi::Color laneColor = mainTrack_
                ? theme.routeMain
                : (detached_ ? theme.muted : theme.routeSystem);

            DrawRect(
                translation.x,
                translation.y + 18.0f,
                scale.x,
                1.0f,
                theme.borderSoft,
                cmd);
            DrawRect(
                translation.x,
                translation.y + 18.0f,
                std::min(36.0f, scale.x),
                2.0f,
                laneColor,
                cmd);

            DrawText(
                TwoDigit(visibleIndex_ + 1),
                translation.x,
                translation.y,
                theme.fontCardMeta,
                laneColor,
                cmd);
            DrawText(
                title_.empty()
                    ? (mainTrack_ ? "MAIN JOURNEY" :
                        (detached_ ? "DETACHED" : "ALTERNATE BRANCH"))
                    : title_,
                translation.x + 27.0f,
                translation.y - 1.0f,
                theme.fontCardMeta,
                mainTrack_ ? theme.text : theme.muted,
                cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyLane";
        }

    private:
        static void DrawRect(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            if (width <= 0.0f || height <= 0.0f)
                return;
            wi::image::Params params(x, y, width, height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawText(
            const std::string& text,
            const float x,
            const float y,
            const int size,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            wi::font::Params params(
                x,
                y,
                size,
                wi::font::WIFALIGN_LEFT,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.style = theme.fontStyle;
            params.spacingX = theme.fontTracking;
            params.bolden = theme.fontBolden;
            wi::font::Draw(text, params, cmd);
        }

        [[nodiscard]] static std::string TwoDigit(const std::size_t value)
        {
            std::ostringstream stream;
            stream << std::setw(2) << std::setfill('0') << value;
            return stream.str();
        }

        std::size_t trackIndex_ = 0;
        std::size_t visibleIndex_ = 0;
        bool mainTrack_ = false;
        bool detached_ = false;
        std::string title_;
    };
}
