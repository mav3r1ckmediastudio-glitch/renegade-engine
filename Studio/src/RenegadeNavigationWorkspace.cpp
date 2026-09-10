#include "RenegadeNavigationWorkspace.h"

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using renegade::studio::SceneInspectorButton;
    using renegade::studio::SceneInspectorCheckBox;
    using renegade::studio::SceneInspectorSlider;

    constexpr float HeaderHeight = 82.0f;
    constexpr float RowHeight = 30.0f;
    constexpr float RowGap = 6.0f;
    constexpr float SectionGap = 12.0f;

    constexpr wi::Color Surface0 = wi::Color(8, 12, 16, 255);
    constexpr wi::Color Surface1 = wi::Color(12, 18, 22, 255);
    constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
    constexpr wi::Color TextSecondary = wi::Color(214, 222, 226, 255);
    constexpr wi::Color Muted = wi::Color(139, 151, 158, 255);
    constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
    constexpr wi::Color Error = wi::Color(229, 92, 92, 255);

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

    void DrawText(
        const std::string& value,
        const float x,
        const float y,
        const int size,
        const wi::Color color,
        const wi::graphics::CommandList cmd,
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
        params.bolden = bolden;
        wi::font::Draw(value, params, cmd);
    }

    std::uint32_t ResolutionForHalfWidth(
        const float halfWidth,
        const float voxelSize) noexcept
    {
        if (!(halfWidth > 0.0f) || !(voxelSize > 0.0f))
            return 4u;
        return std::clamp(
            static_cast<std::uint32_t>(std::ceil(halfWidth / voxelSize)),
            4u,
            1024u);
    }
}

namespace renegade::studio
{
    struct RenegadeNavigationWorkspace::Impl
    {
        bool created = false;
        bool active = false;
        bool pointerConsumed = false;
        bool refreshPending = true;
        bool pendingDirty = false;
        bool showVoxels = true;
        XMFLOAT4 bounds = {};
        bridge::StudioSession* session = nullptr;
        wi::ecs::Entity selected = wi::ecs::INVALID_ENTITY;
        std::uint64_t sceneRevision = 0;
        bridge::NavigationGridSettings pending;
        std::string status = "NAVIGATION // READY";
        bool statusError = false;
        std::string gridInfo = "NO NAVIGATION GRID SELECTED";
        std::vector<wi::gui::Widget*> controls;

        SceneInspectorSlider resolutionX;
        SceneInspectorSlider resolutionY;
        SceneInspectorSlider resolutionZ;
        SceneInspectorSlider voxelSize;
        SceneInspectorButton rebuild;
        SceneInspectorButton fitScene;
        SceneInspectorCheckBox debugVoxels;

        [[nodiscard]] wi::scene::Scene* Scene() const noexcept
        {
            return session != nullptr ? &session->Scenes().GetScene() : nullptr;
        }

        [[nodiscard]] bool SelectedIsGrid() const noexcept
        {
            const auto* scene = Scene();
            return scene != nullptr &&
                bridge::IsRenegadeNavigationGrid(*scene, selected);
        }

        void SetStatus(std::string value, const bool error = false)
        {
            status = std::move(value);
            statusError = error;
        }

        void MarkPending()
        {
            pendingDirty = true;
            SetStatus("NAVIGATION SETTINGS CHANGED // PRESS REBUILD");
        }

