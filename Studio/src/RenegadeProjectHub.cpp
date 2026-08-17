#include "RenegadeProjectHub.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <utility>

namespace
{
    constexpr float DesignWidth = 1672.0f;
    constexpr float DesignHeight = 941.0f;

    constexpr wi::Color Ink = wi::Color(4, 9, 13, 248);
    constexpr wi::Color InkSolid = wi::Color(4, 9, 13, 255);
    constexpr wi::Color Surface = wi::Color(7, 16, 22, 248);
    constexpr wi::Color SurfaceRaised = wi::Color(9, 22, 29, 250);
    constexpr wi::Color Border = wi::Color(42, 76, 90, 230);
    constexpr wi::Color Cyan = wi::Color(101, 207, 235, 255);
    constexpr wi::Color CyanSoft = wi::Color(74, 154, 181, 220);
    constexpr wi::Color Orange = wi::Color(224, 105, 31, 255);
    constexpr wi::Color Text = wi::Color(220, 226, 228, 255);
    constexpr wi::Color TextStrong = wi::Color(245, 248, 249, 255);
    constexpr wi::Color Muted = wi::Color(122, 145, 154, 255);
    constexpr wi::Color Success = wi::Color(82, 205, 121, 255);
    constexpr wi::Color Danger = wi::Color(214, 86, 62, 255);

    // Hit regions are authored against the owner-approved 1672x941 Hub plate.
    // Keep these aligned with the visible controls rather than the rejected
    // first-pass layout.
    const XMFLOAT4 LeftNew = XMFLOAT4(100.0f, 178.0f, 382.0f, 273.0f);
    const XMFLOAT4 LeftOpen = XMFLOAT4(100.0f, 278.0f, 382.0f, 375.0f);
    const XMFLOAT4 LeftBack = XMFLOAT4(100.0f, 498.0f, 382.0f, 604.0f);
    const XMFLOAT4 LeftExit = XMFLOAT4(100.0f, 716.0f, 382.0f, 810.0f);
    const XMFLOAT4 Featured = XMFLOAT4(408.0f, 186.0f, 1147.0f, 636.0f);
    const XMFLOAT4 LowerNew = XMFLOAT4(408.0f, 651.0f, 1147.0f, 817.0f);
    const XMFLOAT4 LowerRecent0 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    const XMFLOAT4 LowerRecent1 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    const XMFLOAT4 LowerRecent2 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    const XMFLOAT4 OpenSelected = XMFLOAT4(1188.0f, 733.0f, 1538.0f, 810.0f);

    void DrawRect(
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

    void DrawBorderedRect(
        const XMFLOAT4& rect,
        const wi::Color fill,
        const wi::Color edge,
        const wi::graphics::CommandList cmd)
    {
        DrawRect(rect.x, rect.y, rect.z - rect.x, rect.w - rect.y, edge, cmd);
        DrawRect(
            rect.x + 1.0f,
            rect.y + 1.0f,
            rect.z - rect.x - 2.0f,
            rect.w - rect.y - 2.0f,
            fill,
            cmd);
    }

    void DrawText(
        const std::string& text,
        const float x,
        const float y,
        const int size,
        const wi::Color color,
        const wi::graphics::CommandList cmd,
        const float tracking = 0.7f,
        const float bolden = 0.12f)
    {
        wi::font::Params params(
            x,
            y,
            size,
            wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP,
            color,
            wi::Color::Transparent());
        params.spacingX = tracking;
        params.bolden = bolden;
        wi::font::Draw(text, params, cmd);
    }

    std::string UpperAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c)
        {
            return static_cast<char>(std::toupper(c));
        });
        return value;
    }

    std::string Ellipsize(std::string value, const std::size_t maximum)
    {
        if (value.size() <= maximum)
            return value;
        if (maximum <= 3u)
            return value.substr(0, maximum);
        value.resize(maximum - 3u);
        value += "...";
        return value;
    }

    std::string BackendLabel()
    {
        const auto* device = wi::graphics::GetDevice();
        if (device != nullptr && std::string(device->GetTag()) == "[Vulkan]")
            return "VULKAN";
        return "DX12";
    }
}

namespace renegade::studio
{
    void RenegadeProjectHub::Create()
    {
        SetName("Renegade-owned Project Hub");
        conceptPlate_ = wi::resourcemanager::Load(
            "Content/ui/renegade-project-hub-concept.jpg");
        SetShadowRadius(0.0f);
        SetLayout(width_, height_);
    }

