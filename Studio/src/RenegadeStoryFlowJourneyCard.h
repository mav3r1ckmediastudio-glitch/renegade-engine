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
    // Gate 9B destination card: image-led, rounded and shadowed. Node kind is
    // communicated by text/iconography; it never becomes a coloured frame.
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
            const bool mainCard,
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
            mainCard_ = mainCard;
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
            const float button = mainCard_ ? 22.0f : 18.0f;
            return XMFLOAT4(
                translation.x + scale.x - button - 7.0f,
                translation.y + scale.y - button - 7.0f,
                button,
                button);
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible()) return;
            ApplyScissor(canvas, scissorRect, cmd);

            const XMFLOAT4 bounds(
                translation.x, translation.y, scale.x, scale.y);
            const float radius = mainCard_ ? 7.0f : 5.0f;

            Rounded({bounds.x + 3.0f, bounds.y + 5.0f, bounds.z, bounds.w},
                radius, wi::Color(0, 0, 0, 118), cmd);

            // Neutral edge by default. Blue is selection only; validation uses
            // a small state mark in the footer and never recolours the frame.
            Rounded(bounds, radius,
                selected_ ? SelectionBlue : Border, cmd);
            Rounded({bounds.x + 1.0f, bounds.y + 1.0f,
                std::max(1.0f, bounds.z - 2.0f),
                std::max(1.0f, bounds.w - 2.0f)},
                std::max(1.0f, radius - 1.0f), Surface, cmd);

            const XMFLOAT4 media(
                bounds.x + 2.0f,
                bounds.y + 2.0f,
                std::max(1.0f, bounds.z - 4.0f),
                std::max(1.0f, bounds.w - 4.0f));
            if (thumbnail_.IsValid())
                DrawResourceCover(thumbnail_, media, radius - 1.0f, cmd);
            else
                DrawPlaceholder(media, radius - 1.0f, cmd);

            if (mainCard_)
                RenderMainCard(bounds, cmd);
            else
                RenderBranchCard(bounds, cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyCard";
        }

    private:
        static constexpr wi::Color Surface = wi::Color(12, 20, 26, 255);
        static constexpr wi::Color Border = wi::Color(55, 69, 78, 255);
        static constexpr wi::Color SelectionBlue = wi::Color(59, 164, 239, 255);
        static constexpr wi::Color TextStrong = wi::Color(247, 248, 249, 255);
        static constexpr wi::Color Text = wi::Color(214, 222, 227, 255);
        static constexpr wi::Color Muted = wi::Color(167, 179, 187, 255);
        static constexpr wi::Color Success = wi::Color(113, 205, 111, 255);
        static constexpr wi::Color Warning = wi::Color(232, 174, 77, 255);
        static constexpr wi::Color Error = wi::Color(226, 83, 76, 255);

        static void Rect(float x, float y, float width, float height,
            wi::Color color, wi::graphics::CommandList cmd)
        {
            if (width <= 0.0f || height <= 0.0f) return;
            wi::image::Params params(x, y, width, height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void Rounded(const XMFLOAT4& bounds, float radius,
            wi::Color color, wi::graphics::CommandList cmd)
        {
            wi::image::Params params(
                bounds.x, bounds.y, bounds.z, bounds.w, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            params.enableCornerRounding();
            for (auto& corner : params.corners_rounding)
            {
                corner.radius = radius;
                corner.segments = 8;
            }
            wi::image::Draw(nullptr, params, cmd);
        }

        static void TextLabel(const std::string& text, float x, float y,
            int size, wi::Color color, wi::graphics::CommandList cmd,
            wi::font::Alignment align = wi::font::WIFALIGN_LEFT)
        {
            wi::font::Params params(x, y, size, align,
                wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
            params.spacingX = 0.12f;
            params.bolden = 0.14f;
            wi::font::Draw(text, params, cmd);
        }

        static void DrawResourceCover(const wi::Resource& resource,
            const XMFLOAT4& bounds, float radius,
            wi::graphics::CommandList cmd)
        {
            const auto desc = resource.GetTexture().GetDesc();
            XMFLOAT4 sourceRect(
                0.0f, 0.0f,
                static_cast<float>(desc.width),
                static_cast<float>(desc.height));
            if (desc.width > 0 && desc.height > 0)
            {
                const float sourceAspect = static_cast<float>(desc.width) /
                    static_cast<float>(desc.height);
                const float targetAspect = bounds.z / bounds.w;
                if (sourceAspect > targetAspect)
                {
                    sourceRect.z = static_cast<float>(desc.height) * targetAspect;
                    sourceRect.x =
                        (static_cast<float>(desc.width) - sourceRect.z) * 0.5f;
                }
                else
                {
                    sourceRect.w = static_cast<float>(desc.width) / targetAspect;
                    sourceRect.y =
                        (static_cast<float>(desc.height) - sourceRect.w) * 0.5f;
                }
            }
            wi::image::Params image(
                bounds.x, bounds.y, bounds.z, bounds.w);
            image.blendFlag = wi::enums::BLENDMODE_ALPHA;
            image.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
            image.drawRect = sourceRect;
            image.enableCornerRounding();
            for (auto& corner : image.corners_rounding)
            {
                corner.radius = radius;
                corner.segments = 8;
            }
            wi::image::Draw(&resource.GetTexture(), image, cmd);
        }

        void DrawPlaceholder(const XMFLOAT4& bounds, float radius,
            wi::graphics::CommandList cmd) const
        {
            Rounded(bounds, radius, PlaceholderSky(kind_), cmd);
            const float horizon = bounds.y + bounds.w * 0.57f;
            // A restrained native landscape placeholder keeps empty governed
            // cards image-led without pretending an authored thumbnail exists.
            Rect(bounds.x, horizon, bounds.z, bounds.y + bounds.w - horizon,
                wi::Color(8, 17, 22, 235), cmd);
            Rect(bounds.x + bounds.z * 0.12f, horizon - bounds.w * 0.14f,
                bounds.z * 0.26f, bounds.w * 0.14f,
                wi::Color(18, 33, 41, 210), cmd);
            Rect(bounds.x + bounds.z * 0.48f, horizon - bounds.w * 0.23f,
                bounds.z * 0.34f, bounds.w * 0.23f,
                wi::Color(13, 27, 34, 225), cmd);
            Rounded({bounds.x + bounds.z * 0.5f - 13.0f,
                bounds.y + bounds.w * 0.46f - 13.0f, 26.0f, 26.0f},
                13.0f, wi::Color(70, 150, 205, 96), cmd);
            TextLabel(KindLabel(kind_), bounds.x + bounds.z * 0.5f,
                bounds.y + bounds.w * 0.63f, 6,
                wi::Color(194, 209, 219, 180), cmd,
                wi::font::WIFALIGN_CENTER);
        }

        void RenderMainCard(const XMFLOAT4& bounds,
            wi::graphics::CommandList cmd) const
        {
            Rect(bounds.x + 2.0f, bounds.y + 2.0f,
                bounds.z - 4.0f, 66.0f,
                wi::Color(4, 9, 13, 205), cmd);
            Rect(bounds.x + 2.0f, bounds.y + bounds.w - 47.0f,
                bounds.z - 4.0f, 45.0f,
                wi::Color(4, 9, 13, 218), cmd);
            TextLabel(TwoDigit(sequence_ + 1), bounds.x + 12.0f,
                bounds.y + 11.0f, 8, Muted, cmd);
            TextLabel(Shorten(name_, 22), bounds.x + 12.0f,
                bounds.y + 34.0f, 11, TextStrong, cmd);
            if (!subtitle_.empty())
            {
                TextLabel(Shorten(subtitle_, 27), bounds.x + 12.0f,
                    bounds.y + 53.0f, 7, Text, cmd);
            }

            TextLabel(KindLabel(kind_), bounds.x + 11.0f,
                bounds.y + bounds.w - 26.0f, 7, Muted, cmd);
            const wi::Color state = error_ ? Error : (warning_ ? Warning : Success);
            Rounded({bounds.x + bounds.z - 18.0f,
                bounds.y + bounds.w - 20.0f,
                7.0f, 7.0f}, 4.0f, state, cmd);
            if (exitCount_ > 0)
            {
                TextLabel(std::to_string(exitCount_), bounds.x + bounds.z - 31.0f,
                    bounds.y + bounds.w - 25.0f, 7, Muted, cmd,
                    wi::font::WIFALIGN_RIGHT);
            }
            RenderThumbnailButton(cmd);
        }

        void RenderBranchCard(const XMFLOAT4& bounds,
            wi::graphics::CommandList cmd) const
        {
            Rect(bounds.x + 2.0f, bounds.y + 2.0f,
                bounds.z * 0.68f, bounds.w - 4.0f,
                wi::Color(4, 9, 13, 198), cmd);
            TextLabel(Shorten(name_, 25), bounds.x + 12.0f,
                bounds.y + 13.0f, 9, TextStrong, cmd);
            if (!subtitle_.empty())
            {
                TextLabel(Shorten(subtitle_, 31), bounds.x + 12.0f,
                    bounds.y + 31.0f, 7, Text, cmd);
            }
            const wi::Color state = error_ ? Error : (warning_ ? Warning : Success);
            Rounded({bounds.x + bounds.z - 16.0f,
                bounds.y + bounds.w - 16.0f,
                6.0f, 6.0f}, 3.0f, state, cmd);
            RenderThumbnailButton(cmd);
        }

        void RenderThumbnailButton(wi::graphics::CommandList cmd) const
        {
            if (!canChooseThumbnail_ || !detailed_) return;
            const XMFLOAT4 button = ThumbnailButtonBounds();
            Rounded(button, 3.0f, wi::Color(6, 12, 16, 214), cmd);
            TextLabel("IMG", button.x + button.z * 0.5f,
                button.y + button.w * 0.5f - 4.0f, 5, Muted, cmd,
                wi::font::WIFALIGN_CENTER);
        }

        [[nodiscard]] static wi::Color PlaceholderSky(
            bridge::FlowNodeKind kind) noexcept
        {
            switch (kind)
            {
            case bridge::FlowNodeKind::GameStart:
                return wi::Color(10, 31, 49, 255);
            case bridge::FlowNodeKind::Level:
                return wi::Color(28, 43, 42, 255);
            case bridge::FlowNodeKind::Screen:
                return wi::Color(31, 41, 52, 255);
            case bridge::FlowNodeKind::CompleteGame:
                return wi::Color(50, 37, 30, 255);
            case bridge::FlowNodeKind::Quit:
                return wi::Color(49, 24, 24, 255);
            default:
                return wi::Color(24, 35, 43, 255);
            }
        }

        [[nodiscard]] static const char* KindLabel(
            bridge::FlowNodeKind kind) noexcept
        {
            switch (kind)
            {
            case bridge::FlowNodeKind::GameStart: return "ENTRY POINT";
            case bridge::FlowNodeKind::Level: return "LEVEL";
            case bridge::FlowNodeKind::Screen: return "SCREEN";
            case bridge::FlowNodeKind::CompleteGame: return "VICTORY";
            case bridge::FlowNodeKind::ReturnToMainMenu: return "RETURN";
            case bridge::FlowNodeKind::Quit: return "QUIT";
            default: return "FLOW";
            }
        }

        [[nodiscard]] static std::string TwoDigit(std::size_t value)
        {
            std::ostringstream stream;
            stream << std::setw(2) << std::setfill('0') << value;
            return stream.str();
        }

        [[nodiscard]] static std::string Shorten(
            std::string value, std::size_t maximum)
        {
            if (value.size() <= maximum) return value;
            if (maximum <= 3) return value.substr(0, maximum);
            value.resize(maximum - 3);
            value += "...";
            return value;
        }

        bridge::StableId nodeId_;
        bridge::FlowNodeKind kind_ = bridge::FlowNodeKind::GameStart;
        std::size_t sequence_ = 0;
        std::size_t exitCount_ = 0;
        std::string name_;
        std::string subtitle_;
        wi::Resource thumbnail_;
        bool selected_ = false;
        bool error_ = false;
        bool warning_ = false;
        bool mainCard_ = true;
        bool detailed_ = true;
        bool canChooseThumbnail_ = false;
    };
}
