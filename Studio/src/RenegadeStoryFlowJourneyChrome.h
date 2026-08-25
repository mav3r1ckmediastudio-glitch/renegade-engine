#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowJourneyLayout.h"

namespace renegade::studio
{
    // Gate 9A fixed native shell. The Journey canvas is the only region whose
    // contents may pan or zoom; the header, rail, Inspector, controls and
    // overview remain in screen space exactly as the approved concept shows.
    class RenegadeStoryFlowJourneyChrome final : public wi::gui::Widget
    {
    public:
        enum class Action
        {
            Hub, StoryFlow, Levels, Screens, Assets, Variables, TestPlay,
            Select, Arrange, Filter, Search, Preview, Validate,
            Undo, Redo, ProjectSelector, Settings, MainMenu,
            ZoomOut, ZoomIn, Fit, Start,
        };

        static constexpr float NavigationRailWidth = 96.0f;
        static constexpr float WorkspaceHeaderHeight = 182.0f;
        static constexpr float PreferredInspectorWidth = 336.0f;

        void Create()
        {
            SetName("Renegade Story Flow Journey chrome");
            SetShadowRadius(0.0f);
            brandLockup_ = wi::resourcemanager::Load(
                "Content/ui/renegade-engine-fractured-crest-logo.png");
            SetLayout(width_, height_);
        }

        void OnAction(std::function<void(Action)> callback)
        {
            action_ = std::move(callback);
        }

        void OnZoomRequested(std::function<void(float)> callback)
        {
            zoomRequested_ = std::move(callback);
        }

        void SetProjectContext(
            std::string projectName,
            const bool dirty,
            const bool canUndo,
            const bool canRedo,
            const float zoom,
            const bool filterActive)
        {
            projectName_ = std::move(projectName);
            dirty_ = dirty;
            canUndo_ = canUndo;
            canRedo_ = canRedo;
            zoom_ = std::clamp(zoom, 0.82f, 1.18f);
            filterActive_ = filterActive;
        }

        void SetGraphViewActive(const bool active) noexcept
        {
            graphViewActive_ = active;
        }

        void SetLayout(const float width, const float height)
        {
            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);
            shell_ = ComputeJourneyShellLayout(width_, height_);
            SetSize(XMFLOAT2(width_, height_));

            const float navStep = std::clamp(
                (shell_.navigationRail.height - 26.0f) /
                    static_cast<float>(navBounds_.size()),
                64.0f, 82.0f);
            for (std::size_t i = 0; i < navBounds_.size(); ++i)
            {
                navBounds_[i] = {
                    8.0f,
                    shell_.navigationRail.y + 13.0f +
                        static_cast<float>(i) * navStep,
                    NavigationRailWidth - 16.0f,
                    std::min(70.0f, navStep - 4.0f)};
            }

            const float utilityWidth = width_ < 1440.0f ? 374.0f : 432.0f;
            utilityStartX_ = std::max(744.0f, width_ - utilityWidth);
            const float commandStart = std::clamp(
                utilityStartX_ - 474.0f, 432.0f, 566.0f);
            const float commandWidth = std::clamp(
                (utilityStartX_ - commandStart - 18.0f) / 6.0f,
                58.0f, 76.0f);
            for (std::size_t i = 0; i < commandBounds_.size(); ++i)
            {
                commandBounds_[i] = {
                    commandStart + static_cast<float>(i) * commandWidth,
                    5.0f,
                    commandWidth - 3.0f,
                    61.0f};
            }

            float utilityX = utilityStartX_ + 5.0f;
            savedBounds_ = {utilityX, 18.0f, 72.0f, 31.0f};
            utilityX += 79.0f;
            undoBounds_ = {utilityX, 18.0f, 31.0f, 31.0f};
            redoBounds_ = {utilityX + 35.0f, 18.0f, 31.0f, 31.0f};
            utilityX += 74.0f;
            const float projectWidth = width_ < 1440.0f ? 104.0f : 154.0f;
            projectBounds_ = {utilityX, 16.0f, projectWidth, 35.0f};
            utilityX += projectWidth + 8.0f;
            settingsBounds_ = {utilityX, 18.0f, 30.0f, 31.0f};
            menuBounds_ = {utilityX + 35.0f, 18.0f, 30.0f, 31.0f};

