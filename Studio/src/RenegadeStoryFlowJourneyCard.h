#pragma once

#include "renegade/bridge/StoryFlowAuthoringModel.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Gate 9B reusable native Journey destination card. This is a real Renegade
    // widget object per semantic Flow node; it owns presentation only and never
    // duplicates or mutates Story Flow routing semantics.
    class RenegadeStoryFlowJourneyCard final : public wi::gui::Widget
    {
    public:
        void Create(bridge::StableId nodeId)
        {
            nodeId_ = std::move(nodeId);
            SetName("Journey card " + nodeId_);
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
            const bridge::FlowNodeKind kind,
            const std::size_t sequence,
            std::string name,
            std::string subtitle,
            const std::size_t exitCount,
            const bool selected,
            const bool error,
            const bool warning,
            const bool detailed,
            const bool canChooseThumbnail,
            wi::Resource thumbnail)
        {
            kind_ = kind;
            sequence_ = sequence;
            name_ = std::move(name);
            subtitle_ = std::move(subtitle);
            exitCount_ = exitCount;
            selected_ = selected;
            error_ = error;
            warning_ = warning;
            detailed_ = detailed;
            canChooseThumbnail_ = canChooseThumbnail;
            thumbnail_ = std::move(thumbnail);
        }

        [[nodiscard]] const bridge::StableId& NodeId() const noexcept
        {
            return nodeId_;
        }

        [[nodiscard]] XMFLOAT4 ThumbnailButtonBounds() const noexcept
        {
            if (!canChooseThumbnail_ || !detailed_)
                return {};
            const XMFLOAT4 media = MediaBounds();
            const float button = std::clamp(scale.x * 0.115f, 20.0f, 28.0f);
            return XMFLOAT4(
                media.x + media.z - button - 6.0f,
                media.y + media.w - button - 6.0f,
                button,
                button);
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;

            ApplyScissor(canvas, scissorRect, cmd);
            const wi::Color edge = error_
                ? Error
                : (selected_ ? Forge : (warning_ ? Warning : Border));
            const wi::Color fill = selected_ ? SelectedSurface : Surface2;
            DrawPanel(translation.x, translation.y, scale.x, scale.y, fill, edge, cmd);
            DrawRect(
                translation.x,
                translation.y,
                scale.x,
                std::max(2.0f, std::min(4.0f, scale.y * 0.025f)),
                TypeColor(kind_),
                cmd);

            if (!detailed_)
            {
                DrawText(
                    TwoDigit(sequence_ + 1),
                    translation.x + 9.0f,
                    translation.y + 10.0f,
                    8,
                    Muted,
                    cmd);
                DrawText(
                    Shorten(name_, 24),
                    translation.x + 36.0f,
                    translation.y + 9.0f,
                    9,
                    TextStrong,
                    cmd);
                return;
            }

            DrawText(
                TwoDigit(sequence_ + 1),
                translation.x + 10.0f,
                translation.y + 10.0f,
                8,
                Forge,
                cmd);
            DrawText(
                KindLabel(kind_),
                translation.x + scale.x - 10.0f,
                translation.y + 10.0f,
                7,
                Muted,
                cmd,
                wi::font::WIFALIGN_RIGHT);

            const XMFLOAT4 media = MediaBounds();
            DrawPanel(
                media.x,
                media.y,
                media.z,
                media.w,
                Surface0,
                BorderSoft,
                cmd);
            if (thumbnail_.IsValid())
            {
                DrawResourceContained(thumbnail_, media, cmd);
            }
            else
            {
                DrawPlaceholder(media, cmd);
            }

            if (canChooseThumbnail_)
            {
                const XMFLOAT4 button = ThumbnailButtonBounds();
                const XMFLOAT4 pointer = wi::input::GetPointer();
                const bool hovered = Contains(button, pointer);
                DrawPanel(
                    button.x,
                    button.y,
                    button.z,
                    button.w,
                    hovered ? HoverSurface : Surface2,
                    hovered ? Forge : Border,
                    cmd);
                DrawText(
                    "IMG",
                    button.x + button.z * 0.5f,
                    button.y + button.w * 0.5f - 4.0f,
                    6,
                    hovered ? TextStrong : Muted,
                    cmd,
                    wi::font::WIFALIGN_CENTER);
            }

            const float titleY = media.y + media.w + 8.0f;
            DrawText(
                Shorten(name_, 30),
                translation.x + 10.0f,
                titleY,
                10,
                TextStrong,
                cmd);
            if (!subtitle_.empty())
            {
                DrawText(
                    Shorten(subtitle_, 34),
                    translation.x + 10.0f,
                    titleY + 18.0f,
                    7,
                    Muted,
                    cmd);
            }

            const float footerY = translation.y + scale.y - 20.0f;
            const std::string exitLabel =
                std::to_string(exitCount_) + " EXIT" + (exitCount_ == 1 ? "" : "S");
            DrawText(
                exitLabel,
                translation.x + 10.0f,
                footerY,
                7,
                exitCount_ == 0 ? Muted : Text,
                cmd);

            const char* state = error_ ? "ERROR" : (warning_ ? "CHECK" : "READY");
            const wi::Color stateColor = error_ ? Error : (warning_ ? Warning : Success);
            DrawRect(
                translation.x + scale.x - 57.0f,
                footerY + 3.0f,
                5.0f,
                5.0f,
                stateColor,
                cmd);
            DrawText(
                state,
                translation.x + scale.x - 10.0f,
                footerY,
                7,
                stateColor,
                cmd,
                wi::font::WIFALIGN_RIGHT);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyCard";
        }

    private:
        static constexpr wi::Color Surface0 = wi::Color(8, 12, 16, 255);
        static constexpr wi::Color Surface2 = wi::Color(16, 23, 28, 255);
        static constexpr wi::Color SelectedSurface = wi::Color(23, 20, 18, 255);
        static constexpr wi::Color HoverSurface = wi::Color(27, 31, 34, 255);
        static constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
        static constexpr wi::Color BorderSoft = wi::Color(25, 36, 43, 255);
        static constexpr wi::Color Text = wi::Color(214, 214, 214, 255);
        static constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
        static constexpr wi::Color Muted = wi::Color(142, 151, 156, 255);
        static constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
        static constexpr wi::Color Success = wi::Color(76, 195, 138, 255);
        static constexpr wi::Color Warning = wi::Color(224, 165, 82, 255);
        static constexpr wi::Color Error = wi::Color(229, 92, 92, 255);

        [[nodiscard]] XMFLOAT4 MediaBounds() const noexcept
        {
            const float x = translation.x + 9.0f;
            const float y = translation.y + 30.0f;
            const float width = std::max(1.0f, scale.x - 18.0f);
            const float desired = std::max(38.0f, scale.y * 0.47f);
            const float height = std::min(
                desired,
                std::max(1.0f, scale.y - 82.0f));
            return XMFLOAT4(x, y, width, height);
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

        static void DrawPanel(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color fill,
            const wi::Color edge,
            const wi::graphics::CommandList cmd)
        {
            DrawRect(x, y, width, height, edge, cmd);
            DrawRect(
                x + 1.0f,
                y + 1.0f,
                std::max(0.0f, width - 2.0f),
                std::max(0.0f, height - 2.0f),
                fill,
                cmd);
        }

        static void DrawText(
            const std::string& text,
            const float x,
            const float y,
            const int size,
            const wi::Color color,
            const wi::graphics::CommandList cmd,
            const wi::font::Alignment align = wi::font::WIFALIGN_LEFT)
        {
            wi::font::Params params(
                x,
                y,
                size,
                align,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.spacingX = 0.2f;
            params.bolden = 0.12f;
            wi::font::Draw(text, params, cmd);
        }

        static void DrawResourceContained(
            const wi::Resource& resource,
            const XMFLOAT4& bounds,
            const wi::graphics::CommandList cmd)
        {
            const auto desc = resource.GetTexture().GetDesc();
            float width = bounds.z;
            float height = bounds.w;
            if (desc.width > 0 && desc.height > 0)
            {
                const float aspect = static_cast<float>(desc.width) /
                    static_cast<float>(desc.height);
                height = width / aspect;
                if (height > bounds.w)
                {
                    height = bounds.w;
                    width = height * aspect;
                }
            }
            const float x = bounds.x + (bounds.z - width) * 0.5f;
            const float y = bounds.y + (bounds.w - height) * 0.5f;
            wi::image::Params image(x, y, width, height);
            image.blendFlag = wi::enums::BLENDMODE_ALPHA;
            image.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
            wi::image::Draw(&resource.GetTexture(), image, cmd);
        }

        void DrawPlaceholder(
            const XMFLOAT4& media,
            const wi::graphics::CommandList cmd) const
        {
            const wi::Color type = TypeColor(kind_);
            const float centerX = media.x + media.z * 0.5f;
            const float centerY = media.y + media.w * 0.5f;
            DrawPanel(
                centerX - 18.0f,
                centerY - 12.0f,
                36.0f,
                24.0f,
                wi::Color(12, 17, 21, 255),
                type,
                cmd);
            DrawText(
                kind_ == bridge::FlowNodeKind::Level ? "LEVEL" :
                    (kind_ == bridge::FlowNodeKind::Screen ? "SCREEN" : "FLOW"),
                centerX,
                centerY + 18.0f,
                6,
                Muted,
                cmd,
                wi::font::WIFALIGN_CENTER);
        }

        [[nodiscard]] static wi::Color TypeColor(
            const bridge::FlowNodeKind kind) noexcept
        {
            switch (kind)
            {
            case bridge::FlowNodeKind::GameStart:
                return Forge;
            case bridge::FlowNodeKind::Level:
                return wi::Color(185, 113, 58, 255);
            case bridge::FlowNodeKind::Screen:
                return wi::Color(91, 122, 143, 255);
            case bridge::FlowNodeKind::CompleteGame:
                return Success;
            case bridge::FlowNodeKind::ReturnToMainMenu:
                return wi::Color(120, 132, 139, 255);
            case bridge::FlowNodeKind::Quit:
                return wi::Color(181, 78, 78, 255);
            default:
                return Border;
            }
        }

        [[nodiscard]] static const char* KindLabel(
            const bridge::FlowNodeKind kind) noexcept
        {
            switch (kind)
            {
            case bridge::FlowNodeKind::GameStart: return "ENTRY";
            case bridge::FlowNodeKind::Level: return "LEVEL";
            case bridge::FlowNodeKind::Screen: return "SCREEN";
            case bridge::FlowNodeKind::CompleteGame: return "COMPLETE";
            case bridge::FlowNodeKind::ReturnToMainMenu: return "RETURN";
            case bridge::FlowNodeKind::Quit: return "QUIT";
            default: return "FLOW";
            }
        }

        [[nodiscard]] static std::string TwoDigit(const std::size_t value)
        {
            std::ostringstream stream;
            stream << std::setw(2) << std::setfill('0') << value;
            return stream.str();
        }

        [[nodiscard]] static std::string Shorten(
            std::string value,
            const std::size_t maximum)
        {
            if (value.size() <= maximum)
                return value;
            if (maximum <= 3)
                return value.substr(0, maximum);
            value.resize(maximum - 3);
            value += "...";
            return value;
        }

        [[nodiscard]] static bool Contains(
            const XMFLOAT4& bounds,
            const XMFLOAT4& pointer) noexcept
        {
            return bounds.z > 0.0f && bounds.w > 0.0f &&
                pointer.x >= bounds.x && pointer.y >= bounds.y &&
                pointer.x < bounds.x + bounds.z &&
                pointer.y < bounds.y + bounds.w;
        }

        bridge::StableId nodeId_;
        bridge::FlowNodeKind kind_ = bridge::FlowNodeKind::Level;
        std::size_t sequence_ = 0;
        std::size_t exitCount_ = 0;
        std::string name_;
        std::string subtitle_;
        bool selected_ = false;
        bool error_ = false;
        bool warning_ = false;
        bool detailed_ = true;
        bool canChooseThumbnail_ = false;
        wi::Resource thumbnail_;
    };
}
