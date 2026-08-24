#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowVisualTheme.h"

namespace renegade::studio
{
    // Gate 9E concept-fidelity shell. Every visible region is native Wicked UI;
    // the approved concept bitmap is never rendered as interface. Geometry,
    // typography, colours and assets are driven by StoryFlowVisualTheme.
    class RenegadeStoryFlowJourneyChrome final : public wi::gui::Widget
    {
    public:
        static inline float NavigationRailWidth = 86.0f;
        static inline float WorkspaceHeaderHeight = 78.0f;
        static inline float PreferredInspectorWidth = 344.0f;

        void Create()
        {
            SetName("Renegade Story Flow Journey chrome");
            SetShadowRadius(0.0f);

            const auto& theme = StoryFlowVisualTheme::Get();
            NavigationRailWidth = theme.navigationRailWidth;
            WorkspaceHeaderHeight = theme.headerHeight;
            PreferredInspectorWidth = theme.inspectorWidth;

            navigationRail_.Create("Story Flow navigation rail", true, {}, Region::Role::Rail);
            topBarGuide_.Create("Story Flow top bar", true, {}, Region::Role::Header);
            journeyViewport_.Create("Story Flow Journey viewport", true, {}, Region::Role::Canvas);
            inspector_.Create("Story Flow Inspector frame", true, {}, Region::Role::Inspector);
            overviewHost_.Create("Story Flow overview host", true, "STORY OVERVIEW", Region::Role::Generic);
            zoomHost_.Create("Story Flow zoom host", true, {}, Region::Role::Generic);

            navItems_[0].Create("HUB", false);
            navItems_[1].Create("STORY FLOW", true);
            navItems_[2].Create("LEVELS", false);
            navItems_[3].Create("SCREENS", false);
            navItems_[4].Create("ASSETS", false);
            navItems_[5].Create("VARIABLES", false);
            navItems_[6].Create("TEST PLAY", false);

            brand_ = wi::resourcemanager::Load(theme.logoPath);
            if (!theme.backgroundPath.empty())
                background_ = wi::resourcemanager::Load(theme.backgroundPath);

            overviewHost_.SetVisible(false);
            zoomHost_.SetVisible(false);

            SetLayout(width_, height_);
        }

        void SetLayout(const float width, const float height)
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            NavigationRailWidth = theme.navigationRailWidth;
            WorkspaceHeaderHeight = theme.headerHeight;
            PreferredInspectorWidth = theme.inspectorWidth;

            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);
            SetSize(XMFLOAT2(width_, height_));

            topBarGuide_.SetPos(XMFLOAT2(0.0f, 0.0f));
            topBarGuide_.SetSize(XMFLOAT2(width_, WorkspaceHeaderHeight));

            navigationRail_.SetPos(XMFLOAT2(0.0f, WorkspaceHeaderHeight));
            navigationRail_.SetSize(XMFLOAT2(
                NavigationRailWidth,
                std::max(1.0f, height_ - WorkspaceHeaderHeight)));

            const float workspaceWidth = std::max(1.0f, width_ - NavigationRailWidth);
            const float inspectorWidth = std::min(
                PreferredInspectorWidth,
                workspaceWidth * 0.42f);
            const float viewportWidth = std::max(1.0f, workspaceWidth - inspectorWidth);
            const float inspectorX = NavigationRailWidth + viewportWidth;

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

            const float overviewWidth = std::min(
                theme.minimapWidth,
                std::max(140.0f, viewportWidth * 0.28f));
            const float overviewHeight = theme.minimapHeight;
            overviewHost_.SetPos(XMFLOAT2(
                inspectorX - overviewWidth - theme.shellPadding,
                WorkspaceHeaderHeight + theme.shellPadding));
            overviewHost_.SetSize(XMFLOAT2(overviewWidth, overviewHeight));

            zoomHost_.SetPos(XMFLOAT2(
                NavigationRailWidth + theme.shellPadding,
                std::max(
                    WorkspaceHeaderHeight + theme.shellPadding,
                    height_ - theme.canvasControlHeight - theme.shellPadding)));
            zoomHost_.SetSize(XMFLOAT2(214.0f, theme.canvasControlHeight));

