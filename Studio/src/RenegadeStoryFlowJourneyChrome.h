#pragma once

#include <algorithm>
#include <array>
#include <string>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Gate 9A native Journey chrome. The approved concept is a visual reference
    // only: no concept bitmap is loaded or rendered. Every visible shell region
    // below is an independent native Wicked GUI object with its own geometry.
    class RenegadeStoryFlowJourneyChrome final : public wi::gui::Widget
    {
    public:
        static constexpr float NavigationRailWidth = 86.0f;
        static constexpr float WorkspaceHeaderHeight = 78.0f;
        static constexpr float PreferredInspectorWidth = 320.0f;

        void Create()
        {
            SetName("Renegade Story Flow Journey chrome");
            SetShadowRadius(0.0f);

            navigationRail_.Create("Story Flow navigation rail", true, "RENEGADE");
            topBarGuide_.Create("Story Flow top bar", false, {});
            journeyViewport_.Create("Story Flow Journey viewport", false, {});
            inspector_.Create("Story Flow Inspector frame", false, "INSPECTOR");
            overviewHost_.Create("Story Flow overview host", false, "STORY OVERVIEW");
            zoomHost_.Create("Story Flow zoom host", false, {});

            navItems_[0].Create("PROJECT", false);
            navItems_[1].Create("STORY FLOW", true);
            navItems_[2].Create("ASSETS", false);
            navItems_[3].Create("BUILD", false);
            navItems_[4].Create("SETTINGS", false);

            // 9D will activate the real overview and navigation controls. The
            // objects exist now so later gates add behaviour rather than paint
            // replacement rectangles over the workspace.
            overviewHost_.SetVisible(false);
            zoomHost_.SetVisible(false);

            SetLayout(width_, height_);
        }

        void SetLayout(const float width, const float height)
        {
            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);
            SetSize(XMFLOAT2(width_, height_));

            navigationRail_.SetPos(XMFLOAT2(0.0f, 0.0f));
            navigationRail_.SetSize(XMFLOAT2(NavigationRailWidth, height_));

            const float workspaceWidth = std::max(1.0f, width_ - NavigationRailWidth);
            const float inspectorWidth = std::min(
                PreferredInspectorWidth,
                workspaceWidth * 0.42f);
            const float viewportWidth = std::max(1.0f, workspaceWidth - inspectorWidth);
            const float inspectorX = NavigationRailWidth + viewportWidth;

            topBarGuide_.SetPos(XMFLOAT2(NavigationRailWidth, 0.0f));
            topBarGuide_.SetSize(XMFLOAT2(workspaceWidth, WorkspaceHeaderHeight));

            journeyViewport_.SetPos(XMFLOAT2(
                NavigationRailWidth,
                WorkspaceHeaderHeight));
            journeyViewport_.SetSize(XMFLOAT2(
                viewportWidth,
                std::max(1.0f, height_ - WorkspaceHeaderHeight)));

            inspector_.SetPos(XMFLOAT2(inspectorX, WorkspaceHeaderHeight));
            inspector_.SetSize(XMFLOAT2(
                inspectorWidth,
                std::max(1.0f, height_ - WorkspaceHeaderHeight)));

            const float overviewWidth = std::max(140.0f, inspectorWidth - 28.0f);
            const float overviewHeight = 132.0f;
            overviewHost_.SetPos(XMFLOAT2(
                inspectorX + 14.0f,
                std::max(
                    WorkspaceHeaderHeight + 14.0f,
                    height_ - overviewHeight - 42.0f)));
            overviewHost_.SetSize(XMFLOAT2(overviewWidth, overviewHeight));

            zoomHost_.SetPos(XMFLOAT2(
                NavigationRailWidth + 18.0f,
                std::max(
                    WorkspaceHeaderHeight + 18.0f,
                    height_ - 52.0f)));
            zoomHost_.SetSize(XMFLOAT2(178.0f, 34.0f));

            const float navTop = 108.0f;
            for (std::size_t i = 0; i < navItems_.size(); ++i)
            {
                navItems_[i].SetPos(XMFLOAT2(
                    8.0f,
                    navTop + static_cast<float>(i) * 62.0f));
                navItems_[i].SetSize(XMFLOAT2(NavigationRailWidth - 16.0f, 48.0f));
            }
        }

        [[nodiscard]] float WorkspaceLeftInset() const noexcept
        {
            return NavigationRailWidth;
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;

            ApplyScissor(canvas, scissorRect, cmd);

            navigationRail_.Render(canvas, cmd);
            topBarGuide_.Render(canvas, cmd);
            journeyViewport_.Render(canvas, cmd);
            inspector_.Render(canvas, cmd);

            for (const auto& item : navItems_)
                item.Render(canvas, cmd);

            if (overviewHost_.IsVisible())
                overviewHost_.Render(canvas, cmd);
            if (zoomHost_.IsVisible())
                zoomHost_.Render(canvas, cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyChrome";
        }

    private:
        static constexpr wi::Color RailSurface = wi::Color(7, 10, 12, 255);
        static constexpr wi::Color PanelSurface = wi::Color(13, 19, 23, 255);
        static constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
        static constexpr wi::Color Text = wi::Color(239, 242, 243, 255);
        static constexpr wi::Color Muted = wi::Color(122, 135, 142, 255);
        static constexpr wi::Color Accent = wi::Color(210, 91, 29, 255);

        static void Rect(
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

        static void Label(
            const std::string& value,
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
            params.bolden = 0.14f;
            wi::font::Draw(value, params, cmd);
        }

        class Region final : public wi::gui::Widget
        {
        public:
            void Create(
                std::string name,
                const bool filled,
                std::string title)
            {
                SetName(std::move(name));
                SetShadowRadius(0.0f);
                filled_ = filled;
                title_ = std::move(title);
            }

            void Render(
                const wi::Canvas& canvas,
                const wi::graphics::CommandList cmd) const override
            {
                if (!IsVisible())
                    return;
                ApplyScissor(canvas, scissorRect, cmd);

                if (filled_)
                {
                    RenegadeStoryFlowJourneyChrome::Rect(
                        translation.x,
                        translation.y,
                        scale.x,
                        scale.y,
                        RailSurface,
                        cmd);
                }

                RenegadeStoryFlowJourneyChrome::Rect(
                    translation.x,
                    translation.y,
                    scale.x,
                    1.0f,
                    Border,
                    cmd);
                RenegadeStoryFlowJourneyChrome::Rect(
                    translation.x,
                    translation.y + scale.y - 1.0f,
                    scale.x,
                    1.0f,
                    Border,
                    cmd);
                RenegadeStoryFlowJourneyChrome::Rect(
                    translation.x,
                    translation.y,
                    1.0f,
                    scale.y,
                    Border,
                    cmd);
                RenegadeStoryFlowJourneyChrome::Rect(
                    translation.x + scale.x - 1.0f,
                    translation.y,
                    1.0f,
                    scale.y,
                    Border,
                    cmd);

                if (!title_.empty())
                {
                    RenegadeStoryFlowJourneyChrome::Label(
                        title_,
                        translation.x + 12.0f,
                        translation.y + 12.0f,
                        8,
                        title_ == "RENEGADE" ? Accent : Muted,
                        cmd);
                }
            }

            const char* GetWidgetTypeName() const override
            {
                return "RenegadeStoryFlowJourneyRegion";
            }

        private:
            bool filled_ = false;
            std::string title_;
        };

        class NavItem final : public wi::gui::Widget
        {
        public:
            void Create(std::string label, const bool active)
            {
                SetName("Story Flow navigation item " + label);
                SetShadowRadius(0.0f);
                label_ = std::move(label);
                active_ = active;
            }

            void Render(
                const wi::Canvas& canvas,
                const wi::graphics::CommandList cmd) const override
            {
                if (!IsVisible())
                    return;
                ApplyScissor(canvas, scissorRect, cmd);

                if (active_)
                {
                    RenegadeStoryFlowJourneyChrome::Rect(
                        translation.x,
                        translation.y,
                        3.0f,
                        scale.y,
                        Accent,
                        cmd);
                    RenegadeStoryFlowJourneyChrome::Rect(
                        translation.x + 3.0f,
                        translation.y,
                        std::max(0.0f, scale.x - 3.0f),
                        scale.y,
                        wi::Color(24, 19, 16, 255),
                        cmd);
                }

                const std::string compact = label_ == "STORY FLOW"
                    ? "STORY\nFLOW"
                    : label_;
                const std::size_t breakAt = compact.find('\n');
                if (breakAt == std::string::npos)
                {
                    RenegadeStoryFlowJourneyChrome::Label(
                        compact,
                        translation.x + 10.0f,
                        translation.y + 18.0f,
                        7,
                        active_ ? Text : Muted,
                        cmd);
                }
                else
                {
                    RenegadeStoryFlowJourneyChrome::Label(
                        compact.substr(0, breakAt),
                        translation.x + 10.0f,
                        translation.y + 10.0f,
                        7,
                        active_ ? Text : Muted,
                        cmd);
                    RenegadeStoryFlowJourneyChrome::Label(
                        compact.substr(breakAt + 1),
                        translation.x + 10.0f,
                        translation.y + 24.0f,
                        7,
                        active_ ? Accent : Muted,
                        cmd);
                }
            }

            const char* GetWidgetTypeName() const override
            {
                return "RenegadeStoryFlowJourneyNavItem";
            }

        private:
            std::string label_;
            bool active_ = false;
        };

        float width_ = 1920.0f;
        float height_ = 1080.0f;
        Region navigationRail_;
        Region topBarGuide_;
        Region journeyViewport_;
        Region inspector_;
        Region overviewHost_;
        Region zoomHost_;
        std::array<NavItem, 5> navItems_;
    };
}
