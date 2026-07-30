#include "StudioApplication.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace
{
    constexpr std::uint8_t SelectionStencilReference = 0x0F;
    constexpr wi::Color HologramIdle = wi::Color(8, 30, 42, 224);
    constexpr wi::Color HologramFocus = wi::Color(0, 126, 164, 238);
    constexpr wi::Color HologramActive = wi::Color(92, 232, 255, 255);
    constexpr wi::Color HologramText = wi::Color(196, 244, 255, 255);
    constexpr wi::Color HologramMuted = wi::Color(102, 166, 181, 255);
    constexpr wi::Color HologramBorder = wi::Color(0, 183, 224, 150);
    constexpr wi::Color HologramPanel = wi::Color(3, 12, 20, 236);
    constexpr wi::Color HologramSelected = wi::Color(0, 102, 138, 245);
    constexpr wi::Color WarningAmber = wi::Color(255, 150, 40, 255);
}

namespace renegade::studio
{
    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
    {
        session_ = &session;
        scene = &session.Scenes().GetScene();
    }

    void StudioRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);
        setFXAAEnabled(false);
        setBloomEnabled(true);

        const XMVECTOR eye = XMVectorSet(10.5f, 7.0f, -14.0f, 1.0f);
        const XMVECTOR at = XMVectorSet(0.0f, 1.6f, 2.0f, 1.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
        editorCameraTransform_.ClearTransform();
        editorCameraTransform_.MatrixTransform(
            XMMatrixInverse(nullptr, view));
        editorCameraTransform_.UpdateTransform();
        camera->TransformCamera(editorCameraTransform_);
        camera->UpdateCamera();

        CreateWorkspaceShell();
        CreateProjectHub();
        ApplyRenegadeTheme();

        gizmo_.isTranslator = true;
        gizmo_.isRotator = false;
        gizmo_.isScalator = false;
        gizmo_.translate_snap = 0.1f;

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        RefreshProjectHub();
        SetProjectHubVisible(true);

        RenderPath3D::Load();
    }

    void StudioRenderPath::DeleteGPUResources()
    {
        selectionOutlineMask_ = {};
        selectionOutlineMaskMsaa_ = {};
        RenderPath3D::DeleteGPUResources();
    }

    void StudioRenderPath::ResizeBuffers()
    {
        RenderPath3D::ResizeBuffers();

        const auto* depthStencil = GetDepthStencil();
        if (depthStencil == nullptr)
        {
            return;
        }

        auto* device = wi::graphics::GetDevice();
        const XMUINT2 resolution = GetInternalResolution();
        wi::graphics::TextureDesc description;
        description.width = resolution.x;
        description.height = resolution.y;
        description.format = wi::graphics::Format::R8_UNORM;
        description.bind_flags =
            wi::graphics::BindFlag::RENDER_TARGET |
            wi::graphics::BindFlag::SHADER_RESOURCE;

        if (getMSAASampleCount() > 1)
        {
            description.sample_count = getMSAASampleCount();
            description.bind_flags = wi::graphics::BindFlag::RENDER_TARGET;
            if (device->CreateTexture(
                    &description,
                    nullptr,
                    &selectionOutlineMaskMsaa_))
            {
                device->SetName(
                    &selectionOutlineMaskMsaa_,
                    "renegade.selectionOutlineMaskMsaa");
            }
            description.sample_count = 1;
            description.bind_flags =
                wi::graphics::BindFlag::RENDER_TARGET |
                wi::graphics::BindFlag::SHADER_RESOURCE;
        }

        if (device->CreateTexture(
                &description,
                nullptr,
                &selectionOutlineMask_))
        {
            device->SetName(
                &selectionOutlineMask_,
                "renegade.selectionOutlineMask");
        }
    }

    void StudioRenderPath::Render() const
    {
        RenderPath3D::Render();

        const auto* depthStencil = GetDepthStencil();
        if (projectHubVisible_ ||
            outlinedEntity_ == wi::ecs::INVALID_ENTITY ||
            depthStencil == nullptr ||
            !selectionOutlineMask_.IsValid())
        {
            return;
        }

        auto* device = wi::graphics::GetDevice();
        const auto commandList = device->BeginCommandList();
        device->EventBegin("Renegade Selection Outline Mask", commandList);

        if (selectionOutlineMaskMsaa_.IsValid())
        {
            const wi::graphics::RenderPassImage renderPass[] = {
                wi::graphics::RenderPassImage::RenderTarget(
                    &selectionOutlineMaskMsaa_,
                    wi::graphics::RenderPassImage::LoadOp::CLEAR,
                    wi::graphics::RenderPassImage::StoreOp::DONTCARE),
                wi::graphics::RenderPassImage::Resolve(
                    &selectionOutlineMask_),
                wi::graphics::RenderPassImage::DepthStencil(
                    depthStencil,
                    wi::graphics::RenderPassImage::LoadOp::LOAD,
                    wi::graphics::RenderPassImage::StoreOp::STORE),
            };
            device->RenderPassBegin(
                renderPass,
                arraysize(renderPass),
                commandList);
        }
        else
        {
            const wi::graphics::RenderPassImage renderPass[] = {
                wi::graphics::RenderPassImage::RenderTarget(
                    &selectionOutlineMask_,
                    wi::graphics::RenderPassImage::LoadOp::CLEAR),
                wi::graphics::RenderPassImage::DepthStencil(
                    depthStencil,
                    wi::graphics::RenderPassImage::LoadOp::LOAD,
                    wi::graphics::RenderPassImage::StoreOp::STORE),
            };
            device->RenderPassBegin(
                renderPass,
                arraysize(renderPass),
                commandList);
        }

        wi::graphics::Viewport viewport;
        viewport.width =
            static_cast<float>(selectionOutlineMask_.GetDesc().width);
        viewport.height =
            static_cast<float>(selectionOutlineMask_.GetDesc().height);
        device->BindViewports(1, &viewport, commandList);

        wi::image::Params mask;
        mask.enableFullScreen();
        mask.stencilComp = wi::image::STENCILMODE::STENCILMODE_EQUAL;
        mask.stencilRefMode = wi::image::STENCILREFMODE_USER;
        mask.stencilRef = SelectionStencilReference;
        wi::image::Draw(nullptr, mask, commandList);

        device->RenderPassEnd(commandList);
        device->EventEnd(commandList);
    }

    void StudioRenderPath::CreateWorkspaceShell()
    {
        toolbarPanel_.Create(
            "Renegade Command Bar",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        toolbarPanel_.SetShadowRadius(8.0f);
        GetGUI().AddWidget(&toolbarPanel_);

        workspaceTitle_.Create("Renegade Workspace Title");
        workspaceTitle_.SetText("RENEGADE STUDIO // PROVING GROUND");
        workspaceTitle_.font.params.size = 19;
        workspaceTitle_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        toolbarPanel_.AddWidget(&workspaceTitle_);

        projectHubButton_.Create("Open Project Hub");
        projectHubButton_.SetText("PROJECTS");
        projectHubButton_.SetTooltip("Return to the Renegade Project Hub");
        projectHubButton_.SetAngularHighlightWidth(4.0f);
        projectHubButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ReturnToProjectHub();
        });
        toolbarPanel_.AddWidget(&projectHubButton_);

        statusLabel_.Create("Renegade Studio Status");
        statusLabel_.SetSize(XMFLOAT2(720.0f, 22.0f));
        statusLabel_.font.params.size = 14;
        statusLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        toolbarPanel_.AddWidget(&statusLabel_);

        hierarchyPanel_.Create(
            "World Outliner",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        hierarchyPanel_.SetShadowRadius(8.0f);
        GetGUI().AddWidget(&hierarchyPanel_);

        hierarchyLabel_.Create("Scene Hierarchy");
        hierarchyLabel_.SetText("WORLD // HIERARCHY");
        hierarchyLabel_.font.params.size = 16;
        hierarchyLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        hierarchyPanel_.AddWidget(&hierarchyLabel_);

        hierarchyTree_.Create("Renegade Hierarchy");
        hierarchyTree_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
            {
                return;
            }
            session_->Selection().Select(
                static_cast<wi::ecs::Entity>(args.userdata));
            RefreshInspector();
            RefreshStatus();
        });
        hierarchyPanel_.AddWidget(&hierarchyTree_);

        inspectorPanel_.Create(
            "Inspector",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        inspectorPanel_.SetShadowRadius(8.0f);
        GetGUI().AddWidget(&inspectorPanel_);

        inspectorLabel_.Create("Transform Inspector");
        inspectorLabel_.SetText("TRANSFORM // SELECT AN ENTITY");
        inspectorLabel_.font.params.size = 16;
        inspectorLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        inspectorPanel_.AddWidget(&inspectorLabel_);

        const auto createTranslationInput = [this](
            wi::gui::TextInputField& input,
            const char* name,
            const char* description,
            const int axis)
        {
            input.Create(name);
            input.SetDescription(description);
            input.SetValue(0.0f);
            input.SetSize(XMFLOAT2(90.0f, 28.0f));
            input.OnInputAccepted([this, axis](const wi::gui::EventArgs& args)
            {
                ApplySelectedTranslation(axis, args.fValue);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createTranslationInput(translationX_, "Translation X", "X: ", 0);
        createTranslationInput(translationY_, "Translation Y", "Y: ", 1);
        createTranslationInput(translationZ_, "Translation Z", "Z: ", 2);

        undoButton_.Create("Undo Transform");
        undoButton_.SetText("UNDO");
        undoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        undoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingHistoryAction_ = HistoryAction::Undo;
        });
        inspectorPanel_.AddWidget(&undoButton_);

        redoButton_.Create("Redo Transform");
        redoButton_.SetText("REDO");
        redoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        redoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingHistoryAction_ = HistoryAction::Redo;
        });
        inspectorPanel_.AddWidget(&redoButton_);

        saveAsButton_.Create("Save Scene As");
        saveAsButton_.SetText("SAVE AS...");
        saveAsButton_.SetSize(XMFLOAT2(112.0f, 28.0f));
        saveAsButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SaveSceneAs();
        });
        inspectorPanel_.AddWidget(&saveAsButton_);

        reopenButton_.Create("Reopen Scene");
        reopenButton_.SetText("REOPEN");
        reopenButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        reopenButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ReopenScene();
        });
        inspectorPanel_.AddWidget(&reopenButton_);

        contentPanel_.Create(
            "Content Browser",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        contentPanel_.SetShadowRadius(8.0f);
        GetGUI().AddWidget(&contentPanel_);

        contentLabel_.Create("Content Browser Title");
        contentLabel_.SetText("CONTENT // PROJECT ASSETS");
        contentLabel_.font.params.size = 16;
        contentLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        contentPanel_.AddWidget(&contentLabel_);

        contentPlaceholder_.Create("Content Browser Placeholder");
        contentPlaceholder_.SetText(
            "The project-aware content browser arrives in Phase 4. "
            "This panel now owns its permanent workspace region.");
        contentPlaceholder_.SetFitTextEnabled(true);
        contentPlaceholder_.font.params.color = HologramMuted;
        contentPlaceholder_.font.params.size = 14;
        contentPanel_.AddWidget(&contentPlaceholder_);
    }

    void StudioRenderPath::CreateProjectHub()
    {
        projectHubPanel_.Create(
            "Renegade Project Hub",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        projectHubPanel_.SetShadowRadius(12.0f);
        GetGUI().AddWidget(&projectHubPanel_);

        hubBrandLabel_.Create("Renegade Hub Brand");
        hubBrandLabel_.SetText("RENEGADE // IDENTITY ACCEPTED");
        hubBrandLabel_.font.params.size = 14;
        hubBrandLabel_.font.params.color = HologramMuted;
        hubBrandLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubBrandLabel_);

        hubTitleLabel_.Create("Renegade Project Hub Title");
        hubTitleLabel_.SetText("PROJECT HUB");
        hubTitleLabel_.font.params.size = 34;
        hubTitleLabel_.font.params.bolden = 0.25f;
        hubTitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubTitleLabel_);

        hubSubtitleLabel_.Create("Renegade Project Hub Subtitle");
        hubSubtitleLabel_.SetText(
            "Create a new world, open an existing .renegade project, "
            "or resume a recent operation.");
        hubSubtitleLabel_.font.params.size = 16;
        hubSubtitleLabel_.font.params.color = HologramMuted;
        hubSubtitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubSubtitleLabel_);

        projectNameInput_.Create("New Project Name");
        projectNameInput_.SetDescription("PROJECT NAME: ");
        projectNameInput_.SetText("New Renegade Project");
        projectNameInput_.SetCancelInputEnabled(false);
        projectHubPanel_.AddWidget(&projectNameInput_);

        createProjectButton_.Create("Create Renegade Project");
        createProjectButton_.SetText("CREATE PROJECT");
        createProjectButton_.SetTooltip(
            "Choose a parent folder and create a project from the Proving Ground");
        createProjectButton_.SetAngularHighlightWidth(5.0f);
        createProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            CreateProject();
        });
        projectHubPanel_.AddWidget(&createProjectButton_);

        openProjectButton_.Create("Open Renegade Project");
        openProjectButton_.SetText("OPEN PROJECT...");
        openProjectButton_.SetAngularHighlightWidth(5.0f);
        openProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenProject();
        });
        projectHubPanel_.AddWidget(&openProjectButton_);

        recentProjectsLabel_.Create("Recent Projects");
        recentProjectsLabel_.SetText("RECENT OPERATIONS");
        recentProjectsLabel_.font.params.size = 17;
        recentProjectsLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&recentProjectsLabel_);

        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            auto& button = recentProjectButtons_[index];
            button.Create("Recent Project " + std::to_string(index));
            button.SetText("");
            button.SetAngularHighlightWidth(4.0f);
            button.font.params.h_align = wi::font::WIFALIGN_LEFT;
            button.font.params.v_align = wi::font::WIFALIGN_TOP;
            button.OnClick([this, index](const wi::gui::EventArgs&)
            {
                SelectRecentProject(index);
            });
            projectHubPanel_.AddWidget(&button);
        }

        selectedProjectLabel_.Create("Selected Project");
        selectedProjectLabel_.SetText("SELECT A PROJECT CARD");
        selectedProjectLabel_.SetFitTextEnabled(true);
        selectedProjectLabel_.font.params.size = 17;
        selectedProjectLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&selectedProjectLabel_);

        launchProjectButton_.Create("Launch Selected Project");
        launchProjectButton_.SetText("LAUNCH PROJECT");
        launchProjectButton_.SetAngularHighlightWidth(5.0f);
        launchProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenSelectedRecentProject();
        });
        projectHubPanel_.AddWidget(&launchProjectButton_);

        continueProjectButton_.Create("Continue Current Project");
        continueProjectButton_.SetText("RETURN TO CURRENT PROJECT");
        continueProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SetProjectHubVisible(false);
        });
        projectHubPanel_.AddWidget(&continueProjectButton_);

        hubMessageLabel_.Create("Project Hub Message");
        hubMessageLabel_.SetText("SYSTEM READY // PROJECT SERVICES ONLINE");
        hubMessageLabel_.SetFitTextEnabled(true);
        hubMessageLabel_.font.params.size = 14;
        hubMessageLabel_.font.params.color = HologramMuted;
        hubMessageLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubMessageLabel_);
    }

    void StudioRenderPath::ApplyRenegadeTheme()
    {
        wi::gui::Theme theme;
        theme.image.background = true;
        theme.image.blendFlag = wi::enums::BLENDMODE_ALPHA;
        theme.image.corner_rounding = true;
        for (auto& corner : theme.image.corners_rounding)
        {
            corner.radius = 7.0f;
        }
        theme.font.color = HologramText;
        theme.font.shadow_color = wi::Color(0, 0, 0, 220);
        theme.shadow = 3.0f;
        theme.shadow_color = HologramBorder;
        theme.shadow_highlight = true;
        theme.shadow_highlight_color = XMFLOAT3(0.0f, 0.72f, 1.0f);
        theme.shadow_highlight_spread = 0.35f;
        theme.tooltipImage = theme.image;
        theme.tooltipImage.color = HologramIdle;
        theme.tooltipFont = theme.font;
        theme.tooltip_shadow_color = HologramBorder;

        auto& gui = GetGUI();
        gui.SetTheme(theme);
        gui.SetColor(HologramIdle, wi::gui::IDLE);
        gui.SetColor(HologramFocus, wi::gui::FOCUS);
        gui.SetColor(HologramActive, wi::gui::ACTIVE);
        gui.SetColor(HologramFocus, wi::gui::DEACTIVATING);
        gui.SetColor(HologramPanel, wi::gui::WIDGET_ID_WINDOW_BASE);
        gui.SetColor(
            wi::Color(4, 18, 28, 245),
            wi::gui::WIDGET_ID_TEXTINPUTFIELD_IDLE);
        gui.SetColor(
            HologramFocus,
            wi::gui::WIDGET_ID_TEXTINPUTFIELD_FOCUS);
        gui.SetColor(
            HologramActive,
            wi::gui::WIDGET_ID_TEXTINPUTFIELD_ACTIVE);
        gui.SetColor(
            wi::Color(2, 12, 20, 245),
            wi::gui::WIDGET_ID_SCROLLBAR_BASE_IDLE);
        gui.SetColor(
            HologramFocus,
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_HOVER);
        gui.SetColor(
            HologramActive,
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_GRABBED);

        projectHubPanel_.SetColor(
            wi::Color(2, 9, 16, 232),
            wi::gui::WIDGET_ID_WINDOW_BASE);
    }

    void StudioRenderPath::Update(const float dt)
    {
        RenderPath3D::Update(dt);

        if (session_ == nullptr || projectHubVisible_)
        {
            return;
        }

        // wiGUI invokes OnClick while Button::Update is still active. Apply
        // history only after the complete GUI update has returned.
        if (pendingHistoryAction_ != HistoryAction::None)
        {
            const HistoryAction action = pendingHistoryAction_;
            pendingHistoryAction_ = HistoryAction::None;

            const bool changed = action == HistoryAction::Undo
                ? session_->Commands().Undo()
                : session_->Commands().Redo();
            if (changed)
            {
                RefreshInspector();
                RefreshStatus();
            }
            return;
        }

        if (gizmoEntity_ != session_->Selection().SelectedEntity())
        {
            SyncGizmoSelection();
            SyncSelectionOutline();
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        HandleViewportNavigation(dt, pointer);

        if (GetGUI().HasFocus() && !gizmoDragActive_)
        {
            return;
        }

        if (gizmoEntity_ != wi::ecs::INVALID_ENTITY &&
            !flyCameraActive_)
        {
            gizmo_.Update(*camera, pointer, *this);
        }

        if (HandleViewportSelection(pointer))
        {
            return;
        }

        if (gizmoEntity_ == wi::ecs::INVALID_ENTITY ||
            flyCameraActive_)
        {
            return;
        }

        if (gizmo_.IsDragStarted())
        {
            gizmoDragActive_ = true;
        }

        if (gizmo_.IsDragEnded())
        {
            gizmoDragActive_ = false;
            auto* transform =
                session_->Scenes().GetScene().transforms.GetComponent(gizmoEntity_);
            if (transform == nullptr)
            {
                return;
            }

            const XMFLOAT3 translationAfter = transform->translation_local;
            transform->translation_local = gizmoTranslationBefore_;
            transform->SetDirty();
            transform->UpdateTransform();

            session_->Commands().Execute(
                std::make_unique<bridge::SetTranslationCommand>(
                    session_->Scenes().GetScene(),
                    gizmoEntity_,
                    gizmoTranslationBefore_,
                    translationAfter));
            gizmoTranslationBefore_ = translationAfter;
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::Compose(const wi::graphics::CommandList cmd) const
    {
        RenderPath3D::Compose(cmd);

        auto* device = wi::graphics::GetDevice();
        const wi::graphics::Rect viewportScissor = {
            static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.x)),
            static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.y)),
            static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.z)),
            static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.w)),
        };
        device->BindScissorRects(1, &viewportScissor, cmd);

        if (!projectHubVisible_ &&
            outlinedEntity_ != wi::ecs::INVALID_ENTITY &&
            selectionOutlineMask_.IsValid())
        {
            wi::renderer::BindCommonResources(cmd);
            wi::renderer::Postprocess_Outline(
                selectionOutlineMask_,
                cmd,
                0.1f,
                2.0f,
                XMFLOAT4(0.0f, 0.86f, 1.0f, 0.95f));
        }

        if (!projectHubVisible_ && gizmoEntity_ != wi::ecs::INVALID_ENTITY)
        {
            gizmo_.Draw(*camera, wi::input::GetPointer(), cmd);
        }

        const wi::graphics::Rect fullScissor = {
            0,
            0,
            static_cast<std::int32_t>(GetPhysicalWidth()),
            static_cast<std::int32_t>(GetPhysicalHeight()),
        };
        device->BindScissorRects(1, &fullScissor, cmd);
    }

    void StudioRenderPath::ResizeLayout()
    {
        RenderPath3D::ResizeLayout();

        const float width = GetLogicalWidth();
        const float height = GetLogicalHeight();
        const float toolbarHeight = 54.0f;
        const float leftWidth = std::clamp(width * 0.2f, 250.0f, 310.0f);
        const float rightWidth = std::clamp(width * 0.22f, 290.0f, 350.0f);
        const float bottomHeight = std::clamp(height * 0.22f, 160.0f, 220.0f);

        viewportBounds_ = XMFLOAT4(
            leftWidth + 16.0f,
            toolbarHeight + 16.0f,
            width - rightWidth - 16.0f,
            height - bottomHeight - 16.0f);

        toolbarPanel_.SetPos(XMFLOAT2(8.0f, 8.0f));
        toolbarPanel_.SetSize(XMFLOAT2(width - 16.0f, toolbarHeight));
        workspaceTitle_.SetPos(XMFLOAT2(14.0f, 12.0f));
        workspaceTitle_.SetSize(XMFLOAT2(440.0f, 30.0f));
        projectHubButton_.SetPos(XMFLOAT2(width - 132.0f, 9.0f));
        projectHubButton_.SetSize(XMFLOAT2(108.0f, 30.0f));
        statusLabel_.SetPos(XMFLOAT2(455.0f, 14.0f));
        statusLabel_.SetSize(XMFLOAT2(
            std::max(200.0f, width - 610.0f),
            24.0f));

        hierarchyPanel_.SetPos(XMFLOAT2(8.0f, toolbarHeight + 16.0f));
        hierarchyPanel_.SetSize(XMFLOAT2(
            leftWidth,
            height - toolbarHeight - 24.0f));
        hierarchyLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        hierarchyLabel_.SetSize(XMFLOAT2(leftWidth - 24.0f, 28.0f));
        hierarchyTree_.SetPos(XMFLOAT2(10.0f, 44.0f));
        hierarchyTree_.SetSize(XMFLOAT2(
            leftWidth - 20.0f,
            height - toolbarHeight - 82.0f));

        inspectorPanel_.SetPos(XMFLOAT2(
            width - rightWidth - 8.0f,
            toolbarHeight + 16.0f));
        inspectorPanel_.SetSize(XMFLOAT2(
            rightWidth,
            height - toolbarHeight - 24.0f));
        inspectorLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        inspectorLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 28.0f));
        translationX_.SetPos(XMFLOAT2(12.0f, 50.0f));
        translationY_.SetPos(XMFLOAT2(112.0f, 50.0f));
        translationZ_.SetPos(XMFLOAT2(212.0f, 50.0f));
        undoButton_.SetPos(XMFLOAT2(12.0f, 92.0f));
        redoButton_.SetPos(XMFLOAT2(112.0f, 92.0f));
        saveAsButton_.SetPos(XMFLOAT2(12.0f, 134.0f));
        reopenButton_.SetPos(XMFLOAT2(134.0f, 134.0f));

        contentPanel_.SetPos(XMFLOAT2(
            leftWidth + 16.0f,
            height - bottomHeight - 8.0f));
        contentPanel_.SetSize(XMFLOAT2(
            width - leftWidth - rightWidth - 32.0f,
            bottomHeight));
        contentLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        contentLabel_.SetSize(XMFLOAT2(
            contentPanel_.GetSize().x - 24.0f,
            28.0f));
        contentPlaceholder_.SetPos(XMFLOAT2(12.0f, 50.0f));
        contentPlaceholder_.SetSize(XMFLOAT2(
            contentPanel_.GetSize().x - 24.0f,
            bottomHeight - 62.0f));

        projectHubPanel_.SetPos(XMFLOAT2(12.0f, 12.0f));
        projectHubPanel_.SetSize(XMFLOAT2(width - 24.0f, height - 24.0f));
        hubBrandLabel_.SetPos(XMFLOAT2(38.0f, 24.0f));
        hubBrandLabel_.SetSize(XMFLOAT2(500.0f, 24.0f));
        hubTitleLabel_.SetPos(XMFLOAT2(38.0f, 58.0f));
        hubTitleLabel_.SetSize(XMFLOAT2(500.0f, 48.0f));
        hubSubtitleLabel_.SetPos(XMFLOAT2(40.0f, 112.0f));
        hubSubtitleLabel_.SetSize(XMFLOAT2(width - 100.0f, 30.0f));

        projectNameInput_.SetPos(XMFLOAT2(40.0f, 160.0f));
        projectNameInput_.SetSize(XMFLOAT2(330.0f, 32.0f));
        createProjectButton_.SetPos(XMFLOAT2(388.0f, 160.0f));
        createProjectButton_.SetSize(XMFLOAT2(170.0f, 32.0f));
        openProjectButton_.SetPos(XMFLOAT2(570.0f, 160.0f));
        openProjectButton_.SetSize(XMFLOAT2(170.0f, 32.0f));

        recentProjectsLabel_.SetPos(XMFLOAT2(40.0f, 218.0f));
        recentProjectsLabel_.SetSize(XMFLOAT2(500.0f, 28.0f));

        const float previewX = std::max(820.0f, width * 0.64f);
        const float cardsWidth = std::max(620.0f, previewX - 70.0f);
        const float cardWidth = (cardsWidth - 18.0f) * 0.5f;
        constexpr float cardHeight = 76.0f;
        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            const float x =
                40.0f + static_cast<float>(index % 2) * (cardWidth + 18.0f);
            const float y =
                254.0f + static_cast<float>(index / 2) * (cardHeight + 14.0f);
            recentProjectButtons_[index].SetPos(XMFLOAT2(x, y));
            recentProjectButtons_[index].SetSize(
                XMFLOAT2(cardWidth, cardHeight));
            recentProjectButtons_[index].font.params.h_wrap =
                cardWidth - 20.0f;
        }

        selectedProjectLabel_.SetPos(XMFLOAT2(previewX, 254.0f));
        selectedProjectLabel_.SetSize(XMFLOAT2(
            std::max(260.0f, width - previewX - 50.0f),
            140.0f));
        launchProjectButton_.SetPos(XMFLOAT2(previewX, 410.0f));
        launchProjectButton_.SetSize(XMFLOAT2(240.0f, 42.0f));
        continueProjectButton_.SetPos(XMFLOAT2(previewX, 466.0f));
        continueProjectButton_.SetSize(XMFLOAT2(240.0f, 36.0f));

        hubMessageLabel_.SetPos(XMFLOAT2(40.0f, height - 74.0f));
        hubMessageLabel_.SetSize(XMFLOAT2(width - 90.0f, 38.0f));
    }

    void StudioRenderPath::RefreshStatus()
    {
        if (session_ == nullptr)
        {
            statusLabel_.SetText("ENGINEBRIDGE SESSION UNAVAILABLE");
            return;
        }

        const auto& scenes = session_->Scenes();
        if (!scenes.LastError().empty())
        {
            statusLabel_.SetText("SCENE ERROR // " + scenes.LastError());
            return;
        }

        const std::string projectName = session_->Projects().HasProject()
            ? session_->Projects().CurrentProject().name
            : "PROVING GROUND";
        statusLabel_.SetText(
            projectName + " // " +
            std::to_string(scenes.EntityCount()) +
            " ENTITIES // UNDO " +
            std::to_string(session_->Commands().UndoCount()) +
            " // REDO " +
            std::to_string(session_->Commands().RedoCount()));
    }

    void StudioRenderPath::RefreshHierarchy()
    {
        hierarchyTree_.ClearItems();
        if (session_ == nullptr)
        {
            return;
        }

        const auto selected = session_->Selection().SelectedEntity();
        for (const auto& entity : session_->Scenes().ListEntities())
        {
            wi::gui::TreeList::Item item;
            item.name = entity.name;
            item.level = entity.depth;
            item.userdata = entity.entity;
            item.open = true;
            item.selected = entity.entity == selected;
            hierarchyTree_.AddItem(item);
        }
    }

    void StudioRenderPath::RefreshInspector()
    {
        const bool hasSession = session_ != nullptr;
        const auto entity = hasSession
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        auto* transform = hasSession
            ? session_->Scenes().GetScene().transforms.GetComponent(entity)
            : nullptr;
        SyncSelectionOutline();

        const bool hasTransform = transform != nullptr;
        translationX_.SetEnabled(hasTransform);
        translationY_.SetEnabled(hasTransform);
        translationZ_.SetEnabled(hasTransform);
        undoButton_.SetEnabled(hasSession && session_->Commands().CanUndo());
        redoButton_.SetEnabled(hasSession && session_->Commands().CanRedo());
        reopenButton_.SetEnabled(
            hasSession && !session_->Scenes().CurrentPath().empty());

        if (!hasTransform)
        {
            inspectorLabel_.SetText("TRANSFORM // SELECT AN ENTITY");
            translationX_.SetValue(0.0f);
            translationY_.SetValue(0.0f);
            translationZ_.SetValue(0.0f);
            return;
        }

        inspectorLabel_.SetText("TRANSFORM // ENTITY " + std::to_string(entity));
        translationX_.SetValue(transform->translation_local.x);
        translationY_.SetValue(transform->translation_local.y);
        translationZ_.SetValue(transform->translation_local.z);
        SyncGizmoSelection();
    }

    bool StudioRenderPath::IsPointerOverViewport(
        const XMFLOAT4& pointer) const noexcept
    {
        return pointer.x >= viewportBounds_.x &&
            pointer.x < viewportBounds_.z &&
            pointer.y >= viewportBounds_.y &&
            pointer.y < viewportBounds_.w;
    }

    void StudioRenderPath::HandleViewportNavigation(
        const float dt,
        const XMFLOAT4& pointer)
    {
        const bool pointerOverViewport = IsPointerOverViewport(pointer);
        if (!flyCameraActive_ &&
            pointerOverViewport &&
            !GetGUI().HasFocus() &&
            wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
        {
            flyCameraActive_ = true;
            cameraPointerAnchor_ = pointer;
        }

        if (flyCameraActive_ &&
            !wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT))
        {
            flyCameraActive_ = false;
            wi::input::HidePointer(false);
            return;
        }

        if (pointerOverViewport && !GetGUI().HasFocus())
        {
            if (pointer.z > 0.1f)
            {
                cameraMoveSpeed_ = std::min(
                    100.0f,
                    cameraMoveSpeed_ * 1.25f);
            }
            else if (pointer.z < -0.1f)
            {
                cameraMoveSpeed_ = std::max(
                    0.1f,
                    cameraMoveSpeed_ / 1.25f);
            }
        }

        if (!flyCameraActive_)
        {
            return;
        }

        const auto& mouse = wi::input::GetMouseState();
        constexpr float lookSensitivity = 0.0017f;
        const float yaw = mouse.delta_position.x * lookSensitivity;
        const float pitch = mouse.delta_position.y * lookSensitivity;

        XMVECTOR movement = XMVectorZero();
        const auto key = [](const char value)
        {
            return static_cast<wi::input::BUTTON>(value);
        };
        if (wi::input::Down(key('W')))
        {
            movement += XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }
        if (wi::input::Down(key('S')))
        {
            movement += XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
        }
        if (wi::input::Down(key('A')))
        {
            movement += XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f);
        }
        if (wi::input::Down(key('D')))
        {
            movement += XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }
        if (wi::input::Down(key('Q')))
        {
            movement += XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
        }
        if (wi::input::Down(key('E')))
        {
            movement += XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }

        const float movementLength =
            XMVectorGetX(XMVector3LengthSq(movement));
        if (movementLength > 0.0f)
        {
            movement = XMVector3Normalize(movement);
            const float speedMultiplier =
                wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT)
                ? 4.0f
                : 1.0f;
            movement *=
                cameraMoveSpeed_ *
                speedMultiplier *
                std::min(dt, 0.1f);
            const XMMATRIX cameraRotation = XMMatrixRotationQuaternion(
                XMLoadFloat4(&editorCameraTransform_.rotation_local));
            editorCameraTransform_.Translate(
                XMVector3TransformNormal(movement, cameraRotation));
        }

        if (yaw != 0.0f || pitch != 0.0f)
        {
            editorCameraTransform_.RotateRollPitchYaw(
                XMFLOAT3(pitch, yaw, 0.0f));
        }

        if (movementLength > 0.0f || yaw != 0.0f || pitch != 0.0f)
        {
            editorCameraTransform_.UpdateTransform();
            camera->TransformCamera(editorCameraTransform_);
            camera->UpdateCamera();
        }

        wi::input::SetPointer(cameraPointerAnchor_);
        wi::input::HidePointer(true);
    }

    bool StudioRenderPath::HandleViewportSelection(
        const XMFLOAT4& pointer)
    {
        if (session_ == nullptr ||
            flyCameraActive_ ||
            GetGUI().HasFocus() ||
            !IsPointerOverViewport(pointer) ||
            !wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) ||
            gizmo_.IsInteracting())
        {
            return false;
        }

        const auto pickRay = wi::renderer::GetPickRay(
            static_cast<long>(pointer.x),
            static_cast<long>(pointer.y),
            *this,
            *camera);
        const auto picked = wi::scene::Pick(
            pickRay,
            wi::enums::FILTER_OBJECT_ALL,
            ~0u,
            session_->Scenes().GetScene());
        const auto current = session_->Selection().SelectedEntity();
        if (picked.entity == current)
        {
            return false;
        }

        if (picked.entity == wi::ecs::INVALID_ENTITY)
        {
            session_->Selection().Clear();
        }
        else
        {
            session_->Selection().Select(picked.entity);
        }

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
    }

    void StudioRenderPath::RefreshProjectHub()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto& projects = session_->Projects().RecentProjects();
        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            auto& button = recentProjectButtons_[index];
            const bool hasProject = index < projects.size();
            button.SetVisible(hasProject);
            if (!hasProject)
            {
                continue;
            }

            button.SetText(
                projects[index].name + "\n" +
                projects[index].descriptorPath);
            button.SetColor(
                static_cast<int>(index) == selectedRecentProject_
                    ? HologramSelected
                    : HologramIdle,
                wi::gui::IDLE);
        }

        if (selectedRecentProject_ < 0 ||
            static_cast<std::size_t>(selectedRecentProject_) >= projects.size())
        {
            selectedRecentProject_ = -1;
            selectedProjectLabel_.SetText(
                projects.empty()
                    ? "NO RECENT PROJECTS\n\nCreate a project to initialise "
                      "the Renegade workspace."
                    : "SELECT A PROJECT CARD\n\nThe selected project details "
                      "and launch control appear here.");
        }
        else
        {
            const auto& selected =
                projects[static_cast<std::size_t>(selectedRecentProject_)];
            selectedProjectLabel_.SetText(
                selected.name + "\n\n" +
                selected.descriptorPath +
                "\n\nFORMAT // RENEGADE PROJECT V1");
        }

        launchProjectButton_.SetEnabled(selectedRecentProject_ >= 0);
        continueProjectButton_.SetVisible(session_->Projects().HasProject());
        if (session_->Projects().HasProject())
        {
            continueProjectButton_.SetText(
                "RETURN TO " +
                session_->Projects().CurrentProject().name);
        }
    }

    void StudioRenderPath::ApplySelectedTranslation(
        const int axis,
        const float value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        auto* transform =
            session_->Scenes().GetScene().transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        XMFLOAT3 translation = transform->translation_local;
        if (axis == 0)
        {
            translation.x = value;
        }
        else if (axis == 1)
        {
            translation.y = value;
        }
        else
        {
            translation.z = value;
        }

        session_->Commands().Execute(
            std::make_unique<bridge::SetTranslationCommand>(
                session_->Scenes().GetScene(),
                entity,
                translation));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CreateProject()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const std::string projectName = projectNameInput_.GetText();
        const std::string parentDirectory = wi::helper::FolderDialog(
            "Select the folder that will contain the new Renegade project.");
        if (parentDirectory.empty())
        {
            return;
        }

        wi::eventhandler::Subscribe_Once(
            wi::eventhandler::EVENT_THREAD_SAFE_POINT,
            [this, parentDirectory, projectName](uint64_t)
            {
                if (!session_->Projects().CreateProject(
                        parentDirectory,
                        projectName,
                        "Content/ProvingGround.wiscene"))
                {
                    hubMessageLabel_.font.params.color = WarningAmber;
                    hubMessageLabel_.SetText(
                        "PROJECT CREATE FAILED // " +
                        session_->Projects().LastError());
                    return;
                }

                ClearSelectionOutline();
                if (!session_->LoadScene(session_->Projects().StartupScenePath()))
                {
                    hubMessageLabel_.font.params.color = WarningAmber;
                    hubMessageLabel_.SetText(
                        "PROJECT SCENE FAILED // " +
                        session_->Scenes().LastError());
                    return;
                }

                workspaceTitle_.SetText(
                    "RENEGADE STUDIO // " +
                    session_->Projects().CurrentProject().name);
                hubMessageLabel_.font.params.color = HologramMuted;
                hubMessageLabel_.SetText(
                    "PROJECT CREATED // " +
                    session_->Projects().CurrentProject().descriptorPath);
                selectedRecentProject_ = -1;
                RefreshProjectHub();
                SetProjectHubVisible(false);
            });
    }

    void StudioRenderPath::OpenProject()
    {
        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Renegade Project (.renegade)";
        params.extensions.push_back("renegade");
        wi::helper::FileDialog(
            params,
            [this](const std::string& descriptorPath)
            {
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, descriptorPath](uint64_t)
                    {
                        OpenProjectDescriptor(descriptorPath);
                    });
            });
    }

    void StudioRenderPath::OpenProjectDescriptor(
        const std::string& descriptorPath)
    {
        if (session_ == nullptr)
        {
            return;
        }

        if (!session_->Projects().OpenProject(descriptorPath))
        {
            hubMessageLabel_.font.params.color = WarningAmber;
            hubMessageLabel_.SetText(
                "PROJECT OPEN FAILED // " +
                session_->Projects().LastError());
            return;
        }
        ClearSelectionOutline();
        if (!session_->LoadScene(session_->Projects().StartupScenePath()))
        {
            hubMessageLabel_.font.params.color = WarningAmber;
            hubMessageLabel_.SetText(
                "PROJECT SCENE FAILED // " +
                session_->Scenes().LastError());
            return;
        }

        workspaceTitle_.SetText(
            "RENEGADE STUDIO // " +
            session_->Projects().CurrentProject().name);
        hubMessageLabel_.font.params.color = HologramMuted;
        hubMessageLabel_.SetText(
            "PROJECT ONLINE // " +
            session_->Projects().CurrentProject().descriptorPath);
        selectedRecentProject_ = -1;
        RefreshProjectHub();
        SetProjectHubVisible(false);
    }

    void StudioRenderPath::OpenSelectedRecentProject()
    {
        if (session_ == nullptr || selectedRecentProject_ < 0)
        {
            return;
        }

        const auto& recent = session_->Projects().RecentProjects();
        const auto index = static_cast<std::size_t>(selectedRecentProject_);
        if (index >= recent.size())
        {
            return;
        }

        OpenProjectDescriptor(recent[index].descriptorPath);
    }

    void StudioRenderPath::ReturnToProjectHub()
    {
        if (session_ == nullptr)
        {
            return;
        }

        if (session_->Commands().CanUndo())
        {
            const auto result = wi::helper::messageBoxCustom(
                "This scene has editor history. Save it before opening another "
                "project if you want to keep those changes.\n\n"
                "Return to the Project Hub?",
                "Return to Project Hub",
                "YesNo");
            if (result != wi::helper::MessageBoxResult::Yes)
            {
                return;
            }
        }

        selectedRecentProject_ = -1;
        hubMessageLabel_.font.params.color = HologramMuted;
        hubMessageLabel_.SetText("PROJECT HUB ONLINE // SELECT AN OPERATION");
        RefreshProjectHub();
        SetProjectHubVisible(true);
    }

    void StudioRenderPath::SelectRecentProject(const std::size_t index)
    {
        if (session_ == nullptr ||
            index >= session_->Projects().RecentProjects().size())
        {
            return;
        }

        selectedRecentProject_ = static_cast<int>(index);
        RefreshProjectHub();
    }

    void StudioRenderPath::SetProjectHubVisible(const bool visible)
    {
        if (visible && flyCameraActive_)
        {
            flyCameraActive_ = false;
            wi::input::HidePointer(false);
        }

        projectHubVisible_ = visible;
        projectHubPanel_.SetVisible(visible);
        toolbarPanel_.SetVisible(!visible);
        hierarchyPanel_.SetVisible(!visible);
        inspectorPanel_.SetVisible(!visible);
        contentPanel_.SetVisible(!visible);

        if (!visible)
        {
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::SyncGizmoSelection()
    {
        gizmo_.selected.clear();
        gizmo_.selectedEntitiesNonRecursive.clear();
        gizmoEntity_ = wi::ecs::INVALID_ENTITY;
        gizmoDragActive_ = false;

        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        wi::scene::PickResult selected;
        selected.entity = entity;
        gizmo_.scene = &scene;
        gizmo_.selected.push_back(selected);
        gizmo_.selectedEntitiesNonRecursive.push_back(entity);
        gizmo_.PreTranslate();
        gizmoEntity_ = entity;
        gizmoTranslationBefore_ = transform->translation_local;
    }

    void StudioRenderPath::ClearSelectionOutline() noexcept
    {
        if (session_ != nullptr &&
            outlinedEntity_ != wi::ecs::INVALID_ENTITY)
        {
            auto* object = session_->Scenes()
                .GetScene()
                .objects
                .GetComponent(outlinedEntity_);
            if (object != nullptr)
            {
                object->SetUserStencilRef(
                    outlinedEntityPreviousStencil_);
            }
        }

        outlinedEntity_ = wi::ecs::INVALID_ENTITY;
        outlinedEntityPreviousStencil_ = 0;
    }

    void StudioRenderPath::SyncSelectionOutline()
    {
        const auto selected = session_ != nullptr
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        if (selected == outlinedEntity_)
        {
            return;
        }

        ClearSelectionOutline();
        if (session_ == nullptr ||
            selected == wi::ecs::INVALID_ENTITY)
        {
            return;
        }

        auto* object =
            session_->Scenes().GetScene().objects.GetComponent(selected);
        if (object == nullptr)
        {
            return;
        }

        outlinedEntity_ = selected;
        outlinedEntityPreviousStencil_ = object->userStencilRef;
        object->SetUserStencilRef(SelectionStencilReference);
    }

    void StudioRenderPath::SaveSceneAs()
    {
        if (session_ == nullptr)
        {
            return;
        }

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::SAVE;
        params.description = "Renegade Scene (.wiscene)";
        params.extensions.push_back("wiscene");
        wi::helper::FileDialog(
            params,
            [this](const std::string& selectedPath)
            {
                const std::string scenePath =
                    wi::helper::ForceExtension(selectedPath, "wiscene");
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, scenePath](uint64_t)
                    {
                        ClearSelectionOutline();
                        session_->SaveScene(scenePath);
                        SyncSelectionOutline();
                        RefreshStatus();
                        RefreshInspector();
                    });
            });
    }

    void StudioRenderPath::ReopenScene()
    {
        if (session_ == nullptr)
        {
            return;
        }

        ClearSelectionOutline();
        session_->ReloadScene();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioApplication::SetStartupScene(std::string filePath)
    {
        if (!filePath.empty())
        {
            startupScene_ = std::move(filePath);
        }
    }

    void StudioApplication::PrepareProvingGround()
    {
        if (wi::helper::FileExists(startupScene_) &&
            session_.LoadScene(startupScene_))
        {
            return;
        }

        session_.Scenes().CreateProvingGround();
        session_.SaveScene(startupScene_);
    }

    void StudioApplication::Initialize()
    {
        wi::Application::Initialize();

        infoDisplay.active = true;
        infoDisplay.watermark = false;
        infoDisplay.device_name = true;
        infoDisplay.resolution = true;
        infoDisplay.logical_size = true;
        infoDisplay.colorspace = true;
        infoDisplay.fpsinfo = true;

        session_.Projects().Initialize("Saved/RenegadeStudio.ini");
        PrepareProvingGround();

        renderer_.BindSession(session_);
        renderer_.init(canvas);
        renderer_.Load();
        ActivatePath(&renderer_);
    }
}