            const auto& nav = shell_.canvasNavigation;
            const float controlY = nav.y + 6.0f;
            startBounds_ = {nav.x + 6.0f, controlY, 84.0f, 34.0f};
            zoomOutBounds_ = {nav.x + 96.0f, controlY, 34.0f, 34.0f};
            zoomInBounds_ = {nav.x + nav.width - 78.0f, controlY, 34.0f, 34.0f};
            fitBounds_ = {nav.x + nav.width - 39.0f, controlY, 36.0f, 34.0f};
            zoomTrackBounds_ = {
                zoomOutBounds_.Right() + 12.0f,
                nav.y + 17.0f,
                std::max(34.0f,
                    zoomInBounds_.x - zoomOutBounds_.Right() - 24.0f),
                13.0f};
        }

        [[nodiscard]] const JourneyShellLayout& ShellLayout() const noexcept
        {
            return shell_;
        }

        [[nodiscard]] float WorkspaceLeftInset() const noexcept
        {
            return shell_.workspace.x;
        }

        [[nodiscard]] bool OwnsPointer(const XMFLOAT4& pointer) const noexcept
        {
            return Contains(shell_.topBar, pointer) ||
                Contains(shell_.navigationRail, pointer) ||
                Contains(shell_.inspector, pointer) ||
                (!graphViewActive_ &&
                    (Contains(shell_.canvasNavigation, pointer) ||
                        Contains(shell_.storyOverview, pointer)));
        }

        [[nodiscard]] bool CanvasOverlayOwnsPointer(
            const XMFLOAT4& pointer) const noexcept
        {
            return Contains(shell_.topBar, pointer) ||
                Contains(shell_.navigationRail, pointer) ||
                (!graphViewActive_ &&
                    (Contains(shell_.canvasNavigation, pointer) ||
                        Contains(shell_.storyOverview, pointer)));
        }

