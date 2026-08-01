#include "StudioApplication.h"

#include <algorithm>
#include <cmath>
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
    constexpr int LayoutPreferenceBits = 10;

    int ReadLayoutPreference(
        const renegade::bridge::ProjectService& projects,
        const std::string& key,
        const int fallback)
    {
        int value = 0;
        for (int bit = 0; bit < LayoutPreferenceBits; ++bit)
        {
            if (projects.GetEditorPreference(
                    key + "_bit_" + std::to_string(bit),
                    false))
            {
                value |= 1 << bit;
            }
        }
        return value > 0 ? value : fallback;
    }

    void WriteLayoutPreference(
        renegade::bridge::ProjectService& projects,
        const std::string& key,
        const int value)
    {
        for (int bit = 0; bit < LayoutPreferenceBits; ++bit)
        {
            projects.SetEditorPreference(
                key + "_bit_" + std::to_string(bit),
                (value & (1 << bit)) != 0);
        }
    }
}

namespace renegade::studio
{
    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
    {
        session_ = &session;
        scene = &session.Scenes().GetScene();
    }

    void StudioRenderPath::BindDiagnostics(
        wi::Application::InfoDisplayer& diagnostics) noexcept
    {
        diagnostics_ = &diagnostics;
    }

    void StudioRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);
        setFXAAEnabled(false);
        setBloomEnabled(true);
        setBloomThreshold(1.35f);

        // Ambient occlusion grounds the deck props against the terrain, and
        // volumetric lights are what make the scene's fog react to lighting
        // instead of reading as a flat screen-space overlay.
        setAO(AO::AO_MSAO);
        setAOPower(1.4f);
        setVolumeLightsEnabled(true);

        // Fixed exposure. Eye adaption would make the viewport brightness
        // depend on where the creator points the camera, which is not
        // acceptable for a colour-accurate authoring viewport.
        setEyeAdaptionEnabled(false);
        setExposure(1.0f);

        // Renegade draws its own grid. Wicked's stock helper is a fixed 20x20
        // unit line list whose adaptive path is gated behind gridHelper2D -
        // which rotates the grid into the vertical plane - and whose axis line
        // colours are hardcoded. It stays off permanently.
        wi::renderer::SetToDrawGridHelper(false);
        LoadGridResources();

        // The generated Proving Ground is composed around the world origin so
        // that it shares the grid helper's footprint.
        const XMVECTOR eye = XMVectorSet(13.0f, 7.6f, -16.5f, 1.0f);
        const XMVECTOR at = XMVectorSet(0.0f, 1.8f, 0.0f, 1.0f);
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

        gizmo_.translate_snap = 0.1f;
        gizmo_.rotate_snap = 15.0f / 180.0f * XM_PI;
        gizmo_.scale_snap = 0.1f;

        // Translator sizes itself as distance-to-camera * 0.05 * tool_scale,
        // so tool_scale is a direct screen-space multiplier. The default of
        // 1.0 dominated the viewport. Thinner arms and slightly reduced
        // opacity match the restrained-glow direction; negative axes are
        // darkened hard so the gizmo reads as a projected instrument rather
        // than a solid object.
        gizmo_.tool_scale = 0.60f;
        gizmo_.tool_thickness = 0.70f;
        gizmo_.tool_opacity = 0.85f;
        gizmo_.tool_darken_negative_axes = 0.35f;

        SetTransformTool(TransformTool::Translate);

        // Restore the creator's saved grid preference. This is Renegade's
        // first persisted editor preference; camera speed and layout should
        // follow the same route rather than inventing a second one.
        if (session_ != nullptr)
        {
            gridVisible_ =
                session_->Projects().GetEditorPreference("grid_visible", true);
        }
        gridToggleButton_.SetText(gridVisible_ ? "GRID ON" : "GRID OFF");
        studioChrome_.SetGridVisible(gridVisible_);

        if (session_ != nullptr)
        {
            for (int index = 0; index < 4; ++index)
            {
                if (session_->Projects().GetEditorPreference(
                        "drawer_tab_" + std::to_string(index),
                        index == 0))
                {
                    lastDrawerTab_ = index;
                    break;
                }
            }
            const bool drawerOpen =
                session_->Projects().GetEditorPreference(
                    "drawer_open",
                    false);
            studioChrome_.SetActiveBottomTab(
                drawerOpen ? lastDrawerTab_ : -1);

            auto& projects = session_->Projects();
            if (projects.GetEditorPreference(
                    "workspace_layout_saved",
                    false))
            {
                studioChrome_.SetPanelSizes(
                    static_cast<float>(ReadLayoutPreference(
                        projects,
                        "hierarchy_width",
                        static_cast<int>(studioChrome_.HierarchyWidth()))),
                    static_cast<float>(ReadLayoutPreference(
                        projects,
                        "inspector_width",
                        static_cast<int>(studioChrome_.InspectorWidth()))),
                    static_cast<float>(ReadLayoutPreference(
                        projects,
                        "drawer_height",
                        static_cast<int>(studioChrome_.DrawerHeight()))));
            }
        }

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        RefreshProjectHub();
        SetProjectHubVisible(true);

        RenderPath3D::Load();
    }

    void StudioRenderPath::LoadGridResources()
    {
        auto* device = wi::graphics::GetDevice();
        if (device == nullptr)
        {
            return;
        }

        // Renegade owns these shaders, so they are not in Wicked's shader dump
        // and must be compiled from source shipped beside the executable. This
        // is the same approach Wicked's own Example_ImGui sample uses: point
        // the shader source path at the working directory just long enough to
        // resolve them, then restore it so Wicked's own shaders are unaffected.
        const std::string previousSourcePath =
            wi::renderer::GetShaderSourcePath();
        wi::renderer::SetShaderSourcePath(
            wi::helper::GetCurrentPath() + "/Content/shaders/");

        const bool vertexLoaded = wi::renderer::LoadShader(
            wi::graphics::ShaderStage::VS,
            gridVertexShader_,
            "RenegadeGridVS.cso");
        const bool pixelLoaded = wi::renderer::LoadShader(
            wi::graphics::ShaderStage::PS,
            gridPixelShader_,
            "RenegadeGridPS.cso");

        wi::renderer::SetShaderSourcePath(previousSourcePath);

        if (!vertexLoaded || !pixelLoaded ||
            !gridVertexShader_.IsValid() || !gridPixelShader_.IsValid())
        {
            // A missing grid is a visual downgrade, not a failure worth
            // taking the editor down for. Everything downstream checks
            // gridPipeline_ before drawing.
            wi::backlog::post(
                "Renegade: the editor grid shaders could not be loaded. "
                "The viewport will render without a grid.",
                wi::backlog::LogLevel::Warning);
            return;
        }

        wi::graphics::PipelineStateDesc description;
        description.vs = &gridVertexShader_;
        description.ps = &gridPixelShader_;
        description.rs = wi::renderer::GetRasterizerState(
            wi::enums::RSTYPE_DOUBLESIDED);
        // Depth read with no write. The pixel shader writes SV_Depth from the
        // ground intersection, so the hardware test occludes the grid behind
        // scene geometry without this pass ever sampling the depth buffer.
        description.dss = wi::renderer::GetDepthStencilState(
            wi::enums::DSSTYPE_DEPTHREAD);
        description.bs = wi::renderer::GetBlendState(
            wi::enums::BSTYPE_PREMULTIPLIED);
        description.pt = wi::graphics::PrimitiveTopology::TRIANGLELIST;

        if (!device->CreatePipelineState(&description, &gridPipeline_))
        {
            wi::backlog::post(
                "Renegade: the editor grid pipeline could not be created. "
                "The viewport will render without a grid.",
                wi::backlog::LogLevel::Warning);
        }
    }

    void StudioRenderPath::DrawEditorGrid(
        const wi::graphics::CommandList cmd) const
    {
        if (!gridVisible_ || projectHubVisible_ ||
            !gridPipeline_.IsValid() || camera == nullptr)
        {
            return;
        }

        auto* device = wi::graphics::GetDevice();
        device->EventBegin("Renegade Editor Grid", cmd);

        const XMMATRIX viewProjection = camera->GetViewProjection();

        GridConstants constants = {};
        XMStoreFloat4x4(&constants.viewProjection, viewProjection);
        XMStoreFloat4x4(
            &constants.inverseViewProjection,
            XMMatrixInverse(nullptr, viewProjection));
        // w is the grid plane height. The generated deck's top surface is at
        // exactly y = 0, so a grid drawn at y = 0 is coplanar with it and
        // loses the GREATER depth test. 2 cm is invisible at any working
        // camera distance and is the same trick Wicked's own helper uses.
        constants.cameraPosition = XMFLOAT4(
            camera->Eye.x,
            camera->Eye.y,
            camera->Eye.z,
            0.02f);

        // Ice-blue is the approved interaction colour. Unlike Wicked's helper,
        // every line including the two axes is Renegade's to choose.
        constants.minorColor = XMFLOAT4(0.36f, 0.84f, 1.0f, 0.28f);
        constants.majorColor = XMFLOAT4(0.46f, 0.90f, 1.0f, 0.50f);
        constants.axisColorX = XMFLOAT4(1.00f, 0.42f, 0.06f, 0.70f);
        constants.axisColorZ = XMFLOAT4(0.30f, 0.78f, 1.00f, 0.70f);

        // Fade start/end, base spacing, master opacity. The fade window keeps
        // the horizon from turning into an aliased smear.
        constants.params = XMFLOAT4(60.0f, 320.0f, 1.0f, 1.0f);

        device->BindPipelineState(&gridPipeline_, cmd);
        device->BindDynamicConstantBuffer(constants, 0, cmd);
        device->Draw(3, 0, cmd);

        device->EventEnd(cmd);
    }

    void StudioRenderPath::RenderTransparents(
        const wi::graphics::CommandList cmd) const
    {
        RenderPath3D::RenderTransparents(cmd);

        if (!gridVisible_ || projectHubVisible_ ||
            !gridPipeline_.IsValid() || camera == nullptr)
        {
            return;
        }

        // Wicked ends every render pass before RenderTransparents() returns.
        // Open an explicit pass over the main colour and depth attachments so
        // the grid is valid on both DX12 and Vulkan. Match Wicked's own
        // transparent-pass attachment setup, including an MSAA resolve.
        auto* device = wi::graphics::GetDevice();
        wi::graphics::RenderPassImage attachments[3] = {};
        std::uint32_t attachmentCount = 0;
        attachments[attachmentCount++] =
            wi::graphics::RenderPassImage::RenderTarget(
                &rtMain_render,
                wi::graphics::RenderPassImage::LoadOp::LOAD);
        if (getMSAASampleCount() > 1)
        {
            attachments[attachmentCount++] =
                wi::graphics::RenderPassImage::Resolve(&rtMain);
        }
        attachments[attachmentCount++] =
            wi::graphics::RenderPassImage::DepthStencil(
                &depthBuffer_Main,
                wi::graphics::RenderPassImage::LoadOp::LOAD,
                wi::graphics::RenderPassImage::StoreOp::STORE,
                wi::graphics::ResourceState::DEPTHSTENCIL,
                wi::graphics::ResourceState::DEPTHSTENCIL,
                wi::graphics::ResourceState::DEPTHSTENCIL);

        device->RenderPassBegin(attachments, attachmentCount, cmd);

        wi::graphics::Viewport viewport;
        viewport.width =
            static_cast<float>(depthBuffer_Main.GetDesc().width);
        viewport.height =
            static_cast<float>(depthBuffer_Main.GetDesc().height);
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;
        device->BindViewports(1, &viewport, cmd);

        const wi::graphics::Rect scissor = GetScissorInternalResolution();
        device->BindScissorRects(1, &scissor, cmd);

        DrawEditorGrid(cmd);
        device->RenderPassEnd(cmd);
    }

    void StudioRenderPath::SetGridVisible(const bool visible)
    {
        gridVisible_ = visible;
        gridToggleButton_.SetText(visible ? "GRID ON" : "GRID OFF");
        studioChrome_.SetGridVisible(visible);

        if (session_ != nullptr)
        {
            session_->Projects().SetEditorPreference("grid_visible", visible);
        }
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
        // Size 19 overflowed the 300px slot and clipped mid-word. Fit-text
        // also keeps long project names inside the label rather than running
        // them under the tool buttons.
        workspaceTitle_.font.params.size = 16;
        workspaceTitle_.SetFitTextEnabled(true);
        workspaceTitle_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        toolbarPanel_.AddWidget(&workspaceTitle_);

        const auto createToolButton = [this](
            wi::gui::Button& button,
            const char* name,
            const char* text,
            const char* tooltip,
            const EditorAction action)
        {
            button.Create(name);
            button.SetText(text);
            button.SetTooltip(tooltip);
            button.SetAngularHighlightWidth(4.0f);
            button.OnClick([this, action](const wi::gui::EventArgs&)
            {
                pendingAction_ = action;
            });
            toolbarPanel_.AddWidget(&button);
        };
        createToolButton(
            translateToolButton_,
            "Translate Tool",
            "MOVE [W]",
            "Translate the selected entity",
            EditorAction::TranslateTool);
        createToolButton(
            rotateToolButton_,
            "Rotate Tool",
            "ROTATE [E]",
            "Rotate the selected entity",
            EditorAction::RotateTool);
        createToolButton(
            scaleToolButton_,
            "Scale Tool",
            "SCALE [R]",
            "Scale the selected entity",
            EditorAction::ScaleTool);

        projectHubButton_.Create("Open Project Hub");
        gridToggleButton_.Create("Grid Toggle");
        gridToggleButton_.SetText("GRID ON");
        gridToggleButton_.SetTooltip(
            "Show or hide the editor grid [G]. The grid is never saved into a "
            "scene.");
        gridToggleButton_.OnClick([this](wi::gui::EventArgs)
        {
            pendingAction_ = EditorAction::ToggleGrid;
        });
        toolbarPanel_.AddWidget(&gridToggleButton_);

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
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshInspector();
            RefreshStatus();
        });
        hierarchyPanel_.AddWidget(&hierarchyTree_);

        hierarchySearch_.Create("Hierarchy Search");
        hierarchySearch_.SetDescription("⌕  ");
        hierarchySearch_.SetValue("");
        hierarchySearch_.SetPlaceholder("SEARCH SCENE...");
        hierarchySearch_.SetTooltip("Filter the visible scene hierarchy");
        hierarchySearch_.SetCancelInputEnabled(false);
        hierarchySearch_.OnInput([this](const wi::gui::EventArgs& args)
        {
            studioChrome_.SetHierarchyFilter(args.sValue);
        });
        hierarchySearch_.OnInputAccepted(
            [this](const wi::gui::EventArgs& args)
        {
            studioChrome_.SetHierarchyFilter(args.sValue);
        });
        GetGUI().AddWidget(&hierarchySearch_);

        inspectorPanel_.Create(
            "Inspector",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        inspectorPanel_.SetShadowRadius(0.0f);
        inspectorPanel_.SetColor(wi::Color::Transparent());
        inspectorPanel_.SetColor(
            wi::Color(8, 11, 13, 255),
            wi::gui::WIDGET_ID_WINDOW_BASE);
        GetGUI().AddWidget(&inspectorPanel_);

        inspectorLabel_.Create("Transform Inspector");
        inspectorLabel_.SetText("TRANSFORM // SELECT AN ENTITY");
        inspectorLabel_.font.params.size = 16;
        inspectorLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        inspectorLabel_.SetColor(wi::Color::Transparent());
        inspectorPanel_.AddWidget(&inspectorLabel_);

        const auto createSectionLabel = [this](
            wi::gui::Label& label,
            const char* name,
            const char* text)
        {
            label.Create(name);
            label.SetText(text);
            label.font.params.size = 13;
            label.font.params.color = HologramMuted;
            label.font.params.h_align = wi::font::WIFALIGN_LEFT;
            label.SetColor(wi::Color::Transparent());
            inspectorPanel_.AddWidget(&label);
        };
        createSectionLabel(
            positionLabel_,
            "Position Section",
            "POSITION");
        createSectionLabel(
            rotationLabel_,
            "Rotation Section",
            "ROTATION // DEGREES");
        createSectionLabel(
            scaleLabel_,
            "Scale Section",
            "SCALE");

        const auto createTransformInput = [this](
            wi::gui::TextInputField& input,
            const char* name,
            const char* description,
            const TransformTool tool,
            const int axis)
        {
            input.Create(name);
            input.SetDescription(description);
            input.SetValue(0.0f);
            input.SetSize(XMFLOAT2(90.0f, 28.0f));
            input.OnInputAccepted(
                [this, tool, axis](const wi::gui::EventArgs& args)
            {
                ApplySelectedTransformValue(tool, axis, args.fValue);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createTransformInput(
            translationX_,
            "Translation X",
            "X: ",
            TransformTool::Translate,
            0);
        createTransformInput(
            translationY_,
            "Translation Y",
            "Y: ",
            TransformTool::Translate,
            1);
        createTransformInput(
            translationZ_,
            "Translation Z",
            "Z: ",
            TransformTool::Translate,
            2);
        createTransformInput(
            rotationX_,
            "Rotation X",
            "X: ",
            TransformTool::Rotate,
            0);
        createTransformInput(
            rotationY_,
            "Rotation Y",
            "Y: ",
            TransformTool::Rotate,
            1);
        createTransformInput(
            rotationZ_,
            "Rotation Z",
            "Z: ",
            TransformTool::Rotate,
            2);
        createTransformInput(
            scaleX_,
            "Scale X",
            "X: ",
            TransformTool::Scale,
            0);
        createTransformInput(
            scaleY_,
            "Scale Y",
            "Y: ",
            TransformTool::Scale,
            1);
        createTransformInput(
            scaleZ_,
            "Scale Z",
            "Z: ",
            TransformTool::Scale,
            2);

        createSectionLabel(
            environmentSkyLabel_,
            "Environment Sky Section",
            "SKY // ATMOSPHERE");
        environmentPreset_.Create("Environment Preset");
        environmentPreset_.AddItem("CUSTOM", 0);
        environmentPreset_.AddItem("CLEAR", 1);
        environmentPreset_.AddItem("SCATTERED", 2);
        environmentPreset_.AddItem("OVERCAST", 3);
        environmentPreset_.AddItem("STORM", 4);
        environmentPreset_.SetTooltip(
            "Apply a curated starting point. Every preset is a single "
            "Undo/Redo command.");
        environmentPreset_.OnSelect(
            [this](const wi::gui::EventArgs& args)
        {
            if (args.userdata != 0)
            {
                ApplyWeatherPreset(static_cast<int>(args.userdata));
            }
        });
        inspectorPanel_.AddWidget(&environmentPreset_);

        skyMode_.Create("Sky Mode");
        skyMode_.AddItem(
            "REALISTIC SKY",
            static_cast<std::uint64_t>(
                bridge::WeatherState::SkyMode::Realistic));
        skyMode_.AddItem(
            "REALISTIC + CLOUDS",
            static_cast<std::uint64_t>(
                bridge::WeatherState::SkyMode::RealisticWithClouds));
        skyMode_.AddItem(
            "SKYBOX TEXTURE",
            static_cast<std::uint64_t>(
                bridge::WeatherState::SkyMode::Skybox));
        skyMode_.SetTooltip(
            "Choose Wicked's physical atmosphere, volumetric clouds, or the "
            "weather component's existing skybox texture.");
        skyMode_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedSkyMode(
                static_cast<bridge::WeatherState::SkyMode>(args.userdata));
        });
        inspectorPanel_.AddWidget(&skyMode_);

        const auto createWeatherToggle = [this](
            wi::gui::CheckBox& input,
            const char* name,
            const char* tooltip,
            const WeatherToggle toggle)
        {
            input.Create(name);
            input.SetTooltip(tooltip);
            input.OnClick([this, toggle](const wi::gui::EventArgs& args)
            {
                ApplySelectedWeatherToggle(toggle, args.bValue);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createWeatherToggle(
            aerialPerspective_,
            "Aerial perspective: ",
            "Apply atmospheric scattering to scene geometry.",
            WeatherToggle::AerialPerspective);

        const auto createWeatherSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const WeatherField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(
                minimum,
                maximum,
                0.0f,
                steps,
                name,
                label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginWeatherSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewWeatherSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitWeatherSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createWeatherSlider(
            skyExposure_,
            "Sky Exposure",
            "EXPOSURE",
            "Brightness of the physical sky.",
            WeatherField::SkyExposure,
            0.0f,
            4.0f,
            400.0f);
        createWeatherSlider(
            ambientIntensity_,
            "Ambient Intensity",
            "AMBIENT",
            "Neutral intensity applied while preserving the authored hue.",
            WeatherField::AmbientIntensity,
            0.0f,
            2.0f,
            400.0f);

        createSectionLabel(
            environmentFogLabel_,
            "Environment Fog Section",
            "FOG // HEIGHT LAYER");
        createWeatherSlider(
            fogStart_,
            "Fog Start",
            "START",
            "Distance from the camera before fog begins.",
            WeatherField::FogStart,
            0.0f,
            500.0f,
            500.0f);
        createWeatherSlider(
            fogDensity_,
            "Fog Density",
            "DENSITY",
            "Overall atmospheric fog density.",
            WeatherField::FogDensity,
            0.0f,
            0.1f,
            1000.0f);
        createWeatherToggle(
            heightFog_,
            "Height fog: ",
            "Restrict fog vertically between the authored heights.",
            WeatherToggle::HeightFog);
        createWeatherSlider(
            fogHeightStart_,
            "Fog Height Start",
            "BASE",
            "Lower height of the fog layer.",
            WeatherField::FogHeightStart,
            -100.0f,
            100.0f,
            400.0f);
        createWeatherSlider(
            fogHeightEnd_,
            "Fog Height End",
            "TOP",
            "Upper height of the fog layer.",
            WeatherField::FogHeightEnd,
            -100.0f,
            200.0f,
            600.0f);

        createSectionLabel(
            environmentCloudLabel_,
            "Environment Cloud Section",
            "VOLUMETRIC CLOUDS");
        createWeatherSlider(
            cloudCoverage_,
            "Cloud Coverage",
            "COVERAGE",
            "Primary cloud-layer coverage amount.",
            WeatherField::CloudCoverage,
            0.0f,
            1.0f,
            100.0f);
        createWeatherSlider(
            cloudStartHeight_,
            "Cloud Start Height",
            "BASE",
            "Altitude where the volumetric cloud volume begins.",
            WeatherField::CloudStartHeight,
            100.0f,
            10000.0f,
            990.0f);
        createWeatherSlider(
            cloudThickness_,
            "Cloud Thickness",
            "DEPTH",
            "Vertical depth of the volumetric cloud volume.",
            WeatherField::CloudThickness,
            100.0f,
            10000.0f,
            990.0f);
        createWeatherToggle(
            cloudsCastShadow_,
            "Cloud shadows: ",
            "Allow volumetric clouds to cast moving shadows on the world.",
            WeatherToggle::CloudsCastShadow);

        createSectionLabel(
            precipitationLabel_,
            "Environment Precipitation Section",
            "PRECIPITATION // NATIVE PARTICLES");

        precipitationMode_.Create("Precipitation Mode");
        precipitationMode_.AddItem(
            "OFF",
            static_cast<std::uint64_t>(bridge::PrecipitationMode::None));
        precipitationMode_.AddItem(
            "RAIN",
            static_cast<std::uint64_t>(bridge::PrecipitationMode::Rain));
        precipitationMode_.AddItem(
            "SNOW",
            static_cast<std::uint64_t>(bridge::PrecipitationMode::Snow));
        precipitationMode_.SetTooltip(
            "Rain uses Wicked's native precipitation renderer. Snow uses a "
            "Renegade-authored slow flake profile over the same GPU emitter.");
        precipitationMode_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplyPrecipitationMode(
                static_cast<bridge::PrecipitationMode>(args.userdata));
        });
        inspectorPanel_.AddWidget(&precipitationMode_);

        const auto createPrecipitationSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const PrecipitationField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginPrecipitationSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewPrecipitationSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitPrecipitationSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createPrecipitationSlider(
            precipitationIntensity_,
            "Precipitation Intensity",
            "INTENSITY",
            "Particle density. Zero disables precipitation.",
            PrecipitationField::Intensity,
            0.0f,
            1.0f,
            200.0f);
        createPrecipitationSlider(
            precipitationFallSpeed_,
            "Precipitation Fall Speed",
            "FALL SPEED",
            "Downward particle speed; snow profiles start much slower.",
            PrecipitationField::FallSpeed,
            0.01f,
            2.0f,
            400.0f);
        createPrecipitationSlider(
            precipitationParticleScale_,
            "Precipitation Particle Scale",
            "PARTICLE SIZE",
            "Rendered particle size.",
            PrecipitationField::ParticleScale,
            0.005f,
            0.1f,
            400.0f);
        createPrecipitationSlider(
            precipitationWindAzimuth_,
            "Precipitation Wind Azimuth",
            "WIND DIRECTION",
            "Horizontal wind direction in degrees.",
            PrecipitationField::WindAzimuth,
            -180.0f,
            180.0f,
            360.0f);
        createPrecipitationSlider(
            precipitationWindSpeed_,
            "Precipitation Wind Speed",
            "WIND SPEED",
            "Horizontal wind strength applied to precipitation.",
            PrecipitationField::WindSpeed,
            0.0f,
            20.0f,
            400.0f);
        createPrecipitationSlider(
            precipitationTurbulence_,
            "Precipitation Turbulence",
            "TURBULENCE",
            "Random particle drift; higher values create snow flurries.",
            PrecipitationField::Turbulence,
            0.0f,
            20.0f,
            400.0f);

        createSectionLabel(
            sunLabel_,
            "Environment Sun Section",
            "SUN // TIME OF DAY");
        sunPreset_.Create("Sun Preset");
        sunPreset_.AddItem("CUSTOM", 0);
        sunPreset_.AddItem(
            "DAWN",
            static_cast<std::uint64_t>(bridge::SunPreset::Dawn) + 1u);
        sunPreset_.AddItem(
            "MIDDAY",
            static_cast<std::uint64_t>(bridge::SunPreset::Midday) + 1u);
        sunPreset_.AddItem(
            "GOLDEN HOUR",
            static_cast<std::uint64_t>(bridge::SunPreset::GoldenHour) + 1u);
        sunPreset_.AddItem(
            "DUSK",
            static_cast<std::uint64_t>(bridge::SunPreset::Dusk) + 1u);
        sunPreset_.AddItem(
            "MIDNIGHT",
            static_cast<std::uint64_t>(bridge::SunPreset::Midnight) + 1u);
        sunPreset_.SetTooltip(
            "Move the serialized scene sun to a curated time of day.");
        sunPreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (args.userdata > 0)
            {
                ApplySunPreset(static_cast<bridge::SunPreset>(
                    args.userdata - 1u));
            }
        });
        inspectorPanel_.AddWidget(&sunPreset_);

        const auto createSunSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const SunField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginSunSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewSunSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitSunSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createSunSlider(
            sunTime_,
            "Sun Time",
            "TIME // HOURS",
            "Time from 00:00 to 24:00. The value box accepts direct input.",
            SunField::Time,
            0.0f,
            24.0f,
            288.0f);
        createSunSlider(
            sunAzimuth_,
            "Sun Azimuth",
            "AZIMUTH",
            "Horizontal sun direction in degrees.",
            SunField::Azimuth,
            -180.0f,
            180.0f,
            360.0f);
        createSunSlider(
            sunElevation_,
            "Sun Elevation",
            "ELEVATION",
            "Sun height above or below the horizon in degrees.",
            SunField::Elevation,
            -90.0f,
            90.0f,
            360.0f);

        sunPreviewSpeed_.Create(
            0.001f,
            24.0f,
            0.100f,
            23999.0f,
            "Sun Preview Speed",
            "PREVIEW HOURS / SEC");
        sunPreviewSpeed_.SetTooltip(
            "Editor-only preview speed from 0.001 to 24.000 hours per "
            "second. It is not written to the scene.");
        sunPreviewSpeed_.OnValuePreview([this](const float value)
        {
            sunPreviewSpeedHoursPerSecond_ = value;
        });
        sunPreviewSpeed_.OnValueCommitted([this](const float value)
        {
            sunPreviewSpeedHoursPerSecond_ = value;
        });
        inspectorPanel_.AddWidget(&sunPreviewSpeed_);

        sunPlayButton_.Create("Play Sun Preview");
        sunPlayButton_.SetText("PLAY DAY");
        sunPlayButton_.SetTooltip(
            "Preview the 24-hour path. Pausing commits one Undo step.");
        sunPlayButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::StartSunPreview;
        });
        inspectorPanel_.AddWidget(&sunPlayButton_);

        sunPauseButton_.Create("Pause Sun Preview");
        sunPauseButton_.SetText("PAUSE");
        sunPauseButton_.SetTooltip(
            "Pause the preview and commit its final time as one Undo step.");
        sunPauseButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::PauseSunPreview;
        });
        inspectorPanel_.AddWidget(&sunPauseButton_);

        createSectionLabel(
            oceanLabel_,
            "Environment Ocean Section",
            "OCEAN // NATIVE FFT");
        oceanEnabled_.Create("Ocean enabled: ");
        oceanEnabled_.SetTooltip(
            "Enable Wicked's infinite camera-relative FFT ocean surface.");
        oceanEnabled_.OnClick([this](const wi::gui::EventArgs& args)
        {
            pendingOceanEnabled_ = args.bValue;
            pendingAction_ = EditorAction::SetOceanEnabled;
        });
        inspectorPanel_.AddWidget(&oceanEnabled_);

        oceanPreset_.Create("Ocean Preset");
        oceanPreset_.AddItem("CUSTOM", 0);
        oceanPreset_.AddItem(
            "CALM",
            static_cast<std::uint64_t>(bridge::OceanPreset::Calm) + 1u);
        oceanPreset_.AddItem(
            "COASTAL",
            static_cast<std::uint64_t>(bridge::OceanPreset::Coastal) + 1u);
        oceanPreset_.AddItem(
            "STORM",
            static_cast<std::uint64_t>(bridge::OceanPreset::Storm) + 1u);
        oceanPreset_.AddItem(
            "ALIEN",
            static_cast<std::uint64_t>(bridge::OceanPreset::Alien) + 1u);
        oceanPreset_.SetTooltip(
            "Apply a complete native-ocean starting point as one Undo step.");
        oceanPreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (args.userdata > 0)
            {
                pendingOceanPreset_ = static_cast<bridge::OceanPreset>(
                    args.userdata - 1u);
                pendingAction_ = EditorAction::ApplyOceanPreset;
            }
        });
        inspectorPanel_.AddWidget(&oceanPreset_);

        oceanResolution_.Create("Ocean FFT Resolution");
        oceanResolution_.AddItem("64 // LOW", 64);
        oceanResolution_.AddItem("128", 128);
        oceanResolution_.AddItem("256", 256);
        oceanResolution_.AddItem("512 // DEFAULT", 512);
        oceanResolution_.AddItem("1024 // EXPENSIVE", 1024);
        oceanResolution_.SetTooltip(
            "FFT displacement-map dimension. 1024 can be expensive and "
            "recreates the native simulation resources.");
        oceanResolution_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            pendingOceanResolution_ = static_cast<int>(args.userdata);
            pendingAction_ = EditorAction::SetOceanResolution;
        });
        inspectorPanel_.AddWidget(&oceanResolution_);

        const auto createOceanSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const OceanField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginOceanSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewOceanSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitOceanSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createOceanSlider(oceanWaterHeight_, "Ocean Water Height", "LEVEL",
            "World-space ocean height.", OceanField::WaterHeight,
            -100.0f, 100.0f, 800.0f);
        createOceanSlider(oceanPatchLength_, "Ocean Patch Length", "PATCH SIZE",
            "FFT tiling scale; changing it recreates the simulation.",
            OceanField::PatchLength, 1.0f, 1000.0f, 999.0f);
        createOceanSlider(oceanWaveAmplitude_, "Ocean Wave Amplitude", "WAVE AMPLITUDE",
            "Transverse wave energy; changing it recreates the simulation.",
            OceanField::WaveAmplitude, 0.0f, 1000.0f, 1000.0f);
        createOceanSlider(oceanChoppyScale_, "Ocean Choppy Scale", "CHOPPINESS",
            "Longitudinal wave displacement.", OceanField::ChoppyScale,
            0.0f, 10.0f, 1000.0f);
        createOceanSlider(oceanTimeScale_, "Ocean Time Scale", "SIMULATION SPEED",
            "Speed of FFT wave evolution.", OceanField::TimeScale,
            0.0f, 4.0f, 4000.0f);
        createOceanSlider(oceanWindAzimuth_, "Ocean Wind Azimuth", "WIND DIRECTION",
            "Ocean-specific horizontal wind direction in degrees.",
            OceanField::WindAzimuth, -180.0f, 180.0f, 720.0f);
        createOceanSlider(oceanWindSpeed_, "Ocean Wind Speed", "WIND SPEED",
            "Ocean spectrum wind speed; changing it recreates the simulation.",
            OceanField::WindSpeed, 0.0f, 1200.0f, 1200.0f);
        createOceanSlider(oceanWindDependency_, "Ocean Wind Dependency", "WIND DEPENDENCY",
            "Smaller values strengthen alignment with wind direction.",
            OceanField::WindDependency, 0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanSurfaceDetail_, "Ocean Surface Detail", "SURFACE DETAIL",
            "Geometry detail from 1 to 10; high values cost GPU time.",
            OceanField::SurfaceDetail, 1.0f, 10.0f, 9.0f);
        createOceanSlider(oceanDisplacementTolerance_,
            "Ocean Displacement Tolerance", "EDGE TOLERANCE",
            "Reduces screen-edge glitches from large waves at a detail cost.",
            OceanField::DisplacementTolerance, 1.0f, 10.0f, 900.0f);
        createOceanSlider(oceanWaterRed_, "Ocean Water Red", "WATER RED",
            "Native water surface red channel.", OceanField::WaterRed,
            0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanWaterGreen_, "Ocean Water Green", "WATER GREEN",
            "Native water surface green channel.", OceanField::WaterGreen,
            0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanWaterBlue_, "Ocean Water Blue", "WATER BLUE",
            "Native water surface blue channel.", OceanField::WaterBlue,
            0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanWaterOpacity_, "Ocean Water Opacity", "WATER OPACITY",
            "Native water surface alpha.", OceanField::WaterOpacity,
            0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanExtinctionRed_, "Ocean Extinction Red", "DEPTH RED",
            "Native absorption/extinction red channel.",
            OceanField::ExtinctionRed, 0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanExtinctionGreen_, "Ocean Extinction Green", "DEPTH GREEN",
            "Native absorption/extinction green channel.",
            OceanField::ExtinctionGreen, 0.0f, 1.0f, 1000.0f);
        createOceanSlider(oceanExtinctionBlue_, "Ocean Extinction Blue", "DEPTH BLUE",
            "Native absorption/extinction blue channel.",
            OceanField::ExtinctionBlue, 0.0f, 1.0f, 1000.0f);

        createSectionLabel(
            terrainLabel_,
            "Terrain Section",
            "TERRAIN // GENERATION");
        createTerrainButton_.Create("Create Native Terrain");
        createTerrainButton_.SetText("CREATE TERRAIN");
        createTerrainButton_.SetTooltip(
            "Create and select one native streamed terrain component");
        createTerrainButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::CreateTerrain;
        });
        inspectorPanel_.AddWidget(&createTerrainButton_);

        terrainPreset_.Create("Terrain Preset");
        terrainPreset_.AddItem("PRESET // CUSTOM", 0u);
        terrainPreset_.AddItem("FLAT WORLD",
            static_cast<std::uint64_t>(bridge::TerrainPreset::FlatWorld) + 1u);
        terrainPreset_.AddItem("ISLAND",
            static_cast<std::uint64_t>(bridge::TerrainPreset::Island) + 1u);
        terrainPreset_.AddItem("COASTLINE",
            static_cast<std::uint64_t>(bridge::TerrainPreset::Coastline) + 1u);
        terrainPreset_.AddItem("HIGHLANDS",
            static_cast<std::uint64_t>(bridge::TerrainPreset::Highlands) + 1u);
        terrainPreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (args.userdata == 0u)
            {
                return;
            }
            pendingTerrainPreset_ = static_cast<bridge::TerrainPreset>(
                args.userdata - 1u);
            pendingAction_ = EditorAction::ApplyTerrainPreset;
        });
        inspectorPanel_.AddWidget(&terrainPreset_);

        const auto createTerrainSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const TerrainField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginTerrainSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewTerrainSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitTerrainSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createTerrainSlider(terrainVisibleRadius_, "Terrain Visible Radius",
            "VISIBLE CHUNKS", "Generated chunk radius around the terrain origin.",
            TerrainField::VisibleChunkRadius, 1.0f, 16.0f, 15.0f);
        createTerrainSlider(terrainChunkScale_, "Terrain Chunk Scale",
            "CHUNK SCALE", "World scale of each streamed terrain chunk.",
            TerrainField::ChunkScale, 0.25f, 16.0f, 1575.0f);
        createTerrainSlider(terrainMinimumHeight_, "Terrain Minimum Height",
            "MIN HEIGHT", "Lowest generated terrain elevation.",
            TerrainField::MinimumHeight, -2000.0f, 1999.0f, 3999.0f);
        createTerrainSlider(terrainMaximumHeight_, "Terrain Maximum Height",
            "MAX HEIGHT", "Highest generated terrain elevation.",
            TerrainField::MaximumHeight, -1999.0f, 2000.0f, 3999.0f);
        createTerrainSlider(terrainLowAltitudeBlend_, "Terrain Rock Slope",
            "ROCK ON SLOPES", "Steepness required before rock appears; lower values put rock on gentler slopes.",
            TerrainField::LowAltitudeBlend, 0.0f, 1.0f, 1000.0f);
        createTerrainSlider(terrainBaseBlend_, "Terrain Low Height",
            "LOW-GROUND MATERIAL", "How far the low-ground material reaches up from minimum height.",
            TerrainField::BaseBlend, 0.0f, 1.0f, 1000.0f);
        createTerrainSlider(terrainSlopeBlend_, "Terrain High Height",
            "HIGH-GROUND MATERIAL", "How far the high-ground material reaches down from maximum height.",
            TerrainField::SlopeBlend, 0.0f, 1.0f, 1000.0f);
        createTerrainSlider(terrainLodBias_, "Terrain LOD Bias",
            "LOD BIAS", "Terrain detail bias; zero is the safe default.",
            TerrainField::LodBias, -4.0f, 4.0f, 800.0f);

        createSectionLabel(terrainSculptLabel_, "Terrain Sculpt Section", "SCULPT // VIEWPORT BRUSH");
        terrainSculptMode_.Create("Terrain Sculpt Mode");
        terrainSculptMode_.AddItem("RAISE", static_cast<std::uint64_t>(bridge::TerrainSculptMode::Raise));
        terrainSculptMode_.AddItem("LOWER", static_cast<std::uint64_t>(bridge::TerrainSculptMode::Lower));
        terrainSculptMode_.AddItem("SMOOTH", static_cast<std::uint64_t>(bridge::TerrainSculptMode::Smooth));
        terrainSculptMode_.AddItem("FLATTEN", static_cast<std::uint64_t>(bridge::TerrainSculptMode::Flatten));
        terrainSculptMode_.SetTooltip("Choose how dragging the left mouse button changes terrain.");
        terrainSculptMode_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            terrainSculptModeValue_ = static_cast<bridge::TerrainSculptMode>(args.userdata);
        });
        inspectorPanel_.AddWidget(&terrainSculptMode_);
        const auto createBrushSlider = [this](RenegadeSlider& slider, const char* name,
            const char* label, const char* tooltip, float minimum, float maximum,
            float value, float steps, float* target)
        {
            slider.Create(minimum, maximum, value, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnValuePreview([target](float v) { *target = v; });
            slider.OnValueCommitted([target](float v) { *target = v; });
            inspectorPanel_.AddWidget(&slider);
        };
        createBrushSlider(terrainBrushRadius_, "Terrain Brush Radius", "BRUSH SIZE",
            "Radius of the brush in world units.", 1.0f, 100.0f, 12.0f, 990.0f, &terrainBrushRadiusValue_);
        createBrushSlider(terrainBrushStrength_, "Terrain Brush Strength", "STRENGTH",
            "Height change applied while dragging.", 0.05f, 5.0f, 1.0f, 990.0f, &terrainBrushStrengthValue_);
        createBrushSlider(terrainBrushFalloff_, "Terrain Brush Falloff", "FALLOFF",
            "Zero is soft; one concentrates the effect at the centre.", 0.0f, 1.0f, 0.55f, 1000.0f, &terrainBrushFalloffValue_);

        focusButton_.Create("Focus Selected");
        focusButton_.SetText("FOCUS [F]");
        focusButton_.SetTooltip("Frame the selected entity in the viewport");
        focusButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::FocusSelection;
        });
        inspectorPanel_.AddWidget(&focusButton_);

        duplicateButton_.Create("Duplicate Selected");
        duplicateButton_.SetText("DUPLICATE");
        duplicateButton_.SetTooltip("Duplicate selected entity (Ctrl+D)");
        duplicateButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::DuplicateSelection;
        });
        inspectorPanel_.AddWidget(&duplicateButton_);

        deleteButton_.Create("Delete Selected");
        deleteButton_.SetText("DELETE");
        deleteButton_.SetTooltip("Delete selected entity (Delete)");
        deleteButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::DeleteSelection;
        });
        inspectorPanel_.AddWidget(&deleteButton_);

        undoButton_.Create("Undo Transform");
        undoButton_.SetText("UNDO");
        undoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        undoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::Undo;
        });
        inspectorPanel_.AddWidget(&undoButton_);

        redoButton_.Create("Redo Transform");
        redoButton_.SetText("REDO");
        redoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        redoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::Redo;
        });
        inspectorPanel_.AddWidget(&redoButton_);

        saveButton_.Create("Save Scene");
        saveButton_.SetText("SAVE");
        saveButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        saveButton_.SetTooltip("Save the current scene (Ctrl+S)");
        saveButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::SaveScene;
        });
        inspectorPanel_.AddWidget(&saveButton_);

        saveAsButton_.Create("Save Scene As");
        saveAsButton_.SetText("SAVE AS...");
        saveAsButton_.SetSize(XMFLOAT2(112.0f, 28.0f));
        saveAsButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::SaveSceneAs;
        });
        inspectorPanel_.AddWidget(&saveAsButton_);

        reopenButton_.Create("Reopen Scene");
        reopenButton_.SetText("REOPEN");
        reopenButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        reopenButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            pendingAction_ = EditorAction::ReopenScene;
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
            "No assets imported yet.\n\n"
            "Asset import and the project-aware browser are not built. Until "
            "they are, scenes are authored from the generated Proving Ground "
            "and edited in the viewport.");
        contentPlaceholder_.SetFitTextEnabled(true);
        contentPlaceholder_.font.params.color = HologramMuted;
        contentPlaceholder_.font.params.size = 14;
        contentPanel_.AddWidget(&contentPlaceholder_);

        // The proof slice is a Renegade-owned renderer. It is added after the
        // legacy widgets so it sits behind future interactive components in
        // wiGUI's back-to-front render order. The legacy workspace panels are
        // hidden by SetProjectHubVisible(); they are retained temporarily as
        // a behavioural reference, not used as the finished presentation.
        studioChrome_.Create();
        studioChrome_.OnHierarchySelected(
            [this](const std::uint64_t entity)
        {
            if (session_ == nullptr)
            {
                return;
            }
            session_->Selection().Select(
                static_cast<wi::ecs::Entity>(entity));
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        });
        studioChrome_.OnToolSelected([this](const int tool)
        {
            pendingAction_ = tool == 0
                ? EditorAction::SelectTool
                : tool == 1
                    ? EditorAction::TranslateTool
                    : tool == 2
                        ? EditorAction::RotateTool
                        : EditorAction::ScaleTool;
        });
        studioChrome_.OnAction(
            [this](const RenegadeStudioChrome::Action action)
        {
            switch (action)
            {
            case RenegadeStudioChrome::Action::ProjectHub:
                pendingAction_ = EditorAction::ProjectHub;
                break;
            case RenegadeStudioChrome::Action::Save:
                pendingAction_ = EditorAction::SaveScene;
                break;
            case RenegadeStudioChrome::Action::SaveAs:
                pendingAction_ = EditorAction::SaveSceneAs;
                break;
            case RenegadeStudioChrome::Action::Reopen:
                pendingAction_ = EditorAction::ReopenScene;
                break;
            case RenegadeStudioChrome::Action::Undo:
                pendingAction_ = EditorAction::Undo;
                break;
            case RenegadeStudioChrome::Action::Redo:
                pendingAction_ = EditorAction::Redo;
                break;
            case RenegadeStudioChrome::Action::Duplicate:
                pendingAction_ = EditorAction::DuplicateSelection;
                break;
            case RenegadeStudioChrome::Action::Delete:
                pendingAction_ = EditorAction::DeleteSelection;
                break;
            case RenegadeStudioChrome::Action::Focus:
                pendingAction_ = EditorAction::FocusSelection;
                break;
            case RenegadeStudioChrome::Action::ToggleGrid:
                pendingAction_ = EditorAction::ToggleGrid;
                break;
            case RenegadeStudioChrome::Action::EnvironmentWorkspace:
                pendingAction_ = EditorAction::OpenEnvironmentWorkspace;
                break;
            case RenegadeStudioChrome::Action::TerrainWorkspace:
                pendingAction_ = EditorAction::OpenTerrainWorkspace;
                break;
            case RenegadeStudioChrome::Action::SceneWorkspace:
                pendingAction_ = EditorAction::OpenSceneWorkspace;
                break;
            }
        });
        studioChrome_.OnDrawerChanged([this](const int tab)
        {
            if (tab >= 0)
            {
                lastDrawerTab_ = tab;
            }
            if (session_ == nullptr)
            {
                return;
            }
            auto& projects = session_->Projects();
            projects.SetEditorPreference("drawer_open", tab >= 0);
            for (int index = 0; index < 4; ++index)
            {
                projects.SetEditorPreference(
                    "drawer_tab_" + std::to_string(index),
                    lastDrawerTab_ == index);
            }
        });
        studioChrome_.OnLayoutChanged(
            [this](
                const float hierarchyWidth,
                const float inspectorWidth,
                const float drawerHeight,
                const bool finished)
        {
            workspaceLayoutDirty_ = true;
            if (!finished || session_ == nullptr)
            {
                return;
            }

            // ProjectService currently exposes durable boolean preferences.
            // Encode the three bounded pixel dimensions without bypassing the
            // service or leaking editor layout into project/scene data.
            auto& projects = session_->Projects();
            WriteLayoutPreference(
                projects,
                "hierarchy_width",
                static_cast<int>(std::round(hierarchyWidth)));
            WriteLayoutPreference(
                projects,
                "inspector_width",
                static_cast<int>(std::round(inspectorWidth)));
            WriteLayoutPreference(
                projects,
                "drawer_height",
                static_cast<int>(std::round(drawerHeight)));
            projects.SetEditorPreference("workspace_layout_saved", true);
        });
        GetGUI().AddWidget(&studioChrome_);
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

        // The global Project Hub theme is intentionally not the workspace
        // theme. Reassert the owned Inspector host after the global pass so
        // Wicked cannot repaint its rounded cyan window or section pills.
        inspectorPanel_.SetColor(wi::Color::Transparent());
        inspectorPanel_.SetColor(
            wi::Color(8, 11, 13, 255),
            wi::gui::WIDGET_ID_WINDOW_BASE);
        inspectorPanel_.SetShadowRadius(0.0f);

        const auto ownLabel = [](wi::gui::Label& label)
        {
            label.SetColor(wi::Color::Transparent());
            label.SetShadowRadius(0.0f);
            label.font.params.color = wi::Color(244, 239, 233, 255);
            label.font.params.bolden = 0.18f;
            label.font.params.shadowColor = wi::Color::Transparent();
        };
        ownLabel(inspectorLabel_);
        ownLabel(positionLabel_);
        ownLabel(rotationLabel_);
        ownLabel(scaleLabel_);
        ownLabel(environmentSkyLabel_);
        ownLabel(environmentFogLabel_);
        ownLabel(environmentCloudLabel_);
        ownLabel(precipitationLabel_);
        ownLabel(sunLabel_);
        ownLabel(oceanLabel_);

        wi::gui::Theme scrollbarTheme = theme;
        scrollbarTheme.image.corner_rounding = false;
        for (auto& corner : scrollbarTheme.image.corners_rounding)
        {
            corner.radius = 0.0f;
        }
        scrollbarTheme.shadow = 0.0f;
        inspectorPanel_.scrollbar_vertical.SetTheme(scrollbarTheme);
        inspectorPanel_.scrollbar_vertical.SetColor(
            wi::Color(12, 18, 22, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_BASE_IDLE);
        inspectorPanel_.scrollbar_vertical.SetColor(
            wi::Color(38, 52, 61, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_INACTIVE);
        inspectorPanel_.scrollbar_vertical.SetColor(
            wi::Color(210, 91, 29, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_HOVER);
        inspectorPanel_.scrollbar_vertical.SetColor(
            wi::Color(210, 91, 29, 255),
            wi::gui::WIDGET_ID_SCROLLBAR_KNOB_GRABBED);
    }

    void StudioRenderPath::Update(const float dt)
    {
        RenderPath3D::Update(dt);

        if (session_ == nullptr || projectHubVisible_)
        {
            return;
        }

        if (workspaceLayoutDirty_)
        {
            workspaceLayoutDirty_ = false;
            ResizeLayout();
        }

        viewportBounds_ = studioChrome_.ViewportBounds();

        if (sunPreviewPlaying_)
        {
            bridge::SetSunTime(
                sunPreviewCurrent_,
                sunPreviewCurrent_.timeHours +
                    dt * sunPreviewSpeedHoursPerSecond_);
            bridge::ApplySun(
                session_->Scenes().GetScene(),
                EditableWeatherEntity(),
                sunPreviewCurrent_);
            sunTime_.SetValue(sunPreviewCurrent_.timeHours);
            sunAzimuth_.SetValue(sunPreviewCurrent_.azimuthDegrees);
            sunElevation_.SetValue(sunPreviewCurrent_.elevationDegrees);
        }

        HandleEditorShortcuts();

        // wiGUI invokes OnClick while Button::Update is still active. Apply
        // editor actions only after the complete GUI update has returned.
        if (pendingAction_ != EditorAction::None)
        {
            ProcessPendingAction();
            return;
        }

        // The Renegade-owned shell uses deliberate hit regions rather than
        // stock Wicked widgets. Never let a chrome click fall through into
        // scene selection, gizmo manipulation, or camera navigation.
        if (studioChrome_.ConsumedPointerThisFrame())
        {
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

        if (HandleTerrainSculpt(pointer))
        {
            return;
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

            const auto transformAfter = bridge::CaptureTransform(*transform);
            transform->translation_local =
                gizmoTransformBefore_.translation;
            transform->rotation_local =
                gizmoTransformBefore_.rotation;
            transform->scale_local =
                gizmoTransformBefore_.scale;
            transform->SetDirty();
            transform->UpdateTransform();

            session_->Commands().Execute(
                std::make_unique<bridge::SetTransformCommand>(
                    session_->Scenes().GetScene(),
                    gizmoEntity_,
                    gizmoTransformBefore_,
                    transformAfter));
            gizmoTransformBefore_ = transformAfter;
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
            // Thickness was 2.0, double Wicked's default, which read as a
            // heavy halo rather than a projected edge. 1.0 is one pixel.
            wi::renderer::Postprocess_Outline(
                selectionOutlineMask_,
                cmd,
                0.1f,
                1.0f,
                XMFLOAT4(0.30f, 0.86f, 1.0f, 0.90f));
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
        studioChrome_.SetLayout(width, height);
        const float toolbarHeight = 54.0f;
        const float leftWidth = projectHubVisible_
            ? std::clamp(width * 0.2f, 250.0f, 310.0f)
            : studioChrome_.HierarchyWidth();
        const float rightWidth = projectHubVisible_
            ? std::clamp(width * 0.22f, 290.0f, 350.0f)
            : studioChrome_.InspectorWidth();
        const float bottomHeight = projectHubVisible_
            ? std::clamp(height * 0.22f, 160.0f, 220.0f)
            : studioChrome_.DrawerHeight();

        viewportBounds_ = projectHubVisible_
            ? XMFLOAT4(
                leftWidth + 16.0f,
                toolbarHeight + 16.0f,
                width - rightWidth - 16.0f,
                height - bottomHeight - 16.0f)
            : studioChrome_.ViewportBounds();

        toolbarPanel_.SetPos(XMFLOAT2(8.0f, 8.0f));
        toolbarPanel_.SetSize(XMFLOAT2(width - 16.0f, toolbarHeight));
        workspaceTitle_.SetPos(XMFLOAT2(14.0f, 12.0f));
        workspaceTitle_.SetSize(XMFLOAT2(300.0f, 30.0f));
        translateToolButton_.SetPos(XMFLOAT2(314.0f, 9.0f));
        translateToolButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        rotateToolButton_.SetPos(XMFLOAT2(414.0f, 9.0f));
        rotateToolButton_.SetSize(XMFLOAT2(100.0f, 30.0f));
        scaleToolButton_.SetPos(XMFLOAT2(522.0f, 9.0f));
        scaleToolButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        gridToggleButton_.SetPos(XMFLOAT2(630.0f, 9.0f));
        gridToggleButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        projectHubButton_.SetPos(XMFLOAT2(width - 132.0f, 9.0f));
        projectHubButton_.SetSize(XMFLOAT2(108.0f, 30.0f));
        statusLabel_.SetPos(XMFLOAT2(736.0f, 14.0f));
        statusLabel_.SetSize(XMFLOAT2(
            std::max(120.0f, width - 890.0f),
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
        hierarchySearch_.SetPos(XMFLOAT2(12.0f, 117.0f));
        hierarchySearch_.SetSize(XMFLOAT2(leftWidth - 24.0f, 31.0f));

        inspectorPanel_.SetPos(XMFLOAT2(
            projectHubVisible_ ? width - rightWidth - 8.0f : width - rightWidth,
            projectHubVisible_ ? toolbarHeight + 16.0f : 64.0f));
        inspectorPanel_.SetSize(XMFLOAT2(
            rightWidth,
            projectHubVisible_
                ? height - toolbarHeight - 24.0f
                : height - 64.0f - 28.0f));
        inspectorLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        inspectorLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 28.0f));
        const float fieldGap = 8.0f;
        const float fieldWidth = (rightWidth - 40.0f) / 3.0f;
        const auto positionInputRow = [&](
            wi::gui::TextInputField& x,
            wi::gui::TextInputField& y,
            wi::gui::TextInputField& z,
            const float rowY)
        {
            x.SetPos(XMFLOAT2(12.0f, rowY));
            y.SetPos(XMFLOAT2(12.0f + fieldWidth + fieldGap, rowY));
            z.SetPos(XMFLOAT2(
                12.0f + (fieldWidth + fieldGap) * 2.0f,
                rowY));
            x.SetSize(XMFLOAT2(fieldWidth, 28.0f));
            y.SetSize(XMFLOAT2(fieldWidth, 28.0f));
            z.SetSize(XMFLOAT2(fieldWidth, 28.0f));
        };
        positionLabel_.SetPos(XMFLOAT2(12.0f, 44.0f));
        positionLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 20.0f));
        positionInputRow(
            translationX_,
            translationY_,
            translationZ_,
            64.0f);
        rotationLabel_.SetPos(XMFLOAT2(12.0f, 104.0f));
        rotationLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 20.0f));
        positionInputRow(rotationX_, rotationY_, rotationZ_, 124.0f);
        scaleLabel_.SetPos(XMFLOAT2(12.0f, 164.0f));
        scaleLabel_.SetSize(XMFLOAT2(rightWidth - 24.0f, 20.0f));
        positionInputRow(scaleX_, scaleY_, scaleZ_, 184.0f);

        const float environmentFieldWidth = rightWidth - 24.0f;
        const auto positionEnvironmentWidget =
            [environmentFieldWidth](
                wi::gui::Widget& widget,
                const float rowY,
                const float height = 28.0f)
        {
            widget.SetPos(XMFLOAT2(12.0f, rowY));
            widget.SetSize(XMFLOAT2(environmentFieldWidth, height));
        };
        positionEnvironmentWidget(environmentSkyLabel_, 44.0f, 20.0f);
        positionEnvironmentWidget(environmentPreset_, 64.0f);
        positionEnvironmentWidget(skyMode_, 98.0f);
        positionEnvironmentWidget(aerialPerspective_, 132.0f);
        positionEnvironmentWidget(skyExposure_, 164.0f);
        positionEnvironmentWidget(ambientIntensity_, 198.0f);
        positionEnvironmentWidget(environmentFogLabel_, 232.0f, 20.0f);
        positionEnvironmentWidget(fogStart_, 252.0f);
        positionEnvironmentWidget(fogDensity_, 286.0f);
        positionEnvironmentWidget(heightFog_, 320.0f);
        positionEnvironmentWidget(fogHeightStart_, 352.0f);
        positionEnvironmentWidget(fogHeightEnd_, 386.0f);
        positionEnvironmentWidget(environmentCloudLabel_, 420.0f, 20.0f);
        positionEnvironmentWidget(cloudCoverage_, 440.0f);
        positionEnvironmentWidget(cloudStartHeight_, 474.0f);
        positionEnvironmentWidget(cloudThickness_, 508.0f);
        positionEnvironmentWidget(cloudsCastShadow_, 542.0f);
        positionEnvironmentWidget(precipitationLabel_, 576.0f, 20.0f);
        positionEnvironmentWidget(precipitationMode_, 596.0f);
        positionEnvironmentWidget(precipitationIntensity_, 630.0f);
        positionEnvironmentWidget(precipitationFallSpeed_, 664.0f);
        positionEnvironmentWidget(precipitationParticleScale_, 698.0f);
        positionEnvironmentWidget(precipitationWindAzimuth_, 732.0f);
        positionEnvironmentWidget(precipitationWindSpeed_, 766.0f);
        positionEnvironmentWidget(precipitationTurbulence_, 800.0f);
        positionEnvironmentWidget(sunLabel_, 834.0f, 20.0f);
        positionEnvironmentWidget(sunPreset_, 854.0f);
        positionEnvironmentWidget(sunTime_, 888.0f);
        positionEnvironmentWidget(sunAzimuth_, 922.0f);
        positionEnvironmentWidget(sunElevation_, 956.0f);
        positionEnvironmentWidget(sunPreviewSpeed_, 990.0f);
        sunPlayButton_.SetPos(XMFLOAT2(12.0f, 1024.0f));
        sunPauseButton_.SetPos(XMFLOAT2(
            20.0f + (environmentFieldWidth - 8.0f) * 0.5f,
            1024.0f));
        sunPlayButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        sunPauseButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        positionEnvironmentWidget(oceanLabel_, 1060.0f, 20.0f);
        positionEnvironmentWidget(oceanEnabled_, 1080.0f);
        positionEnvironmentWidget(oceanPreset_, 1114.0f);
        positionEnvironmentWidget(oceanResolution_, 1148.0f);
        positionEnvironmentWidget(oceanWaterHeight_, 1182.0f);
        positionEnvironmentWidget(oceanPatchLength_, 1216.0f);
        positionEnvironmentWidget(oceanWaveAmplitude_, 1250.0f);
        positionEnvironmentWidget(oceanChoppyScale_, 1284.0f);
        positionEnvironmentWidget(oceanTimeScale_, 1318.0f);
        positionEnvironmentWidget(oceanWindAzimuth_, 1352.0f);
        positionEnvironmentWidget(oceanWindSpeed_, 1386.0f);
        positionEnvironmentWidget(oceanWindDependency_, 1420.0f);
        positionEnvironmentWidget(oceanSurfaceDetail_, 1454.0f);
        positionEnvironmentWidget(oceanDisplacementTolerance_, 1488.0f);
        positionEnvironmentWidget(oceanWaterRed_, 1522.0f);
        positionEnvironmentWidget(oceanWaterGreen_, 1556.0f);
        positionEnvironmentWidget(oceanWaterBlue_, 1590.0f);
        positionEnvironmentWidget(oceanWaterOpacity_, 1624.0f);
        positionEnvironmentWidget(oceanExtinctionRed_, 1658.0f);
        positionEnvironmentWidget(oceanExtinctionGreen_, 1692.0f);
        positionEnvironmentWidget(oceanExtinctionBlue_, 1726.0f);

        positionEnvironmentWidget(terrainLabel_, 44.0f, 20.0f);
        positionEnvironmentWidget(createTerrainButton_, 64.0f);
        positionEnvironmentWidget(terrainPreset_, 64.0f);
        positionEnvironmentWidget(terrainVisibleRadius_, 98.0f);
        positionEnvironmentWidget(terrainChunkScale_, 132.0f);
        positionEnvironmentWidget(terrainMinimumHeight_, 166.0f);
        positionEnvironmentWidget(terrainMaximumHeight_, 200.0f);
        positionEnvironmentWidget(terrainLowAltitudeBlend_, 234.0f);
        positionEnvironmentWidget(terrainBaseBlend_, 268.0f);
        positionEnvironmentWidget(terrainSlopeBlend_, 302.0f);
        positionEnvironmentWidget(terrainLodBias_, 336.0f);
        positionEnvironmentWidget(terrainSculptLabel_, 380.0f, 20.0f);
        positionEnvironmentWidget(terrainSculptMode_, 400.0f);
        positionEnvironmentWidget(terrainBrushRadius_, 434.0f);
        positionEnvironmentWidget(terrainBrushStrength_, 468.0f);
        positionEnvironmentWidget(terrainBrushFalloff_, 502.0f);

        const bool environmentSelected =
            environmentWorkspaceActive_;
        LayoutInspectorActions(environmentSelected);

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

        if (diagnostics_ != nullptr)
        {
            diagnostics_->rect.left = static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.x + 10.0f));
            diagnostics_->rect.top = static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.y + 10.0f));
            diagnostics_->rect.right = static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.z));
            diagnostics_->rect.bottom = static_cast<std::int32_t>(
                LogicalToPhysical(viewportBounds_.w));
        }
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
        const char* transformTool = gizmo_.isRotator
            ? "ROTATE"
            : gizmo_.isScalator
                ? "SCALE"
                : "MOVE";
        statusLabel_.SetText(
            projectName + " // " +
            std::to_string(scenes.ListEntities().size()) +
            " ITEMS // " +
            transformTool +
            " // UNDO " +
            std::to_string(session_->Commands().UndoCount()) +
            " // REDO " +
            std::to_string(session_->Commands().RedoCount()));
        studioChrome_.SetSceneName(projectName);
        studioChrome_.SetStatusText(statusLabel_.GetText());
    }

    void StudioRenderPath::RefreshHierarchy()
    {
        hierarchyTree_.ClearItems();
        std::vector<RenegadeStudioChrome::HierarchyRow> chromeRows;
        if (session_ == nullptr)
        {
            studioChrome_.SetHierarchyRows({});
            return;
        }

        const auto selected = session_->Selection().SelectedEntity();
        const auto weatherEntity = session_->Scenes().WeatherEntity();
        for (const auto& entity : session_->Scenes().ListEntities())
        {
            if (entity.entity == weatherEntity)
            {
                continue;
            }
            wi::gui::TreeList::Item item;
            item.name = entity.name;
            item.level = entity.depth;
            item.userdata = entity.entity;
            item.open = true;
            item.selected = entity.entity == selected;
            hierarchyTree_.AddItem(item);
            chromeRows.push_back({
                entity.name,
                entity.depth,
                entity.entity == selected,
                static_cast<std::uint64_t>(entity.entity),
            });
        }
        studioChrome_.SetHierarchyRows(std::move(chromeRows));
    }

    void StudioRenderPath::LayoutInspectorActions(
        const bool environment,
        const bool terrain)
    {
        const float width = inspectorPanel_.GetSize().x;
        constexpr float gap = 8.0f;
        const float threeButtonWidth = (width - 40.0f) / 3.0f;
        const float twoButtonWidth = (width - 32.0f) / 2.0f;
        const float actionStart = environment
            ? std::max(1772.0f, inspectorPanel_.GetSize().y - 82.0f)
            : terrain
                ? 376.0f
                : 230.0f;
        const float historyRow = environment
            ? actionStart
            : actionStart + 40.0f;
        const float saveRow = historyRow + 40.0f;

        focusButton_.SetPos(XMFLOAT2(12.0f, actionStart));
        duplicateButton_.SetPos(XMFLOAT2(
            12.0f + threeButtonWidth + gap,
            actionStart));
        deleteButton_.SetPos(XMFLOAT2(
            12.0f + (threeButtonWidth + gap) * 2.0f,
            actionStart));
        focusButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
        duplicateButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
        deleteButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));

        undoButton_.SetPos(XMFLOAT2(12.0f, historyRow));
        redoButton_.SetPos(XMFLOAT2(
            20.0f + twoButtonWidth,
            historyRow));
        undoButton_.SetSize(XMFLOAT2(twoButtonWidth, 28.0f));
        redoButton_.SetSize(XMFLOAT2(twoButtonWidth, 28.0f));

        saveButton_.SetPos(XMFLOAT2(12.0f, saveRow));
        saveAsButton_.SetPos(XMFLOAT2(
            12.0f + threeButtonWidth + gap,
            saveRow));
        reopenButton_.SetPos(XMFLOAT2(
            12.0f + (threeButtonWidth + gap) * 2.0f,
            saveRow));
        saveButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
        saveAsButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
        reopenButton_.SetSize(XMFLOAT2(threeButtonWidth, 28.0f));
    }

    void StudioRenderPath::RefreshInspector()
    {
        const bool hasSession = session_ != nullptr;
        const auto selectedEntity = hasSession
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        const auto terrainWorkspaceEntity =
            hasSession && terrainWorkspaceActive_ &&
            session_->Scenes().GetScene().terrains.GetCount() > 0
            ? session_->Scenes().GetScene().terrains.GetEntity(0)
            : wi::ecs::INVALID_ENTITY;
        const auto entity = environmentWorkspaceActive_
            ? EditableWeatherEntity()
            : terrainWorkspaceActive_
                ? terrainWorkspaceEntity
                : selectedEntity;
        auto* transform = hasSession
            ? session_->Scenes().GetScene().transforms.GetComponent(
                environmentWorkspaceActive_ || terrainWorkspaceActive_
                    ? wi::ecs::INVALID_ENTITY
                    : selectedEntity)
            : nullptr;
        auto* weather = hasSession
            ? session_->Scenes().GetScene().weathers.GetComponent(entity)
            : nullptr;
        auto* terrain = hasSession && !environmentWorkspaceActive_
            ? session_->Scenes().GetScene().terrains.GetComponent(entity)
            : nullptr;
        SyncSelectionOutline();

        const bool hasTransform = transform != nullptr;
        const bool hasWeather = weather != nullptr;
        const bool hasTerrain = terrain != nullptr;
        if (hasSession && selectedEntity != wi::ecs::INVALID_ENTITY &&
            !environmentWorkspaceActive_ && !terrainWorkspaceActive_)
        {
            const auto* selectedName =
                session_->Scenes().GetScene().names.GetComponent(
                    selectedEntity);
            studioChrome_.SetSelectionName(
                selectedName != nullptr ? selectedName->name : std::string{});
        }
        else
        {
            studioChrome_.SetSelectionName({});
        }
        LayoutInspectorActions(hasWeather, hasTerrain);
        const auto setTransformVisible = [this, hasWeather, hasTerrain](wi::gui::Widget& widget)
        {
            widget.SetVisible(!environmentWorkspaceActive_ && !terrainWorkspaceActive_ && !hasWeather && !hasTerrain);
        };
        setTransformVisible(positionLabel_);
        setTransformVisible(rotationLabel_);
        setTransformVisible(scaleLabel_);
        setTransformVisible(translationX_);
        setTransformVisible(translationY_);
        setTransformVisible(translationZ_);
        setTransformVisible(rotationX_);
        setTransformVisible(rotationY_);
        setTransformVisible(rotationZ_);
        setTransformVisible(scaleX_);
        setTransformVisible(scaleY_);
        setTransformVisible(scaleZ_);

        const auto setEnvironmentVisible =
            [hasWeather](wi::gui::Widget& widget)
        {
            widget.SetVisible(hasWeather);
        };
        setEnvironmentVisible(environmentSkyLabel_);
        setEnvironmentVisible(environmentPreset_);
        setEnvironmentVisible(skyMode_);
        setEnvironmentVisible(aerialPerspective_);
        setEnvironmentVisible(skyExposure_);
        setEnvironmentVisible(ambientIntensity_);
        setEnvironmentVisible(environmentFogLabel_);
        setEnvironmentVisible(fogStart_);
        setEnvironmentVisible(fogDensity_);
        setEnvironmentVisible(heightFog_);
        setEnvironmentVisible(fogHeightStart_);
        setEnvironmentVisible(fogHeightEnd_);
        setEnvironmentVisible(environmentCloudLabel_);
        setEnvironmentVisible(cloudCoverage_);
        setEnvironmentVisible(cloudStartHeight_);
        setEnvironmentVisible(cloudThickness_);
        setEnvironmentVisible(cloudsCastShadow_);
        setEnvironmentVisible(precipitationLabel_);
        setEnvironmentVisible(precipitationMode_);
        setEnvironmentVisible(precipitationIntensity_);
        setEnvironmentVisible(precipitationFallSpeed_);
        setEnvironmentVisible(precipitationParticleScale_);
        setEnvironmentVisible(precipitationWindAzimuth_);
        setEnvironmentVisible(precipitationWindSpeed_);
        setEnvironmentVisible(precipitationTurbulence_);
        setEnvironmentVisible(sunLabel_);
        setEnvironmentVisible(sunPreset_);
        setEnvironmentVisible(sunTime_);
        setEnvironmentVisible(sunAzimuth_);
        setEnvironmentVisible(sunElevation_);
        setEnvironmentVisible(sunPreviewSpeed_);
        setEnvironmentVisible(sunPlayButton_);
        setEnvironmentVisible(sunPauseButton_);
        setEnvironmentVisible(oceanLabel_);
        setEnvironmentVisible(oceanEnabled_);
        setEnvironmentVisible(oceanPreset_);
        setEnvironmentVisible(oceanResolution_);
        setEnvironmentVisible(oceanWaterHeight_);
        setEnvironmentVisible(oceanPatchLength_);
        setEnvironmentVisible(oceanWaveAmplitude_);
        setEnvironmentVisible(oceanChoppyScale_);
        setEnvironmentVisible(oceanTimeScale_);
        setEnvironmentVisible(oceanWindAzimuth_);
        setEnvironmentVisible(oceanWindSpeed_);
        setEnvironmentVisible(oceanWindDependency_);
        setEnvironmentVisible(oceanSurfaceDetail_);
        setEnvironmentVisible(oceanDisplacementTolerance_);
        setEnvironmentVisible(oceanWaterRed_);
        setEnvironmentVisible(oceanWaterGreen_);
        setEnvironmentVisible(oceanWaterBlue_);
        setEnvironmentVisible(oceanWaterOpacity_);
        setEnvironmentVisible(oceanExtinctionRed_);
        setEnvironmentVisible(oceanExtinctionGreen_);
        setEnvironmentVisible(oceanExtinctionBlue_);

        const auto setTerrainVisible = [this, hasTerrain](wi::gui::Widget& widget)
        {
            widget.SetVisible(terrainWorkspaceActive_ && hasTerrain);
        };
        terrainLabel_.SetVisible(
            terrainWorkspaceActive_);
        createTerrainButton_.SetVisible(
            hasSession && terrainWorkspaceActive_ && !hasTerrain);
        setTerrainVisible(terrainPreset_);
        setTerrainVisible(terrainVisibleRadius_);
        setTerrainVisible(terrainChunkScale_);
        setTerrainVisible(terrainMinimumHeight_);
        setTerrainVisible(terrainMaximumHeight_);
        setTerrainVisible(terrainLowAltitudeBlend_);
        setTerrainVisible(terrainBaseBlend_);
        setTerrainVisible(terrainSlopeBlend_);
        setTerrainVisible(terrainLodBias_);
        setTerrainVisible(terrainSculptLabel_);
        setTerrainVisible(terrainSculptMode_);
        setTerrainVisible(terrainBrushRadius_);
        setTerrainVisible(terrainBrushStrength_);
        setTerrainVisible(terrainBrushFalloff_);

        translationX_.SetEnabled(hasTransform);
        translationY_.SetEnabled(hasTransform);
        translationZ_.SetEnabled(hasTransform);
        rotationX_.SetEnabled(hasTransform);
        rotationY_.SetEnabled(hasTransform);
        rotationZ_.SetEnabled(hasTransform);
        scaleX_.SetEnabled(hasTransform);
        scaleY_.SetEnabled(hasTransform);
        scaleZ_.SetEnabled(hasTransform);
        focusButton_.SetEnabled(hasTransform);
        duplicateButton_.SetEnabled(hasTransform);
        deleteButton_.SetEnabled(hasTransform);
        focusButton_.SetVisible(!hasWeather);
        duplicateButton_.SetVisible(!hasWeather);
        deleteButton_.SetVisible(!hasWeather);
        duplicateButton_.SetEnabled(hasTransform && !hasTerrain);
        undoButton_.SetEnabled(hasSession && session_->Commands().CanUndo());
        redoButton_.SetEnabled(hasSession && session_->Commands().CanRedo());
        saveButton_.SetEnabled(
            hasSession && !session_->Scenes().CurrentPath().empty());
        saveAsButton_.SetEnabled(hasSession);
        reopenButton_.SetEnabled(
            hasSession && !session_->Scenes().CurrentPath().empty());

        if (hasWeather)
        {
            const auto* name =
                session_->Scenes().GetScene().names.GetComponent(entity);
            inspectorLabel_.SetText(
                environmentWorkspaceActive_
                    ? "ENVIRONMENT // SCENE WEATHER"
                    : "ENVIRONMENT // " +
                        (name != nullptr && !name->name.empty()
                            ? name->name
                            : "ENTITY " + std::to_string(entity)));

            const auto state = bridge::CaptureWeather(*weather);
            environmentPreset_.SetSelectedWithoutCallback(0);
            skyMode_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(state.skyMode));
            aerialPerspective_.SetCheck(state.aerialPerspective);
            skyExposure_.SetValue(state.skyExposure);
            ambientIntensity_.SetValue(state.ambientIntensity);
            fogStart_.SetValue(state.fogStart);
            fogDensity_.SetValue(state.fogDensity);
            heightFog_.SetCheck(state.heightFog);
            fogHeightStart_.SetValue(state.fogHeightStart);
            fogHeightEnd_.SetValue(state.fogHeightEnd);
            cloudCoverage_.SetValue(state.cloudCoverage);
            cloudStartHeight_.SetValue(state.cloudStartHeight);
            cloudThickness_.SetValue(state.cloudThickness);
            cloudsCastShadow_.SetCheck(state.cloudsCastShadow);

            const auto precipitation =
                bridge::CapturePrecipitation(*weather);
            precipitationMode_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(precipitation.mode));
            precipitationIntensity_.SetValue(precipitation.intensity);
            precipitationFallSpeed_.SetValue(precipitation.fallSpeed);
            precipitationParticleScale_.SetValue(
                precipitation.particleScale);
            precipitationWindAzimuth_.SetValue(
                precipitation.windAzimuthDegrees);
            precipitationWindSpeed_.SetValue(precipitation.windSpeed);
            precipitationTurbulence_.SetValue(precipitation.turbulence);

            const auto sun = bridge::CaptureSun(
                session_->Scenes().GetScene(),
                entity);
            sunPreset_.SetSelectedWithoutCallback(0);
            sunTime_.SetValue(sun.timeHours);
            sunAzimuth_.SetValue(sun.azimuthDegrees);
            sunElevation_.SetValue(sun.elevationDegrees);
            sunPreviewSpeed_.SetValue(sunPreviewSpeedHoursPerSecond_);
            sunPlayButton_.SetEnabled(!sunPreviewPlaying_);
            sunPauseButton_.SetEnabled(sunPreviewPlaying_);

            const auto ocean = bridge::CaptureOcean(*weather);
            oceanEnabled_.SetCheck(ocean.enabled);
            oceanPreset_.SetSelectedWithoutCallback(0);
            oceanResolution_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(
                    ocean.displacementMapDimension));
            oceanWaterHeight_.SetValue(ocean.waterHeight);
            oceanPatchLength_.SetValue(ocean.patchLength);
            oceanWaveAmplitude_.SetValue(ocean.waveAmplitude);
            oceanChoppyScale_.SetValue(ocean.choppyScale);
            oceanTimeScale_.SetValue(ocean.timeScale);
            oceanWindAzimuth_.SetValue(ocean.windAzimuthDegrees);
            oceanWindSpeed_.SetValue(ocean.windSpeed);
            oceanWindDependency_.SetValue(ocean.windDependency);
            oceanSurfaceDetail_.SetValue(
                static_cast<float>(ocean.surfaceDetail));
            oceanDisplacementTolerance_.SetValue(
                ocean.surfaceDisplacementTolerance);
            oceanWaterRed_.SetValue(ocean.waterColor.x);
            oceanWaterGreen_.SetValue(ocean.waterColor.y);
            oceanWaterBlue_.SetValue(ocean.waterColor.z);
            oceanWaterOpacity_.SetValue(ocean.waterColor.w);
            oceanExtinctionRed_.SetValue(ocean.extinctionColor.x);
            oceanExtinctionGreen_.SetValue(ocean.extinctionColor.y);
            oceanExtinctionBlue_.SetValue(ocean.extinctionColor.z);

            oceanResolution_.SetEnabled(ocean.enabled);
            oceanWaterHeight_.SetEnabled(ocean.enabled);
            oceanPatchLength_.SetEnabled(ocean.enabled);
            oceanWaveAmplitude_.SetEnabled(ocean.enabled);
            oceanChoppyScale_.SetEnabled(ocean.enabled);
            oceanTimeScale_.SetEnabled(ocean.enabled);
            oceanWindAzimuth_.SetEnabled(ocean.enabled);
            oceanWindSpeed_.SetEnabled(ocean.enabled);
            oceanWindDependency_.SetEnabled(ocean.enabled);
            oceanSurfaceDetail_.SetEnabled(ocean.enabled);
            oceanDisplacementTolerance_.SetEnabled(ocean.enabled);
            oceanWaterRed_.SetEnabled(ocean.enabled);
            oceanWaterGreen_.SetEnabled(ocean.enabled);
            oceanWaterBlue_.SetEnabled(ocean.enabled);
            oceanWaterOpacity_.SetEnabled(ocean.enabled);
            oceanExtinctionRed_.SetEnabled(ocean.enabled);
            oceanExtinctionGreen_.SetEnabled(ocean.enabled);
            oceanExtinctionBlue_.SetEnabled(ocean.enabled);

            const bool physicalSky =
                state.skyMode != bridge::WeatherState::SkyMode::Skybox;
            const bool volumetricClouds =
                state.skyMode ==
                bridge::WeatherState::SkyMode::RealisticWithClouds;
            aerialPerspective_.SetEnabled(physicalSky);
            cloudCoverage_.SetEnabled(volumetricClouds);
            cloudStartHeight_.SetEnabled(volumetricClouds);
            cloudThickness_.SetEnabled(volumetricClouds);
            cloudsCastShadow_.SetEnabled(volumetricClouds);
            fogHeightStart_.SetEnabled(state.heightFog);
            fogHeightEnd_.SetEnabled(state.heightFog);
            const bool precipitationEnabled = precipitation.mode !=
                bridge::PrecipitationMode::None;
            precipitationIntensity_.SetEnabled(precipitationEnabled);
            precipitationFallSpeed_.SetEnabled(precipitationEnabled);
            precipitationParticleScale_.SetEnabled(precipitationEnabled);
            precipitationWindAzimuth_.SetEnabled(precipitationEnabled);
            precipitationWindSpeed_.SetEnabled(precipitationEnabled);
            precipitationTurbulence_.SetEnabled(precipitationEnabled);
            SyncGizmoSelection();
            return;
        }

        if (hasTerrain)
        {
            const auto* name =
                session_->Scenes().GetScene().names.GetComponent(entity);
            inspectorLabel_.SetText(
                "TERRAIN // " +
                (name != nullptr && !name->name.empty()
                    ? name->name
                    : "ENTITY " + std::to_string(entity)));
            const auto state = bridge::CaptureTerrain(*terrain);
            terrainPreset_.SetSelectedWithoutCallback(0);
            terrainVisibleRadius_.SetValue(
                static_cast<float>(state.visibleChunkRadius));
            terrainChunkScale_.SetValue(state.chunkScale);
            terrainMinimumHeight_.SetValue(state.minimumHeight);
            terrainMaximumHeight_.SetValue(state.maximumHeight);
            terrainLowAltitudeBlend_.SetValue(state.lowAltitudeBlend);
            terrainBaseBlend_.SetValue(state.baseBlend);
            terrainSlopeBlend_.SetValue(state.slopeBlend);
            terrainLodBias_.SetValue(state.lodBias);
            SyncGizmoSelection();
            return;
        }

        if (!hasTransform)
        {
            inspectorLabel_.SetText(terrainWorkspaceActive_
                ? "TERRAIN // CREATE OR EDIT LANDSCAPE"
                : "TRANSFORM // SELECT AN ENTITY");
            translationX_.SetValue(0.0f);
            translationY_.SetValue(0.0f);
            translationZ_.SetValue(0.0f);
            rotationX_.SetValue(0.0f);
            rotationY_.SetValue(0.0f);
            rotationZ_.SetValue(0.0f);
            scaleX_.SetValue(1.0f);
            scaleY_.SetValue(1.0f);
            scaleZ_.SetValue(1.0f);
            return;
        }

        const auto* name =
            session_->Scenes().GetScene().names.GetComponent(entity);
        inspectorLabel_.SetText(
            "TRANSFORM // " +
            (name != nullptr && !name->name.empty()
                ? name->name
                : "ENTITY " + std::to_string(entity)));
        translationX_.SetValue(transform->translation_local.x);
        translationY_.SetValue(transform->translation_local.y);
        translationZ_.SetValue(transform->translation_local.z);
        const auto rotation =
            wi::math::QuaternionToRollPitchYaw(transform->rotation_local);
        rotationX_.SetValue(rotation.x / XM_PI * 180.0f);
        rotationY_.SetValue(rotation.y / XM_PI * 180.0f);
        rotationZ_.SetValue(rotation.z / XM_PI * 180.0f);
        scaleX_.SetValue(transform->scale_local.x);
        scaleY_.SetValue(transform->scale_local.y);
        scaleZ_.SetValue(transform->scale_local.z);
        SyncGizmoSelection();
    }

    void StudioRenderPath::HandleEditorShortcuts()
    {
        if (projectHubVisible_ ||
            flyCameraActive_ ||
            GetGUI().IsTyping() ||
            pendingAction_ != EditorAction::None)
        {
            return;
        }

        const auto key = [](const char value)
        {
            return static_cast<wi::input::BUTTON>(value);
        };
        const bool control =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
        const bool shift =
            wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);

        if (control && wi::input::Press(key('Z')))
        {
            pendingAction_ = EditorAction::Undo;
        }
        else if (control && wi::input::Press(key('Y')))
        {
            pendingAction_ = EditorAction::Redo;
        }
        else if (control && wi::input::Press(key('D')))
        {
            pendingAction_ = EditorAction::DuplicateSelection;
        }
        else if (control && wi::input::Press(key('S')))
        {
            pendingAction_ = shift
                ? EditorAction::SaveSceneAs
                : EditorAction::SaveScene;
        }
        else if (wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE))
        {
            pendingAction_ = EditorAction::DeleteSelection;
        }
        else if (wi::input::Press(key('F')))
        {
            pendingAction_ = EditorAction::FocusSelection;
        }
        else if (wi::input::Press(key('W')))
        {
            pendingAction_ = EditorAction::TranslateTool;
        }
        else if (wi::input::Press(key('E')))
        {
            pendingAction_ = EditorAction::RotateTool;
        }
        else if (wi::input::Press(key('R')))
        {
            pendingAction_ = EditorAction::ScaleTool;
        }
        else if (wi::input::Press(key('G')))
        {
            pendingAction_ = EditorAction::ToggleGrid;
        }
    }

    bool StudioRenderPath::IsSelectedEntityValid() const
    {
        return session_ != nullptr &&
            session_->Selection().HasSelection() &&
            session_->Scenes().ContainsEntity(
                session_->Selection().SelectedEntity());
    }

    void StudioRenderPath::ProcessPendingAction()
    {
        if (session_ == nullptr)
        {
            pendingAction_ = EditorAction::None;
            return;
        }

        const EditorAction action = pendingAction_;
        pendingAction_ = EditorAction::None;
        switch (action)
        {
        case EditorAction::Undo:
        case EditorAction::Redo:
        {
            StopSunPreview(true);
            ClearSelectionOutline();
            const bool changed = action == EditorAction::Undo
                ? session_->Commands().Undo()
                : session_->Commands().Redo();
            if (changed)
            {
                if (session_->Selection().HasSelection() &&
                    !IsSelectedEntityValid())
                {
                    session_->Selection().Clear();
                }
                RefreshHierarchy();
                RefreshInspector();
                RefreshStatus();
            }
            else
            {
                SyncSelectionOutline();
            }
            break;
        }
        case EditorAction::FocusSelection:
            FocusSelection();
            break;
        case EditorAction::DuplicateSelection:
            DuplicateSelection();
            break;
        case EditorAction::DeleteSelection:
            DeleteSelection();
            break;
        case EditorAction::SaveScene:
            SaveScene();
            break;
        case EditorAction::SaveSceneAs:
            SaveSceneAs();
            break;
        case EditorAction::ReopenScene:
            ReopenScene();
            break;
        case EditorAction::ProjectHub:
            ReturnToProjectHub();
            break;
        case EditorAction::SelectTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetTransformTool(TransformTool::Select);
            break;
        case EditorAction::TranslateTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetTransformTool(TransformTool::Translate);
            break;
        case EditorAction::RotateTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetTransformTool(TransformTool::Rotate);
            break;
        case EditorAction::ScaleTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetTransformTool(TransformTool::Scale);
            break;
        case EditorAction::ToggleGrid:
            SetGridVisible(!gridVisible_);
            break;
        case EditorAction::OpenEnvironmentWorkspace:
            SetEnvironmentWorkspaceActive(true);
            break;
        case EditorAction::OpenTerrainWorkspace:
            SetTerrainWorkspaceActive(true);
            break;
        case EditorAction::OpenSceneWorkspace:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            break;
        case EditorAction::StartSunPreview:
            StartSunPreview();
            break;
        case EditorAction::PauseSunPreview:
            StopSunPreview(true);
            break;
        case EditorAction::SetOceanEnabled:
            ApplyOceanEnabled(pendingOceanEnabled_);
            break;
        case EditorAction::SetOceanResolution:
            ApplyOceanResolution(pendingOceanResolution_);
            break;
        case EditorAction::ApplyOceanPreset:
            ApplyOceanPreset(pendingOceanPreset_);
            break;
        case EditorAction::CreateTerrain:
            CreateTerrain();
            break;
        case EditorAction::ApplyTerrainPreset:
            ApplyTerrainPreset(pendingTerrainPreset_);
            break;
        case EditorAction::None:
        default:
            break;
        }
    }

    void StudioRenderPath::SetTransformTool(const TransformTool tool)
    {
        gizmo_.isTranslator = tool == TransformTool::Translate;
        gizmo_.isRotator = tool == TransformTool::Rotate;
        gizmo_.isScalator = tool == TransformTool::Scale;

        translateToolButton_.SetColor(
            gizmo_.isTranslator ? HologramSelected : HologramIdle,
            wi::gui::IDLE);
        rotateToolButton_.SetColor(
            gizmo_.isRotator ? HologramSelected : HologramIdle,
            wi::gui::IDLE);
        scaleToolButton_.SetColor(
            gizmo_.isScalator ? HologramSelected : HologramIdle,
            wi::gui::IDLE);
        studioChrome_.SetActiveTool(
            tool == TransformTool::Select
                ? 0
                : tool == TransformTool::Translate
                ? 1
                : tool == TransformTool::Rotate
                    ? 2
                    : 3);
        SyncGizmoSelection();
        RefreshStatus();
    }

    wi::ecs::Entity StudioRenderPath::EditableWeatherEntity() const noexcept
    {
        if (session_ == nullptr)
        {
            return wi::ecs::INVALID_ENTITY;
        }
        if (environmentWorkspaceActive_)
        {
            return session_->Scenes().WeatherEntity();
        }
        const auto selected = session_->Selection().SelectedEntity();
        return session_->Scenes().GetScene().weathers.Contains(selected)
            ? selected
            : wi::ecs::INVALID_ENTITY;
    }

    void StudioRenderPath::SetEnvironmentWorkspaceActive(const bool active)
    {
        if (!active && sunPreviewPlaying_)
        {
            StopSunPreview(true);
        }
        environmentWorkspaceActive_ = active && session_ != nullptr &&
            session_->Scenes().WeatherEntity() != wi::ecs::INVALID_ENTITY;
        if (environmentWorkspaceActive_)
        {
            terrainWorkspaceActive_ = false;
        }
        studioChrome_.SetEnvironmentWorkspaceActive(
            environmentWorkspaceActive_);
        studioChrome_.SetTerrainWorkspaceActive(terrainWorkspaceActive_);
        ClearSelectionOutline();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::SetTerrainWorkspaceActive(const bool active)
    {
        if (sunPreviewPlaying_)
        {
            StopSunPreview(true);
        }
        terrainWorkspaceActive_ = active && session_ != nullptr;
        if (terrainWorkspaceActive_)
        {
            environmentWorkspaceActive_ = false;
        }
        studioChrome_.SetEnvironmentWorkspaceActive(environmentWorkspaceActive_);
        studioChrome_.SetTerrainWorkspaceActive(terrainWorkspaceActive_);
        if (terrainWorkspaceActive_ &&
            session_->Scenes().GetScene().terrains.GetCount() > 0)
        {
            session_->Selection().Select(
                session_->Scenes().GetScene().terrains.GetEntity(0));
        }
        ClearSelectionOutline();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::FocusSelection()
    {
        if (!IsSelectedEntityValid())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        scene.Update(0.0f);
        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
        {
            return;
        }

        XMVECTOR center = transform->GetPositionV();
        float distance = 5.0f;
        if (scene.objects.Contains(entity))
        {
            const auto index = scene.objects.GetIndex(entity);
            if (index < scene.aabb_objects.size())
            {
                const auto& bounds = scene.aabb_objects[index];
                const XMFLOAT3 boundsCenter = bounds.getCenter();
                center = XMLoadFloat3(&boundsCenter);
                distance = std::max(2.5f, bounds.getRadius() * 2.5f);
            }
        }

        const XMVECTOR forward = XMVector3Normalize(camera->GetAt());
        const XMVECTOR eye = center - forward * distance;
        const XMMATRIX view = XMMatrixLookAtLH(
            eye,
            center,
            camera->GetUp());
        editorCameraTransform_.ClearTransform();
        editorCameraTransform_.MatrixTransform(
            XMMatrixInverse(nullptr, view));
        editorCameraTransform_.UpdateTransform();
        camera->TransformCamera(editorCameraTransform_);
        camera->UpdateCamera();
    }

    void StudioRenderPath::DuplicateSelection()
    {
        if (!IsSelectedEntityValid())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        ClearSelectionOutline();
        auto command = std::make_unique<bridge::DuplicateEntityCommand>(
            session_->Scenes().GetScene(),
            entity);
        auto* duplicateCommand = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            SyncSelectionOutline();
            return;
        }

        session_->Selection().Select(
            duplicateCommand->DuplicatedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::DeleteSelection()
    {
        if (!IsSelectedEntityValid())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        ClearSelectionOutline();
        if (!session_->Commands().Execute(
                std::make_unique<bridge::DeleteEntityCommand>(
                    session_->Scenes().GetScene(),
                    entity)))
        {
            SyncSelectionOutline();
            return;
        }

        session_->Selection().Clear();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
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

    bool StudioRenderPath::HandleTerrainSculpt(const XMFLOAT4& pointer)
    {
        if (!terrainWorkspaceActive_ || session_ == nullptr || flyCameraActive_ ||
            GetGUI().HasFocus() || !IsPointerOverViewport(pointer) ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return false;
        }
        auto& scene = session_->Scenes().GetScene();
        const auto terrainEntity = scene.terrains.GetEntity(0);
        auto* terrain = scene.terrains.GetComponent(terrainEntity);
        if (terrain == nullptr) return false;
        const auto ray = wi::renderer::GetPickRay(static_cast<long>(pointer.x),
            static_cast<long>(pointer.y), *this, *camera);
        const auto picked = wi::scene::Pick(ray, wi::enums::FILTER_TERRAIN, ~0u, scene);
        if (picked.entity != wi::ecs::INVALID_ENTITY)
        {
            wi::renderer::DrawSphere(wi::primitive::Sphere(picked.position,
                terrainBrushRadiusValue_), XMFLOAT4(0.20f, 0.92f, 1.0f, 0.22f), true);
        }
        if (wi::input::Press(wi::input::MOUSE_BUTTON_LEFT) &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            terrainStrokeActive_ = true;
            terrainStrokeChanged_ = false;
            terrainStrokeEntity_ = terrainEntity;
            terrainStrokeBefore_ = bridge::CaptureTerrainSculpt(scene, *terrain);
            terrainFlattenHeight_ = picked.position.y;
        }
        if (terrainStrokeActive_ && wi::input::Down(wi::input::MOUSE_BUTTON_LEFT) &&
            picked.entity != wi::ecs::INVALID_ENTITY)
        {
            terrainStrokeChanged_ |= bridge::SculptTerrain(scene, *terrain,
                picked.position, terrainBrushRadiusValue_,
                terrainBrushStrengthValue_ * 0.12f, terrainBrushFalloffValue_,
                terrainSculptModeValue_, terrainFlattenHeight_);
            return true;
        }
        if (terrainStrokeActive_ && wi::input::Release(wi::input::MOUSE_BUTTON_LEFT))
        {
            terrainStrokeActive_ = false;
            if (terrainStrokeChanged_)
            {
                auto after = bridge::CaptureTerrainSculpt(scene, *terrain);
                bridge::ApplyTerrainSculpt(scene, *terrain, terrainStrokeBefore_);
                session_->Commands().Execute(std::make_unique<bridge::SculptTerrainCommand>(
                    scene, terrainStrokeEntity_, std::move(terrainStrokeBefore_), std::move(after)));
                RefreshStatus();
            }
            return true;
        }
        return false;
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
        if (picked.entity == current && !environmentWorkspaceActive_)
        {
            return false;
        }

        environmentWorkspaceActive_ = false;
        studioChrome_.SetEnvironmentWorkspaceActive(false);
        terrainWorkspaceActive_ = false;
        studioChrome_.SetTerrainWorkspaceActive(false);

        if (picked.entity == wi::ecs::INVALID_ENTITY ||
            !session_->Scenes().IsHierarchyVisible(picked.entity))
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

    void StudioRenderPath::ApplySelectedTransformValue(
        const TransformTool tool,
        const int axis,
        const float value)
    {
        if (environmentWorkspaceActive_ || session_ == nullptr ||
            !session_->Selection().HasSelection())
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

        auto next = bridge::CaptureTransform(*transform);
        if (tool == TransformTool::Translate)
        {
            if (axis == 0)
            {
                next.translation.x = value;
            }
            else if (axis == 1)
            {
                next.translation.y = value;
            }
            else
            {
                next.translation.z = value;
            }
        }
        else if (tool == TransformTool::Rotate)
        {
            auto rotation =
                wi::math::QuaternionToRollPitchYaw(transform->rotation_local);
            const float radians = value / 180.0f * XM_PI;
            if (axis == 0)
            {
                rotation.x = radians;
            }
            else if (axis == 1)
            {
                rotation.y = radians;
            }
            else
            {
                rotation.z = radians;
            }
            XMStoreFloat4(
                &next.rotation,
                XMQuaternionNormalize(
                    XMQuaternionRotationRollPitchYaw(
                        rotation.x,
                        rotation.y,
                        rotation.z)));
        }
        else
        {
            if (axis == 0)
            {
                next.scale.x = value;
            }
            else if (axis == 1)
            {
                next.scale.y = value;
            }
            else
            {
                next.scale.z = value;
            }
        }

        session_->Commands().Execute(
            std::make_unique<bridge::SetTransformCommand>(
                session_->Scenes().GetScene(),
                entity,
                next));
        SetTransformTool(tool);
        RefreshInspector();
        RefreshStatus();
    }

    bool StudioRenderPath::CommitSelectedWeather(
        const bridge::WeatherState& weather)
    {
        if (session_ == nullptr)
        {
            return false;
        }

        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetWeatherCommand>(
                session_->Scenes().GetScene(),
                entity,
                weather));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::SetWeatherFieldValue(
        bridge::WeatherState& weather,
        const WeatherField field,
        const float value) noexcept
    {
        switch (field)
        {
        case WeatherField::SkyExposure:
            weather.skyExposure = std::clamp(value, 0.0f, 8.0f);
            break;
        case WeatherField::AmbientIntensity:
            weather.ambientIntensity = std::clamp(value, 0.0f, 8.0f);
            break;
        case WeatherField::FogStart:
            weather.fogStart = std::clamp(value, 0.0f, 100000.0f);
            break;
        case WeatherField::FogDensity:
            weather.fogDensity = std::clamp(value, 0.0f, 1.0f);
            break;
        case WeatherField::FogHeightStart:
            weather.fogHeightStart =
                std::clamp(value, -100000.0f, 100000.0f);
            break;
        case WeatherField::FogHeightEnd:
            weather.fogHeightEnd =
                std::clamp(value, -100000.0f, 100000.0f);
            break;
        case WeatherField::CloudCoverage:
            weather.cloudCoverage = std::clamp(value, 0.0f, 1.0f);
            break;
        case WeatherField::CloudStartHeight:
            weather.cloudStartHeight =
                std::clamp(value, 0.0f, 50000.0f);
            break;
        case WeatherField::CloudThickness:
            weather.cloudThickness =
                std::clamp(value, 1.0f, 50000.0f);
            break;
        }
    }

    void StudioRenderPath::BeginWeatherSlider(const WeatherField field)
    {
        weatherSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }
        weatherSliderActive_ = true;
        weatherSliderField_ = field;
        weatherSliderEntity_ = entity;
        weatherSliderBefore_ = bridge::CaptureWeather(*component);
        weatherSliderAfter_ = weatherSliderBefore_;
    }

    void StudioRenderPath::PreviewWeatherSlider(
        const WeatherField field,
        const float value)
    {
        if (!weatherSliderActive_ || weatherSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* component = scene.weathers.GetComponent(weatherSliderEntity_);
        if (component == nullptr)
        {
            weatherSliderActive_ = false;
            return;
        }
        weatherSliderAfter_ = weatherSliderBefore_;
        SetWeatherFieldValue(weatherSliderAfter_, field, value);
        bridge::ApplyWeather(*component, weatherSliderAfter_);
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == weatherSliderEntity_)
        {
            scene.weather = *component;
        }
    }

    void StudioRenderPath::CommitWeatherSlider(
        const WeatherField field,
        const float value)
    {
        if (!weatherSliderActive_ || weatherSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* component = scene.weathers.GetComponent(weatherSliderEntity_);
        if (component == nullptr)
        {
            weatherSliderActive_ = false;
            return;
        }

        SetWeatherFieldValue(weatherSliderAfter_, field, value);
        bridge::ApplyWeather(*component, weatherSliderBefore_);
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == weatherSliderEntity_)
        {
            scene.weather = *component;
        }
        session_->Commands().Execute(
            std::make_unique<bridge::SetWeatherCommand>(
                scene,
                weatherSliderEntity_,
                weatherSliderBefore_,
                weatherSliderAfter_));
        weatherSliderActive_ = false;
        weatherSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedWeatherToggle(
        const WeatherToggle toggle,
        const bool value)
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }

        auto next = bridge::CaptureWeather(*component);
        switch (toggle)
        {
        case WeatherToggle::AerialPerspective:
            next.aerialPerspective = value;
            break;
        case WeatherToggle::HeightFog:
            next.heightFog = value;
            break;
        case WeatherToggle::CloudsCastShadow:
            next.cloudsCastShadow = value;
            break;
        }
        CommitSelectedWeather(next);
    }

    void StudioRenderPath::ApplySelectedSkyMode(
        const bridge::WeatherState::SkyMode mode)
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }

        auto next = bridge::CaptureWeather(*component);
        next.skyMode = mode;
        CommitSelectedWeather(next);
    }

    void StudioRenderPath::ApplyWeatherPreset(const int preset)
    {
        if (session_ == nullptr)
        {
            return;
        }

        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }

        const auto current = bridge::CaptureWeather(*component);
        bridge::WeatherPreset selectedPreset;
        switch (preset)
        {
        case 1:
            selectedPreset = bridge::WeatherPreset::Clear;
            break;
        case 2:
            selectedPreset = bridge::WeatherPreset::Scattered;
            break;
        case 3:
            selectedPreset = bridge::WeatherPreset::Overcast;
            break;
        case 4:
            selectedPreset = bridge::WeatherPreset::Storm;
            break;
        default:
            return;
        }
        CommitSelectedWeather(
            bridge::MakeWeatherPreset(current, selectedPreset));
    }

    bool StudioRenderPath::CommitPrecipitation(
        const bridge::PrecipitationState& precipitation)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetPrecipitationCommand>(
                session_->Scenes().GetScene(),
                entity,
                precipitation));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplyPrecipitationMode(
        const bridge::PrecipitationMode mode)
    {
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }
        CommitPrecipitation(bridge::MakePrecipitationProfile(
            bridge::CapturePrecipitation(*component),
            mode));
    }

    void StudioRenderPath::SetPrecipitationFieldValue(
        bridge::PrecipitationState& precipitation,
        const PrecipitationField field,
        const float value) noexcept
    {
        switch (field)
        {
        case PrecipitationField::Intensity:
            precipitation.intensity = std::clamp(value, 0.0f, 1.0f);
            break;
        case PrecipitationField::FallSpeed:
            precipitation.fallSpeed = std::clamp(value, 0.01f, 2.0f);
            break;
        case PrecipitationField::ParticleScale:
            precipitation.particleScale =
                std::clamp(value, 0.005f, 0.1f);
            break;
        case PrecipitationField::WindAzimuth:
            precipitation.windAzimuthDegrees =
                std::clamp(value, -180.0f, 180.0f);
            break;
        case PrecipitationField::WindSpeed:
            precipitation.windSpeed = std::clamp(value, 0.0f, 50.0f);
            break;
        case PrecipitationField::Turbulence:
            precipitation.turbulence = std::clamp(value, 0.0f, 20.0f);
            break;
        }
    }

    void StudioRenderPath::BeginPrecipitationSlider(
        const PrecipitationField field)
    {
        precipitationSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }
        precipitationSliderActive_ = true;
        precipitationSliderField_ = field;
        precipitationSliderEntity_ = entity;
        precipitationSliderBefore_ =
            bridge::CapturePrecipitation(*component);
        precipitationSliderAfter_ = precipitationSliderBefore_;
    }

    void StudioRenderPath::PreviewPrecipitationSlider(
        const PrecipitationField field,
        const float value)
    {
        if (!precipitationSliderActive_ ||
            precipitationSliderField_ != field || session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* component =
            scene.weathers.GetComponent(precipitationSliderEntity_);
        if (component == nullptr)
        {
            precipitationSliderActive_ = false;
            return;
        }
        precipitationSliderAfter_ = precipitationSliderBefore_;
        SetPrecipitationFieldValue(precipitationSliderAfter_, field, value);
        bridge::ApplyPrecipitation(*component, precipitationSliderAfter_);
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == precipitationSliderEntity_)
        {
            scene.weather = *component;
        }
    }

    void StudioRenderPath::CommitPrecipitationSlider(
        const PrecipitationField field,
        const float value)
    {
        if (!precipitationSliderActive_ ||
            precipitationSliderField_ != field || session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* component =
            scene.weathers.GetComponent(precipitationSliderEntity_);
        if (component == nullptr)
        {
            precipitationSliderActive_ = false;
            return;
        }
        SetPrecipitationFieldValue(precipitationSliderAfter_, field, value);
        bridge::ApplyPrecipitation(*component, precipitationSliderBefore_);
        if (scene.weathers.GetCount() > 0 &&
            scene.weathers.GetEntity(0) == precipitationSliderEntity_)
        {
            scene.weather = *component;
        }
        session_->Commands().Execute(
            std::make_unique<bridge::SetPrecipitationCommand>(
                scene,
                precipitationSliderEntity_,
                precipitationSliderBefore_,
                precipitationSliderAfter_));
        precipitationSliderActive_ = false;
        precipitationSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    bool StudioRenderPath::CommitSun(const bridge::SunState& sun)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetSunCommand>(
                session_->Scenes().GetScene(),
                entity,
                sun));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySunPreset(const bridge::SunPreset preset)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto current = bridge::CaptureSun(
            session_->Scenes().GetScene(),
            entity);
        CommitSun(bridge::MakeSunPreset(current, preset));
    }

    void StudioRenderPath::SetSunFieldValue(
        bridge::SunState& sun,
        const SunField field,
        const float value) noexcept
    {
        switch (field)
        {
        case SunField::Time:
            bridge::SetSunTime(sun, value);
            break;
        case SunField::Azimuth:
            bridge::SetSunAzimuth(sun, value);
            break;
        case SunField::Elevation:
            bridge::SetSunElevation(sun, value);
            break;
        }
    }

    void StudioRenderPath::BeginSunSlider(const SunField field)
    {
        StopSunPreview(true);
        sunSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return;
        }
        sunSliderActive_ = true;
        sunSliderField_ = field;
        sunSliderBefore_ = bridge::CaptureSun(
            session_->Scenes().GetScene(),
            entity);
        sunSliderAfter_ = sunSliderBefore_;
    }

    void StudioRenderPath::PreviewSunSlider(
        const SunField field,
        const float value)
    {
        if (!sunSliderActive_ || sunSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        sunSliderAfter_ = sunSliderBefore_;
        SetSunFieldValue(sunSliderAfter_, field, value);
        bridge::ApplySun(
            session_->Scenes().GetScene(),
            EditableWeatherEntity(),
            sunSliderAfter_);
    }

    void StudioRenderPath::CommitSunSlider(
        const SunField field,
        const float value)
    {
        if (!sunSliderActive_ || sunSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        SetSunFieldValue(sunSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        const auto entity = EditableWeatherEntity();
        bridge::ApplySun(scene, entity, sunSliderBefore_);
        session_->Commands().Execute(
            std::make_unique<bridge::SetSunCommand>(
                scene,
                entity,
                sunSliderBefore_,
                sunSliderAfter_));
        sunSliderActive_ = false;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::StartSunPreview()
    {
        if (sunPreviewPlaying_ || session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return;
        }
        sunPreviewBefore_ = bridge::CaptureSun(
            session_->Scenes().GetScene(),
            entity);
        sunPreviewCurrent_ = sunPreviewBefore_;
        sunPreviewPlaying_ = true;
        sunPlayButton_.SetEnabled(false);
        sunPauseButton_.SetEnabled(true);
    }

    void StudioRenderPath::StopSunPreview(const bool commit)
    {
        if (!sunPreviewPlaying_ || session_ == nullptr)
        {
            return;
        }
        sunPreviewPlaying_ = false;
        auto& scene = session_->Scenes().GetScene();
        const auto entity = EditableWeatherEntity();
        bridge::ApplySun(scene, entity, sunPreviewBefore_);
        if (commit)
        {
            session_->Commands().Execute(
                std::make_unique<bridge::SetSunCommand>(
                    scene,
                    entity,
                    sunPreviewBefore_,
                    sunPreviewCurrent_));
        }
        sunPlayButton_.SetEnabled(true);
        sunPauseButton_.SetEnabled(false);
        RefreshInspector();
        RefreshStatus();
    }

    bool StudioRenderPath::CommitOcean(const bridge::OceanState& ocean)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetOceanCommand>(
                session_->Scenes().GetScene(),
                entity,
                ocean));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplyOceanEnabled(const bool enabled)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* weather =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }
        auto ocean = bridge::CaptureOcean(*weather);
        ocean.enabled = enabled;
        CommitOcean(ocean);
    }

    void StudioRenderPath::ApplyOceanResolution(const int dimension)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* weather =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }
        auto ocean = bridge::CaptureOcean(*weather);
        ocean.displacementMapDimension = dimension;
        CommitOcean(ocean);
    }

    void StudioRenderPath::ApplyOceanPreset(const bridge::OceanPreset preset)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* weather =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }
        CommitOcean(bridge::MakeOceanPreset(
            bridge::CaptureOcean(*weather),
            preset));
    }

    void StudioRenderPath::SetOceanFieldValue(
        bridge::OceanState& ocean,
        const OceanField field,
        const float value) noexcept
    {
        switch (field)
        {
        case OceanField::PatchLength:
            ocean.patchLength = std::clamp(value, 1.0f, 2000.0f);
            break;
        case OceanField::TimeScale:
            ocean.timeScale = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaveAmplitude:
            ocean.waveAmplitude = std::clamp(value, 0.0f, 2000.0f);
            break;
        case OceanField::WindAzimuth:
            ocean.windAzimuthDegrees =
                std::clamp(value, -180.0f, 180.0f);
            break;
        case OceanField::WindSpeed:
            ocean.windSpeed = std::clamp(value, 0.0f, 2000.0f);
            break;
        case OceanField::WindDependency:
            ocean.windDependency = std::clamp(value, 0.0f, 1.0f);
            break;
        case OceanField::ChoppyScale:
            ocean.choppyScale = std::clamp(value, 0.0f, 10.0f);
            break;
        case OceanField::WaterRed:
            ocean.waterColor.x = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaterGreen:
            ocean.waterColor.y = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaterBlue:
            ocean.waterColor.z = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaterOpacity:
            ocean.waterColor.w = std::clamp(value, 0.0f, 1.0f);
            break;
        case OceanField::ExtinctionRed:
            ocean.extinctionColor.x = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::ExtinctionGreen:
            ocean.extinctionColor.y = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::ExtinctionBlue:
            ocean.extinctionColor.z = std::clamp(value, 0.0f, 4.0f);
            break;
        case OceanField::WaterHeight:
            ocean.waterHeight = std::clamp(value, -1000.0f, 1000.0f);
            break;
        case OceanField::SurfaceDetail:
            ocean.surfaceDetail = static_cast<std::uint32_t>(
                std::clamp(std::lround(value), 1l, 10l));
            break;
        case OceanField::DisplacementTolerance:
            ocean.surfaceDisplacementTolerance =
                std::clamp(value, 1.0f, 10.0f);
            break;
        }
    }

    void StudioRenderPath::BeginOceanSlider(const OceanField field)
    {
        StopSunPreview(true);
        oceanSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = EditableWeatherEntity();
        const auto* weather =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (weather == nullptr)
        {
            return;
        }
        oceanSliderActive_ = true;
        oceanSliderField_ = field;
        oceanSliderEntity_ = entity;
        oceanSliderBefore_ = bridge::CaptureOcean(*weather);
        oceanSliderAfter_ = oceanSliderBefore_;
    }

    void StudioRenderPath::PreviewOceanSlider(
        const OceanField field,
        const float value)
    {
        if (!oceanSliderActive_ || oceanSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        oceanSliderAfter_ = oceanSliderBefore_;
        SetOceanFieldValue(oceanSliderAfter_, field, value);
        bridge::ApplyOcean(
            session_->Scenes().GetScene(),
            oceanSliderEntity_,
            oceanSliderAfter_);
    }

    void StudioRenderPath::CommitOceanSlider(
        const OceanField field,
        const float value)
    {
        if (!oceanSliderActive_ || oceanSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        SetOceanFieldValue(oceanSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        bridge::ApplyOcean(scene, oceanSliderEntity_, oceanSliderBefore_);
        session_->Commands().Execute(
            std::make_unique<bridge::SetOceanCommand>(
                scene,
                oceanSliderEntity_,
                oceanSliderBefore_,
                oceanSliderAfter_));
        oceanSliderActive_ = false;
        oceanSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CreateTerrain()
    {
        if (session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        if (scene.terrains.GetCount() > 0)
        {
            session_->Selection().Select(scene.terrains.GetEntity(0));
            RefreshHierarchy();
            RefreshInspector();
            return;
        }
        auto command = std::make_unique<bridge::CreateTerrainCommand>(
            scene,
            bridge::MakeTerrainPreset(
                bridge::TerrainState{},
                bridge::TerrainPreset::FlatWorld),
            "Terrain");
        auto* createCommand = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            return;
        }
        session_->Selection().Select(createCommand->CreatedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    bool StudioRenderPath::CommitTerrain(const bridge::TerrainState& terrain)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.terrains.Contains(entity))
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetTerrainCommand>(
                scene,
                entity,
                terrain));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplyTerrainPreset(
        const bridge::TerrainPreset preset)
    {
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* terrain =
            session_->Scenes().GetScene().terrains.GetComponent(entity);
        if (terrain == nullptr)
        {
            return;
        }
        CommitTerrain(bridge::MakeTerrainPreset(
            bridge::CaptureTerrain(*terrain),
            preset));
    }

    void StudioRenderPath::SetTerrainFieldValue(
        bridge::TerrainState& terrain,
        const TerrainField field,
        const float value) noexcept
    {
        switch (field)
        {
        case TerrainField::VisibleChunkRadius:
            terrain.visibleChunkRadius = static_cast<int>(
                std::clamp(std::lround(value), 1l, 16l));
            break;
        case TerrainField::ChunkScale:
            terrain.chunkScale = std::clamp(value, 0.25f, 16.0f);
            break;
        case TerrainField::MinimumHeight:
            terrain.minimumHeight = std::clamp(value, -2000.0f, 1999.0f);
            break;
        case TerrainField::MaximumHeight:
            terrain.maximumHeight = std::clamp(value, -1999.0f, 2000.0f);
            break;
        case TerrainField::LowAltitudeBlend:
            terrain.lowAltitudeBlend = std::clamp(value, 0.0f, 1.0f);
            break;
        case TerrainField::BaseBlend:
            terrain.baseBlend = std::clamp(value, 0.0f, 1.0f);
            break;
        case TerrainField::SlopeBlend:
            terrain.slopeBlend = std::clamp(value, 0.0f, 1.0f);
            break;
        case TerrainField::LodBias:
            terrain.lodBias = std::clamp(value, -4.0f, 4.0f);
            break;
        }
    }

    void StudioRenderPath::BeginTerrainSlider(const TerrainField field)
    {
        terrainSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* terrain =
            session_->Scenes().GetScene().terrains.GetComponent(entity);
        if (terrain == nullptr)
        {
            return;
        }
        terrainSliderActive_ = true;
        terrainSliderField_ = field;
        terrainSliderEntity_ = entity;
        terrainSliderBefore_ = bridge::CaptureTerrain(*terrain);
        terrainSliderAfter_ = terrainSliderBefore_;
    }

    void StudioRenderPath::PreviewTerrainSlider(
        const TerrainField field,
        const float value)
    {
        if (!terrainSliderActive_ || terrainSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        auto* terrain = session_->Scenes().GetScene().terrains.GetComponent(
            terrainSliderEntity_);
        if (terrain == nullptr)
        {
            return;
        }
        terrainSliderAfter_ = terrainSliderBefore_;
        SetTerrainFieldValue(terrainSliderAfter_, field, value);
        bridge::ApplyTerrain(*terrain, terrainSliderAfter_, false);
    }

    void StudioRenderPath::CommitTerrainSlider(
        const TerrainField field,
        const float value)
    {
        if (!terrainSliderActive_ || terrainSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        SetTerrainFieldValue(terrainSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        auto* terrain = scene.terrains.GetComponent(terrainSliderEntity_);
        if (terrain != nullptr)
        {
            bridge::ApplyTerrain(*terrain, terrainSliderBefore_, false);
            session_->Commands().Execute(
                std::make_unique<bridge::SetTerrainCommand>(
                    scene,
                    terrainSliderEntity_,
                    terrainSliderBefore_,
                    terrainSliderAfter_));
        }
        terrainSliderActive_ = false;
        terrainSliderEntity_ = wi::ecs::INVALID_ENTITY;
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

        StopSunPreview(true);

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
        // Stock workspace surfaces stay hidden. RenegadeStudioChrome owns the
        // shell, while the opaque Inspector host schedules the functional
        // controls rendered by Renegade subclasses.
        toolbarPanel_.SetVisible(false);
        hierarchyPanel_.SetVisible(false);
        inspectorPanel_.SetVisible(!visible);
        hierarchySearch_.SetVisible(!visible);
        contentPanel_.SetVisible(false);
        studioChrome_.SetVisible(!visible);

        // The frame-rate readout belongs to the authoring viewport and must
        // not appear over the Project Hub. DrawEditorGrid checks
        // projectHubVisible_ for the same reason.
        if (diagnostics_ != nullptr)
        {
            // The stock overlay collides with the Renegade wordmark. Runtime
            // diagnostics belong in the owned bottom drawer instead.
            diagnostics_->active = false;
        }

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

        if (environmentWorkspaceActive_ || session_ == nullptr ||
            !session_->Selection().HasSelection())
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
        gizmoTransformBefore_ = bridge::CaptureTransform(*transform);
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
        if (environmentWorkspaceActive_)
        {
            ClearSelectionOutline();
            return;
        }
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

    void StudioRenderPath::SaveScene()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            SaveSceneAs();
            return;
        }

        StopSunPreview(true);
        ClearSelectionOutline();
        session_->SaveScene(scenePath);
        SyncSelectionOutline();
        RefreshStatus();
        RefreshInspector();
    }

    void StudioRenderPath::SaveSceneAs()
    {
        if (session_ == nullptr)
        {
            return;
        }

        StopSunPreview(true);

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

        StopSunPreview(false);
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
        infoDisplay.device_name = false;
        infoDisplay.resolution = false;
        infoDisplay.logical_size = false;
        infoDisplay.colorspace = false;
        infoDisplay.fpsinfo = true;
        infoDisplay.size = 14;

        session_.Projects().Initialize("Saved/RenegadeStudio.ini");
        PrepareProvingGround();

        renderer_.BindSession(session_);
        renderer_.BindDiagnostics(infoDisplay);
        renderer_.init(canvas);
        renderer_.Load();
        ActivatePath(&renderer_);
    }
}