        void CreateControls()
        {
            resolutionX.Create(
                4.0f, 1024.0f, 128.0f, 1020.0f,
                "Navigation Resolution X", "RESOLUTION X");
            resolutionX.OnValueCommitted([this](const float value)
            {
                pending.resolutionX = static_cast<std::uint32_t>(
                    std::clamp(std::lround(value), 4l, 1024l));
                MarkPending();
            });

            resolutionY.Create(
                4.0f, 1024.0f, 32.0f, 1020.0f,
                "Navigation Resolution Y", "RESOLUTION Y");
            resolutionY.OnValueCommitted([this](const float value)
            {
                pending.resolutionY = static_cast<std::uint32_t>(
                    std::clamp(std::lround(value), 4l, 1024l));
                MarkPending();
            });

            resolutionZ.Create(
                4.0f, 1024.0f, 128.0f, 1020.0f,
                "Navigation Resolution Z", "RESOLUTION Z");
            resolutionZ.OnValueCommitted([this](const float value)
            {
                pending.resolutionZ = static_cast<std::uint32_t>(
                    std::clamp(std::lround(value), 4l, 1024l));
                MarkPending();
            });

            voxelSize.Create(
                0.05f, 4.0f, 0.25f, 395.0f,
                "Navigation Voxel Size", "VOXEL SIZE (M)");
            voxelSize.OnValueCommitted([this](const float value)
            {
                pending.voxelSize = std::clamp(value, 0.05f, 4.0f);
                pending.fitToSceneBounds = false;
                MarkPending();
            });

            rebuild.Create("Navigation Rebuild");
            rebuild.SetText("REBUILD GRID");
            rebuild.SetTooltip(
                "Regenerate this navigation grid from current terrain/navigation meshes and colliders using the authored resolution and voxel size.");
            rebuild.OnClick([this](const wi::gui::EventArgs&)
            {
                Rebuild(false);
            });

            fitScene.Create("Navigation Fit Scene");
            fitScene.SetText("FIT TO SCENE + REBUILD");
            fitScene.SetTooltip(
                "Fit a uniform native Wicked voxel grid around current scene bounds. Voxel size is automatically increased when required to stay inside Renegade's bounded grid limits.");
            fitScene.OnClick([this](const wi::gui::EventArgs&)
            {
                Rebuild(true);
            });

            debugVoxels.Create("Show navigation voxels: ");
            debugVoxels.SetCheck(true);
            debugVoxels.SetTooltip(
                "Draw Wicked VoxelGrid's native debug visualization in the editor viewport. This is editor-only presentation state.");
            debugVoxels.OnClick([this](const wi::gui::EventArgs& args)
            {
                showVoxels = args.bValue;
            });

            controls = {
                &resolutionX, &resolutionY, &resolutionZ, &voxelSize,
                &rebuild, &fitScene, &debugVoxels,
            };
            for (auto* control : controls)
                control->SetVisible(false);
            created = true;
        }

        void Refresh()
        {
            if (session == nullptr)
                session = bridge::StudioSession::Current();

            wi::ecs::Entity next = wi::ecs::INVALID_ENTITY;
            if (session != nullptr && session->Selection().HasSelection())
                next = session->Selection().SelectedEntity();
            selected = next;

            const bool gridSelected = SelectedIsGrid();
            for (auto* control : controls)
                control->SetVisible(active && gridSelected);

            if (gridSelected)
            {
                pending = bridge::CaptureNavigationGridSettings(*Scene(), selected);
                pending.fitToSceneBounds = false;
                pendingDirty = false;
                resolutionX.SetValue(static_cast<float>(pending.resolutionX));
                resolutionY.SetValue(static_cast<float>(pending.resolutionY));
                resolutionZ.SetValue(static_cast<float>(pending.resolutionZ));
                voxelSize.SetValue(pending.voxelSize);
                debugVoxels.SetCheck(showVoxels);

                std::ostringstream stream;
                stream << "GRID "
                       << pending.resolutionX << " x "
                       << pending.resolutionY << " x "
                       << pending.resolutionZ
                       << " // VOXEL " << pending.voxelSize << " M";
                gridInfo = stream.str();
            }
            else
            {
                gridInfo = "NO NAVIGATION GRID SELECTED";
                pendingDirty = false;
            }
            refreshPending = false;
        }

        void Layout()
        {
            const float x = bounds.x + 12.0f;
            const float width = std::max(80.0f, bounds.z - 24.0f);
            float y = bounds.y + HeaderHeight;
            const auto full = [&](wi::gui::Widget& widget)
            {
                widget.SetPos(XMFLOAT2(x, y));
                widget.SetSize(XMFLOAT2(width, RowHeight));
                y += RowHeight + RowGap;
            };
            const auto two = [&](wi::gui::Widget& left, wi::gui::Widget& right)
            {
                const float half = (width - RowGap) * 0.5f;
                left.SetPos(XMFLOAT2(x, y));
                right.SetPos(XMFLOAT2(x + half + RowGap, y));
                left.SetSize(XMFLOAT2(half, RowHeight));
                right.SetSize(XMFLOAT2(half, RowHeight));
                y += RowHeight + RowGap;
            };

            full(resolutionX);
            full(resolutionY);
            full(resolutionZ);
            full(voxelSize);
            y += SectionGap;
            two(rebuild, fitScene);
            full(debugVoxels);
        }

