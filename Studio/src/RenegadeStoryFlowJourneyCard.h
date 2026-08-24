#pragma once

#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "RenegadeStoryFlowVisualTheme.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include <WickedEngine.h>

namespace renegade::studio
{
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
            const float button = std::clamp(scale.x * 0.14f, 18.0f, 26.0f);
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
            const auto& theme = StoryFlowVisualTheme::Get();
            const wi::Color type = TypeColor(kind_);
            const wi::Color edge = error_
                ? theme.error
                : (selected_ ? theme.selection : (warning_ ? theme.warning : type));
            const wi::Color fill = selected_ ? theme.selectionSurface : theme.panel;

            DrawPanel(
                translation.x,
                translation.y,
                scale.x,
                scale.y,
                fill,
                edge,
                theme.cardRadius,
                cmd);

            DrawRoundedRect(
                translation.x + 1.0f,
                translation.y + 1.0f,
                std::max(0.0f, scale.x - 2.0f),
                3.0f,
                std::min(theme.cardRadius, 2.0f),
                type,
                cmd);

            if (!detailed_)
            {
                DrawText(
                    TwoDigit(sequence_ + 1),
                    translation.x + 9.0f,
                    translation.y + 9.0f,
                    theme.fontCardMeta,
                    type,
                    cmd);
                DrawText(
                    Shorten(name_, 25),
                    translation.x + 34.0f,
                    translation.y + 8.0f,
                    theme.fontCardTitle,
                    theme.textStrong,
                    cmd);
                DrawText(
                    KindLabel(kind_),
                    translation.x + scale.x - 9.0f,
                    translation.y + 10.0f,
                    theme.fontCardMeta,
                    theme.muted,
                    cmd,
                    wi::font::WIFALIGN_RIGHT);
                return;
            }

            DrawText(
                TwoDigit(sequence_ + 1),
                translation.x + 9.0f,
                translation.y + 9.0f,
                theme.fontCardMeta,
                selected_ ? theme.selection : type,
                cmd);
            DrawText(
                KindLabel(kind_),
                translation.x + scale.x - 9.0f,
                translation.y + 10.0f,
                theme.fontCardMeta,
                theme.muted,
                cmd,
                wi::font::WIFALIGN_RIGHT);

            const XMFLOAT4 media = MediaBounds();
            DrawPanel(
                media.x,
                media.y,
                media.z,
                media.w,
                theme.canvas,
                theme.borderSoft,
                std::max(2.0f, theme.cardRadius - 1.0f),
                cmd);

            if (thumbnail_.IsValid())
                DrawResourceCover(thumbnail_, media, cmd);
            else
                DrawPlaceholder(media, cmd);

            const float footerTop = media.y + media.w - 1.0f;
            const float footerHeight = std::max(
                1.0f,
                translation.y + scale.y - footerTop - 1.0f);
            DrawRoundedRect(
                translation.x + 1.0f,
                footerTop,
                std::max(0.0f, scale.x - 2.0f),
                footerHeight,
                std::max(0.0f, theme.cardRadius - 1.0f),
                theme.panelRaised,
                cmd);

            const float titleY = footerTop + 7.0f;
            DrawText(
                Shorten(name_, 26),
                translation.x + 9.0f,
                titleY,
                theme.fontCardTitle,
                theme.textStrong,
                cmd);
            if (!subtitle_.empty())
            {
                DrawText(
                    Shorten(subtitle_, 29),
                    translation.x + 9.0f,
                    titleY + 17.0f,
                    theme.fontCardSubtitle,
                    theme.muted,
                    cmd);
            }

            const float footerY = translation.y + scale.y - 16.0f;
            const std::string exitLabel =
                std::to_string(exitCount_) + " EXIT" + (exitCount_ == 1 ? "" : "S");
            DrawText(
                exitLabel,
                translation.x + 9.0f,
                footerY,
                theme.fontCardMeta,
                exitCount_ == 0 ? theme.muted : theme.text,
                cmd);