    void RenegadeProjectHub::SetLayout(const float width, const float height)
    {
        width_ = std::max(1.0f, width);
        height_ = std::max(1.0f, height);
        scale_ = std::min(width_ / DesignWidth, height_ / DesignHeight);
        scale_ = std::max(0.01f, scale_);
        offsetX_ = (width_ - DesignWidth * scale_) * 0.5f;
        offsetY_ = (height_ - DesignHeight * scale_) * 0.5f;
        SetPos(XMFLOAT2(0.0f, 0.0f));
        SetSize(XMFLOAT2(width_, height_));
    }

    void RenegadeProjectHub::SetDeveloperIdentity(std::string identity)
    {
        if (identity.empty())
            identity = "DEVELOPER";
        developerIdentity_ = std::move(identity);
    }

    void RenegadeProjectHub::SetProjects(
        std::vector<ProjectEntry> projects,
        const int selectedIndex)
    {
        projects_ = std::move(projects);
        SetSelectedIndex(selectedIndex);
    }

    void RenegadeProjectHub::SetSelectedIndex(const int selectedIndex) noexcept
    {
        if (selectedIndex < 0 ||
            static_cast<std::size_t>(selectedIndex) >= projects_.size())
        {
            selectedIndex_ = -1;
            return;
        }
        selectedIndex_ = selectedIndex;
    }

    void RenegadeProjectHub::SetCurrentProject(
        std::string name,
        const bool active)
    {
        currentProjectName_ = std::move(name);
        currentProjectActive_ = active;
    }

    void RenegadeProjectHub::SetStatusText(std::string text)
    {
        statusText_ = std::move(text);
    }

    void RenegadeProjectHub::SetStatusProvider(
        std::function<std::string()> provider)
    {
        statusProvider_ = std::move(provider);
    }

    void RenegadeProjectHub::SetNewProjectMode(const bool active) noexcept
    {
        newProjectMode_ = active;
    }

    void RenegadeProjectHub::OnAction(std::function<void(Action)> callback)
    {
        action_ = std::move(callback);
    }

    void RenegadeProjectHub::OnRecentProjectSelected(
        std::function<void(std::size_t)> callback)
    {
        recentProjectSelected_ = std::move(callback);
    }

    bool RenegadeProjectHub::ConsumedPointerThisFrame() const noexcept
    {
        return pointerConsumed_;
    }

    XMFLOAT2 RenegadeProjectHub::ToBase(const float x, const float y) const noexcept
    {
        return XMFLOAT2(
            (x - offsetX_) / scale_,
            (y - offsetY_) / scale_);
    }

    XMFLOAT4 RenegadeProjectHub::ToScreen(const XMFLOAT4& baseRect) const noexcept
    {
        return XMFLOAT4(
            offsetX_ + baseRect.x * scale_,
            offsetY_ + baseRect.y * scale_,
            offsetX_ + baseRect.z * scale_,
            offsetY_ + baseRect.w * scale_);
    }

    bool RenegadeProjectHub::ContainsBase(
        const XMFLOAT2& point,
        const XMFLOAT4& rect) const noexcept
    {
        return point.x >= rect.x && point.x <= rect.z &&
            point.y >= rect.y && point.y <= rect.w;
    }

    RenegadeProjectHub::HoverTarget RenegadeProjectHub::ResolveHover(
        const XMFLOAT2& point) const noexcept
    {
        if (newProjectMode_)
            return HoverTarget::None;
        if (ContainsBase(point, LeftNew))
            return HoverTarget::NewProject;
        if (ContainsBase(point, LeftOpen))
            return HoverTarget::OpenProject;
        if (currentProjectActive_ && ContainsBase(point, LeftBack))
            return HoverTarget::BackToEditor;
        if (ContainsBase(point, LeftExit))
            return HoverTarget::ExitRenegade;
        if (selectedIndex_ >= 0 && ContainsBase(point, Featured))
            return HoverTarget::FeaturedProject;
        if (selectedIndex_ >= 0 && ContainsBase(point, OpenSelected))
            return HoverTarget::OpenSelected;
        if (ContainsBase(point, LowerNew))
            return HoverTarget::LowerNewProject;
        if (projects_.size() > 1u && ContainsBase(point, LowerRecent0))
            return HoverTarget::Recent0;
        if (projects_.size() > 2u && ContainsBase(point, LowerRecent1))
            return HoverTarget::Recent1;
        if (projects_.size() > 3u && ContainsBase(point, LowerRecent2))
            return HoverTarget::Recent2;
        return HoverTarget::None;
    }