        void CreateGrid()
        {
            if (session == nullptr)
                session = bridge::StudioSession::Current();
            if (session == nullptr || !session->Projects().HasProject())
            {
                SetStatus("NAVIGATION // OPEN A PROJECT FIRST", true);
                return;
            }

            auto& scene = session->Scenes().GetScene();
            bridge::NavigationGridSettings settings;
            auto command = std::make_unique<bridge::CreateNavigationGridCommand>(
                scene, settings);
            auto* createdCommand = command.get();
            if (!session->Commands().Execute(std::move(command)))
            {
                SetStatus("NAVIGATION // GRID CREATION FAILED", true);
                return;
            }

            selected = createdCommand->CreatedEntity();
            session->Selection().Select(selected);
            refreshPending = true;
            pendingDirty = false;
            SetStatus("NAVIGATION GRID // CREATED // FIT TO SCENE WHEN READY");
        }

        bool FitSettingsToScene(bridge::NavigationGridSettings& settings)
        {
            auto* scene = Scene();
            if (scene == nullptr)
                return false;

            const XMFLOAT3 halfWidth = scene->bounds.getHalfWidth();
            const XMFLOAT3 center = scene->bounds.getCenter();
            if (!std::isfinite(halfWidth.x) || !std::isfinite(halfWidth.y) ||
                !std::isfinite(halfWidth.z) ||
                halfWidth.x <= 0.0f || halfWidth.y <= 0.0f ||
                halfWidth.z <= 0.0f)
            {
                SetStatus("NAVIGATION // SCENE BOUNDS ARE EMPTY", true);
                return false;
            }

            float size = std::max(0.05f, settings.voxelSize);
            const float axisMinimum = std::max({
                halfWidth.x / 1024.0f,
                halfWidth.y / 1024.0f,
                halfWidth.z / 1024.0f});
            size = std::max(size, axisMinimum);

            // Also respect the global 128M-voxel staging budget. Increasing a
            // uniform voxel size monotonically decreases all three dimensions.
            for (int attempt = 0; attempt < 64; ++attempt)
            {
                const auto rx = ResolutionForHalfWidth(halfWidth.x, size);
                const auto ry = ResolutionForHalfWidth(halfWidth.y, size);
                const auto rz = ResolutionForHalfWidth(halfWidth.z, size);
                const std::uint64_t count =
                    static_cast<std::uint64_t>(rx) * ry * rz;
                if (count <= 128ull * 1024ull * 1024ull)
                {
                    settings.center = center;
                    settings.voxelSize = size;
                    settings.resolutionX = rx;
                    settings.resolutionY = ry;
                    settings.resolutionZ = rz;
                    settings.fitToSceneBounds = false;
                    return true;
                }
                size *= 1.1f;
            }

            SetStatus("NAVIGATION // SCENE CANNOT FIT WITHIN GRID BUDGET", true);
            return false;
        }

        void Rebuild(const bool fitSceneBounds)
        {
            if (!SelectedIsGrid() || session == nullptr)
            {
                SetStatus("NAVIGATION // SELECT A NAVIGATION GRID", true);
                return;
            }

            auto settings = pending;
            if (fitSceneBounds && !FitSettingsToScene(settings))
                return;

            std::string error;
            if (!bridge::ValidateNavigationGridSettings(settings, error))
            {
                SetStatus("NAVIGATION // " + error, true);
                return;
            }

            auto& scene = session->Scenes().GetScene();
            if (!session->Commands().Execute(
                    std::make_unique<bridge::RebuildNavigationGridCommand>(
                        scene, selected, settings)))
            {
                SetStatus("NAVIGATION // REBUILD FAILED", true);
                return;
            }

            pending = settings;
            pendingDirty = false;
            refreshPending = true;
            SetStatus(
                fitSceneBounds
                    ? "NAVIGATION GRID // FIT TO SCENE + REBUILT"
                    : "NAVIGATION GRID // REBUILT");
        }
    };

    RenegadeNavigationWorkspace::RenegadeNavigationWorkspace()
        : impl_(std::make_unique<Impl>())
    {
    }

    RenegadeNavigationWorkspace::~RenegadeNavigationWorkspace() = default;

    void RenegadeNavigationWorkspace::Create()
    {
        if (!impl_->created)
        {
            SetName("Native Navigation Workspace");
            wi::gui::Widget::SetVisible(false);
            impl_->CreateControls();
        }
    }

