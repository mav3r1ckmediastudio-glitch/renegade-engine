#include "RenegadeProjectHub.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <system_error>
#include <utility>

#ifndef RENEGADE_SOURCE_REVISION
#define RENEGADE_SOURCE_REVISION "unknown"
#endif

namespace
{
    namespace fs = std::filesystem;

    constexpr float DesignWidth = 1672.0f;
    constexpr float DesignHeight = 941.0f;

    constexpr wi::Color Background = wi::Color(4, 8, 12, 255);
    constexpr wi::Color Panel = wi::Color(7, 14, 20, 246);
    constexpr wi::Color PanelRaised = wi::Color(9, 19, 27, 250);
    constexpr wi::Color PanelDeep = wi::Color(5, 11, 16, 252);
    constexpr wi::Color Border = wi::Color(34, 58, 72, 230);
    constexpr wi::Color BorderBright = wi::Color(66, 91, 108, 235);
    constexpr wi::Color Cyan = wi::Color(95, 216, 255, 255);
    constexpr wi::Color CyanDim = wi::Color(49, 110, 139, 255);
    constexpr wi::Color Orange = wi::Color(238, 117, 26, 255);
    constexpr wi::Color Text = wi::Color(219, 230, 238, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 248, 251, 255);
    constexpr wi::Color Muted = wi::Color(143, 162, 179, 255);
    constexpr wi::Color Dim = wi::Color(78, 96, 113, 255);
    constexpr wi::Color Success = wi::Color(79, 224, 160, 255);
    constexpr wi::Color Warning = wi::Color(255, 180, 84, 255);

    const XMFLOAT4 Header = XMFLOAT4(18.0f, 18.0f, 1654.0f, 122.0f);
    const XMFLOAT4 LeftPanel = XMFLOAT4(28.0f, 142.0f, 394.0f, 800.0f);
    const XMFLOAT4 CenterPanel = XMFLOAT4(414.0f, 142.0f, 1171.0f, 800.0f);
    const XMFLOAT4 DetailsPanel = XMFLOAT4(1191.0f, 142.0f, 1644.0f, 800.0f);
    const XMFLOAT4 Footer = XMFLOAT4(18.0f, 818.0f, 1654.0f, 923.0f);

    const XMFLOAT4 LeftNew = XMFLOAT4(50.0f, 174.0f, 370.0f, 266.0f);
    const XMFLOAT4 LeftOpen = XMFLOAT4(50.0f, 284.0f, 370.0f, 376.0f);
    const XMFLOAT4 LeftImport = XMFLOAT4(50.0f, 394.0f, 370.0f, 486.0f);
    const XMFLOAT4 LeftBack = XMFLOAT4(50.0f, 690.0f, 370.0f, 752.0f);
    const XMFLOAT4 LeftExit = XMFLOAT4(50.0f, 760.0f, 370.0f, 786.0f);

    const XMFLOAT4 Preview = XMFLOAT4(435.0f, 188.0f, 1150.0f, 592.0f);
    const XMFLOAT4 LowerNew = XMFLOAT4(435.0f, 614.0f, 1150.0f, 780.0f);
    const XMFLOAT4 PreviousRecent = XMFLOAT4(1064.0f, 151.0f, 1097.0f, 181.0f);
    const XMFLOAT4 NextRecent = XMFLOAT4(1111.0f, 151.0f, 1144.0f, 181.0f);
    const std::array<XMFLOAT4, 3> RecentCards = {{
        XMFLOAT4(435.0f, 614.0f, 662.0f, 780.0f),
        XMFLOAT4(679.0f, 614.0f, 906.0f, 780.0f),
        XMFLOAT4(923.0f, 614.0f, 1150.0f, 780.0f),
    }};

    const XMFLOAT4 DetailsHero = XMFLOAT4(1213.0f, 188.0f, 1621.0f, 374.0f);
    const XMFLOAT4 OpenSelected = XMFLOAT4(1213.0f, 708.0f, 1621.0f, 776.0f);

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