        void Update(const wi::Canvas&, float) override
        {
            if (!IsVisible() || !IsEnabled() || !action_ ||
                !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
                return;

            const XMFLOAT4 pointer = wi::input::GetPointer();
            static constexpr std::array<Action, 7> navActions = {
                Action::Hub, Action::StoryFlow, Action::Levels,
                Action::Screens, Action::Assets, Action::Variables,
                Action::TestPlay};
            for (std::size_t i = 0; i < navBounds_.size(); ++i)
            {
                if (Contains(navBounds_[i], pointer))
                {
                    action_(navActions[i]);
                    return;
                }
            }

            static constexpr std::array<Action, 6> commandActions = {
                Action::Select, Action::Arrange, Action::Filter,
                Action::Search, Action::Preview, Action::Validate};
            for (std::size_t i = 0; i < commandBounds_.size(); ++i)
            {
                if (Contains(commandBounds_[i], pointer))
                {
                    action_(commandActions[i]);
                    return;
                }
            }

            if (Contains(undoBounds_, pointer) && canUndo_)
                action_(Action::Undo);
            else if (Contains(redoBounds_, pointer) && canRedo_)
                action_(Action::Redo);
            else if (Contains(projectBounds_, pointer))
                action_(Action::ProjectSelector);
            else if (!graphViewActive_ && Contains(startBounds_, pointer))
                action_(Action::Start);
            else if (!graphViewActive_ && Contains(zoomOutBounds_, pointer))
                action_(Action::ZoomOut);
            else if (!graphViewActive_ &&
                Contains(zoomTrackBounds_, pointer) && zoomRequested_)
            {
                const float fraction = std::clamp(
                    (pointer.x - zoomTrackBounds_.x) /
                        std::max(1.0f, zoomTrackBounds_.width),
                    0.0f, 1.0f);
                zoomRequested_(0.82f + fraction * (1.18f - 0.82f));
            }
            else if (!graphViewActive_ && Contains(zoomInBounds_, pointer))
                action_(Action::ZoomIn);
            else if (!graphViewActive_ && Contains(fitBounds_, pointer))
                action_(Action::Fit);
        }

        void Render(
            const wi::Canvas&,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible()) return;

            Rect(shell_.workspaceTitle.x, shell_.workspaceTitle.y,
                shell_.workspaceTitle.width, shell_.workspaceTitle.height,
                Background, cmd);
            Rect(shell_.navigationRail.x, shell_.navigationRail.y,
                shell_.navigationRail.width, shell_.navigationRail.height,
                RailSurface, cmd);
            Rect(shell_.topBar.x, shell_.topBar.y,
                shell_.topBar.width, shell_.topBar.height, HeaderSurface, cmd);
            Rect(shell_.inspector.x, shell_.inspector.y,
                shell_.inspector.width, 35.0f,
                InspectorSurface, cmd);
            Rect(0.0f, shell_.topBar.Bottom() - 1.0f, width_, 1.0f, Border, cmd);
            Rect(shell_.navigationRail.Right() - 1.0f,
                shell_.navigationRail.y, 1.0f,
                shell_.navigationRail.height, Border, cmd);
            Rect(shell_.inspector.x, shell_.inspector.y, 1.0f,
                shell_.inspector.height, Border, cmd);

            RenderBrand(cmd);
            RenderTopCommands(cmd);
            RenderTopUtilities(cmd);
            RenderNavigationRail(cmd);

            Text(projectName_.empty() ? "UNTITLED PROJECT" : Upper(projectName_),
                shell_.workspaceTitle.x + 20.0f,
                shell_.workspaceTitle.y + 20.0f, 18, TextStrong, cmd);
            Text("Story Flow", shell_.workspaceTitle.x + 20.0f,
                shell_.workspaceTitle.y + 50.0f, 10, TextSecondary, cmd);

            Text("INSPECTOR", shell_.inspector.x + 16.0f,
                shell_.inspector.y + 14.0f, 13, TextStrong, cmd);
            Text("x", shell_.inspector.Right() - 21.0f,
                shell_.inspector.y + 14.0f, 11, Muted, cmd);
            Rect(shell_.inspector.x, shell_.inspector.y + 34.0f,
                shell_.inspector.width, 1.0f, BorderSoft, cmd);

            if (!graphViewActive_)
            {
                RenderCanvasNavigation(cmd);
                RenderOverviewFrame(cmd);
            }
        }

        const char* GetWidgetTypeName() const override
        {
            return "RenegadeStoryFlowJourneyChrome";
        }

    private:
        static constexpr wi::Color Background = wi::Color(5, 9, 13, 255);
        static constexpr wi::Color HeaderSurface = wi::Color(7, 12, 17, 255);
        static constexpr wi::Color RailSurface = wi::Color(6, 11, 15, 255);
        static constexpr wi::Color InspectorSurface = wi::Color(9, 15, 21, 255);
        static constexpr wi::Color Surface2 = wi::Color(14, 22, 29, 255);
        static constexpr wi::Color Border = wi::Color(31, 46, 58, 255);
        static constexpr wi::Color BorderSoft = wi::Color(20, 33, 42, 255);
        static constexpr wi::Color TextStrong = wi::Color(244, 247, 249, 255);
        static constexpr wi::Color TextSecondary = wi::Color(173, 185, 193, 255);
        static constexpr wi::Color Muted = wi::Color(103, 121, 132, 255);
        static constexpr wi::Color Blue = wi::Color(65, 158, 230, 255);
        static constexpr wi::Color Green = wi::Color(113, 205, 111, 255);