    void RenegadeNavigationWorkspace::SetActive(const bool active)
    {
        impl_->active = active;
        impl_->refreshPending = true;
        wi::gui::Widget::SetVisible(active);
        if (!active)
        {
            for (auto* control : impl_->controls)
                control->SetVisible(false);
        }
    }

    bool RenegadeNavigationWorkspace::IsActive() const noexcept
    {
        return impl_->active;
    }

    void RenegadeNavigationWorkspace::SetBounds(const XMFLOAT4& bounds)
    {
        impl_->bounds = bounds;
        SetPos(XMFLOAT2(bounds.x, bounds.y));
        SetSize(XMFLOAT2(bounds.z, bounds.w));
        if (impl_->created)
            impl_->Layout();
    }

    bool RenegadeNavigationWorkspace::ContainsPointer(
        const XMFLOAT4& pointer) const noexcept
    {
        const auto& b = impl_->bounds;
        return impl_->active && pointer.x >= b.x && pointer.x < b.x + b.z &&
            pointer.y >= b.y && pointer.y < b.y + b.w;
    }

    bool RenegadeNavigationWorkspace::ConsumedPointerThisFrame() const noexcept
    {
        return impl_->pointerConsumed;
    }

    bool RenegadeNavigationWorkspace::HasSelectedNavigationGrid() const noexcept
    {
        return impl_->SelectedIsGrid();
    }

    void RenegadeNavigationWorkspace::CreateNavigationGridForScene()
    {
        impl_->CreateGrid();
    }

    void RenegadeNavigationWorkspace::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        impl_->pointerConsumed = false;
        if (!impl_->created || !impl_->active)
            return;

        wi::gui::Widget::Update(canvas, dt);

        auto* currentSession = bridge::StudioSession::Current();
        const wi::ecs::Entity currentSelection =
            currentSession != nullptr && currentSession->Selection().HasSelection()
                ? currentSession->Selection().SelectedEntity()
                : wi::ecs::INVALID_ENTITY;
        const std::uint64_t currentRevision = currentSession != nullptr
            ? currentSession->Scenes().Revision()
            : 0;
        if (currentSession != impl_->session ||
            currentSelection != impl_->selected ||
            (!impl_->pendingDirty && currentRevision != impl_->sceneRevision))
        {
            impl_->session = currentSession;
            impl_->sceneRevision = currentRevision;
            impl_->refreshPending = true;
        }
        if (impl_->refreshPending)
        {
            impl_->Refresh();
            impl_->Layout();
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool inspectorConsumed = ContainsPointer(pointer);
        impl_->pointerConsumed = inspectorConsumed;
        if (inspectorConsumed)
            Activate();
        else
            state = wi::gui::IDLE;

        for (auto* control : impl_->controls)
        {
            if (control->IsVisible())
                control->Update(canvas, dt);
        }
    }

    void RenegadeNavigationWorkspace::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!impl_->created || !impl_->active)
            return;

        if (impl_->showVoxels && impl_->SelectedIsGrid())
        {
            if (const auto* grid = impl_->Scene()->voxel_grids.GetComponent(impl_->selected);
                grid != nullptr && grid->IsValid())
            {
                grid->debugdraw(cmd);
            }
        }

        const auto& b = impl_->bounds;
        DrawRect(b.x, b.y, b.z, b.w, Surface0, cmd);
        DrawRect(b.x, b.y, b.z, 1.0f, Border, cmd);
        DrawText("NAVIGATION // NATIVE WICKED VOXEL GRID",
            b.x + 12.0f, b.y + 10.0f, 13, TextStrong, cmd);
        DrawText(impl_->gridInfo,
            b.x + 12.0f, b.y + 32.0f, 10,
            impl_->SelectedIsGrid() ? TextSecondary : Muted, cmd, 0.08f);
        DrawText("GROUND PATHS + TRUE 3D / FLYING PATHS",
            b.x + 12.0f, b.y + 52.0f, 9, Forge, cmd, 0.08f);

        for (auto* control : impl_->controls)
        {
            if (control->IsVisible())
                control->Render(canvas, cmd);
        }

        const float statusY = b.y + b.w - 28.0f;
        DrawRect(b.x + 8.0f, statusY - 4.0f, b.z - 16.0f, 24.0f, Surface1, cmd);
        DrawText(impl_->status,
            b.x + 12.0f,
            statusY,
            9,
            impl_->statusError ? Error : Muted,
            cmd,
            0.08f);
    }
}