    void RenegadeProjectHub::Invoke(const Action action)
    {
        if (action_)
            action_(action);
    }

    XMFLOAT4 RenegadeProjectHub::NewProjectInputBounds() const noexcept
    {
        return ToScreen(XMFLOAT4(612.0f, 405.0f, 1060.0f, 445.0f));
    }

    XMFLOAT4 RenegadeProjectHub::NewProjectConfirmBounds() const noexcept
    {
        return ToScreen(XMFLOAT4(612.0f, 463.0f, 826.0f, 510.0f));
    }

    XMFLOAT4 RenegadeProjectHub::NewProjectCancelBounds() const noexcept
    {
        return ToScreen(XMFLOAT4(846.0f, 463.0f, 1060.0f, 510.0f));
    }

    void RenegadeProjectHub::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        Widget::Update(canvas, dt);
        pointerConsumed_ = IsVisible();
        if (!IsVisible())
        {
            hovered_ = HoverTarget::None;
            return;
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const XMFLOAT2 basePointer = ToBase(pointer.x, pointer.y);
        hovered_ = ResolveHover(basePointer);

        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
        {
            if (newProjectMode_)
            {
                Invoke(Action::CancelNewProject);
                return;
            }
            if (currentProjectActive_)
            {
                Invoke(Action::BackToEditor);
                return;
            }
        }

        if (!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            return;

        switch (hovered_)
        {
        case HoverTarget::NewProject:
        case HoverTarget::LowerNewProject:
            Invoke(Action::NewProject);
            break;
        case HoverTarget::OpenProject:
            Invoke(Action::OpenProject);
            break;
        case HoverTarget::BackToEditor:
            Invoke(Action::BackToEditor);
            break;
        case HoverTarget::ExitRenegade:
            Invoke(Action::ExitRenegade);
            break;
        case HoverTarget::FeaturedProject:
        case HoverTarget::OpenSelected:
            Invoke(Action::OpenSelectedProject);
            break;
        case HoverTarget::Recent0:
            if (recentProjectSelected_)
                recentProjectSelected_(1u);
            break;
        case HoverTarget::Recent1:
            if (recentProjectSelected_)
                recentProjectSelected_(2u);
            break;
        case HoverTarget::Recent2:
            if (recentProjectSelected_)
                recentProjectSelected_(3u);
            break;
        case HoverTarget::None:
        default:
            break;
        }
    }

    void RenegadeProjectHub::Render(
        const wi::Canvas&,
        const wi::graphics::CommandList cmd) const
    {
        if (!IsVisible())
            return;

        DrawRect(0.0f, 0.0f, width_, height_, InkSolid, cmd);

        const bool plateReady = conceptPlate_.IsValid();
        if (plateReady)
        {
            wi::image::Params plate(
                offsetX_,
                offsetY_,
                DesignWidth * scale_,
                DesignHeight * scale_);
            plate.blendFlag = wi::enums::BLENDMODE_ALPHA;
            plate.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
            wi::image::Draw(&conceptPlate_.GetTexture(), plate, cmd);
        }

        const auto sx = [this](const float value) { return offsetX_ + value * scale_; };
        const auto sy = [this](const float value) { return offsetY_ + value * scale_; };
        const auto sw = [this](const float value) { return value * scale_; };
        const auto fontSize = [this](const int value)
        {
            return std::max(8, static_cast<int>(std::round(value * scale_)));
        };
        const auto text = [&] (
            const std::string& value,
            const float x,
            const float y,
            const int size,
            const wi::Color color,
            const float tracking = 0.7f,
            const float bolden = 0.12f)
        {
            DrawText(
                value,
                sx(x),
                sy(y),
                fontSize(size),
                color,
                cmd,
                tracking * scale_,
                bolden);
        };
        const auto mask = [&] (
            const float x,
            const float y,
            const float width,
            const float height,
            const wi::Color color = Ink)
        {
            DrawRect(sx(x), sy(y), sw(width), sw(height), color, cmd);
        };
        const auto bordered = [&] (
            const XMFLOAT4& base,
            const wi::Color fill,
            const wi::Color edge)
        {
            DrawBorderedRect(ToScreen(base), fill, edge, cmd);
        };

        if (!plateReady)
        {
            text("PROJECT HUB VISUAL ASSET FAILED TO LOAD", 548.0f, 115.0f, 14, Danger, 1.1f, 0.16f);
        }

        // The plate carries the permanent authored chrome. Only live identity,
        // project data and truthful runtime values are painted over it.
        mask(1368.0f, 34.0f, 264.0f, 78.0f, wi::Color(5, 12, 17, 248));
        text(
            "WELCOME, " + UpperAscii(Ellipsize(developerIdentity_, 24u)),
            1390.0f,
            47.0f,
            16,
            TextStrong,
            1.35f,
            0.15f);
        text("SYSTEM NOMINAL", 1454.0f, 82.0f, 9, Success, 1.0f, 0.14f);
        DrawRect(sx(1601.0f), sy(84.0f), sw(6.0f), sw(6.0f), Success, cmd);

        // BACK TO EDITOR exists only when there is a real active project.
        // The approved plate illustrates the active state, so blank that row
        // when the Hub is startup-only and replace only its live subtitle
        // while a project is active.
        if (!currentProjectActive_)
        {
            mask(104.0f, 499.0f, 276.0f, 106.0f, wi::Color(5, 13, 18, 248));
        }
        else
        {
            mask(192.0f, 558.0f, 176.0f, 24.0f, wi::Color(5, 13, 18, 248));
            text(UpperAscii(Ellipsize(currentProjectName_, 24u)), 193.0f, 560.0f, 8, Muted, 0.45f, 0.10f);
        }

        if (hovered_ == HoverTarget::NewProject)
            bordered(LeftNew, wi::Color::Transparent(), Orange);
        if (hovered_ == HoverTarget::OpenProject)
            bordered(LeftOpen, wi::Color::Transparent(), Orange);
        if (currentProjectActive_ && hovered_ == HoverTarget::BackToEditor)
            bordered(LeftBack, wi::Color::Transparent(), Cyan);
        if (hovered_ == HoverTarget::ExitRenegade)
            bordered(LeftExit, wi::Color::Transparent(), Danger);

        // The centre preview stays honestly empty until Gate 4/user capture.
        // Only the lower metadata strip is live.
        mask(411.0f, 527.0f, 733.0f, 106.0f, wi::Color(5, 15, 21, 250));
        if (selectedIndex_ >= 0 &&
            static_cast<std::size_t>(selectedIndex_) < projects_.size())
        {
            const auto& selected = projects_[static_cast<std::size_t>(selectedIndex_)];
            text(UpperAscii(Ellipsize(selected.name, 38u)), 432.0f, 546.0f, 20, TextStrong, 1.15f, 0.14f);
            text(
                UpperAscii(Ellipsize(selected.rootPath.empty() ? selected.descriptorPath : selected.rootPath, 67u)),
                432.0f,
                582.0f,
                8,
                Cyan,
                0.35f,
                0.10f);
            text(
                "FORMAT // V" + std::to_string(selected.formatVersion),
                1030.0f,
                579.0f,
                8,
                Text,
                0.45f,
                0.10f);
            text(
                selected.descriptorValid ? "READY" : "WARNING",
                1060.0f,
                607.0f,
                9,
                selected.descriptorValid ? Success : Danger,
                0.55f,
                0.12f);
            if (hovered_ == HoverTarget::FeaturedProject)
                bordered(Featured, wi::Color::Transparent(), Cyan);
        }
        else
        {
            text("NO RECENT PROJECT SELECTED", 432.0f, 548.0f, 18, TextStrong, 1.0f, 0.14f);
            text("CREATE A NEW PROJECT OR OPEN AN EXISTING .RENEGADE PROJECT", 432.0f, 582.0f, 8, Muted, 0.35f, 0.10f);
        }

        // Preserve the authored NEW PROJECT tile rather than repainting it as
        // a flat widget. Hover adds only a thin interaction outline.
        if (hovered_ == HoverTarget::LowerNewProject)
            bordered(LowerNew, wi::Color::Transparent(), Orange);

        // The approved plate already contains the correct no-selection state.
        // Replace the detail body only when a real recent project is selected.
        if (selectedIndex_ >= 0 &&
            static_cast<std::size_t>(selectedIndex_) < projects_.size())
        {
            const auto& selected = projects_[static_cast<std::size_t>(selectedIndex_)];
            mask(1188.0f, 388.0f, 341.0f, 334.0f, wi::Color(5, 14, 19, 250));
            text("PROJECT NAME", 1207.0f, 405.0f, 8, Muted, 0.55f, 0.10f);
            text(UpperAscii(Ellipsize(selected.name, 28u)), 1207.0f, 428.0f, 10, TextStrong, 0.45f, 0.13f);
            text("PROJECT PATH", 1207.0f, 474.0f, 8, Muted, 0.55f, 0.10f);
            text(Ellipsize(selected.rootPath.empty() ? selected.descriptorPath : selected.rootPath, 40u), 1207.0f, 497.0f, 8, Text, 0.22f, 0.10f);
            text("STATUS", 1207.0f, 544.0f, 8, Muted, 0.55f, 0.10f);
            text(selected.descriptorValid ? "READY" : "DESCRIPTOR WARNING", 1322.0f, 544.0f, 8, selected.descriptorValid ? Success : Danger, 0.45f, 0.11f);
            text("ENGINE / PROJECT FORMAT", 1207.0f, 588.0f, 8, Muted, 0.55f, 0.10f);
            text("RENEGADE PROJECT V" + std::to_string(selected.formatVersion), 1207.0f, 611.0f, 8, Text, 0.32f, 0.10f);
            text("STARTUP SCENE", 1207.0f, 653.0f, 8, Muted, 0.55f, 0.10f);
            text(Ellipsize(selected.startupScene.empty() ? "Content/Scenes/Main.wiscene" : selected.startupScene, 40u), 1207.0f, 676.0f, 8, Text, 0.22f, 0.10f);

            mask(1190.0f, 737.0f, 345.0f, 70.0f, SurfaceRaised);
            bordered(
                OpenSelected,
                SurfaceRaised,
                hovered_ == HoverTarget::OpenSelected ? Cyan : Border);
            text("OPEN PROJECT  >>", 1240.0f, 758.0f, 16, TextStrong, 0.9f, 0.15f);
        }

        // Preserve the technical footer frame/dividers from the authored
        // plate, but never lie about live telemetry that Gate 3 does not own.
        mask(58.0f, 883.0f, 116.0f, 29.0f, wi::Color(5, 13, 18, 248));
        text("NOMINAL", 65.0f, 888.0f, 9, Success, 0.55f, 0.12f);
        mask(210.0f, 883.0f, 92.0f, 29.0f, wi::Color(5, 13, 18, 248));
        text("DEV", 212.0f, 888.0f, 9, TextStrong, 0.55f, 0.12f);
        mask(337.0f, 883.0f, 120.0f, 29.0f, wi::Color(5, 13, 18, 248));
        text(BackendLabel(), 340.0f, 888.0f, 9, TextStrong, 0.55f, 0.12f);
        mask(500.0f, 883.0f, 145.0f, 29.0f, wi::Color(5, 13, 18, 248));
        text("WINDOWS PC", 503.0f, 888.0f, 9, TextStrong, 0.55f, 0.12f);
        mask(932.0f, 883.0f, 218.0f, 33.0f, wi::Color(5, 13, 18, 248));
        text("N/A", 936.0f, 888.0f, 9, Muted, 0.55f, 0.12f);
        mask(1192.0f, 883.0f, 168.0f, 33.0f, wi::Color(5, 13, 18, 248));
        text("N/A", 1197.0f, 888.0f, 9, Muted, 0.55f, 0.12f);
        mask(1410.0f, 883.0f, 174.0f, 33.0f, wi::Color(5, 13, 18, 248));
        text("N/A", 1415.0f, 888.0f, 9, Muted, 0.55f, 0.12f);

        if (newProjectMode_)
        {
            mask(0.0f, 0.0f, DesignWidth, DesignHeight, wi::Color(0, 4, 7, 185));
            bordered(XMFLOAT4(560.0f, 337.0f, 1112.0f, 554.0f), SurfaceRaised, CyanSoft);
            text("CREATE NEW PROJECT", 612.0f, 360.0f, 20, TextStrong, 1.2f, 0.15f);
            text("PROJECT NAME", 612.0f, 393.0f, 8, Muted, 0.8f, 0.10f);
            text("Choose a name now. Folder selection follows after CREATE PROJECT.", 612.0f, 526.0f, 8, Muted, 0.25f, 0.10f);
        }
    }
}
