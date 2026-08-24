#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowVisualTheme.h"

namespace renegade::studio
{
    // Gate 9E shell only. It owns branding, header, rail and separators but does
    // not repaint the Journey canvas or Graph Inspector, so proven authoring
    // surfaces remain visible and keep their existing input ownership.
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
            brand_ = wi::resourcemanager::Load(theme.logoPath);

            navItems_[0].Create("HUB", false);
            navItems_[1].Create("STORY FLOW", true);
            navItems_[2].Create("LEVELS", false);
            navItems_[3].Create("SCREENS", false);
            navItems_[4].Create("ASSETS", false);
            navItems_[5].Create("VARIABLES", false);
            navItems_[6].Create("TEST PLAY", false);
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
            SetPos(XMFLOAT2(0.0f, 0.0f));
            SetSize(XMFLOAT2(width_, height_));

            const float availableNavHeight = std::max(
                1.0f, height_ - WorkspaceHeaderHeight - 26.0f);
            const float navGap = std::clamp(
                availableNavHeight / static_cast<float>(navItems_.size()),
                50.0f, 75.0f);
            const float navTop = WorkspaceHeaderHeight + 18.0f;
            for (std::size_t i = 0; i < navItems_.size(); ++i)
            {
                navItems_[i].SetPos(XMFLOAT2(
                    7.0f,
                    navTop + static_cast<float>(i) * navGap));
                navItems_[i].SetSize(XMFLOAT2(
                    std::max(1.0f, NavigationRailWidth - 14.0f),
                    std::min(56.0f, navGap - 4.0f)));
            }
        }

        [[nodiscard]] float WorkspaceLeftInset() const noexcept
        {
            return NavigationRailWidth;
        }

        // Decorative only: never call Widget::Update() on this full-canvas shell.
        void Update(const wi::Canvas&, float) override {}

        void Render(
            const wi::Canvas&,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible()) return;
            const auto& theme = StoryFlowVisualTheme::Get();

            DrawRect(0.0f, 0.0f, width_, WorkspaceHeaderHeight,
                theme.header, cmd);
            DrawRect(0.0f, WorkspaceHeaderHeight,
                NavigationRailWidth,
                std::max(1.0f, height_ - WorkspaceHeaderHeight),
                theme.rail, cmd);
            DrawRect(0.0f, WorkspaceHeaderHeight - 1.0f,
                width_, 1.0f, theme.borderSoft, cmd);
            DrawRect(NavigationRailWidth - 1.0f, WorkspaceHeaderHeight,
                1.0f, std::max(1.0f, height_ - WorkspaceHeaderHeight),
                theme.borderSoft, cmd);

            DrawBrand(cmd);
            for (const auto& item : navItems_)
                item.Render({}, cmd);
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyChrome";
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
            if (width <= 0.0f || height <= 0.0f) return;
            wi::image::Params params(x, y, width, height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void DrawText(
            const std::string& value,
            const float x,
            const float y,
            const int size,
            const wi::Color color,
            const wi::graphics::CommandList cmd,
            const wi::font::Alignment align = wi::font::WIFALIGN_LEFT)
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            wi::font::Params params(x, y, size, align,
                wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
            params.style = theme.fontStyle;
            params.spacingX = theme.fontTracking;
            params.bolden = theme.fontBolden;
            wi::font::Draw(value, params, cmd);
        }

        void DrawBrand(const wi::graphics::CommandList cmd) const
        {
            const auto& theme = StoryFlowVisualTheme::Get();
            const float targetHeight = std::min(
                theme.headerBrandHeight,
                WorkspaceHeaderHeight - 8.0f);
            const float targetWidth = std::min(
                theme.headerBrandWidth,
                NavigationRailWidth + 112.0f);
            const float x = theme.shellPadding;
            const float y = (WorkspaceHeaderHeight - targetHeight) * 0.5f;

            if (brand_.IsValid())
            {
                const auto desc = brand_.GetTexture().GetDesc();
                float width = targetWidth;
                float height = targetHeight;
                if (desc.width > 0 && desc.height > 0)
                {
                    const float aspect = static_cast<float>(desc.width) /
                        static_cast<float>(desc.height);
                    height = width / aspect;
                    if (height > targetHeight)
                    {
                        height = targetHeight;
                        width = height * aspect;
                    }
                }
                wi::image::Params image(
                    x,
                    y + (targetHeight - height) * 0.5f,
                    width,
                    height);
                image.blendFlag = wi::enums::BLENDMODE_ALPHA;
                image.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
                wi::image::Draw(&brand_.GetTexture(), image, cmd);
                return;
            }

            DrawText("RENEGADE ENGINE", x, y + 17.0f,
                theme.fontHeaderMeta + 1, theme.textStrong, cmd);
        }

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
                if (!IsVisible()) return;
                const auto& theme = StoryFlowVisualTheme::Get();
                if (active_)
                {
                    DrawRect(translation.x, translation.y, scale.x, scale.y,
                        theme.selectionSurface, cmd);
                    DrawRect(translation.x, translation.y, 3.0f, scale.y,
                        theme.selection, cmd);
                }

                const float glyph = std::min(18.0f, scale.y * 0.34f);
                const float gx = translation.x + (scale.x - glyph) * 0.5f;
                const float gy = translation.y + 7.0f;
                const wi::Color icon = active_ ? theme.textStrong : theme.muted;
                DrawRect(gx, gy, glyph, 1.0f, icon, cmd);
                DrawRect(gx, gy + glyph - 1.0f, glyph, 1.0f, icon, cmd);
                DrawRect(gx, gy, 1.0f, glyph, icon, cmd);
                DrawRect(gx + glyph - 1.0f, gy, 1.0f, glyph, icon, cmd);

                const int size = std::max(6, theme.fontCardMeta - 1);
                if (label_ == "STORY FLOW")
                {
                    DrawText("STORY", translation.x + scale.x * 0.5f,
                        translation.y + scale.y - 25.0f, size,
                        theme.textStrong, cmd, wi::font::WIFALIGN_CENTER);
                    DrawText("FLOW", translation.x + scale.x * 0.5f,
                        translation.y + scale.y - 14.0f, size,
                        theme.selection, cmd, wi::font::WIFALIGN_CENTER);
                }
                else
                {
                    DrawText(label_, translation.x + scale.x * 0.5f,
                        translation.y + scale.y - 16.0f, size,
                        active_ ? theme.textStrong : theme.muted,
                        cmd, wi::font::WIFALIGN_CENTER);
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
        std::array<NavItem, 7> navItems_;
    };
}
