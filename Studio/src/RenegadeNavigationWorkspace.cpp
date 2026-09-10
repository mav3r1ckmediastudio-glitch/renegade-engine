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
        bool showPath = true;
        bool pathValid = false;
        XMFLOAT4 bounds = {};
        bridge::StudioSession* session = nullptr;
        wi::ecs::Entity selected = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity previewAgent = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity previewGrid = wi::ecs::INVALID_ENTITY;
        std::uint64_t sceneRevision = 0;
        bridge::NavigationGridSettings pending;
        bridge::NavigationPathResult previewPath;
        std::string status = "NAVIGATION // READY";
        bool statusError = false;
        std::string selectionInfo = "NO NAVIGATION ENTITY SELECTED";
        std::vector<wi::gui::Widget*> controls;

        SceneInspectorSlider resolutionX;
        SceneInspectorSlider resolutionY;
        SceneInspectorSlider resolutionZ;
        SceneInspectorSlider voxelSize;
        SceneInspectorButton rebuild;
        SceneInspectorButton fitScene;
        SceneInspectorButton createAgentPair;
        SceneInspectorButton refreshPath;
        SceneInspectorCheckBox debugVoxels;
        SceneInspectorCheckBox debugPath;

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

        [[nodiscard]] bool SelectedIsAgent() const noexcept
        {
            const auto* scene = Scene();
            return scene != nullptr &&
                bridge::IsRenegadeNavigationAgent(*scene, selected);
        }

        [[nodiscard]] bool SelectedIsDestination() const noexcept
        {
            const auto* scene = Scene();
            return scene != nullptr &&
                bridge::IsRenegadeNavigationDestination(*scene, selected);
        }

        [[nodiscard]] bool SelectedIsNavigation() const noexcept
        {
            return SelectedIsGrid() || SelectedIsAgent() ||
                SelectedIsDestination();
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

            createAgentPair.Create("Navigation Create Agent Pair");
            createAgentPair.SetText("CREATE TEST AGENT + TARGET");
            createAgentPair.SetTooltip(
                "Create a visible native Wicked CharacterComponent agent and linked destination for this grid. Move both markers with the normal gizmo, refresh the path, then run Test Level to prove native following.");
            createAgentPair.OnClick([this](const wi::gui::EventArgs&)
            {
                CreateAgentPair();
            });

            refreshPath.Create("Navigation Refresh Path");
            refreshPath.SetText("REFRESH PATH");
            refreshPath.SetTooltip(
                "Run Wicked PathQuery between the authored agent and destination and update the editor path preview.");
            refreshPath.OnClick([this](const wi::gui::EventArgs&)
            {
                RefreshPathPreview(true);
            });

            debugVoxels.Create("Show navigation voxels: ");
            debugVoxels.SetCheck(true);
            debugVoxels.SetTooltip(
                "Draw Wicked VoxelGrid's native debug visualization in the editor viewport. This is editor-only presentation state.");
            debugVoxels.OnClick([this](const wi::gui::EventArgs& args)
            {
                showVoxels = args.bValue;
            });

            debugPath.Create("Show queried path: ");
            debugPath.SetCheck(true);
            debugPath.SetTooltip(
                "Draw the latest native Wicked PathQuery waypoints in the editor viewport. Move the target and press Refresh Path, or let scene edits refresh it automatically.");
            debugPath.OnClick([this](const wi::gui::EventArgs& args)
            {
                showPath = args.bValue;
            });

            controls = {
                &resolutionX, &resolutionY, &resolutionZ, &voxelSize,
                &rebuild, &fitScene, &createAgentPair, &refreshPath,
                &debugVoxels, &debugPath,
            };
            for (auto* control : controls)
                control->SetVisible(false);
            created = true;
        }

        [[nodiscard]] wi::ecs::Entity FindPreviewAgent() const
        {
            const auto* scene = Scene();
            if (scene == nullptr)
                return wi::ecs::INVALID_ENTITY;
            if (SelectedIsAgent())
                return selected;

            for (const auto agent : bridge::CollectNavigationAgents(*scene))
            {
                bridge::NavigationAgentBinding binding;
                std::string ignored;
                if (!bridge::ResolveNavigationAgentBinding(
                        *scene, agent, binding, ignored))
                    continue;
                if (SelectedIsGrid() && binding.grid == selected)
                    return agent;
                if (SelectedIsDestination() && binding.destination == selected)
                    return agent;
            }
            return wi::ecs::INVALID_ENTITY;
        }

        [[nodiscard]] wi::ecs::Entity GridForSelection() const
        {
            if (SelectedIsGrid())
                return selected;
            const auto* scene = Scene();
            if (scene == nullptr)
                return wi::ecs::INVALID_ENTITY;
            const auto agent = FindPreviewAgent();
            if (agent == wi::ecs::INVALID_ENTITY)
                return wi::ecs::INVALID_ENTITY;
            bridge::NavigationAgentBinding binding;
            std::string ignored;
            if (!bridge::ResolveNavigationAgentBinding(
                    *scene, agent, binding, ignored))
                return wi::ecs::INVALID_ENTITY;
            return binding.grid;
        }

        void RefreshPathPreview(const bool reportStatus)
        {
            pathValid = false;
            previewAgent = FindPreviewAgent();
            previewGrid = wi::ecs::INVALID_ENTITY;
            previewPath = {};
            if (previewAgent == wi::ecs::INVALID_ENTITY || Scene() == nullptr)
            {
                if (reportStatus)
                    SetStatus("NAVIGATION PATH // CREATE OR SELECT AN AGENT", true);
                return;
            }

            bridge::NavigationAgentBinding binding;
            std::string error;
            if (!bridge::ResolveNavigationAgentBinding(
                    *Scene(), previewAgent, binding, error))
            {
                if (reportStatus)
                    SetStatus("NAVIGATION PATH // " + error, true);
                return;
            }
            previewGrid = binding.grid;
            if (!bridge::QueryNavigationAgentPath(
                    *Scene(), previewAgent, previewPath, error))
            {
                if (reportStatus)
                    SetStatus("NAVIGATION PATH // " + error, true);
                return;
            }

            pathValid = previewPath.successful &&
                previewPath.waypoints.size() >= 2;
            if (reportStatus)
            {
                if (pathValid)
                {
                    SetStatus(
                        "NAVIGATION PATH // " +
                        std::to_string(previewPath.waypoints.size()) +
                        " WAYPOINTS // READY FOR TEST LEVEL");
                }
                else
                {
                    SetStatus(
                        "NAVIGATION PATH // NO ROUTE // MOVE MARKERS ONTO WALKABLE VOXELS",
                        true);
                }
            }
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
            const bool navigationSelected = SelectedIsNavigation();
            for (auto* control : controls)
                control->SetVisible(false);

            resolutionX.SetVisible(active && gridSelected);
            resolutionY.SetVisible(active && gridSelected);
            resolutionZ.SetVisible(active && gridSelected);
            voxelSize.SetVisible(active && gridSelected);
            rebuild.SetVisible(active && gridSelected);
            fitScene.SetVisible(active && gridSelected);
            createAgentPair.SetVisible(active && gridSelected);
            debugVoxels.SetVisible(active && navigationSelected);

            if (gridSelected)
            {
                pending = bridge::CaptureNavigationGridSettings(*Scene(), selected);
                pending.fitToSceneBounds = false;
                pendingDirty = false;
                resolutionX.SetValue(static_cast<float>(pending.resolutionX));
                resolutionY.SetValue(static_cast<float>(pending.resolutionY));
                resolutionZ.SetValue(static_cast<float>(pending.resolutionZ));
                voxelSize.SetValue(pending.voxelSize);

                std::ostringstream stream;
                stream << "GRID "
                       << pending.resolutionX << " x "
                       << pending.resolutionY << " x "
                       << pending.resolutionZ
                       << " // VOXEL " << pending.voxelSize << " M";
                selectionInfo = stream.str();
            }
            else if (SelectedIsAgent())
            {
                selectionInfo = "NAVIGATION AGENT // NATIVE WICKED CHARACTER";
                pendingDirty = false;
            }
            else if (SelectedIsDestination())
            {
                selectionInfo = "NAVIGATION DESTINATION // MOVE WITH GIZMO";
                pendingDirty = false;
            }
            else
            {
                selectionInfo = "NO NAVIGATION ENTITY SELECTED";
                pendingDirty = false;
            }

            debugVoxels.SetCheck(showVoxels);
            RefreshPathPreview(false);
            const bool hasPathAgent = previewAgent != wi::ecs::INVALID_ENTITY;
            refreshPath.SetVisible(active && navigationSelected && hasPathAgent);
            debugPath.SetVisible(active && navigationSelected && hasPathAgent);
            debugPath.SetCheck(showPath);
            refreshPending = false;
        }

        void Layout()
        {
            const float x = bounds.x + 12.0f;
            const float width = std::max(80.0f, bounds.z - 24.0f);
            float y = bounds.y + HeaderHeight;
            const auto full = [&](wi::gui::Widget& widget)
            {
                if (!widget.IsVisible())
                    return;
                widget.SetPos(XMFLOAT2(x, y));
                widget.SetSize(XMFLOAT2(width, RowHeight));
                y += RowHeight + RowGap;
            };
            const auto two = [&](wi::gui::Widget& left, wi::gui::Widget& right)
            {
                const bool leftVisible = left.IsVisible();
                const bool rightVisible = right.IsVisible();
                if (!leftVisible && !rightVisible)
                    return;
                if (leftVisible && rightVisible)
                {
                    const float half = (width - RowGap) * 0.5f;
                    left.SetPos(XMFLOAT2(x, y));
                    right.SetPos(XMFLOAT2(x + half + RowGap, y));
                    left.SetSize(XMFLOAT2(half, RowHeight));
                    right.SetSize(XMFLOAT2(half, RowHeight));
                }
                else
                {
                    auto& visible = leftVisible ? left : right;
                    visible.SetPos(XMFLOAT2(x, y));
                    visible.SetSize(XMFLOAT2(width, RowHeight));
                }
                y += RowHeight + RowGap;
            };

            full(resolutionX);
            full(resolutionY);
            full(resolutionZ);
            full(voxelSize);
            if (SelectedIsGrid())
                y += SectionGap;
            two(rebuild, fitScene);
            full(createAgentPair);
            if (refreshPath.IsVisible() || debugPath.IsVisible())
                y += SectionGap;
            full(refreshPath);
            full(debugPath);
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

        void CreateAgentPair()
        {
            if (!SelectedIsGrid() || session == nullptr)
            {
                SetStatus("NAVIGATION // SELECT A NAVIGATION GRID", true);
                return;
            }

            const auto& camera = wi::scene::GetCamera();
            const XMFLOAT3 agentPosition{
                camera.Eye.x + camera.At.x * 3.0f,
                camera.Eye.y + camera.At.y * 3.0f,
                camera.Eye.z + camera.At.z * 3.0f};
            const XMFLOAT3 destinationPosition{
                camera.Eye.x + camera.At.x * 8.0f,
                camera.Eye.y + camera.At.y * 8.0f,
                camera.Eye.z + camera.At.z * 8.0f};

            auto& scene = session->Scenes().GetScene();
            auto command = std::make_unique<bridge::CreateNavigationAgentPairCommand>(
                scene, selected, agentPosition, destinationPosition);
            auto* createdCommand = command.get();
            if (!session->Commands().Execute(std::move(command)))
            {
                SetStatus("NAVIGATION // AGENT + TARGET CREATION FAILED", true);
                return;
            }

            // Leave the target selected so the creator can immediately place it
            // with the normal transform gizmo. Selecting either marker keeps
            // this specialist Inspector active.
            selected = createdCommand->CreatedDestination();
            session->Selection().Select(selected);
            refreshPending = true;
            SetStatus(
                "NAVIGATION AGENT + TARGET // CREATED // MOVE BOTH ONTO WALKABLE SURFACE");
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

        void QueuePathDebugDraw() const
        {
            if (!showPath || !pathValid || previewPath.waypoints.size() < 2)
                return;
            for (std::size_t index = 1; index < previewPath.waypoints.size(); ++index)
            {
                wi::renderer::RenderableLine line;
                line.start = previewPath.waypoints[index - 1];
                line.end = previewPath.waypoints[index];
                line.color_start = XMFLOAT4(1.0f, 0.45f, 0.08f, 1.0f);
                line.color_end = XMFLOAT4(1.0f, 0.75f, 0.12f, 1.0f);
                wi::renderer::DrawLine(line);
            }
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
        impl_->QueuePathDebugDraw();
    }

    void RenegadeNavigationWorkspace::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!impl_->created || !impl_->active)
            return;

        // This workspace renders its child controls manually. RenegadeSlider
        // deliberately sets a tight per-control scissor, so restore the
        // workspace scissor before every subsequent control/status draw or the
        // last slider clips all buttons and check boxes beneath it.
        ApplyScissor(canvas, scissorRect, cmd);

        if (impl_->showVoxels)
        {
            const auto gridEntity = impl_->GridForSelection();
            if (gridEntity != wi::ecs::INVALID_ENTITY && impl_->Scene() != nullptr)
            {
                if (const auto* grid =
                        impl_->Scene()->voxel_grids.GetComponent(gridEntity);
                    grid != nullptr && grid->IsValid())
                {
                    grid->debugdraw(cmd);
                }
            }
        }

        const auto& b = impl_->bounds;
        DrawRect(b.x, b.y, b.z, b.w, Surface0, cmd);
        DrawRect(b.x, b.y, b.z, 1.0f, Border, cmd);
        DrawText("NAVIGATION // NATIVE WICKED VOXEL GRID",
            b.x + 12.0f, b.y + 10.0f, 13, TextStrong, cmd);
        DrawText(impl_->selectionInfo,
            b.x + 12.0f, b.y + 32.0f, 10,
            impl_->SelectedIsNavigation() ? TextSecondary : Muted, cmd, 0.08f);
        DrawText("PATHQUERY + CHARACTER TURN/MOVE // TEST LEVEL READY",
            b.x + 12.0f, b.y + 52.0f, 9, Forge, cmd, 0.08f);

        for (auto* control : impl_->controls)
        {
            if (control->IsVisible())
            {
                ApplyScissor(canvas, scissorRect, cmd);
                control->Render(canvas, cmd);
            }
        }

        ApplyScissor(canvas, scissorRect, cmd);
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
