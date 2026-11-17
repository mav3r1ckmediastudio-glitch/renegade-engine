#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "RenegadeStoryFlowJourneyLayout.h"
#include "StudioIcons.h"

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
            Select, Arrange, Filter, Search, BuildGame, Validate,
            Undo, Redo, ProjectSelector, Settings, MainMenu,
            ZoomOut, ZoomIn, Fit, Start,
        };

        static constexpr float NavigationRailWidth = 96.0f;
        static constexpr float WorkspaceHeaderHeight = JourneyWorkspaceHeaderHeight;
        static constexpr float PreferredInspectorWidth = 336.0f;

        void Create()
        {
            SetName("Renegade Story Flow Journey chrome");
            icons::Load();
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
            zoom_ = std::clamp(zoom, 0.05f, 1.18f);
            filterActive_ = filterActive;
        }

        void SetGraphViewActive(const bool active) noexcept
        {
            graphViewActive_ = active;
        }

        void SetProjectCommandState(
            const bool testGameRunning,
            const bool buildGameBusy) noexcept
        {
            testGameRunning_ = testGameRunning;
            buildGameBusy_ = buildGameBusy;
        }

        void SetLayout(const float width, const float height)
        {
            width_ = std::max(1.0f, width);
            height_ = std::max(1.0f, height);
            shell_ = ComputeJourneyShellLayout(width_, height_);
            SetSize(XMFLOAT2(width_, height_));

            for(std::size_t i=0;i<navBounds_.size();++i)
                navBounds_[i]={32.f,shell_.navigationRail.y+16.f+float(i)*40.f,32,32};
            utilityStartX_=std::max(744.f,width_-432.f);
            for(std::size_t i=0;i<commandBounds_.size();++i)
                commandBounds_[i]={432.f+float(i)*36.f,19,32,32};
            float x=utilityStartX_+5.f;
            savedBounds_={x,19,72,32};x+=80;
            undoBounds_={x,19,32,32};redoBounds_={x+36,19,32,32};x+=80;
            projectBounds_={x,19,32,32};x+=40;
            settingsBounds_={x,19,32,32};menuBounds_={x+36,19,32,32};

            const auto& nav = shell_.canvasNavigation;
            const float controlY = nav.y + 6.0f;
            startBounds_ = {nav.x + 6.0f, controlY, 32.0f, 32.0f};
            zoomOutBounds_ = {nav.x + 46.0f, controlY, 32.0f, 32.0f};
            zoomInBounds_ = {nav.x + nav.width - 78.0f, controlY, 32.0f, 32.0f};
            fitBounds_ = {nav.x + nav.width - 39.0f, controlY, 32.0f, 32.0f};
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

        void Update(const wi::Canvas&, float dt) override
        {
            const int command=interaction_.Update(Controls(),dt,IsVisible()&&IsEnabled()&&bool(action_));
            if(command>=0 && action_)action_(static_cast<Action>(command));
            if(!IsVisible() || !IsEnabled() || !action_)return;
            const auto p=wi::input::GetPointer();
            if(!graphViewActive_ && wi::input::Press(wi::input::MOUSE_BUTTON_LEFT)) {
                if(Contains(shell_.storyOverview,p)&&wi::input::IsDoubleClicked())action_(Action::Fit);
                else if(Contains(zoomTrackBounds_,p)&&zoomRequested_) {
                    const float fraction=std::clamp((p.x-zoomTrackBounds_.x)/std::max(1.f,zoomTrackBounds_.width),0.f,1.f);
                    zoomRequested_(0.05f + fraction * (1.18f - 0.05f));
                }
            }
        }
        void RenderIconTooltip(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const
        {
            if(IsVisible()&&IsEnabled())interaction_.RenderTooltip(canvas,cmd);
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
        static constexpr wi::Color Background = theme::Canvas;
        static constexpr wi::Color HeaderSurface = theme::Panel;
        static constexpr wi::Color RailSurface = theme::Canvas;
        static constexpr wi::Color InspectorSurface = theme::Panel;
        static constexpr wi::Color Surface2 = theme::Control;
        static constexpr wi::Color Border = theme::Border;
        static constexpr wi::Color BorderSoft = theme::Idle;
        static constexpr wi::Color TextStrong = theme::Text;
        static constexpr wi::Color TextSecondary = theme::Muted;
        static constexpr wi::Color Muted = theme::DisabledText;
        static constexpr wi::Color Blue = theme::Accent;
        static constexpr wi::Color Green = wi::Color(113, 205, 111, 255);
        static constexpr wi::Color Orange = theme::Accent;

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

        icons::Interaction interaction_;
        static XMFLOAT4 Bounds(const JourneyUiRect& b) {return {b.x,b.y,b.width,b.height};}
        std::vector<icons::Control> Controls() const
        {
            std::vector<icons::Control> c;
            const Action nav[]={Action::Hub,Action::StoryFlow,Action::Levels,Action::Screens,Action::Assets,Action::Variables,Action::TestPlay};
            const char* keys[]={"hub","storyflow","levels","screens","assets","variables",testGameRunning_?"stop":"play"};
            for(int i=0;i<7;++i)c.push_back({int(nav[i]),keys[i],Bounds(navBounds_[i]),i!=5,i==1||(i==6&&testGameRunning_),
                i==5?"Variables (not available yet)":i==6?(testGameRunning_?"Stop Test Game":"Test Game"):icons::Title(keys[i])});
            const Action actions[]={Action::Select,Action::Arrange,Action::Filter,Action::Search,Action::BuildGame,Action::Validate};
            const char* commands[]={"select","arrange","filter","search","build-game","validate"};
            for(int i=0;i<6;++i)c.push_back({int(actions[i]),commands[i],Bounds(commandBounds_[i]),!(graphViewActive_&&i==2),
                i==0||(i==2&&filterActive_)||(i==4&&buildGameBusy_)});
            c.push_back({int(Action::Undo),"undo",Bounds(undoBounds_),canUndo_});
            c.push_back({int(Action::Redo),"redo",Bounds(redoBounds_),canRedo_});
            c.push_back({int(Action::ProjectSelector),"hub",Bounds(projectBounds_),true,false,"Project: "+projectName_});
            c.push_back({int(Action::Settings),"settings",Bounds(settingsBounds_),false,false,"Settings (not available yet)"});
            c.push_back({int(Action::MainMenu),"layers",Bounds(menuBounds_),false,false,"Menu (not available yet)"});
            if(!graphViewActive_) {
                c.push_back({int(Action::Start),"game-start",Bounds(startBounds_)});
                c.push_back({int(Action::ZoomOut),"",Bounds(zoomOutBounds_),true,false,"Zoom out"});
                c.push_back({int(Action::ZoomIn),"",Bounds(zoomInBounds_),true,false,"Zoom in"});
                c.push_back({int(Action::Fit),"fit",Bounds(fitBounds_)});
            }
            return c;
        }
        void DrawControl(const icons::Control& c,wi::graphics::CommandList cmd) const
        {
            icons::Button(c,icons::Contains(c.bounds,wi::input::GetPointer()),interaction_.Held(c.command),interaction_.Focused(c.command),cmd);
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
            for(const auto& c:Controls())
                if(c.command>=int(Action::Select)&&c.command<=int(Action::Validate))DrawControl(c,cmd);
        }
        void RenderTopUtilities(wi::graphics::CommandList cmd) const
        {
            Rounded(savedBounds_,4.f,theme::Idle,cmd);
            Text(dirty_?"Unsaved":"Saved",savedBounds_.x+10,savedBounds_.y+9,10,dirty_?Orange:Green,cmd);
            for(const auto& c:Controls())
                if(c.command>=int(Action::Undo)&&c.command<=int(Action::MainMenu))DrawControl(c,cmd);
        }
        void RenderNavigationRail(wi::graphics::CommandList cmd) const
        {
            for(const auto& c:Controls())if(c.command<=int(Action::TestPlay))DrawControl(c,cmd);
        }

        void RenderCanvasNavigation(wi::graphics::CommandList cmd) const
        {
            const auto& bounds = shell_.canvasNavigation;
            Rounded(bounds, 6.0f, wi::Color(8, 15, 20, 242), cmd);
            for(const auto& c:Controls())if(c.command>=int(Action::ZoomOut)) {
                DrawControl(c,cmd);
                if(c.icon.empty())Text(c.command==int(Action::ZoomOut)?"-":"+",c.bounds.x+16,c.bounds.y+8,14,theme::Text,cmd,wi::font::WIFALIGN_CENTER);
            }
            const float fraction = std::clamp(
                (zoom_ - 0.05f) / (1.18f - 0.05f), 0.0f, 1.0f);
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
                bounds.y + 6.0f, 8, TextSecondary, cmd,
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
            Text("OVERVIEW // DOUBLE-CLICK FIT", bounds.x + 10.0f,
                bounds.y + 7.0f, 8, TextSecondary, cmd);
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
        bool testGameRunning_ = false;
        bool buildGameBusy_ = false;
        std::function<void(Action)> action_;
        std::function<void(float)> zoomRequested_;
    };
}