        static bool Contains(const JourneyUiRect& bounds, const XMFLOAT4& pointer)
        {
            return pointer.x >= bounds.x && pointer.y >= bounds.y &&
                pointer.x < bounds.Right() && pointer.y < bounds.Bottom();
        }

        static void Rect(float x, float y, float width, float height,
            wi::Color color, wi::graphics::CommandList cmd)
        {
            if (width <= 0.0f || height <= 0.0f) return;
            wi::image::Params params(x, y, width, height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            wi::image::Draw(nullptr, params, cmd);
        }

        static void Rounded(const JourneyUiRect& bounds, float radius,
            wi::Color color, wi::graphics::CommandList cmd)
        {
            if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
            wi::image::Params params(
                bounds.x, bounds.y, bounds.width, bounds.height, color);
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            params.enableCornerRounding();
            for (auto& corner : params.corners_rounding)
            {
                corner.radius = radius;
                corner.segments = 8;
            }
            wi::image::Draw(nullptr, params, cmd);
        }

        static void Text(const std::string& value, float x, float y, int size,
            wi::Color color, wi::graphics::CommandList cmd,
            wi::font::Alignment align = wi::font::WIFALIGN_LEFT)
        {
            wi::font::Params params(x, y, size, align,
                wi::font::WIFALIGN_TOP, color, wi::Color::Transparent());
            params.bolden = 0.14f;
            params.spacingX = 0.15f;
            wi::font::Draw(value, params, cmd);
        }

        static std::string Upper(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](const unsigned char c)
                {
                    return static_cast<char>(std::toupper(c));
                });
            return value;
        }

        static void Glyph(const std::string& glyph, const JourneyUiRect& bounds,
            wi::Color color, wi::graphics::CommandList cmd)
        {
            const float cx = bounds.x + bounds.width * 0.5f;
            const float cy = bounds.y + 19.0f;
            if (glyph == "flow")
            {
                Rounded({cx - 11.0f, cy - 5.0f, 8.0f, 8.0f}, 4.0f, color, cmd);
                Rounded({cx + 3.0f, cy - 5.0f, 8.0f, 8.0f}, 4.0f, color, cmd);
                Rounded({cx - 4.0f, cy + 7.0f, 8.0f, 8.0f}, 4.0f, color, cmd);
                Rect(cx - 4.0f, cy - 1.0f, 8.0f, 1.0f, color, cmd);
                Rect(cx - 1.0f, cy + 2.0f, 1.0f, 6.0f, color, cmd);
            }
            else if (glyph == "screen")
            {
                Rect(cx - 11.0f, cy - 7.0f, 22.0f, 14.0f, color, cmd);
                Rect(cx - 9.0f, cy - 5.0f, 18.0f, 10.0f,
                    RailSurface, cmd);
                Rect(cx - 1.0f, cy + 7.0f, 2.0f, 5.0f, color, cmd);
                Rect(cx - 6.0f, cy + 11.0f, 12.0f, 1.0f, color, cmd);
            }
            else if (glyph == "home")
            {
                Rect(cx - 10.0f, cy - 3.0f, 20.0f, 2.0f, color, cmd);
                Rect(cx - 7.0f, cy - 6.0f, 14.0f, 3.0f, color, cmd);
                Rect(cx - 9.0f, cy - 1.0f, 18.0f, 11.0f, color, cmd);
                Rect(cx - 2.0f, cy + 3.0f, 4.0f, 7.0f, RailSurface, cmd);
            }
            else if (glyph == "levels")
            {
                Rect(cx - 11.0f, cy + 8.0f, 22.0f, 2.0f, color, cmd);
                Rect(cx - 8.0f, cy + 2.0f, 16.0f, 2.0f, color, cmd);
                Rect(cx - 4.0f, cy - 4.0f, 8.0f, 2.0f, color, cmd);
                Rounded({cx - 2.0f, cy - 10.0f, 4.0f, 4.0f}, 2.0f, color, cmd);
            }
            else if (glyph == "assets")
            {
                Rounded({cx - 10.0f, cy - 9.0f, 20.0f, 20.0f},
                    3.0f, color, cmd);
                Rounded({cx - 7.0f, cy - 6.0f, 6.0f, 6.0f},
                    2.0f, RailSurface, cmd);
                Rounded({cx + 1.0f, cy - 6.0f, 6.0f, 6.0f},
                    2.0f, RailSurface, cmd);
                Rounded({cx - 7.0f, cy + 2.0f, 14.0f, 6.0f},
                    2.0f, RailSurface, cmd);
            }
            else if (glyph == "variables")
            {
                Rect(cx - 9.0f, cy - 8.0f, 2.0f, 18.0f, color, cmd);
                Rect(cx + 7.0f, cy - 8.0f, 2.0f, 18.0f, color, cmd);
                Rounded({cx - 3.0f, cy - 6.0f, 6.0f, 6.0f},
                    3.0f, color, cmd);
                Rounded({cx - 3.0f, cy + 3.0f, 6.0f, 6.0f},
                    3.0f, color, cmd);
            }
            else if (glyph == "play")
            {
                Rounded({cx - 11.0f, cy - 10.0f, 22.0f, 22.0f},
                    11.0f, color, cmd);
                Rounded({cx - 9.0f, cy - 8.0f, 18.0f, 18.0f},
                    9.0f, RailSurface, cmd);
                Text(">", cx + 1.0f, cy - 5.0f, 9, color, cmd,
                    wi::font::WIFALIGN_CENTER);
            }
        }

        static void CommandGlyph(const std::size_t index,
            const JourneyUiRect& bounds, const wi::Color color,
            const wi::graphics::CommandList cmd)
        {
            const float cx = bounds.x + bounds.width * 0.5f;
            const float cy = bounds.y + 20.0f;
            switch (index)
            {
            case 0:
                Rect(cx - 7.0f, cy - 9.0f, 3.0f, 18.0f, color, cmd);
                Rect(cx - 4.0f, cy - 6.0f, 4.0f, 3.0f, color, cmd);
                Rect(cx - 1.0f, cy - 3.0f, 4.0f, 3.0f, color, cmd);
                Rect(cx + 2.0f, cy, 4.0f, 3.0f, color, cmd);
                break;
            case 1:
                for (const float x : {-7.0f, 5.0f})
                    for (const float y : {-6.0f, 6.0f})
                        Rounded({cx + x - 3.0f, cy + y - 3.0f,
                            6.0f, 6.0f}, 3.0f, color, cmd);
                break;
            case 2:
                Rect(cx - 10.0f, cy - 8.0f, 20.0f, 2.0f, color, cmd);
                Rect(cx - 7.0f, cy - 2.0f, 14.0f, 2.0f, color, cmd);
                Rect(cx - 3.0f, cy + 4.0f, 6.0f, 7.0f, color, cmd);
                break;
            case 3:
                Rounded({cx - 9.0f, cy - 9.0f, 16.0f, 16.0f},
                    8.0f, color, cmd);
                Rounded({cx - 6.0f, cy - 6.0f, 10.0f, 10.0f},
                    5.0f, HeaderSurface, cmd);
                Rect(cx + 5.0f, cy + 5.0f, 7.0f, 2.0f, color, cmd);
                Rect(cx + 9.0f, cy + 5.0f, 2.0f, 6.0f, color, cmd);
                break;
            case 4:
                Rounded({cx - 11.0f, cy - 10.0f, 22.0f, 22.0f},
                    11.0f, color, cmd);
                Rounded({cx - 9.0f, cy - 8.0f, 18.0f, 18.0f},
                    9.0f, HeaderSurface, cmd);
                Text(">", cx + 1.0f, cy - 5.0f, 9, color, cmd,
                    wi::font::WIFALIGN_CENTER);
                break;
            case 5:
                Rounded({cx - 10.0f, cy - 9.0f, 20.0f, 20.0f},
                    10.0f, color, cmd);
                Rounded({cx - 8.0f, cy - 7.0f, 16.0f, 16.0f},
                    8.0f, HeaderSurface, cmd);
                Rect(cx - 4.0f, cy + 1.0f, 4.0f, 2.0f, color, cmd);
                Rect(cx, cy - 3.0f, 2.0f, 6.0f, color, cmd);
                Rect(cx + 2.0f, cy - 5.0f, 5.0f, 2.0f, color, cmd);
                break;
            default:
                break;
            }
        }

        void RenderBrand(wi::graphics::CommandList cmd) const
        {
            if (brandLockup_.IsValid())
            {
                const auto desc = brandLockup_.GetTexture().GetDesc();
                constexpr float maxWidth = 151.0f;
                constexpr float maxHeight = 62.0f;
                float drawWidth = maxWidth;
                float drawHeight = maxHeight;
                if (desc.width > 0 && desc.height > 0)
                {
                    const float aspect =
                        static_cast<float>(desc.width) /
                        static_cast<float>(desc.height);
                    drawWidth = drawHeight * aspect;
                    if (drawWidth > maxWidth)
                    {
                        drawWidth = maxWidth;
                        drawHeight = drawWidth / aspect;
                    }
                }
                wi::image::Params logo(
                    18.0f,
                    (70.0f - drawHeight) * 0.5f,
                    drawWidth,
                    drawHeight);
                logo.blendFlag = wi::enums::BLENDMODE_ALPHA;
                logo.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
                wi::image::Draw(&brandLockup_.GetTexture(), logo, cmd);
            }
            else
            {
                Text("RENEGADE STUDIO", 20.0f, 24.0f, 11, TextStrong, cmd);
            }
            Rect(184.0f, 14.0f, 1.0f, 42.0f, Border, cmd);
            if (utilityStartX_ > 1000.0f)
            {
                Text(projectName_.empty() ? "UNTITLED" : projectName_,
                    203.0f, 20.0f, 11, TextStrong, cmd);
                Text("Story Flow", 203.0f, 39.0f, 8, TextSecondary, cmd);
            }
        }

        void RenderTopCommands(wi::graphics::CommandList cmd) const
        {
            static constexpr std::array<const char*, 6> labels = {
                "SELECT", "ARRANGE", "FILTER", "SEARCH", "PREVIEW", "VALIDATE"};
            for (std::size_t i = 0; i < commandBounds_.size(); ++i)
            {
                const bool active = i == 0 || (i == 2 && filterActive_);
                if (active)
                {
                    Rounded(commandBounds_[i], 4.0f,
                        wi::Color(15, 34, 50, 255), cmd);
                    Rect(commandBounds_[i].x + 10.0f,
                        commandBounds_[i].Bottom() - 2.0f,
                        commandBounds_[i].width - 20.0f, 2.0f, Blue, cmd);
                }
                CommandGlyph(i, commandBounds_[i],
                    active ? TextStrong : TextSecondary, cmd);
                Text(labels[i], commandBounds_[i].x + commandBounds_[i].width * 0.5f,
                    commandBounds_[i].y + 39.0f, 6,
                    active ? TextStrong : Muted, cmd,
                    wi::font::WIFALIGN_CENTER);
            }
        }

        void RenderTopUtilities(wi::graphics::CommandList cmd) const
        {
            Rounded(savedBounds_, 4.0f, wi::Color(8, 17, 18, 255), cmd);
            Rounded({savedBounds_.x + 10.0f, savedBounds_.y + 13.0f, 7.0f, 7.0f},
                4.0f, dirty_ ? wi::Color(229, 165, 82, 255) : Green, cmd);
            Text(dirty_ ? "Unsaved" : "Saved", savedBounds_.x + 23.0f,
                savedBounds_.y + 9.0f, 8,
                dirty_ ? TextSecondary : Green, cmd);
            Text("<", undoBounds_.x + 15.0f, undoBounds_.y + 7.0f, 12,
                canUndo_ ? TextSecondary : Muted, cmd, wi::font::WIFALIGN_CENTER);
            Text(">", redoBounds_.x + 15.0f, redoBounds_.y + 7.0f, 12,
                canRedo_ ? TextSecondary : Muted, cmd, wi::font::WIFALIGN_CENTER);
            Rounded(projectBounds_, 4.0f, Surface2, cmd);
            Text(projectName_.empty() ? "PROJECT" : projectName_,
                projectBounds_.x + 10.0f, projectBounds_.y + 11.0f, 8,
                TextSecondary, cmd);
            Text("v", projectBounds_.Right() - 13.0f,
                projectBounds_.y + 10.0f, 8, Muted, cmd);
            Rounded({settingsBounds_.x + 7.0f, settingsBounds_.y + 7.0f,
                16.0f, 16.0f}, 8.0f, Muted, cmd);
            Rounded({settingsBounds_.x + 11.0f, settingsBounds_.y + 11.0f,
                8.0f, 8.0f}, 4.0f, HeaderSurface, cmd);
            for (int bar = 0; bar < 3; ++bar)
                Rect(menuBounds_.x + 7.0f,
                    menuBounds_.y + 8.0f + static_cast<float>(bar) * 6.0f,
                    17.0f, 2.0f, Muted, cmd);
            Text("-", settingsBounds_.x + 15.0f,
                settingsBounds_.y + 23.0f, 5, Muted, cmd,
                wi::font::WIFALIGN_CENTER);
            Text("-", menuBounds_.x + 15.0f,
                menuBounds_.y + 23.0f, 5, Muted, cmd,
                wi::font::WIFALIGN_CENTER);
        }

        void RenderNavigationRail(wi::graphics::CommandList cmd) const
        {
            static constexpr std::array<const char*, 7> labels = {
                "HUB", "STORY FLOW", "LEVELS", "SCREENS",
                "ASSETS", "VARIABLES", "TEST PLAY"};
            static constexpr std::array<const char*, 7> glyphs = {
                "home", "flow", "levels", "screen",
                "assets", "variables", "play"};
            for (std::size_t i = 0; i < navBounds_.size(); ++i)
            {
                const bool active = i == 1;
                const bool unavailable = i == 5;
                if (active)
                {
                    Rounded(navBounds_[i], 5.0f, wi::Color(13, 34, 50, 255), cmd);
                    Rect(navBounds_[i].x, navBounds_[i].y,
                        3.0f, navBounds_[i].height, Blue, cmd);
                }
                const wi::Color colour = unavailable
                    ? wi::Color(73, 87, 96, 255)
                    : (active ? TextStrong : TextSecondary);
                Glyph(glyphs[i], navBounds_[i], colour, cmd);
                Text(labels[i], navBounds_[i].x + navBounds_[i].width * 0.5f,
                    navBounds_[i].y + navBounds_[i].height - 17.0f,
                    std::string(labels[i]).size() > 8 ? 6 : 7,
                    active ? TextStrong : colour, cmd, wi::font::WIFALIGN_CENTER);
                if (unavailable)
                {
                    Text("NOT YET", navBounds_[i].x + navBounds_[i].width * 0.5f,
                        navBounds_[i].y + navBounds_[i].height - 8.0f,
                        5, Muted, cmd, wi::font::WIFALIGN_CENTER);
                }
            }
        }

        void RenderCanvasNavigation(wi::graphics::CommandList cmd) const
        {
            const auto& bounds = shell_.canvasNavigation;
            Rounded(bounds, 6.0f, wi::Color(8, 15, 20, 242), cmd);
            Rounded(startBounds_, 4.0f, Surface2, cmd);
            Text("START", startBounds_.x + startBounds_.width * 0.5f,
                startBounds_.y + 11.0f, 7, TextSecondary, cmd,
                wi::font::WIFALIGN_CENTER);
            Rounded(zoomOutBounds_, 4.0f, Surface2, cmd);
            Text("-", zoomOutBounds_.x + 17.0f, zoomOutBounds_.y + 7.0f,
                13, TextSecondary, cmd, wi::font::WIFALIGN_CENTER);
            const float fraction = std::clamp(
                (zoom_ - 0.82f) / (1.18f - 0.82f), 0.0f, 1.0f);
            Rounded({zoomTrackBounds_.x, bounds.y + 21.0f,
                zoomTrackBounds_.width, 4.0f},
                2.0f, Border, cmd);
            Rounded({zoomTrackBounds_.x, bounds.y + 21.0f,
                zoomTrackBounds_.width * fraction, 4.0f},
                2.0f, Blue, cmd);
            Rounded({zoomTrackBounds_.x + zoomTrackBounds_.width * fraction - 4.0f,
                bounds.y + 18.0f, 8.0f, 10.0f}, 4.0f, TextStrong, cmd);
            Text(std::to_string(static_cast<int>(zoom_ * 100.0f + 0.5f)) + "%",
                zoomTrackBounds_.x + zoomTrackBounds_.width * 0.5f,
                bounds.y + 7.0f, 6, TextSecondary, cmd,
                wi::font::WIFALIGN_CENTER);
            Rounded(zoomInBounds_, 4.0f, Surface2, cmd);
            Text("+", zoomInBounds_.x + 17.0f, zoomInBounds_.y + 7.0f,
                13, TextSecondary, cmd, wi::font::WIFALIGN_CENTER);
            Rounded(fitBounds_, 4.0f, Surface2, cmd);
            Text("FIT", fitBounds_.x + fitBounds_.width * 0.5f,
                fitBounds_.y + 11.0f, 7, TextSecondary, cmd,
                wi::font::WIFALIGN_CENTER);
        }

        void RenderOverviewFrame(wi::graphics::CommandList cmd) const
        {
            const auto& bounds = shell_.storyOverview;
            Rect(bounds.x, bounds.y, bounds.width, 1.0f, Border, cmd);
            Rect(bounds.x, bounds.Bottom() - 1.0f, bounds.width, 1.0f, Border, cmd);
            Rect(bounds.x, bounds.y, 1.0f, bounds.height, Border, cmd);
            Rect(bounds.Right() - 1.0f, bounds.y, 1.0f, bounds.height, Border, cmd);
            Rect(bounds.x + 1.0f, bounds.y + 1.0f,
                bounds.width - 2.0f, 23.0f, wi::Color(8, 15, 20, 244), cmd);
            Text("STORY OVERVIEW", bounds.x + 10.0f,
                bounds.y + 8.0f, 6, Muted, cmd);
        }

        float width_ = 1920.0f;
        float height_ = 1080.0f;
        float utilityStartX_ = 1500.0f;
        JourneyShellLayout shell_;
        std::array<JourneyUiRect, 7> navBounds_ = {};
        std::array<JourneyUiRect, 6> commandBounds_ = {};
        JourneyUiRect savedBounds_;
        JourneyUiRect undoBounds_;
        JourneyUiRect redoBounds_;
        JourneyUiRect projectBounds_;
        JourneyUiRect settingsBounds_;
        JourneyUiRect menuBounds_;
        JourneyUiRect startBounds_;
        JourneyUiRect zoomOutBounds_;
        JourneyUiRect zoomTrackBounds_;
        JourneyUiRect zoomInBounds_;
        JourneyUiRect fitBounds_;
        wi::Resource brandLockup_;
        std::string projectName_ = "The Paganacht";
        bool dirty_ = false;
        bool canUndo_ = false;
        bool canRedo_ = false;
        float zoom_ = 1.0f;
        bool filterActive_ = false;
        bool graphViewActive_ = false;
        std::function<void(Action)> action_;
        std::function<void(float)> zoomRequested_;
    };
}
