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

    const XMFLOAT4 LeftNew = XMFLOAT4(186.0f, 245.0f, 423.0f, 318.0f);
    const XMFLOAT4 LeftOpen = XMFLOAT4(186.0f, 340.0f, 423.0f, 414.0f);
    const XMFLOAT4 LeftBack = XMFLOAT4(186.0f, 536.0f, 423.0f, 610.0f);
    const XMFLOAT4 LeftExit = XMFLOAT4(186.0f, 651.0f, 423.0f, 725.0f);
    const XMFLOAT4 Featured = XMFLOAT4(464.0f, 193.0f, 1155.0f, 590.0f);
    const XMFLOAT4 LowerNew = XMFLOAT4(464.0f, 606.0f, 1155.0f, 765.0f);
    const XMFLOAT4 LowerRecent0 = XMFLOAT4(742.0f, 606.0f, 868.0f, 765.0f);
    const XMFLOAT4 LowerRecent1 = XMFLOAT4(879.0f, 606.0f, 1005.0f, 765.0f);
    const XMFLOAT4 LowerRecent2 = XMFLOAT4(1016.0f, 606.0f, 1155.0f, 765.0f);
    const XMFLOAT4 OpenSelected = XMFLOAT4(1192.0f, 687.0f, 1477.0f, 765.0f);

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

        if (conceptPlate_.IsValid())
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

        mask(1310.0f, 41.0f, 270.0f, 70.0f, wi::Color(5, 12, 17, 246));
        text(
            "WELCOME, " + UpperAscii(Ellipsize(developerIdentity_, 24u)),
            1326.0f,
            54.0f,
            16,
            TextStrong,
            1.5f,
            0.15f);
        text("SYSTEM NOMINAL", 1420.0f, 85.0f, 10, Success, 1.2f, 0.14f);
        DrawRect(sx(1558.0f), sy(87.0f), sw(6.0f), sw(6.0f), Success, cmd);

        for (const auto& row : std::array<XMFLOAT4, 4>{LeftNew, LeftOpen, LeftBack, LeftExit})
        {
            const bool activeRow =
                (row.x == LeftBack.x && row.y == LeftBack.y) ? currentProjectActive_ : true;
            if (activeRow)
                mask(row.x + 70.0f, row.y + 7.0f, row.z - row.x - 75.0f, row.w - row.y - 14.0f);
        }
        mask(255.0f, 438.0f, 160.0f, 62.0f);

        if (hovered_ == HoverTarget::NewProject)
            bordered(LeftNew, wi::Color(8, 20, 25, 185), Orange);
        if (hovered_ == HoverTarget::OpenProject)
            bordered(LeftOpen, wi::Color(8, 20, 25, 185), Orange);
        if (currentProjectActive_ && hovered_ == HoverTarget::BackToEditor)
            bordered(LeftBack, wi::Color(8, 20, 25, 185), Cyan);
        if (hovered_ == HoverTarget::ExitRenegade)
            bordered(LeftExit, wi::Color(26, 12, 12, 205), Danger);

        text("NEW PROJECT", 263.0f, 271.0f, 13, TextStrong, 1.1f, 0.14f);
        text(">", 392.0f, 270.0f, 15, Orange, 0.0f, 0.18f);
        text("OPEN PROJECT", 263.0f, 367.0f, 13, TextStrong, 1.1f, 0.14f);
        text(">", 392.0f, 366.0f, 15, Orange, 0.0f, 0.18f);
        text("IMPORT PROJECT", 263.0f, 464.0f, 12, Muted, 0.9f, 0.12f);
        text("PLANNED", 350.0f, 486.0f, 8, Muted, 0.8f, 0.10f);
        if (currentProjectActive_)
        {
            text("BACK TO EDITOR", 263.0f, 562.0f, 12, Cyan, 0.8f, 0.14f);
            text("ESC", 369.0f, 585.0f, 8, Muted, 0.7f, 0.10f);
        }
        text("EXIT RENEGADE", 263.0f, 679.0f, 12, Text, 0.9f, 0.13f);
        text("POWER DOWN", 343.0f, 701.0f, 8, Danger, 0.8f, 0.10f);

        mask(470.0f, 523.0f, 676.0f, 59.0f, wi::Color(5, 15, 21, 252));
        if (selectedIndex_ >= 0 &&
            static_cast<std::size_t>(selectedIndex_) < projects_.size())
        {
            const auto& selected = projects_[static_cast<std::size_t>(selectedIndex_)];
            text(UpperAscii(Ellipsize(selected.name, 38u)), 487.0f, 535.0f, 22, TextStrong, 1.5f, 0.14f);
            text(
                "LOCATION  //  " + UpperAscii(Ellipsize(selected.rootPath.empty() ? selected.descriptorPath : selected.rootPath, 62u)),
                488.0f,
                568.0f,
                8,
                Muted,
                0.45f,
                0.10f);
            text(
                "FORMAT  //  V" + std::to_string(selected.formatVersion),
                1033.0f,
                568.0f,
                8,
                CyanSoft,
                0.5f,
                0.10f);
            if (hovered_ == HoverTarget::FeaturedProject)
                bordered(Featured, wi::Color::Transparent(), Cyan);
        }
        else
        {
            text("NO RECENT PROJECT SELECTED", 487.0f, 535.0f, 20, TextStrong, 1.4f, 0.14f);
            text("CREATE A NEW PROJECT OR OPEN AN EXISTING .RENEGADE PROJECT", 488.0f, 568.0f, 8, Muted, 0.45f, 0.10f);
        }

        mask(462.0f, 603.0f, 697.0f, 166.0f, wi::Color(5, 14, 19, 238));
        bordered(
            LowerNew,
            hovered_ == HoverTarget::LowerNewProject ? SurfaceRaised : Surface,
            hovered_ == HoverTarget::LowerNewProject ? Orange : Border);
        text("+", 518.0f, 646.0f, 34, TextStrong, 0.0f, 0.08f);
        text("NEW PROJECT", 637.0f, 653.0f, 18, TextStrong, 1.2f, 0.13f);
        text("CREATE A NEW RENEGADE PROJECT", 637.0f, 684.0f, 8, Muted, 0.6f, 0.10f);

        mask(1191.0f, 383.0f, 287.0f, 286.0f, wi::Color(5, 14, 19, 250));
        if (selectedIndex_ >= 0 &&
            static_cast<std::size_t>(selectedIndex_) < projects_.size())
        {
            const auto& selected = projects_[static_cast<std::size_t>(selectedIndex_)];
            text("PROJECT NAME", 1208.0f, 392.0f, 8, Muted, 0.6f, 0.10f);
            text(UpperAscii(Ellipsize(selected.name, 27u)), 1208.0f, 411.0f, 10, TextStrong, 0.55f, 0.13f);
            text("STATUS", 1208.0f, 450.0f, 8, Muted, 0.6f, 0.10f);
            text(selected.descriptorValid ? "READY" : "DESCRIPTOR WARNING", 1320.0f, 450.0f, 8, selected.descriptorValid ? Success : Danger, 0.6f, 0.11f);
            text("FORMAT VERSION", 1208.0f, 486.0f, 8, Muted, 0.6f, 0.10f);
            text("RENEGADE PROJECT V" + std::to_string(selected.formatVersion), 1320.0f, 486.0f, 8, Text, 0.4f, 0.10f);
            text("PROJECT PATH", 1208.0f, 526.0f, 8, Muted, 0.6f, 0.10f);
            text(Ellipsize(selected.rootPath.empty() ? selected.descriptorPath : selected.rootPath, 38u), 1208.0f, 545.0f, 8, Text, 0.25f, 0.10f);
            text("STARTUP SCENE", 1208.0f, 584.0f, 8, Muted, 0.6f, 0.10f);
            text(Ellipsize(selected.startupScene.empty() ? "Content/Scenes/Main.wiscene" : selected.startupScene, 38u), 1208.0f, 603.0f, 8, Text, 0.25f, 0.10f);
            text("PROJECT DESCRIPTOR", 1208.0f, 636.0f, 8, Muted, 0.6f, 0.10f);
            text(Ellipsize(selected.descriptorPath, 38u), 1208.0f, 654.0f, 7, Text, 0.2f, 0.10f);
        }
        else
        {
            text("NO PROJECT SELECTED", 1208.0f, 410.0f, 11, TextStrong, 0.8f, 0.13f);
            text("Create a new project or open an existing", 1208.0f, 447.0f, 8, Muted, 0.25f, 0.10f);
            text(".renegade project to begin.", 1208.0f, 465.0f, 8, Muted, 0.25f, 0.10f);
        }

        mask(1201.0f, 696.0f, 266.0f, 60.0f, SurfaceRaised);
        bordered(
            OpenSelected,
            selectedIndex_ >= 0 ? SurfaceRaised : Surface,
            hovered_ == HoverTarget::OpenSelected ? Cyan : Border);
        text(
            selectedIndex_ >= 0 ? "OPEN PROJECT  >>" : "SELECT A PROJECT",
            1235.0f,
            715.0f,
            16,
            selectedIndex_ >= 0 ? TextStrong : Muted,
            1.0f,
            0.15f);

        mask(82.0f, 812.0f, 1504.0f, 72.0f, wi::Color(5, 13, 18, 248));
        text("SYSTEM STATUS", 113.0f, 823.0f, 8, Muted, 0.7f, 0.10f);
        text("NOMINAL", 113.0f, 846.0f, 9, Success, 0.7f, 0.12f);
        text("RENDER BACKEND", 347.0f, 823.0f, 8, Muted, 0.7f, 0.10f);
        text(BackendLabel(), 347.0f, 846.0f, 9, TextStrong, 0.7f, 0.12f);
        text("PROJECT FORMAT", 540.0f, 823.0f, 8, Muted, 0.7f, 0.10f);
        text("RENEGADE V1", 540.0f, 846.0f, 9, TextStrong, 0.7f, 0.12f);
        text("RECENT PROJECTS", 756.0f, 823.0f, 8, Muted, 0.7f, 0.10f);
        text(std::to_string(projects_.size()) + " / 8", 756.0f, 846.0f, 9, TextStrong, 0.7f, 0.12f);
        text("SESSION", 967.0f, 823.0f, 8, Muted, 0.7f, 0.10f);
        text(currentProjectActive_ ? "PROJECT ACTIVE" : "HUB ONLY", 967.0f, 846.0f, 9, currentProjectActive_ ? Success : Text, 0.7f, 0.12f);
        text("STATUS", 1196.0f, 823.0f, 8, Muted, 0.7f, 0.10f);
        const std::string liveStatus = statusProvider_ ? statusProvider_() : statusText_;
        text(Ellipsize(UpperAscii(liveStatus), 46u), 1196.0f, 846.0f, 8, Text, 0.35f, 0.10f);

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
