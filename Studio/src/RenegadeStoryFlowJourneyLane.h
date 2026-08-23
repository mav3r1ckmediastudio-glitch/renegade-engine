#pragma once

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Gate 9B reel/lane object. Lanes are presentation-only containers derived
    // from the Journey projection; they never own routes or Runtime semantics.
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

            DrawRect(
                translation.x,
                translation.y,
                scale.x,
                scale.y,
                mainTrack_
                    ? wi::Color(10, 15, 19, 150)
                    : wi::Color(9, 14, 18, 112),
                cmd);
            DrawRect(
                translation.x,
                translation.y,
                scale.x,
                1.0f,
                mainTrack_ ? Forge : Border,
                cmd);
            DrawRect(
                translation.x,
                translation.y,
                mainTrack_ ? 3.0f : 1.0f,
                scale.y,
                mainTrack_ ? Forge : BorderSoft,
                cmd);

            DrawText(
                TwoDigit(visibleIndex_ + 1),
                translation.x + 12.0f,
                translation.y + 10.0f,
                7,
                mainTrack_ ? Forge : Muted,
                cmd);
            DrawText(
                title_.empty()
                    ? (mainTrack_ ? "MAIN JOURNEY" :
                        (detached_ ? "DETACHED JOURNEY" : "ALTERNATE BRANCH"))
                    : title_,
                translation.x + 38.0f,
                translation.y + 9.0f,
                8,
                mainTrack_ ? TextStrong : Muted,
                cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyLane";
        }

    private:
        static constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
        static constexpr wi::Color BorderSoft = wi::Color(25, 36, 43, 255);
        static constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
        static constexpr wi::Color Muted = wi::Color(142, 151, 156, 255);
        static constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);

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
            wi::font::Params params(
                x,
                y,
                size,
                wi::font::WIFALIGN_LEFT,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.spacingX = 0.25f;
            params.bolden = 0.12f;
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
