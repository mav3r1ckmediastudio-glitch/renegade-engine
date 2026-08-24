#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Gate 9A native Journey chrome. The approved concept is a visual reference
    // only: no concept bitmap is loaded or rendered. Every visible shell region
    // below is an independent native Wicked GUI object with its own geometry.
    class RenegadeStoryFlowJourneyChrome final : public wi::gui::Widget
    {
    public:
        enum class Action
        {
            Hub, StoryFlow, Levels, Screens, Assets, Variables, TestPlay,
            Select, Arrange, Filter, Search, Preview, Validate,
        };

        static constexpr float NavigationRailWidth = 86.0f;
        static constexpr float WorkspaceHeaderHeight = 78.0f;
        static constexpr float PreferredInspectorWidth = 320.0f;

        void Create()
        {
            SetName("Renegade Story Flow Journey chrome");
            SetShadowRadius(0.0f);

            navigationRail_.Create("Story Flow navigation rail", true, {});
            topBarGuide_.Create("Story Flow top bar", true, {});
            journeyViewport_.Create("Story Flow Journey viewport", false, {});
            inspector_.Create("Story Flow Inspector frame", false, {});
            overviewHost_.Create("Story Flow overview host", false, "STORY OVERVIEW");
            zoomHost_.Create("Story Flow zoom host", false, {});

            navItems_[0].Create("HUB", false);
            navItems_[1].Create("STORY FLOW", true);
            navItems_[2].Create("LEVELS", false);
            navItems_[3].Create("SCREENS", false);
            navItems_[4].Create("ASSETS", false);
            navItems_[5].Create("VARIABLES", false);
            navItems_[6].Create("TEST PLAY", false);

            topCommands_[0].Create("SELECT", true);
            topCommands_[1].Create("ARRANGE", false);
            topCommands_[2].Create("FILTER", false);
            topCommands_[3].Create("SEARCH", false);
            topCommands_[4].Create("PREVIEW", false);
            topCommands_[5].Create("VALIDATE", false);

            // 9D will activate the real overview and navigation controls. The
            // objects exist now so later gates add behaviour rather than paint
            // replacement rectangles over the workspace.
            overviewHost_.SetVisible(false);
            zoomHost_.SetVisible(false);

            SetLayout(width_, height_);
        }

        void OnAction(std::function<void(Action)> callback)
        {
            action_ = std::move(callback);
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

            const float availableNavHeight = std::max(
                1.0f, height_ - WorkspaceHeaderHeight - 24.0f);
            const float navStep = std::clamp(
                availableNavHeight / static_cast<float>(navItems_.size()),
                52.0f, 74.0f);
            const float navTop = WorkspaceHeaderHeight + 16.0f;
            for (std::size_t i = 0; i < navItems_.size(); ++i)
            {
                navItems_[i].SetPos(XMFLOAT2(
                    8.0f,
                    navTop + static_cast<float>(i) * navStep));
                navItems_[i].SetSize(XMFLOAT2(
                    NavigationRailWidth - 16.0f,
                    std::min(56.0f, navStep - 4.0f)));
            }

            const float inspectorReserve = std::min(
                PreferredInspectorWidth,
                std::max(1.0f, width_ - NavigationRailWidth) * 0.42f);
            const float commandRight = std::max(
                NavigationRailWidth + 520.0f,
                width_ - inspectorReserve - 16.0f);
            const float commandStart = std::clamp(
                NavigationRailWidth + width_ * 0.25f,
                NavigationRailWidth + 330.0f,
                commandRight - 360.0f);
            const float commandWidth = std::max(
                56.0f,
                std::min(74.0f,
                    (commandRight - commandStart) /
                        static_cast<float>(topCommands_.size())));
            for (std::size_t i = 0; i < topCommands_.size(); ++i)
            {
                topCommands_[i].SetPos(XMFLOAT2(
                    commandStart + static_cast<float>(i) * commandWidth,
                    8.0f));
                topCommands_[i].SetSize(XMFLOAT2(commandWidth - 4.0f, 54.0f));
            }
        }

        [[nodiscard]] float WorkspaceLeftInset() const noexcept
        {
            return NavigationRailWidth;
        }

        void Update(const wi::Canvas&, float) override
        {
            if (!IsVisible() || !IsEnabled() || !action_ ||
                !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
                return;

            const XMFLOAT4 pointer = wi::input::GetPointer();
            for (std::size_t i = 0; i < navItems_.size(); ++i)
            {
                if (Contains(navItems_[i], pointer))
                {
                    static constexpr std::array<Action, 7> actions = {
                        Action::Hub, Action::StoryFlow, Action::Levels,
                        Action::Screens, Action::Assets, Action::Variables,
                        Action::TestPlay};
                    action_(actions[i]);
                    return;
                }
            }
            for (std::size_t i = 0; i < topCommands_.size(); ++i)
            {
                if (Contains(topCommands_[i], pointer))
                {
                    static constexpr std::array<Action, 6> actions = {
                        Action::Select, Action::Arrange, Action::Filter,
                        Action::Search, Action::Preview, Action::Validate};
                    action_(actions[i]);
                    return;
                }
            }
        }

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;

            // The parent chrome spans the whole Story Flow surface, so there is
            // no child scissor dependency. This keeps the region objects safe
            // even though they are composed and rendered by the chrome rather
            // than individually registered with Wicked's GUI manager.
            navigationRail_.Render(canvas, cmd);
            topBarGuide_.Render(canvas, cmd);
            journeyViewport_.Render(canvas, cmd);
            inspector_.Render(canvas, cmd);

            Label("RENEGADE", 14.0f, 22.0f, 10, Text, cmd);
            Label("STUDIO", 14.0f, 39.0f, 7, Muted, cmd);

            for (const auto& item : navItems_)
                item.Render(canvas, cmd);
            for (const auto& command : topCommands_)
                command.Render(canvas, cmd);

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
        [[nodiscard]] static bool Contains(
            const wi::gui::Widget& widget, const XMFLOAT4& pointer) noexcept
        {
            const XMFLOAT2 position = widget.GetPos();
            const XMFLOAT2 size = widget.GetSize();
            return pointer.x >= position.x && pointer.y >= position.y &&
                pointer.x < position.x + size.x &&
                pointer.y < position.y + size.y;
        }
        static constexpr wi::Color RailSurface = wi::Color(7, 10, 12, 255);
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
                const wi::Canvas&,
                const wi::graphics::CommandList cmd) const override
            {
                if (!IsVisible())
                    return;

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
                command_ = label_ == "SELECT" || label_ == "ARRANGE" ||
                    label_ == "FILTER" || label_ == "SEARCH" ||
                    label_ == "PREVIEW" || label_ == "VALIDATE";
            }

            void Render(
                const wi::Canvas&,
                const wi::graphics::CommandList cmd) const override
            {
                if (!IsVisible())
                    return;

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

                if (command_)
                {
                    const std::string glyph = label_ == "SELECT" ? ">" :
                        (label_ == "ARRANGE" ? "::" :
                        (label_ == "FILTER" ? "Y" :
                        (label_ == "SEARCH" ? "O" :
                        (label_ == "PREVIEW" ? ">|" : "OK"))));
                    RenegadeStoryFlowJourneyChrome::Label(
                        glyph,
                        translation.x + scale.x * 0.5f - 6.0f,
                        translation.y + 7.0f,
                        9,
                        active_ ? Text : Muted,
                        cmd);
                    RenegadeStoryFlowJourneyChrome::Label(
                        label_,
                        translation.x + 7.0f,
                        translation.y + 34.0f,
                        6,
                        active_ ? Text : Muted,
                        cmd);
                    return;
                }

                const std::string glyph = label_ == "HUB" ? "H" :
                    (label_ == "STORY FLOW" ? "<>" :
                    (label_ == "LEVELS" ? "L" :
                    (label_ == "SCREENS" ? "S" :
                    (label_ == "ASSETS" ? "A" :
                    (label_ == "VARIABLES" ? "{}" : ">")))));
                RenegadeStoryFlowJourneyChrome::Label(
                    glyph,
                    translation.x + scale.x * 0.5f - 6.0f,
                    translation.y + 7.0f,
                    9,
                    active_ ? Text : Muted,
                    cmd);

                const std::string compact = label_ == "STORY FLOW"
                    ? "STORY\nFLOW"
                    : (label_ == "TEST PLAY" ? "TEST\nPLAY" : label_);
                const std::size_t breakAt = compact.find('\n');
                if (breakAt == std::string::npos)
                {
                    RenegadeStoryFlowJourneyChrome::Label(
                        compact,
                        translation.x + 10.0f,
                        translation.y + scale.y - 16.0f,
                        7,
                        active_ ? Text : Muted,
                        cmd);
                }
                else
                {
                    RenegadeStoryFlowJourneyChrome::Label(
                        compact.substr(0, breakAt),
                        translation.x + 10.0f,
                        translation.y + scale.y - 25.0f,
                        7,
                        active_ ? Text : Muted,
                        cmd);
                    RenegadeStoryFlowJourneyChrome::Label(
                        compact.substr(breakAt + 1),
                        translation.x + 10.0f,
                        translation.y + scale.y - 14.0f,
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
            bool command_ = false;
        };

        float width_ = 1920.0f;
        float height_ = 1080.0f;
        Region navigationRail_;
        Region topBarGuide_;
        Region journeyViewport_;
        Region inspector_;
        Region overviewHost_;
        Region zoomHost_;
        std::array<NavItem, 7> navItems_;
        std::array<NavItem, 6> topCommands_;
        std::function<void(Action)> action_;
    };
}