    void DrawPanel(
        const XMFLOAT4& rect,
        const wi::Color fill,
        const wi::Color edge,
        const wi::graphics::CommandList cmd,
        const float edgeWidth = 1.0f)
    {
        DrawRect(rect.x, rect.y, rect.z - rect.x, rect.w - rect.y, edge, cmd);
        DrawRect(
            rect.x + edgeWidth,
            rect.y + edgeWidth,
            rect.z - rect.x - edgeWidth * 2.0f,
            rect.w - rect.y - edgeWidth * 2.0f,
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
        const wi::font::Alignment horizontal = wi::font::WIFALIGN_LEFT,
        const float tracking = 0.7f,
        const float bolden = 0.10f)
    {
        wi::font::Params params(
            x,
            y,
            size,
            horizontal,
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

    std::string ShortSourceRevision()
    {
        std::string revision = RENEGADE_SOURCE_REVISION;
        if (revision.size() > 8u)
            revision.resize(8u);
        return UpperAscii(revision);
    }

    bool SupportedArtworkExtension(std::string extension)
    {
        std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        static constexpr std::array<const char*, 5> Supported =
        {
            ".jpg", ".jpeg", ".png", ".bmp", ".tga"
        };
        return std::any_of(Supported.begin(), Supported.end(), [&extension](const char* candidate)
        {
            return extension == candidate;
        });
    }
}

namespace renegade::studio
{
    void RenegadeProjectHub::Create()
    {
        SetName("Renegade-owned Project Hub");
        wordmark_ = wi::resourcemanager::Load("Content/ui/renegade-engine-wordmark.png");
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
        for (auto& project : projects)
        {
            if (project.artworkPath.empty())
                project.artworkPath = FindPersistedArtwork(project);
        }
        projects_ = std::move(projects);
        ReloadProjectArtworkCache();
        SetSelectedIndex(selectedIndex);
    }

    void RenegadeProjectHub::SetSelectedIndex(const int selectedIndex) noexcept
    {
        if (selectedIndex < 0 ||
            static_cast<std::size_t>(selectedIndex) >= projects_.size())
        {
            selectedIndex_ = -1;
            projectArtwork_ = {};
            return;
        }
        selectedIndex_ = selectedIndex;
        ReloadSelectedArtwork();
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

    std::size_t RenegadeProjectHub::VisibleRecentStart() const noexcept
    {
        if (projects_.empty() || selectedIndex_ < 0)
            return 0u;
        const std::size_t selected = static_cast<std::size_t>(selectedIndex_);
        return (selected / RecentCards.size()) * RecentCards.size();
    }

    int RenegadeProjectHub::RecentIndexForTarget(const HoverTarget target) const noexcept
    {
        std::size_t offset = RecentCards.size();
        switch (target)
        {
        case HoverTarget::RecentCard0:
            offset = 0u;
            break;
        case HoverTarget::RecentCard1:
            offset = 1u;
            break;
        case HoverTarget::RecentCard2:
            offset = 2u;
            break;
        default:
            return -1;
        }
        const std::size_t index = VisibleRecentStart() + offset;
        return index < projects_.size() ? static_cast<int>(index) : -1;
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
        if (ContainsBase(point, LeftImport))
            return HoverTarget::ImportProject;
        if (currentProjectActive_ && ContainsBase(point, LeftBack))
            return HoverTarget::BackToEditor;

        if (projects_.empty())
        {
            if (ContainsBase(point, LowerNew))
                return HoverTarget::LowerNewProject;
        }
        else
        {
            const std::size_t start = VisibleRecentStart();
            for (std::size_t slot = 0; slot < RecentCards.size(); ++slot)
            {
                if (start + slot >= projects_.size())
                    break;
                if (!ContainsBase(point, RecentCards[slot]))
                    continue;
                switch (slot)
                {
                case 0u: return HoverTarget::RecentCard0;
                case 1u: return HoverTarget::RecentCard1;
                default: return HoverTarget::RecentCard2;
                }
            }
        }

        if (selectedIndex_ >= 0 &&
            projects_[static_cast<std::size_t>(selectedIndex_)].descriptorValid &&
            ContainsBase(point, Preview))
        {
            return HoverTarget::ProjectPreview;
        }
        if (selectedIndex_ >= 0 &&
            projects_[static_cast<std::size_t>(selectedIndex_)].descriptorValid &&
            ContainsBase(point, OpenSelected))
        {
            return HoverTarget::OpenSelected;
        }
        if (projects_.size() > 1u && ContainsBase(point, PreviousRecent))
            return HoverTarget::PreviousRecent;
        if (projects_.size() > 1u && ContainsBase(point, NextRecent))
            return HoverTarget::NextRecent;
        return HoverTarget::None;
    }

    void RenegadeProjectHub::Invoke(const Action action)
    {
        if (action_)
            action_(action);
    }

    void RenegadeProjectHub::SelectRelativeRecent(const int delta)
    {
        if (projects_.empty())
            return;
        int next = selectedIndex_;
        if (next < 0)
            next = 0;
        next += delta;
        const int count = static_cast<int>(projects_.size());
        if (next < 0)
            next = count - 1;
        else if (next >= count)
            next = 0;
        SetSelectedIndex(next);
        if (recentProjectSelected_)
            recentProjectSelected_(static_cast<std::size_t>(next));
    }

    void RenegadeProjectHub::SelectRecentCard(const HoverTarget target)
    {
        const int index = RecentIndexForTarget(target);
        if (index < 0)
            return;
        SetSelectedIndex(index);
        if (recentProjectSelected_)
            recentProjectSelected_(static_cast<std::size_t>(index));
    }

    std::string RenegadeProjectHub::FindPersistedArtwork(const ProjectEntry& entry)
    {
        fs::path root;
        if (!entry.rootPath.empty())
            root = fs::path(entry.rootPath);
        else if (!entry.descriptorPath.empty())
            root = fs::path(entry.descriptorPath).parent_path();
        if (root.empty())
            return {};

        const fs::path folder = root / "Saved" / "ProjectHub";
        static constexpr std::array<const char*, 5> Extensions =
        {
            ".jpg", ".jpeg", ".png", ".bmp", ".tga"
        };
        std::error_code error;
        for (const char* extension : Extensions)
        {
            const fs::path candidate = folder / (std::string("project-preview") + extension);
            if (fs::is_regular_file(candidate, error))
                return candidate.u8string();
            error.clear();
        }
        return {};
    }

    void RenegadeProjectHub::ReloadProjectArtworkCache()
    {
        projectArtworkCache_.clear();
        projectArtworkCache_.resize(projects_.size());
        for (std::size_t index = 0; index < projects_.size(); ++index)
        {
            auto& project = projects_[index];
            if (project.artworkPath.empty())
                project.artworkPath = FindPersistedArtwork(project);
            if (!project.artworkPath.empty())
                projectArtworkCache_[index] = wi::resourcemanager::Load(project.artworkPath);
        }
    }

    void RenegadeProjectHub::ReloadSelectedArtwork()
    {
        projectArtwork_ = {};
        if (selectedIndex_ < 0 ||
            static_cast<std::size_t>(selectedIndex_) >= projects_.size())
            return;

        const std::size_t index = static_cast<std::size_t>(selectedIndex_);
        if (projectArtworkCache_.size() != projects_.size())
            ReloadProjectArtworkCache();
        if (index < projectArtworkCache_.size())
            projectArtwork_ = projectArtworkCache_[index];
    }

    void RenegadeProjectHub::BeginChooseProjectArtwork()
    {
        if (selectedIndex_ < 0 ||
            static_cast<std::size_t>(selectedIndex_) >= projects_.size())
            return;

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Project Screenshot";
        params.extensions.push_back("jpg");
        params.extensions.push_back("jpeg");
        params.extensions.push_back("png");
        params.extensions.push_back("bmp");
        params.extensions.push_back("tga");

        wi::helper::FileDialog(
            params,
            [this](const std::string& selectedPath)
            {
                if (selectedPath.empty())
                    return;

                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, selectedPath](uint64_t)
                    {
                        if (selectedIndex_ < 0 ||
                            static_cast<std::size_t>(selectedIndex_) >= projects_.size())
                            return;

                        auto& project = projects_[static_cast<std::size_t>(selectedIndex_)];
                        fs::path root;
                        if (!project.rootPath.empty())
                            root = fs::path(project.rootPath);
                        else if (!project.descriptorPath.empty())
                            root = fs::path(project.descriptorPath).parent_path();
                        if (root.empty())
                            return;

                        fs::path source(selectedPath);
                        std::string extension = source.extension().u8string();
                        std::transform(
                            extension.begin(),
                            extension.end(),
                            extension.begin(),
                            [](const unsigned char c)
                            {
                                return static_cast<char>(std::tolower(c));
                            });
                        if (!SupportedArtworkExtension(extension))
                            return;

                        const fs::path folder = root / "Saved" / "ProjectHub";
                        std::error_code error;
                        fs::create_directories(folder, error);
                        if (error)
                            return;

                        static constexpr std::array<const char*, 5> Extensions =
                        {
                            ".jpg", ".jpeg", ".png", ".bmp", ".tga"
                        };
                        for (const char* existingExtension : Extensions)
                        {
                            error.clear();
                            fs::remove(
                                folder / (std::string("project-preview") + existingExtension),
                                error);
                        }

                        const fs::path destination =
                            folder / (std::string("project-preview") + extension);
                        error.clear();
                        fs::copy_file(
                            source,
                            destination,
                            fs::copy_options::overwrite_existing,
                            error);
                        if (error)
                            return;

                        project.artworkPath = destination.u8string();
                        ReloadProjectArtworkCache();
                        ReloadSelectedArtwork();
                        statusText_ = projectArtwork_.IsValid()
                            ? "PROJECT SCREENSHOT // UPDATED"
                            : "PROJECT SCREENSHOT // LOAD FAILED";
                    });
            });
    }

    XMFLOAT4 RenegadeProjectHub::NewProjectInputBounds() const noexcept
    {
        return ToScreen(XMFLOAT4(604.0f, 406.0f, 1068.0f, 448.0f));
    }

    XMFLOAT4 RenegadeProjectHub::NewProjectConfirmBounds() const noexcept
    {
        return ToScreen(XMFLOAT4(604.0f, 468.0f, 828.0f, 518.0f));
    }

    XMFLOAT4 RenegadeProjectHub::NewProjectCancelBounds() const noexcept
    {
        return ToScreen(XMFLOAT4(844.0f, 468.0f, 1068.0f, 518.0f));
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

        if (!newProjectMode_ && ContainsBase(basePointer, LeftExit))
        {
            Invoke(Action::ExitRenegade);
            return;
        }

        switch (hovered_)
        {
        case HoverTarget::NewProject:
        case HoverTarget::LowerNewProject:
            Invoke(Action::NewProject);
            break;
        case HoverTarget::OpenProject:
            Invoke(Action::OpenProject);
            break;
        case HoverTarget::ImportProject:
            Invoke(Action::OpenProject);
            break;
        case HoverTarget::BackToEditor:
            Invoke(Action::BackToEditor);
            break;
        case HoverTarget::ProjectPreview:
            BeginChooseProjectArtwork();
            break;
        case HoverTarget::OpenSelected:
            Invoke(Action::OpenSelectedProject);
            break;
        case HoverTarget::PreviousRecent:
            SelectRelativeRecent(-1);
            break;
        case HoverTarget::NextRecent:
            SelectRelativeRecent(1);
            break;
        case HoverTarget::RecentCard0:
        case HoverTarget::RecentCard1:
        case HoverTarget::RecentCard2:
            SelectRecentCard(hovered_);
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

        DrawRect(0.0f, 0.0f, width_, height_, Background, cmd);

        const auto sx = [this](const float value) { return offsetX_ + value * scale_; };
        const auto sy = [this](const float value) { return offsetY_ + value * scale_; };
        const auto sw = [this](const float value) { return value * scale_; };
        const auto screen = [this](const XMFLOAT4& rect) { return ToScreen(rect); };
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
            const wi::font::Alignment align = wi::font::WIFALIGN_LEFT,
            const float tracking = 0.7f,
            const float bolden = 0.10f)
        {
            DrawText(
                value,
                sx(x),
                sy(y),
                fontSize(size),
                color,
                cmd,
                align,
                tracking * scale_,
                bolden);
        };
        const auto panel = [&](const XMFLOAT4& rect, const wi::Color fill, const wi::Color edge)
        {
            DrawPanel(screen(rect), fill, edge, cmd, std::max(1.0f, scale_));
        };
        const auto line = [&](const float x, const float y, const float width, const wi::Color color)
        {
            DrawRect(sx(x), sy(y), sw(width), std::max(1.0f, scale_), color, cmd);
        };
        const auto drawResourceContained = [&](const wi::Resource& resource, const XMFLOAT4& rect)
        {
            if (!resource.IsValid())
                return;
            const auto desc = resource.GetTexture().GetDesc();
            const float availableWidth = rect.z - rect.x;
            const float availableHeight = rect.w - rect.y;
            float drawWidth = availableWidth;
            float drawHeight = availableHeight;
            if (desc.width > 0 && desc.height > 0)
            {
                const float aspect = static_cast<float>(desc.width) /
                    static_cast<float>(desc.height);
                drawHeight = drawWidth / aspect;
                if (drawHeight > availableHeight)
                {
                    drawHeight = availableHeight;
                    drawWidth = drawHeight * aspect;
                }
            }
            const float imageX = rect.x + (availableWidth - drawWidth) * 0.5f;
            const float imageY = rect.y + (availableHeight - drawHeight) * 0.5f;
            wi::image::Params image(
                sx(imageX), sy(imageY), sw(drawWidth), sw(drawHeight));
            image.blendFlag = wi::enums::BLENDMODE_ALPHA;
            image.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
            wi::image::Draw(&resource.GetTexture(), image, cmd);
        };

        for (float x = 28.0f; x <= 1644.0f; x += 34.0f)
            DrawRect(sx(x), sy(142.0f), std::max(1.0f, scale_ * 0.5f), sw(658.0f), wi::Color(36, 72, 89, 22), cmd);
        for (float y = 142.0f; y <= 800.0f; y += 34.0f)
            DrawRect(sx(28.0f), sy(y), sw(1616.0f), std::max(1.0f, scale_ * 0.5f), wi::Color(36, 72, 89, 22), cmd);

        panel(Header, PanelDeep, Border);
        panel(LeftPanel, Panel, Border);
        panel(CenterPanel, Panel, Border);
        panel(DetailsPanel, Panel, Border);
        panel(Footer, PanelDeep, Border);

        if (wordmark_.IsValid())
        {
            const auto desc = wordmark_.GetTexture().GetDesc();
            const float targetWidth = sw(205.0f);
            const float targetHeight = sw(58.0f);
            float drawWidth = targetWidth;
            float drawHeight = targetHeight;
            if (desc.width > 0 && desc.height > 0)
            {
                const float aspect = static_cast<float>(desc.width) / static_cast<float>(desc.height);
                drawHeight = drawWidth / aspect;
                if (drawHeight > targetHeight)
                {
                    drawHeight = targetHeight;
                    drawWidth = drawHeight * aspect;
                }
            }
            wi::image::Params logo(
                sx(48.0f),
                sy(42.0f),
                drawWidth,
                drawHeight);
            logo.blendFlag = wi::enums::BLENDMODE_ALPHA;
            logo.sampleFlag = wi::image::SAMPLEMODE_CLAMP;
            wi::image::Draw(&wordmark_.GetTexture(), logo, cmd);
        }

        line(520.0f, 70.0f, 150.0f, BorderBright);
        line(1002.0f, 70.0f, 150.0f, BorderBright);
        text("PROJECT HUB", 836.0f, 49.0f, 28, TextStrong, wi::font::WIFALIGN_CENTER, 2.9f, 0.10f);
        text(
            "WELCOME, " + UpperAscii(Ellipsize(developerIdentity_, 24u)),
            1608.0f,
            43.0f,
            15,
            TextStrong,
            wi::font::WIFALIGN_RIGHT,
            1.1f,
            0.10f);
        text("SYSTEM NOMINAL", 1608.0f, 76.0f, 10, Success, wi::font::WIFALIGN_RIGHT, 0.8f, 0.12f);
        DrawRect(sx(1619.0f), sy(78.0f), sw(7.0f), sw(7.0f), Success, cmd);

        const auto actionButton = [&](const XMFLOAT4& rect, const char* icon, const char* labelText, const HoverTarget target)
        {
            const bool hover = hovered_ == target;
            panel(rect, hover ? PanelRaised : PanelDeep, hover ? CyanDim : Border);
            const XMFLOAT4 r = rect;
            panel(
                XMFLOAT4(r.x + 16.0f, r.y + 16.0f, r.x + 78.0f, r.w - 16.0f),
                Panel,
                hover ? Cyan : BorderBright);
            text(icon, r.x + 47.0f, r.y + 27.0f, 29, hover ? Cyan : Text, wi::font::WIFALIGN_CENTER, 0.0f, 0.02f);
            text(labelText, r.x + 98.0f, r.y + 34.0f, 14, TextStrong, wi::font::WIFALIGN_LEFT, 1.0f, 0.11f);
            text(">", r.z - 24.0f, r.y + 34.0f, 18, hover ? Orange : Dim, wi::font::WIFALIGN_CENTER, 0.0f, 0.10f);
        };

        actionButton(LeftNew, "+", "NEW PROJECT", HoverTarget::NewProject);
        actionButton(LeftOpen, "[]", "OPEN PROJECT", HoverTarget::OpenProject);
        actionButton(LeftImport, "v", "IMPORT PROJECT", HoverTarget::ImportProject);

        if (currentProjectActive_)
        {
            const bool hover = hovered_ == HoverTarget::BackToEditor;
            panel(LeftBack, hover ? PanelRaised : PanelDeep, hover ? CyanDim : Border);
            text("BACK TO EDITOR", 70.0f, 706.0f, 12, hover ? Cyan : TextStrong, wi::font::WIFALIGN_LEFT, 0.9f, 0.11f);
            text(UpperAscii(Ellipsize(currentProjectName_, 28u)), 70.0f, 730.0f, 8, Muted, wi::font::WIFALIGN_LEFT, 0.4f, 0.08f);
        }

        const XMFLOAT4 exitPointer = wi::input::GetPointer();
        const bool exitHover = !newProjectMode_ &&
            ContainsBase(ToBase(exitPointer.x, exitPointer.y), LeftExit);
        panel(LeftExit, exitHover ? PanelRaised : PanelDeep, exitHover ? Orange : Border);
        text("EXIT RENEGADE", 70.0f, 766.0f, 9, exitHover ? Orange : Muted, wi::font::WIFALIGN_LEFT, 0.9f, 0.10f);
        text("X", 348.0f, 766.0f, 9, exitHover ? Orange : Dim, wi::font::WIFALIGN_CENTER, 0.0f, 0.10f);

        text("PROJECT ACTIONS", 52.0f, 512.0f, 9, Dim, wi::font::WIFALIGN_LEFT, 1.2f, 0.10f);
        text("LOCAL PROJECT FILESYSTEM", 52.0f, 536.0f, 8, Muted, wi::font::WIFALIGN_LEFT, 0.5f, 0.07f);

        text("RECENT PROJECTS", 435.0f, 154.0f, 15, TextStrong, wi::font::WIFALIGN_LEFT, 1.4f, 0.11f);
        const std::string countLabel = projects_.empty()
            ? "00 / 00"
            : (selectedIndex_ >= 0
                ? (std::to_string(selectedIndex_ + 1) + " / " + std::to_string(projects_.size()))
                : ("-- / " + std::to_string(projects_.size())));
        text(countLabel, 1048.0f, 156.0f, 9, Muted, wi::font::WIFALIGN_RIGHT, 0.5f, 0.08f);
        if (projects_.size() > 1u)
        {
            panel(PreviousRecent, PanelDeep, hovered_ == HoverTarget::PreviousRecent ? CyanDim : Border);
            panel(NextRecent, PanelDeep, hovered_ == HoverTarget::NextRecent ? CyanDim : Border);
            text("<", 1080.0f, 156.0f, 12, hovered_ == HoverTarget::PreviousRecent ? Cyan : Muted, wi::font::WIFALIGN_CENTER, 0.0f, 0.10f);
            text(">", 1127.0f, 156.0f, 12, hovered_ == HoverTarget::NextRecent ? Cyan : Muted, wi::font::WIFALIGN_CENTER, 0.0f, 0.10f);
        }

        const bool hasSelected =
            selectedIndex_ >= 0 &&
            static_cast<std::size_t>(selectedIndex_) < projects_.size();
        const bool selectedOpenable = hasSelected &&
            projects_[static_cast<std::size_t>(selectedIndex_)].descriptorValid;

        panel(
            Preview,
            PanelDeep,
            hovered_ == HoverTarget::ProjectPreview ? CyanDim : BorderBright);
        if (projectArtwork_.IsValid())
        {
            drawResourceContained(
                projectArtwork_,
                XMFLOAT4(Preview.x + 4.0f, Preview.y + 4.0f, Preview.z - 4.0f, Preview.w - 4.0f));
            DrawRect(sx(Preview.x + 10.0f), sy(Preview.w - 40.0f), sw(Preview.z - Preview.x - 20.0f), sw(28.0f), wi::Color(3, 8, 12, 205), cmd);
            if (hasSelected)
            {
                text(
                    UpperAscii(Ellipsize(projects_[static_cast<std::size_t>(selectedIndex_)].name, 48u)),
                    Preview.x + 20.0f,
                    Preview.w - 34.0f,
                    9,
                    TextStrong,
                    wi::font::WIFALIGN_LEFT,
                    0.7f,
                    0.10f);
            }
        }
        else
        {
            for (float x = Preview.x + 20.0f; x < Preview.z - 20.0f; x += 42.0f)
                DrawRect(sx(x), sy(Preview.y + 20.0f), std::max(1.0f, scale_ * 0.45f), sw(Preview.w - Preview.y - 40.0f), wi::Color(95, 216, 255, 20), cmd);
            for (float y = Preview.y + 20.0f; y < Preview.w - 20.0f; y += 42.0f)
                DrawRect(sx(Preview.x + 20.0f), sy(y), sw(Preview.z - Preview.x - 40.0f), std::max(1.0f, scale_ * 0.45f), wi::Color(95, 216, 255, 20), cmd);

            text(hasSelected ? "NO PREVIEW LOADED" : "NO RECENT PROJECTS", 792.0f, 367.0f, 15, TextStrong, wi::font::WIFALIGN_CENTER, 1.1f, 0.11f);
            text(
                hasSelected
                    ? (selectedOpenable ? "CLICK TO SET FROM LOCAL STORAGE" : "PROJECT LOCATION IS UNAVAILABLE")
                    : "CREATE A NEW PROJECT OR OPEN AN EXISTING RENEGADE PROJECT",
                792.0f,
                401.0f,
                9,
                selectedOpenable ? CyanDim : Muted,
                wi::font::WIFALIGN_CENTER,
                0.6f,
                0.08f);
        }

        if (hasSelected)
        {
            const auto& selected = projects_[static_cast<std::size_t>(selectedIndex_)];
            text("SELECTED // " + UpperAscii(Ellipsize(selected.name, 40u)), 448.0f, 565.0f, 8, Muted, wi::font::WIFALIGN_LEFT, 0.45f, 0.08f);
        }

        if (projects_.empty())
        {
            const bool lowerHover = hovered_ == HoverTarget::LowerNewProject;
            panel(LowerNew, lowerHover ? PanelRaised : PanelDeep, lowerHover ? CyanDim : Border);
            panel(XMFLOAT4(459.0f, 640.0f, 568.0f, 752.0f), Panel, lowerHover ? Cyan : BorderBright);
            text("+", 513.5f, 661.0f, 43, lowerHover ? Cyan : Text, wi::font::WIFALIGN_CENTER, 0.0f, 0.01f);
            line(592.0f, 642.0f, 1.0f, BorderBright);
            text("NEW PROJECT", 620.0f, 652.0f, 20, TextStrong, wi::font::WIFALIGN_LEFT, 1.4f, 0.10f);
            text("CREATE A NEW PROJECT", 620.0f, 690.0f, 9, Muted, wi::font::WIFALIGN_LEFT, 1.1f, 0.08f);
            text("Start and save a Renegade project to your local system.", 620.0f, 719.0f, 9, Dim, wi::font::WIFALIGN_LEFT, 0.35f, 0.06f);
        }
        else
        {
            const std::size_t start = VisibleRecentStart();
            const std::array<HoverTarget, 3> cardTargets = {{
                HoverTarget::RecentCard0,
                HoverTarget::RecentCard1,
                HoverTarget::RecentCard2,
            }};
            for (std::size_t slot = 0; slot < RecentCards.size(); ++slot)
            {
                const XMFLOAT4& card = RecentCards[slot];
                const std::size_t index = start + slot;
                if (index >= projects_.size())
                {
                    panel(card, PanelDeep, Border);
                    text("NO PROJECT", (card.x + card.z) * 0.5f, card.y + 72.0f, 8, Dim, wi::font::WIFALIGN_CENTER, 0.7f, 0.08f);
                    continue;
                }

                const auto& project = projects_[index];
                const bool selected = selectedIndex_ == static_cast<int>(index);
                const bool hover = hovered_ == cardTargets[slot];
                panel(
                    card,
                    selected || hover ? PanelRaised : PanelDeep,
                    selected ? Cyan : (hover ? CyanDim : Border));
                if (selected)
                    DrawRect(sx(card.x + 1.0f), sy(card.y + 1.0f), sw(card.z - card.x - 2.0f), std::max(2.0f, sw(3.0f)), Orange, cmd);

                const XMFLOAT4 imageRect(
                    card.x + 4.0f,
                    card.y + 5.0f,
                    card.z - 4.0f,
                    card.y + 130.0f);
                DrawRect(
                    sx(imageRect.x),
                    sy(imageRect.y),
                    sw(imageRect.z - imageRect.x),
                    sw(imageRect.w - imageRect.y),
                    wi::Color(3, 8, 12, 255),
                    cmd);
                if (index < projectArtworkCache_.size() &&
                    projectArtworkCache_[index].IsValid())
                {
                    drawResourceContained(projectArtworkCache_[index], imageRect);
                }
                else
                {
                    text("RENEGADE", (card.x + card.z) * 0.5f, card.y + 48.0f, 10, Dim, wi::font::WIFALIGN_CENTER, 1.3f, 0.09f);
                    text("PROJECT", (card.x + card.z) * 0.5f, card.y + 73.0f, 7, CyanDim, wi::font::WIFALIGN_CENTER, 1.0f, 0.07f);
                }

                DrawRect(
                    sx(card.x + 4.0f),
                    sy(card.y + 132.0f),
                    sw(card.z - card.x - 8.0f),
                    sw(30.0f),
                    wi::Color(4, 10, 14, 246),
                    cmd);
                text(
                    UpperAscii(Ellipsize(project.name, 22u)),
                    card.x + 11.0f,
                    card.y + 138.0f,
                    8,
                    project.descriptorValid ? TextStrong : Warning,
                    wi::font::WIFALIGN_LEFT,
                    0.45f,
                    0.08f);
                text(
                    project.descriptorValid ? "READY" : "MISSING",
                    card.z - 10.0f,
                    card.y + 139.0f,
                    7,
                    project.descriptorValid ? Success : Warning,
                    wi::font::WIFALIGN_RIGHT,
                    0.35f,
                    0.08f);
            }
        }

        text("PROJECT DETAILS", 1213.0f, 154.0f, 15, TextStrong, wi::font::WIFALIGN_LEFT, 1.4f, 0.11f);
        panel(DetailsHero, PanelDeep, BorderBright);
        if (projectArtwork_.IsValid())
        {
            drawResourceContained(
                projectArtwork_,
                XMFLOAT4(DetailsHero.x + 4.0f, DetailsHero.y + 4.0f, DetailsHero.z - 4.0f, DetailsHero.w - 4.0f));
        }
        else
        {
            text("RENEGADE", 1417.0f, 257.0f, 18, Dim, wi::font::WIFALIGN_CENTER, 2.0f, 0.10f);
            text("PROJECT", 1417.0f, 291.0f, 9, CyanDim, wi::font::WIFALIGN_CENTER, 1.5f, 0.08f);
        }

        float detailY = 394.0f;
        const auto detail = [&](const char* label, const std::string& value, const wi::Color valueColor = Text)
        {
            text(label, 1214.0f, detailY, 8, Dim, wi::font::WIFALIGN_LEFT, 0.8f, 0.08f);
            text(value, 1214.0f, detailY + 20.0f, 9, valueColor, wi::font::WIFALIGN_LEFT, 0.35f, 0.08f);
            detailY += 54.0f;
            line(1214.0f, detailY - 10.0f, 405.0f, Border);
        };

        if (hasSelected)
        {
            const auto& selected = projects_[static_cast<std::size_t>(selectedIndex_)];
            detail("PROJECT NAME", UpperAscii(Ellipsize(selected.name, 42u)), TextStrong);
            detail("STATUS", selected.descriptorValid ? "READY" : "PROJECT UNAVAILABLE", selected.descriptorValid ? Success : Warning);
            detail("ENGINE / PROJECT FORMAT", "RENEGADE PROJECT V" + std::to_string(selected.formatVersion), Text);
            detail("PROJECT PATH", Ellipsize(selected.rootPath.empty() ? selected.descriptorPath : selected.rootPath, 52u), CyanDim);
            if (!selected.startupFlow.empty())
            {
                detail("PROJECT HOME", Ellipsize(selected.startupFlow, 52u), Muted);
            }
            else
            {
                detail("STARTUP SCENE", Ellipsize(selected.startupScene, 52u), Muted);
            }
        }
        else
        {
            detail("PROJECT NAME", "NO PROJECT SELECTED", Muted);
            detail("STATUS", "-", Dim);
            detail("ENGINE / PROJECT FORMAT", "RENEGADE PROJECT", Text);
            detail("PROJECT PATH", "-", Dim);
            detail("PROJECT HOME", "-", Dim);
        }

        panel(
            OpenSelected,
            selectedOpenable ? PanelRaised : PanelDeep,
            selectedOpenable
                ? (hovered_ == HoverTarget::OpenSelected ? Cyan : CyanDim)
                : Border);
        text(
            selectedOpenable ? "OPEN PROJECT  >>" : "PROJECT UNAVAILABLE",
            1417.0f,
            726.0f,
            selectedOpenable ? 17 : 12,
            selectedOpenable ? TextStrong : Dim,
            wi::font::WIFALIGN_CENTER,
            1.2f,
            0.10f);

        text("SYSTEM STATUS", 50.0f, 843.0f, 8, Dim, wi::font::WIFALIGN_LEFT, 0.8f, 0.08f);
        text("OPTIMAL", 50.0f, 870.0f, 11, Success, wi::font::WIFALIGN_LEFT, 0.7f, 0.10f);
        line(180.0f, 836.0f, 1.0f, Border);
        text("SOURCE", 205.0f, 843.0f, 8, Dim, wi::font::WIFALIGN_LEFT, 0.8f, 0.08f);
        text(ShortSourceRevision(), 205.0f, 870.0f, 10, Text, wi::font::WIFALIGN_LEFT, 0.5f, 0.08f);
        line(330.0f, 836.0f, 1.0f, Border);
        text("RENDER BACKEND", 355.0f, 843.0f, 8, Dim, wi::font::WIFALIGN_LEFT, 0.8f, 0.08f);
        text(BackendLabel(), 355.0f, 870.0f, 10, Text, wi::font::WIFALIGN_LEFT, 0.6f, 0.08f);
        line(510.0f, 836.0f, 1.0f, Border);
        text("TARGET PLATFORM", 535.0f, 843.0f, 8, Dim, wi::font::WIFALIGN_LEFT, 0.8f, 0.08f);
        text("WINDOWS PC", 535.0f, 870.0f, 10, Text, wi::font::WIFALIGN_LEFT, 0.6f, 0.08f);

        const std::string status =
            statusProvider_ ? statusProvider_() : statusText_;
        text(
            UpperAscii(Ellipsize(status, 66u)),
            1620.0f,
            855.0f,
            9,
            Muted,
            wi::font::WIFALIGN_RIGHT,
            0.45f,
            0.08f);
        text(
            "RECENT PROJECTS // " + std::to_string(projects_.size()),
            1620.0f,
            882.0f,
            8,
            Dim,
            wi::font::WIFALIGN_RIGHT,
            0.45f,
            0.08f);

        if (newProjectMode_)
        {
            DrawRect(sx(0.0f), sy(0.0f), sw(DesignWidth), sw(DesignHeight), wi::Color(0, 4, 7, 194), cmd);
            panel(XMFLOAT4(548.0f, 328.0f, 1124.0f, 558.0f), PanelRaised, CyanDim);
            text("CREATE NEW PROJECT", 604.0f, 354.0f, 21, TextStrong, wi::font::WIFALIGN_LEFT, 1.3f, 0.11f);
            text("PROJECT NAME", 604.0f, 390.0f, 8, Muted, wi::font::WIFALIGN_LEFT, 0.8f, 0.08f);
            text("Choose a project name, then select the local parent folder.", 604.0f, 531.0f, 8, Muted, wi::font::WIFALIGN_LEFT, 0.35f, 0.07f);
        }
    }
}