            const char* state = error_ ? "ERROR" : (warning_ ? "CHECK" : "READY");
            const wi::Color stateColor = error_
                ? theme.error
                : (warning_ ? theme.warning : theme.success);
            DrawRoundedRect(
                translation.x + scale.x - 51.0f,
                footerY + 3.0f,
                5.0f,
                5.0f,
                2.5f,
                stateColor,
                cmd);
            DrawText(
                state,
                translation.x + scale.x - 8.0f,
                footerY,
                theme.fontCardMeta,
                stateColor,
                cmd,
                wi::font::WIFALIGN_RIGHT);

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
                    hovered ? theme.panelHover : theme.panelRaised,
                    hovered ? theme.selection : theme.border,
                    3.0f,
                    cmd);
                DrawText(
                    "IMG",
                    button.x + button.z * 0.5f,
                    button.y + button.w * 0.5f - 4.0f,
                    std::max(6, theme.fontCardMeta - 1),
                    hovered ? theme.textStrong : theme.muted,
                    cmd,
                    wi::font::WIFALIGN_CENTER);
            }
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyCard";
        }

    private:
        [[nodiscard]] XMFLOAT4 MediaBounds() const noexcept
        {
            const float x = translation.x + 7.0f;
            const float y = translation.y + 25.0f;
            const float width = std::max(1.0f, scale.x - 14.0f);
            const float maximum = std::max(1.0f, scale.y - 74.0f);
            const float height = std::min(
                maximum,
                std::max(38.0f, scale.y * 0.58f));
            return XMFLOAT4(x, y, width, height);
        }

        static void DrawRoundedRect(
            const float x,
            const float y,
            const float width,
            const float height,
            const float radius,
            const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            if (width <= 0.0f || height <= 0.0f)
                return;
            wi::image::Params params(x, y, width, height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            if (radius > 0.0f)
            {
                params.enableCornerRounding();
                for (auto& corner : params.corners_rounding)
                {
                    corner.radius = std::min(radius, std::min(width, height) * 0.5f);
                    corner.segments = 10;
                }
            }
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawPanel(
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color fill,
            const wi::Color edge,
            const float radius,
            const wi::graphics::CommandList cmd)
        {
            DrawRoundedRect(x, y, width, height, radius, edge, cmd);
            DrawRoundedRect(
                x + 1.0f,
                y + 1.0f,
                std::max(0.0f, width - 2.0f),
                std::max(0.0f, height - 2.0f),
                std::max(0.0f, radius - 1.0f),
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
            const auto& theme = StoryFlowVisualTheme::Get();
            wi::font::Params params(
                x,
                y,
                size,
                align,
                wi::font::WIFALIGN_TOP,
                color,
                wi::Color::Transparent());
            params.style = theme.fontStyle;
            params.spacingX = theme.fontTracking;
            params.bolden = theme.fontBolden;
            wi::font::Draw(text, params, cmd);
        }

        static void DrawResourceCover(
            const wi::Resource& resource,
            const XMFLOAT4& bounds,
            const wi::graphics::CommandList cmd)
        {
            if (!resource.IsValid()) return;
            const auto desc = resource.GetTexture().GetDesc();
            if (desc.width == 0 || desc.height == 0) return;

            const float sourceAspect = static_cast<float>(desc.width) /
                static_cast<float>(desc.height);
            const float targetAspect = bounds.z / std::max(1.0f, bounds.w);
            float width = bounds.z;
            float height = bounds.w;
            if (sourceAspect > targetAspect)
                width = height * sourceAspect;
            else
                height = width / sourceAspect;
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
            const auto& theme = StoryFlowVisualTheme::Get();
            const wi::Color type = TypeColor(kind_);
            const float centerX = media.x + media.z * 0.5f;
            const float centerY = media.y + media.w * 0.5f;

            if (kind_ == bridge::FlowNodeKind::GameStart)
            {
                const float size = std::clamp(
                    std::min(media.z, media.w) * 0.44f,
                    28.0f,
                    58.0f);
                DrawRoundedRect(
                    centerX - size * 0.5f,
                    centerY - size * 0.5f,
                    size,
                    size,
                    size * 0.5f,
                    theme.selectionSurface,
                    cmd);
                DrawPanel(
                    centerX - size * 0.34f,
                    centerY - size * 0.34f,
                    size * 0.68f,
                    size * 0.68f,
                    theme.canvas,
                    theme.gameStart,
                    size * 0.34f,
                    cmd);
                DrawText(
                    ">",
                    centerX,
                    centerY - 6.0f,
                    theme.fontCardTitle + 2,
                    theme.textStrong,
                    cmd,
                    wi::font::WIFALIGN_CENTER);
                return;
            }

            DrawPanel(
                centerX - 22.0f,
                centerY - 14.0f,
                44.0f,
                28.0f,
                theme.panelRaised,
                type,
                4.0f,
                cmd);
            DrawText(
                kind_ == bridge::FlowNodeKind::Level ? "LEVEL" :
                    (kind_ == bridge::FlowNodeKind::Screen ? "SCREEN" : "FLOW"),
                centerX,
                centerY + 20.0f,
                theme.fontCardMeta,
                theme.muted,
                cmd,
                wi::font::WIFALIGN_CENTER);
        }

        [[nodiscard]] static wi::Color TypeColor(
            const bridge::FlowNodeKind kind) noexcept
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            switch (kind)
            {
            case bridge::FlowNodeKind::GameStart: return theme.gameStart;
            case bridge::FlowNodeKind::Level: return theme.level;
            case bridge::FlowNodeKind::Screen: return theme.screen;
            case bridge::FlowNodeKind::CompleteGame: return theme.success;
            case bridge::FlowNodeKind::ReturnToMainMenu: return theme.routeOther;
            case bridge::FlowNodeKind::Quit: return theme.terminal;
            default: return theme.border;
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
