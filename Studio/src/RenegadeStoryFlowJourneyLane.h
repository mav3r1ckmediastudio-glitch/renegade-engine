#pragma once

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowJourneyRole.h"

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
            const JourneyBranchRole role,
            const bool collapsed,
            std::string title)
        {
            visibleIndex_ = visibleIndex;
            mainTrack_ = mainTrack;
            detached_ = detached;
            role_ = role;
            collapsed_ = collapsed;
            title_ = std::move(title);
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;
            ApplyScissor(canvas, scissorRect, cmd);

            const wi::Color roleColour = RoleColor(role_);
            DrawRect(
                translation.x,
                translation.y,
                scale.x,
                scale.y,
                mainTrack_
                    ? wi::Color(12, 20, 27, 218)
                    : RoleSurface(role_),
                cmd);
            DrawRect(
                translation.x,
                translation.y,
                scale.x,
                1.0f,
                Border,
                cmd);
            DrawRect(
                translation.x,
                translation.y,
                mainTrack_ ? 1.0f : 6.0f,
                scale.y,
                mainTrack_ ? Border : roleColour,
                cmd);

            if (mainTrack_)
            {
                DrawText(TwoDigit(visibleIndex_ + 1),
                    translation.x + 12.0f, translation.y + 10.0f,
                    7, Muted, cmd);
                DrawText(title_.empty() ? "MAIN JOURNEY" : title_,
                    translation.x + 38.0f, translation.y + 9.0f,
                    8, TextStrong, cmd);
            }
            else
            {
                const char laneLetter = static_cast<char>(
                    'A' + std::min<std::size_t>(25, visibleIndex_ - 1));
                DrawText(std::string(1, laneLetter),
                    translation.x + 9.0f,
                    translation.y + scale.y * 0.5f - 4.0f,
                    7, roleColour, cmd);
                DrawText(collapsed_ ? ">" : "v",
                    translation.x + scale.x - 17.0f,
                    translation.y + scale.y * 0.5f - 5.0f,
                    9, Muted, cmd);
            }
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
        static constexpr wi::Color Purple = wi::Color(143, 73, 205, 255);
        static constexpr wi::Color Turquoise = wi::Color(53, 166, 174, 255);
        static constexpr wi::Color Red = wi::Color(205, 67, 61, 255);
        static constexpr wi::Color Amber = wi::Color(210, 157, 62, 255);
        static constexpr wi::Color Blue = wi::Color(65, 158, 230, 255);

        [[nodiscard]] static wi::Color RoleColor(
            const JourneyBranchRole role) noexcept
        {
            switch (role)
            {
            case JourneyBranchRole::Options: return Purple;
            case JourneyBranchRole::LoadSave: return Turquoise;
            case JourneyBranchRole::Failure: return Red;
            case JourneyBranchRole::Detached: return Amber;
            case JourneyBranchRole::Custom: return Blue;
            case JourneyBranchRole::Main:
            default: return Border;
            }
        }

        [[nodiscard]] static wi::Color RoleSurface(
            const JourneyBranchRole role) noexcept
        {
            switch (role)
            {
            case JourneyBranchRole::Options:
                return wi::Color(22, 17, 30, 238);
            case JourneyBranchRole::LoadSave:
                return wi::Color(12, 27, 31, 238);
            case JourneyBranchRole::Failure:
                return wi::Color(30, 16, 19, 238);
            case JourneyBranchRole::Detached:
                return wi::Color(29, 25, 14, 238);
            case JourneyBranchRole::Custom:
                return wi::Color(13, 23, 31, 238);
            case JourneyBranchRole::Main:
            default:
                return wi::Color(12, 20, 27, 238);
            }
        }

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
        JourneyBranchRole role_ = JourneyBranchRole::Custom;
        bool collapsed_ = false;
        std::string title_;
    };
}