            const float availableNavHeight = std::max(
                1.0f,
                height_ - WorkspaceHeaderHeight - 36.0f);
            const float navGap = std::clamp(
                availableNavHeight / static_cast<float>(navItems_.size()),
                52.0f,
                76.0f);
            const float navTop = WorkspaceHeaderHeight + 24.0f;
            for (std::size_t i = 0; i < navItems_.size(); ++i)
            {
                navItems_[i].SetPos(XMFLOAT2(
                    7.0f,
                    navTop + static_cast<float>(i) * navGap));
                navItems_[i].SetSize(XMFLOAT2(
                    std::max(1.0f, NavigationRailWidth - 14.0f),
                    std::min(58.0f, navGap - 5.0f)));
            }
        }

        [[nodiscard]] float WorkspaceLeftInset() const noexcept
        {
            return NavigationRailWidth;
        }

        void Update(const wi::Canvas&, float) override {}

        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;

            const auto& theme = StoryFlowVisualTheme::Get();

            if (background_.IsValid())
                DrawResourceCover(background_, Bounds(journeyViewport_), cmd);
            journeyViewport_.Render(canvas, cmd);
            Rect(
                journeyViewport_.translation.x,
                journeyViewport_.translation.y,
                journeyViewport_.scale.x,
                journeyViewport_.scale.y,
                theme.canvasOverlay,
                cmd);

            navigationRail_.Render(canvas, cmd);
            topBarGuide_.Render(canvas, cmd);
            inspector_.Render(canvas, cmd);

            DrawBrand(cmd);
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
        static XMFLOAT4 Bounds(const wi::gui::Widget& widget) noexcept
        {
            return XMFLOAT4(
                widget.translation.x,
                widget.translation.y,
                widget.scale.x,
                widget.scale.y);
        }

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
            wi::font::Draw(value, params, cmd);
        }

        static void DrawResourceCover(
            const wi::Resource& resource,
            const XMFLOAT4& bounds,
            const wi::graphics::CommandList cmd)
        {
            if (!resource.IsValid() || bounds.z <= 0.0f || bounds.w <= 0.0f)
                return;
            const auto desc = resource.GetTexture().GetDesc();
            if (desc.width == 0 || desc.height == 0)
                return;
            const float sourceAspect = static_cast<float>(desc.width) /
                static_cast<float>(desc.height);
            const float targetAspect = bounds.z / bounds.w;
            float width = bounds.z;
            float height = bounds.w;
            if (sourceAspect > targetAspect)
                width = height * sourceAspect;
            else
                height = width / sourceAspect;
            const float x = bounds.x + (bounds.z - width) * 0.5f;
            const float y = bounds.y + (bounds.w - height) * 0.5f;
            wi::image::Params params(x, y, width, height);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            params.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
            wi::image::Draw(&resource.GetTexture(), params, cmd);
        }

        void DrawBrand(const wi::graphics::CommandList cmd) const
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            const float x = theme.shellPadding;
            const float y = std::max(
                4.0f,
                (WorkspaceHeaderHeight - theme.headerBrandHeight) * 0.5f);
            const XMFLOAT4 target(
                x,
                y,
                std::min(theme.headerBrandWidth, NavigationRailWidth + 104.0f),
                std::min(theme.headerBrandHeight, WorkspaceHeaderHeight - 8.0f));

            if (brand_.IsValid())
            {
                const auto desc = brand_.GetTexture().GetDesc();
                float width = target.z;
                float height = target.w;
                if (desc.width > 0 && desc.height > 0)
                {
                    const float aspect = static_cast<float>(desc.width) /
                        static_cast<float>(desc.height);
                    height = width / aspect;
                    if (height > target.w)
                    {
                        height = target.w;
                        width = height * aspect;
                    }
                }
                wi::image::Params params(
                    target.x,
                    target.y + (target.w - height) * 0.5f,
                    width,
                    height);
                params.blendFlag = wi::enums::BLENDMODE_ALPHA;
                params.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
                wi::image::Draw(&brand_.GetTexture(), params, cmd);
                return;
            }

            Label(
                "RENEGADE ENGINE",
                target.x,
                target.y + 17.0f,
                theme.fontHeaderMeta + 1,
                theme.textStrong,
                cmd);
        }

        class Region final : public wi::gui::Widget
        {
        public:
            enum class Role
            {
                Generic,
                Rail,
                Header,
                Canvas,
                Inspector,
            };

            void Create(
                std::string name,
                const bool filled,
                std::string title,
                const Role role)
            {
                SetName(std::move(name));
                SetShadowRadius(0.0f);
                filled_ = filled;
                title_ = std::move(title);
                role_ = role;
            }

            void Render(
                const wi::Canvas&,
                const wi::graphics::CommandList cmd) const override
            {
                if (!IsVisible())
                    return;
                const auto& theme = StoryFlowVisualTheme::Get();

                if (filled_)
                {
                    wi::Color fill = theme.panel;
                    switch (role_)
                    {
                    case Role::Rail: fill = theme.rail; break;
                    case Role::Header: fill = theme.header; break;
                    case Role::Canvas: fill = theme.canvas; break;
                    case Role::Inspector: fill = theme.inspector; break;
                    case Role::Generic:
                    default: break;
                    }
                    Rect(translation.x, translation.y, scale.x, scale.y, fill, cmd);
                }

                Rect(translation.x, translation.y, scale.x, 1.0f, theme.borderSoft, cmd);
                Rect(
                    translation.x,
                    translation.y + scale.y - 1.0f,
                    scale.x,
                    1.0f,
                    theme.borderSoft,
                    cmd);
                Rect(translation.x, translation.y, 1.0f, scale.y, theme.borderSoft, cmd);
                Rect(
                    translation.x + scale.x - 1.0f,
                    translation.y,
                    1.0f,
                    scale.y,
                    theme.borderSoft,
                    cmd);

                if (!title_.empty())
                {
                    Label(
                        title_,
                        translation.x + 12.0f,
                        translation.y + 10.0f,
                        theme.fontHeaderMeta,
                        theme.muted,
                        cmd);
                }
            }

            const char* GetWidgetTypeName() const override
            {
                return "RenegadeStoryFlowJourneyRegion";
            }

        private:
            bool filled_ = false;
            Role role_ = Role::Generic;
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
                const wi::Canvas&,
                const wi::graphics::CommandList cmd) const override
            {
                if (!IsVisible())
                    return;
                const auto& theme = StoryFlowVisualTheme::Get();

                if (active_)
                {
                    Rect(
                        translation.x,
                        translation.y,
                        scale.x,
                        scale.y,
                        theme.selectionSurface,
                        cmd);
                    Rect(
                        translation.x,
                        translation.y,
                        3.0f,
                        scale.y,
                        theme.selection,
                        cmd);
                }

                const float glyph = std::min(18.0f, scale.y * 0.36f);
                const float gx = translation.x + (scale.x - glyph) * 0.5f;
                const float gy = translation.y + 8.0f;
                Rect(gx, gy, glyph, 1.0f, active_ ? theme.textStrong : theme.muted, cmd);
                Rect(gx, gy + glyph - 1.0f, glyph, 1.0f,
                    active_ ? theme.textStrong : theme.muted, cmd);
                Rect(gx, gy, 1.0f, glyph, active_ ? theme.textStrong : theme.muted, cmd);
                Rect(gx + glyph - 1.0f, gy, 1.0f, glyph,
                    active_ ? theme.textStrong : theme.muted, cmd);

                const std::string compact = label_ == "STORY FLOW" ? "STORY\nFLOW" : label_;
                const std::size_t breakAt = compact.find('\n');
                if (breakAt == std::string::npos)
                {
                    Label(
                        compact,
                        translation.x + scale.x * 0.5f,
                        translation.y + scale.y - 16.0f,
                        std::max(6, theme.fontCardMeta - 1),
                        active_ ? theme.textStrong : theme.muted,
                        cmd,
                        wi::font::WIFALIGN_CENTER);
                }
                else
                {
                    Label(
                        compact.substr(0, breakAt),
                        translation.x + scale.x * 0.5f,
                        translation.y + scale.y - 25.0f,
                        std::max(6, theme.fontCardMeta - 1),
                        active_ ? theme.textStrong : theme.muted,
                        cmd,
                        wi::font::WIFALIGN_CENTER);
                    Label(
                        compact.substr(breakAt + 1),
                        translation.x + scale.x * 0.5f,
                        translation.y + scale.y - 14.0f,
                        std::max(6, theme.fontCardMeta - 1),
                        active_ ? theme.selection : theme.muted,
                        cmd,
                        wi::font::WIFALIGN_CENTER);
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
        wi::Resource brand_;
        wi::Resource background_;
        Region navigationRail_;
        Region topBarGuide_;
        Region journeyViewport_;
        Region inspector_;
        Region overviewHost_;
        Region zoomHost_;
        std::array<NavItem, 7> navItems_;
    };
}
