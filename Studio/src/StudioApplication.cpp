#include "StudioApplication.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>

#include <wiBacklog.h>

namespace
{
    constexpr std::uint8_t SelectionStencilReference = 0x0F;

    // Renegade brand palette, pinned to Renegade_Studio_UI_Design_Tokens_v1.0.json.
    // Names are kept from the pre-brand "Hologram" placeholder theme to limit
    // the diff; the values below are the accepted brand hex values, not
    // approximations. Forge is the general UI accent (focus/active/selection);
    // Tech Cyan is reserved for the viewport (grid + camera-relative chrome),
    // per the brand quick-reference. See docs/PHASE3_BRAND_IDENTITY_APPLICATION.md.
    constexpr wi::Color HologramIdle = wi::Color(46, 43, 42, 224);      // graphite #2E2B2A
    constexpr wi::Color HologramFocus = wi::Color(210, 91, 29, 238);   // forge    #D25B1D
    constexpr wi::Color HologramActive = wi::Color(237, 123, 45, 255); // ignition #ED7B2D
    constexpr wi::Color HologramText = wi::Color(210, 200, 188, 255);  // bone     #D2C8BC
    constexpr wi::Color HologramMuted = wi::Color(133, 126, 120, 255); // ash      #857E78
    constexpr wi::Color HologramBorder = wi::Color(71, 68, 66, 210);   // gunmetal #474442 (border default)
    constexpr wi::Color HologramPanel = wi::Color(23, 23, 22, 236);    // carbon   #171716 (panel background)
    constexpr wi::Color HologramSelected = wi::Color(210, 91, 29, 245);// forge    #D25B1D
    constexpr wi::Color WarningAmber = wi::Color(240, 166, 64, 255);   // warning  #F0A640
    constexpr wi::Color ViewportCyan = wi::Color(56, 183, 215, 238);   // tech_cyan #38B7D7
    constexpr wi::Color DangerColor = wi::Color(217, 85, 85, 255);     // danger   #D95555
    constexpr wi::Color SuccessColor = wi::Color(76, 195, 138, 255);   // success  #4CC38A
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

        // Tech Cyan is the brand's dedicated viewport colour (see
        // Renegade_Studio_UI_Design_Tokens_v1.0.json); every grid line
        // including the minor/major/Z-axis uses it. The X axis keeps its role
        // as the deliberate orientation accent, now the brand's Ignition
        // orange rather than an ad hoc amber approximation.
        constants.minorColor = XMFLOAT4(0.220f, 0.718f, 0.843f, 0.28f); // tech_cyan #38B7D7
        constants.majorColor = XMFLOAT4(0.220f, 0.718f, 0.843f, 0.50f); // tech_cyan #38B7D7
        constants.axisColorX = XMFLOAT4(0.929f, 0.482f, 0.176f, 0.70f); // ignition  #ED7B2D
        constants.axisColorZ = XMFLOAT4(0.220f, 0.718f, 0.843f, 0.70f); // tech_cyan #38B7D7

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
        // The grid toggle is the one command-bar control that governs
        // viewport-only presentation, so it borrows Tech Cyan rather than the
        // Forge accent used for every other interactive control.
        gridToggleButton_.SetColor(
            visible ? ViewportCyan : HologramIdle,
            wi::gui::IDLE);

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
        gridToggleButton_.SetColor(ViewportCyan, wi::gui::IDLE);
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
            RequestCloseScene();
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

        const auto createWeatherInput = [this](
            wi::gui::TextInputField& input,
            const char* name,
            const char* description,
            const char* tooltip,
            const WeatherField field)
        {
            input.Create(name);
            input.SetDescription(description);
            input.SetTooltip(tooltip);
            input.SetValue(0.0f);
            input.OnInputAccepted(
                [this, field](const wi::gui::EventArgs& args)
            {
                ApplySelectedWeatherValue(field, args.fValue);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createWeatherInput(
            skyExposure_,
            "Sky Exposure",
            "Exposure: ",
            "Brightness of the physical sky.",
            WeatherField::SkyExposure);
        createWeatherInput(
            ambientIntensity_,
            "Ambient Intensity",
            "Ambient: ",
            "Neutral intensity applied while preserving the authored hue.",
            WeatherField::AmbientIntensity);

        createSectionLabel(
            environmentFogLabel_,
            "Environment Fog Section",
            "FOG // HEIGHT LAYER");
        createWeatherInput(
            fogStart_,
            "Fog Start",
            "Start: ",
            "Distance from the camera before fog begins.",
            WeatherField::FogStart);
        createWeatherInput(
            fogDensity_,
            "Fog Density",
            "Density: ",
            "Overall atmospheric fog density.",
            WeatherField::FogDensity);
        createWeatherToggle(
            heightFog_,
            "Height fog: ",
            "Restrict fog vertically between the authored heights.",
            WeatherToggle::HeightFog);
        createWeatherInput(
            fogHeightStart_,
            "Fog Height Start",
            "Base: ",
            "Lower height of the fog layer.",
            WeatherField::FogHeightStart);
        createWeatherInput(
            fogHeightEnd_,
            "Fog Height End",
            "Top: ",
            "Upper height of the fog layer.",
            WeatherField::FogHeightEnd);

        createSectionLabel(
            environmentCloudLabel_,
            "Environment Cloud Section",
            "VOLUMETRIC CLOUDS");
        createWeatherInput(
            cloudCoverage_,
            "Cloud Coverage",
            "Coverage: ",
            "Primary cloud-layer coverage amount.",
            WeatherField::CloudCoverage);
        createWeatherInput(
            cloudStartHeight_,
            "Cloud Start Height",
            "Base: ",
            "Altitude where the volumetric cloud volume begins.",
            WeatherField::CloudStartHeight);
        createWeatherInput(
            cloudThickness_,
            "Cloud Thickness",
            "Depth: ",
            "Vertical depth of the volumetric cloud volume.",
            WeatherField::CloudThickness);
        createWeatherToggle(
            cloudsCastShadow_,
            "Cloud shadows: ",
            "Allow volumetric clouds to cast moving shadows on the world.",
            WeatherToggle::CloudsCastShadow);

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
            ReopenScene();
        });
        inspectorPanel_.AddWidget(&reopenButton_);

        CreateMenuBar();
        CreateSceneTabStrip();
        CreateBottomDock();
        CreateAssetBrowser();
        CreateConsolePanel();
        CreateModalAndToast();
        CreatePanelChrome();
        LoadPanelGeometry();
    }

    void StudioRenderPath::CreateMenuBar()
    {
        const auto createMenuButton = [this](
            wi::gui::Button& button,
            const char* name,
            const char* text,
            const Menu menu)
        {
            button.Create(name);
            button.SetText(text);
            button.OnClick([this, menu](const wi::gui::EventArgs&)
            {
                ToggleMenu(menu);
            });
            toolbarPanel_.AddWidget(&button);
        };
        createMenuButton(menuButtonFile_, "Menu File", "FILE", Menu::File);
        createMenuButton(menuButtonEdit_, "Menu Edit", "EDIT", Menu::Edit);
        createMenuButton(menuButtonView_, "Menu View", "VIEW", Menu::View);
        createMenuButton(
            menuButtonWindow_,
            "Menu Window",
            "WINDOW",
            Menu::Window);

        const auto createDropdown = [this](wi::gui::Window& dropdown, const char* name)
        {
            dropdown.Create(name, wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
            dropdown.SetShadowRadius(6.0f);
            dropdown.SetVisible(false);
            GetGUI().AddWidget(&dropdown);
        };
        createDropdown(menuDropdownFile_, "File Menu Dropdown");
        createDropdown(menuDropdownEdit_, "Edit Menu Dropdown");
        createDropdown(menuDropdownView_, "View Menu Dropdown");
        createDropdown(menuDropdownWindow_, "Window Menu Dropdown");

        const auto createMenuItem = [this](
            wi::gui::Button& item,
            wi::gui::Window& dropdown,
            const char* name,
            const char* text,
            std::function<void()> action)
        {
            item.Create(name);
            item.SetText(text);
            item.font.params.h_align = wi::font::WIFALIGN_LEFT;
            item.OnClick([this, action](const wi::gui::EventArgs&)
            {
                CloseAllMenus();
                action();
            });
            dropdown.AddWidget(&item);
        };

        createMenuItem(
            menuFileSave_,
            menuDropdownFile_,
            "Menu File Save",
            "SAVE SCENE (CTRL+S)",
            [this] { pendingAction_ = EditorAction::SaveScene; });
        createMenuItem(
            menuFileSaveAs_,
            menuDropdownFile_,
            "Menu File Save As",
            "SAVE AS...",
            [this] { pendingAction_ = EditorAction::SaveSceneAs; });
        createMenuItem(
            menuFileCloseScene_,
            menuDropdownFile_,
            "Menu File Close Scene",
            "CLOSE SCENE",
            [this] { RequestCloseScene(); });

        createMenuItem(
            menuEditUndo_,
            menuDropdownEdit_,
            "Menu Edit Undo",
            "UNDO (CTRL+Z)",
            [this] { pendingAction_ = EditorAction::Undo; });
        createMenuItem(
            menuEditRedo_,
            menuDropdownEdit_,
            "Menu Edit Redo",
            "REDO (CTRL+Y)",
            [this] { pendingAction_ = EditorAction::Redo; });

        createMenuItem(
            menuViewHierarchy_,
            menuDropdownView_,
            "Menu View Hierarchy",
            "TOGGLE HIERARCHY",
            [this] { SetLeftPanelVisible(!hierarchyVisible_); });
        createMenuItem(
            menuViewInspector_,
            menuDropdownView_,
            "Menu View Inspector",
            "TOGGLE INSPECTOR",
            [this] { SetRightPanelVisible(!inspectorVisible_); });
        createMenuItem(
            menuViewAssets_,
            menuDropdownView_,
            "Menu View Assets",
            "ASSET BROWSER",
            [this] { ToggleDockTab(DockTab::Assets); });
        createMenuItem(
            menuViewConsole_,
            menuDropdownView_,
            "Menu View Console",
            "CONSOLE",
            [this] { ToggleDockTab(DockTab::Console); });
        createMenuItem(
            menuViewOutput_,
            menuDropdownView_,
            "Menu View Output",
            "OUTPUT",
            [this] { ToggleDockTab(DockTab::Output); });
        createMenuItem(
            menuViewDiagnostics_,
            menuDropdownView_,
            "Menu View Diagnostics",
            "DIAGNOSTICS",
            [this] { ToggleDockTab(DockTab::Diagnostics); });

        createMenuItem(
            menuWindowResetLayout_,
            menuDropdownWindow_,
            "Menu Window Reset Layout",
            "RESET WORKSPACE",
            [this] { ResetWorkspaceLayout(); });
    }

    void StudioRenderPath::CreateSceneTabStrip()
    {
        sceneTabsPanel_.Create(
            "Scene Tabs",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        sceneTabsPanel_.SetShadowRadius(4.0f);
        GetGUI().AddWidget(&sceneTabsPanel_);

        sceneTabButton_.Create("Scene Tab");
        sceneTabButton_.SetText("PROVING_GROUND.WISCENE");
        sceneTabButton_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        sceneTabButton_.SetColor(HologramFocus, wi::gui::IDLE);
        sceneTabsPanel_.AddWidget(&sceneTabButton_);

        sceneTabCloseButton_.Create("Scene Tab Close");
        sceneTabCloseButton_.SetText("x");
        sceneTabCloseButton_.SetTooltip(
            "Close the scene. Prompts to save if there are unsaved changes.");
        sceneTabCloseButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestCloseScene();
        });
        sceneTabsPanel_.AddWidget(&sceneTabCloseButton_);
    }

    void StudioRenderPath::CreateBottomDock()
    {
        contentPanel_.Create(
            "Bottom Dock",
            wi::gui::Window::WindowControls::RESIZE_TOP |
                wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        contentPanel_.SetShadowRadius(8.0f);
        GetGUI().AddWidget(&contentPanel_);

        const auto createDockTab = [this](
            wi::gui::Button& button,
            const char* name,
            const char* text,
            const DockTab tab)
        {
            button.Create(name);
            button.SetText(text);
            button.OnClick([this, tab](const wi::gui::EventArgs&)
            {
                ToggleDockTab(tab);
            });
            contentPanel_.AddWidget(&button);
        };
        createDockTab(
            dockTabAssetsButton_,
            "Dock Tab Assets",
            "ASSET BROWSER",
            DockTab::Assets);
        createDockTab(
            dockTabConsoleButton_,
            "Dock Tab Console",
            "CONSOLE",
            DockTab::Console);
        createDockTab(
            dockTabOutputButton_,
            "Dock Tab Output",
            "OUTPUT",
            DockTab::Output);
        createDockTab(
            dockTabDiagnosticsButton_,
            "Dock Tab Diagnostics",
            "DIAGNOSTICS",
            DockTab::Diagnostics);

        dockCollapseButton_.Create("Dock Collapse");
        dockCollapseButton_.SetText("v");
        dockCollapseButton_.SetTooltip("Collapse the bottom dock");
        dockCollapseButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SetDockTab(activeDockTab_, false);
        });
        contentPanel_.AddWidget(&dockCollapseButton_);
    }

    void StudioRenderPath::CreateAssetBrowser()
    {
        assetSearchField_.Create("Asset Search");
        assetSearchField_.SetDescription("SEARCH: ");
        assetSearchField_.SetCancelInputEnabled(false);
        assetSearchField_.OnInputAccepted([this](const wi::gui::EventArgs& args)
        {
            assetSearchFilter_ = args.sValue;
            RefreshAssetBrowser();
        });
        contentPanel_.AddWidget(&assetSearchField_);

        assetBreadcrumbLabel_.Create("Asset Breadcrumb");
        assetBreadcrumbLabel_.SetText("CONTENT");
        assetBreadcrumbLabel_.font.params.color = HologramMuted;
        assetBreadcrumbLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        contentPanel_.AddWidget(&assetBreadcrumbLabel_);

        for (std::size_t index = 0; index < assetFolderButtons_.size(); ++index)
        {
            auto& button = assetFolderButtons_[index];
            button.Create("Asset Folder " + std::to_string(index));
            button.SetText("");
            button.font.params.h_align = wi::font::WIFALIGN_LEFT;
            button.OnClick([this, index](const wi::gui::EventArgs&)
            {
                if (session_ == nullptr)
                {
                    return;
                }
                const auto& folders = session_->Content().Folders();
                if (index >= folders.size())
                {
                    return;
                }
                session_->Content().SelectFolder(folders[index].relativePath);
                RefreshAssetBrowser();
            });
            contentPanel_.AddWidget(&button);
        }

        for (std::size_t index = 0; index < assetCardButtons_.size(); ++index)
        {
            auto& button = assetCardButtons_[index];
            button.Create("Asset Card " + std::to_string(index));
            button.SetText("");
            button.font.params.size = 12;
            button.font.params.v_align = wi::font::WIFALIGN_TOP;
            contentPanel_.AddWidget(&button);
        }

        assetEmptyStateLabel_.Create("Asset Browser Empty State");
        assetEmptyStateLabel_.SetText(
            "This folder has no files yet. Renegade lists what is really on "
            "disk under the project's Content directory; asset import is a "
            "separate capability that is not built yet.");
        assetEmptyStateLabel_.SetFitTextEnabled(true);
        assetEmptyStateLabel_.font.params.color = HologramMuted;
        assetEmptyStateLabel_.font.params.size = 13;
        contentPanel_.AddWidget(&assetEmptyStateLabel_);
    }

    void StudioRenderPath::CreateConsolePanel()
    {
        consoleClearButton_.Create("Console Clear");
        consoleClearButton_.SetText("CLEAR");
        consoleClearButton_.SetTooltip("Clear the engine log (wi::backlog)");
        consoleClearButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            wi::backlog::clear();
            RefreshConsole();
        });
        contentPanel_.AddWidget(&consoleClearButton_);

        consoleLogLabel_.Create("Console Log");
        consoleLogLabel_.SetText("");
        consoleLogLabel_.SetFitTextEnabled(false);
        consoleLogLabel_.SetWrapEnabled(true);
        consoleLogLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        consoleLogLabel_.font.params.v_align = wi::font::WIFALIGN_TOP;
        consoleLogLabel_.font.params.size = 13;
        contentPanel_.AddWidget(&consoleLogLabel_);

        diagnosticsLabel_.Create("Diagnostics Panel");
        diagnosticsLabel_.SetText("");
        diagnosticsLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        diagnosticsLabel_.font.params.v_align = wi::font::WIFALIGN_TOP;
        diagnosticsLabel_.font.params.size = 13;
        contentPanel_.AddWidget(&diagnosticsLabel_);
    }

    void StudioRenderPath::CreateModalAndToast()
    {
        modalWindow_.Create(
            "Unsaved Changes Modal",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        modalWindow_.SetShadowRadius(16.0f);
        modalWindow_.SetVisible(false);
        GetGUI().AddWidget(&modalWindow_);

        modalTitleLabel_.Create("Modal Title");
        modalTitleLabel_.SetText("UNSAVED CHANGES");
        modalTitleLabel_.font.params.size = 16;
        modalTitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        modalWindow_.AddWidget(&modalTitleLabel_);

        modalBodyLabel_.Create("Modal Body");
        modalBodyLabel_.SetText(
            "This scene has unsaved editor history.\n"
            "Save your work before closing it?");
        modalBodyLabel_.SetFitTextEnabled(true);
        modalBodyLabel_.font.params.color = HologramMuted;
        modalBodyLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        modalWindow_.AddWidget(&modalBodyLabel_);

        modalCancelButton_.Create("Modal Cancel");
        modalCancelButton_.SetText("CANCEL");
        modalCancelButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            HideUnsavedChangesModal();
        });
        modalWindow_.AddWidget(&modalCancelButton_);

        modalDiscardButton_.Create("Modal Discard");
        modalDiscardButton_.SetText("DISCARD");
        modalDiscardButton_.SetColor(DangerColor, wi::gui::IDLE);
        modalDiscardButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            HideUnsavedChangesModal();
            ReturnToProjectHub();
        });
        modalWindow_.AddWidget(&modalDiscardButton_);

        modalSaveButton_.Create("Modal Save");
        modalSaveButton_.SetText("SAVE");
        modalSaveButton_.SetColor(HologramFocus, wi::gui::IDLE);
        modalSaveButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SaveScene();
            HideUnsavedChangesModal();
            ReturnToProjectHub();
        });
        modalWindow_.AddWidget(&modalSaveButton_);

        toastPanel_.Create(
            "Toast",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        toastPanel_.SetShadowRadius(6.0f);
        toastPanel_.SetVisible(false);
        GetGUI().AddWidget(&toastPanel_);

        toastTitleLabel_.Create("Toast Title");
        toastTitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        toastPanel_.AddWidget(&toastTitleLabel_);

        toastDetailLabel_.Create("Toast Detail");
        toastDetailLabel_.font.params.color = HologramMuted;
        toastDetailLabel_.font.params.size = 12;
        toastDetailLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        toastPanel_.AddWidget(&toastDetailLabel_);
    }

    void StudioRenderPath::CreatePanelChrome()
    {
        // Native wi::gui::Window resize controls give the creator the
        // "visible splitters, minimum sizes protect usability" behaviour the
        // brand guidelines (Panel and window behaviour) call for, without
        // Renegade needing to reimplement hit-testing and dragging.
        hierarchyPanel_.controls = hierarchyPanel_.controls |
            wi::gui::Window::WindowControls::RESIZE_RIGHT;
        hierarchyPanel_.OnResize([this]
        {
            leftPanelWidth_ = std::clamp(
                hierarchyPanel_.GetSize().x,
                240.0f,
                520.0f);
            ResizeLayout();
            PersistPanelGeometry();
        });

        inspectorPanel_.controls = inspectorPanel_.controls |
            wi::gui::Window::WindowControls::RESIZE_LEFT;
        inspectorPanel_.OnResize([this]
        {
            rightPanelWidth_ = std::clamp(
                inspectorPanel_.GetSize().x,
                300.0f,
                560.0f);
            ResizeLayout();
            PersistPanelGeometry();
        });

        contentPanel_.OnResize([this]
        {
            bottomDockHeight_ = std::clamp(
                contentPanel_.GetSize().y,
                180.0f,
                640.0f);
            ResizeLayout();
            PersistPanelGeometry();
        });
    }

    void StudioRenderPath::CreateProjectHub()
    {
        projectHubPanel_.Create(
            "Renegade Project Hub",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        projectHubPanel_.SetShadowRadius(12.0f);
        GetGUI().AddWidget(&projectHubPanel_);

        hubBrandLabel_.Create("Renegade Hub Brand");
        hubBrandLabel_.SetText("RENEGADE // BUILD WITHOUT PERMISSION.");
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
            // radius_px.panel from the design tokens.
            corner.radius = 8.0f;
        }
        theme.font.color = HologramText;
        theme.font.shadow_color = wi::Color(0, 0, 0, 220);
        theme.shadow = 3.0f;
        theme.shadow_color = HologramBorder;
        // Glow is reserved for focus, launch and live energy (brand guidelines,
        // "Elevation"), not a constant ambient effect on every panel, so the
        // theme-wide highlight stays off. Focus/active states still read as
        // Forge because IDLE/FOCUS/ACTIVE widget colours below carry it.
        theme.shadow_highlight = false;
        theme.shadow_highlight_color = XMFLOAT3(0.824f, 0.357f, 0.114f); // forge #D25B1D, used only if enabled per-widget
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

        if (toastTimer_ > 0.0f)
        {
            toastTimer_ -= dt;
            if (toastTimer_ <= 0.0f)
            {
                toastTimer_ = 0.0f;
                toastPanel_.SetVisible(false);
            }
        }

        if (session_ == nullptr || projectHubVisible_)
        {
            return;
        }

        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
        {
            if (modalVisible_)
            {
                HideUnsavedChangesModal();
            }
            else if (openMenu_ != Menu::None)
            {
                CloseAllMenus();
            }
        }

        if (modalVisible_)
        {
            // The modal blocks the rest of the frame: main panels are
            // SetEnabled(false) in ShowUnsavedChangesModal(), but the
            // viewport/gizmo/shortcut path below is bypassed here too so a
            // key press can't act on the scene behind the dialog.
            return;
        }

        // Live views only need to be rebuilt while their tab is visible.
        if (bottomDockOpen_ &&
            (activeDockTab_ == DockTab::Console ||
             activeDockTab_ == DockTab::Output ||
             activeDockTab_ == DockTab::Diagnostics))
        {
            RefreshConsole();
        }

        HandleEditorShortcuts();

        // wiGUI invokes OnClick while Button::Update is still active. Apply
        // editor actions only after the complete GUI update has returned.
        if (pendingAction_ != EditorAction::None)
        {
            ProcessPendingAction();
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
        const float toolbarHeight = 54.0f;
        const float sceneTabHeight = 34.0f;
        const float collapsedDockHeight = 32.0f;

        // Persisted, user-resizable panel geometry (drag the panel edges or
        // Window > Reset Workspace), not proportions recomputed from the
        // window size every time. Still clamped so an extreme window size
        // cannot make the viewport disappear.
        const float leftWidth = hierarchyVisible_
            ? std::clamp(leftPanelWidth_, 240.0f, std::max(240.0f, width - 600.0f))
            : 0.0f;
        const float rightWidth = inspectorVisible_
            ? std::clamp(rightPanelWidth_, 300.0f, std::max(300.0f, width - 600.0f))
            : 0.0f;
        const float bottomHeight = bottomDockOpen_
            ? std::clamp(
                bottomDockHeight_,
                180.0f,
                std::max(180.0f, height * 0.6f))
            : collapsedDockHeight;

        viewportBounds_ = XMFLOAT4(
            leftWidth + 16.0f,
            toolbarHeight + sceneTabHeight + 16.0f,
            width - rightWidth - 16.0f,
            height - bottomHeight - 16.0f);

        toolbarPanel_.SetPos(XMFLOAT2(8.0f, 8.0f));
        toolbarPanel_.SetSize(XMFLOAT2(width - 16.0f, toolbarHeight));
        workspaceTitle_.SetPos(XMFLOAT2(14.0f, 12.0f));
        workspaceTitle_.SetSize(XMFLOAT2(270.0f, 30.0f));
        menuButtonFile_.SetPos(XMFLOAT2(292.0f, 12.0f));
        menuButtonFile_.SetSize(XMFLOAT2(52.0f, 26.0f));
        menuButtonEdit_.SetPos(XMFLOAT2(346.0f, 12.0f));
        menuButtonEdit_.SetSize(XMFLOAT2(52.0f, 26.0f));
        menuButtonView_.SetPos(XMFLOAT2(400.0f, 12.0f));
        menuButtonView_.SetSize(XMFLOAT2(52.0f, 26.0f));
        menuButtonWindow_.SetPos(XMFLOAT2(454.0f, 12.0f));
        menuButtonWindow_.SetSize(XMFLOAT2(72.0f, 26.0f));

        const auto layoutDropdown = [](
            wi::gui::Window& dropdown,
            const std::vector<wi::gui::Widget*>& items,
            const float x,
            const float y,
            const float itemWidth)
        {
            constexpr float itemHeight = 30.0f;
            dropdown.SetPos(XMFLOAT2(x, y));
            dropdown.SetSize(XMFLOAT2(
                itemWidth,
                itemHeight * static_cast<float>(items.size()) + 8.0f));
            float rowY = 4.0f;
            for (auto* item : items)
            {
                item->SetPos(XMFLOAT2(4.0f, rowY));
                item->SetSize(XMFLOAT2(itemWidth - 8.0f, itemHeight - 4.0f));
                rowY += itemHeight;
            }
        };
        layoutDropdown(
            menuDropdownFile_,
            {&menuFileSave_, &menuFileSaveAs_, &menuFileCloseScene_},
            292.0f,
            toolbarHeight + 2.0f,
            215.0f);
        layoutDropdown(
            menuDropdownEdit_,
            {&menuEditUndo_, &menuEditRedo_},
            346.0f,
            toolbarHeight + 2.0f,
            190.0f);
        layoutDropdown(
            menuDropdownView_,
            {
                &menuViewHierarchy_,
                &menuViewInspector_,
                &menuViewAssets_,
                &menuViewConsole_,
                &menuViewOutput_,
                &menuViewDiagnostics_,
            },
            400.0f,
            toolbarHeight + 2.0f,
            220.0f);
        layoutDropdown(
            menuDropdownWindow_,
            {&menuWindowResetLayout_},
            454.0f,
            toolbarHeight + 2.0f,
            220.0f);

        translateToolButton_.SetPos(XMFLOAT2(546.0f, 9.0f));
        translateToolButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        rotateToolButton_.SetPos(XMFLOAT2(646.0f, 9.0f));
        rotateToolButton_.SetSize(XMFLOAT2(100.0f, 30.0f));
        scaleToolButton_.SetPos(XMFLOAT2(754.0f, 9.0f));
        scaleToolButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        gridToggleButton_.SetPos(XMFLOAT2(862.0f, 9.0f));
        gridToggleButton_.SetSize(XMFLOAT2(92.0f, 30.0f));
        projectHubButton_.SetPos(XMFLOAT2(width - 132.0f, 9.0f));
        projectHubButton_.SetSize(XMFLOAT2(108.0f, 30.0f));
        statusLabel_.SetPos(XMFLOAT2(968.0f, 14.0f));
        statusLabel_.SetSize(XMFLOAT2(
            std::max(120.0f, width - 1122.0f),
            24.0f));

        LayoutSceneTabStrip(leftWidth, rightWidth, toolbarHeight + 16.0f);

        hierarchyPanel_.SetVisible(hierarchyVisible_);
        hierarchyPanel_.SetPos(XMFLOAT2(8.0f, toolbarHeight + 16.0f));
        hierarchyPanel_.SetSize(XMFLOAT2(
            leftWidth,
            height - toolbarHeight - 24.0f));
        // Child sizing always uses the "real" stored panel width, even
        // when the panel is hidden and its outer leftWidth/rightWidth
        // collapse to 0 for viewport/sibling layout - this keeps every
        // child widget a valid, non-negative size while simply not being
        // rendered (SetVisible(false) on the parent already handles that).
        const float safeLeftWidth = hierarchyVisible_ ? leftWidth : leftPanelWidth_;
        const float safeRightWidth = inspectorVisible_ ? rightWidth : rightPanelWidth_;

        hierarchyLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        hierarchyLabel_.SetSize(XMFLOAT2(safeLeftWidth - 24.0f, 28.0f));
        hierarchyTree_.SetPos(XMFLOAT2(10.0f, 44.0f));
        hierarchyTree_.SetSize(XMFLOAT2(
            safeLeftWidth - 20.0f,
            height - toolbarHeight - 82.0f));

        inspectorPanel_.SetVisible(inspectorVisible_);
        inspectorPanel_.SetPos(XMFLOAT2(
            width - safeRightWidth - 8.0f,
            toolbarHeight + 16.0f));
        inspectorPanel_.SetSize(XMFLOAT2(
            safeRightWidth,
            height - toolbarHeight - 24.0f));
        inspectorLabel_.SetPos(XMFLOAT2(12.0f, 10.0f));
        inspectorLabel_.SetSize(XMFLOAT2(safeRightWidth - 24.0f, 28.0f));
        const float fieldGap = 8.0f;
        const float fieldWidth = (safeRightWidth - 40.0f) / 3.0f;
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

        const float environmentFieldWidth = safeRightWidth - 24.0f;
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

        const bool environmentSelected =
            session_ != nullptr &&
            session_->Scenes().GetScene().weathers.Contains(
                session_->Selection().SelectedEntity());
        LayoutInspectorActions(environmentSelected);

        LayoutBottomDock(leftWidth, rightWidth, width, height);
        LayoutModalAndToast(width, height);

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

    void StudioRenderPath::LayoutSceneTabStrip(
        const float leftWidth,
        const float rightWidth,
        const float top)
    {
        const float width = GetLogicalWidth();
        constexpr float tabHeight = 34.0f;
        sceneTabsPanel_.SetPos(XMFLOAT2(leftWidth + 16.0f, top));
        sceneTabsPanel_.SetSize(XMFLOAT2(
            std::max(120.0f, width - leftWidth - rightWidth - 32.0f),
            tabHeight));
        sceneTabButton_.SetPos(XMFLOAT2(4.0f, 3.0f));
        sceneTabButton_.SetSize(XMFLOAT2(
            std::max(60.0f, sceneTabsPanel_.GetSize().x - 42.0f),
            tabHeight - 6.0f));
        sceneTabCloseButton_.SetPos(XMFLOAT2(
            sceneTabsPanel_.GetSize().x - 34.0f,
            3.0f));
        sceneTabCloseButton_.SetSize(XMFLOAT2(30.0f, tabHeight - 6.0f));
    }

    void StudioRenderPath::LayoutBottomDock(
        const float leftWidth,
        const float rightWidth,
        const float width,
        const float height)
    {
        constexpr float collapsedDockHeight = 32.0f;
        const float dockHeight = bottomDockOpen_
            ? std::clamp(
                bottomDockHeight_,
                180.0f,
                std::max(180.0f, height * 0.6f))
            : collapsedDockHeight;

        contentPanel_.SetPos(XMFLOAT2(
            leftWidth + 16.0f,
            height - dockHeight - 8.0f));
        contentPanel_.SetSize(XMFLOAT2(
            std::max(200.0f, width - leftWidth - rightWidth - 32.0f),
            dockHeight));

        const float dockWidth = contentPanel_.GetSize().x;
        constexpr float tabRowHeight = 30.0f;
        float tabX = 4.0f;
        const auto layoutTab = [&](wi::gui::Button& button, const float tabWidth)
        {
            button.SetPos(XMFLOAT2(tabX, 2.0f));
            button.SetSize(XMFLOAT2(tabWidth, tabRowHeight - 4.0f));
            tabX += tabWidth + 2.0f;
        };
        layoutTab(dockTabAssetsButton_, 130.0f);
        layoutTab(dockTabConsoleButton_, 90.0f);
        layoutTab(dockTabOutputButton_, 84.0f);
        layoutTab(dockTabDiagnosticsButton_, 104.0f);
        dockCollapseButton_.SetPos(XMFLOAT2(dockWidth - 34.0f, 2.0f));
        dockCollapseButton_.SetSize(XMFLOAT2(28.0f, tabRowHeight - 4.0f));

        const float contentTop = tabRowHeight + 4.0f;
        const float contentHeight = std::max(
            40.0f,
            dockHeight - contentTop - 6.0f);
        LayoutAssetBrowser(dockWidth, contentHeight);
        LayoutConsolePanel(dockWidth, contentHeight);

        const bool showAssets = bottomDockOpen_ && activeDockTab_ == DockTab::Assets;
        const bool showConsole = bottomDockOpen_ &&
            (activeDockTab_ == DockTab::Console || activeDockTab_ == DockTab::Output);
        const bool showDiagnostics = bottomDockOpen_ &&
            activeDockTab_ == DockTab::Diagnostics;

        assetSearchField_.SetVisible(showAssets);
        assetBreadcrumbLabel_.SetVisible(showAssets);
        for (auto& button : assetFolderButtons_)
        {
            button.SetVisible(showAssets && button.IsVisible());
        }
        for (auto& button : assetCardButtons_)
        {
            button.SetVisible(showAssets && button.IsVisible());
        }
        assetEmptyStateLabel_.SetVisible(
            showAssets && assetEmptyStateLabel_.IsVisible());

        consoleClearButton_.SetVisible(showConsole);
        consoleLogLabel_.SetVisible(showConsole);
        diagnosticsLabel_.SetVisible(showDiagnostics);
    }

    void StudioRenderPath::LayoutAssetBrowser(
        const float contentWidth,
        const float contentHeight)
    {
        constexpr float folderColumnWidth = 170.0f;
        assetSearchField_.SetPos(XMFLOAT2(4.0f, 2.0f));
        assetSearchField_.SetSize(XMFLOAT2(folderColumnWidth - 8.0f, 26.0f));

        float folderY = 32.0f;
        for (auto& button : assetFolderButtons_)
        {
            button.SetPos(XMFLOAT2(4.0f, folderY));
            button.SetSize(XMFLOAT2(folderColumnWidth - 8.0f, 22.0f));
            folderY += 23.0f;
        }

        const float gridX = folderColumnWidth + 6.0f;
        const float gridWidth = std::max(120.0f, contentWidth - gridX - 4.0f);
        assetBreadcrumbLabel_.SetPos(XMFLOAT2(gridX, 2.0f));
        assetBreadcrumbLabel_.SetSize(XMFLOAT2(gridWidth, 22.0f));

        constexpr float cardWidth = 108.0f;
        constexpr float cardHeight = 96.0f;
        constexpr float cardGap = 8.0f;
        const int columns = std::max(
            1,
            static_cast<int>((gridWidth + cardGap) / (cardWidth + cardGap)));
        for (std::size_t index = 0; index < assetCardButtons_.size(); ++index)
        {
            const int column = static_cast<int>(index) % columns;
            const int row = static_cast<int>(index) / columns;
            assetCardButtons_[index].SetPos(XMFLOAT2(
                gridX + static_cast<float>(column) * (cardWidth + cardGap),
                30.0f + static_cast<float>(row) * (cardHeight + cardGap)));
            assetCardButtons_[index].SetSize(XMFLOAT2(cardWidth, cardHeight));
        }

        assetEmptyStateLabel_.SetPos(XMFLOAT2(gridX, 30.0f));
        assetEmptyStateLabel_.SetSize(XMFLOAT2(
            gridWidth,
            std::max(40.0f, contentHeight - 34.0f)));
    }

    void StudioRenderPath::LayoutConsolePanel(
        const float contentWidth,
        const float contentHeight)
    {
        consoleClearButton_.SetPos(XMFLOAT2(4.0f, 2.0f));
        consoleClearButton_.SetSize(XMFLOAT2(80.0f, 24.0f));
        consoleLogLabel_.SetPos(XMFLOAT2(4.0f, 30.0f));
        consoleLogLabel_.SetSize(XMFLOAT2(
            std::max(120.0f, contentWidth - 8.0f),
            std::max(40.0f, contentHeight - 34.0f)));
        diagnosticsLabel_.SetPos(XMFLOAT2(4.0f, 2.0f));
        diagnosticsLabel_.SetSize(XMFLOAT2(
            std::max(120.0f, contentWidth - 8.0f),
            contentHeight));
    }

    void StudioRenderPath::LayoutModalAndToast(
        const float width,
        const float height)
    {
        constexpr float modalWidth = 460.0f;
        constexpr float modalHeight = 190.0f;
        modalWindow_.SetPos(XMFLOAT2(
            (width - modalWidth) * 0.5f,
            (height - modalHeight) * 0.5f));
        modalWindow_.SetSize(XMFLOAT2(modalWidth, modalHeight));
        modalTitleLabel_.SetPos(XMFLOAT2(16.0f, 12.0f));
        modalTitleLabel_.SetSize(XMFLOAT2(modalWidth - 32.0f, 24.0f));
        modalBodyLabel_.SetPos(XMFLOAT2(16.0f, 44.0f));
        modalBodyLabel_.SetSize(XMFLOAT2(modalWidth - 32.0f, 80.0f));
        modalCancelButton_.SetPos(XMFLOAT2(16.0f, modalHeight - 42.0f));
        modalCancelButton_.SetSize(XMFLOAT2(110.0f, 30.0f));
        modalDiscardButton_.SetPos(XMFLOAT2(134.0f, modalHeight - 42.0f));
        modalDiscardButton_.SetSize(XMFLOAT2(110.0f, 30.0f));
        modalSaveButton_.SetPos(XMFLOAT2(
            modalWidth - 126.0f,
            modalHeight - 42.0f));
        modalSaveButton_.SetSize(XMFLOAT2(110.0f, 30.0f));

        constexpr float toastWidth = 320.0f;
        constexpr float toastHeight = 64.0f;
        toastPanel_.SetPos(XMFLOAT2(
            width - toastWidth - 18.0f,
            height - toastHeight - 44.0f));
        toastPanel_.SetSize(XMFLOAT2(toastWidth, toastHeight));
        toastTitleLabel_.SetPos(XMFLOAT2(12.0f, 8.0f));
        toastTitleLabel_.SetSize(XMFLOAT2(toastWidth - 24.0f, 20.0f));
        toastDetailLabel_.SetPos(XMFLOAT2(12.0f, 30.0f));
        toastDetailLabel_.SetSize(XMFLOAT2(toastWidth - 24.0f, 26.0f));
    }

    void StudioRenderPath::RefreshAssetBrowser()
    {
        if (session_ == nullptr)
        {
            return;
        }

        auto& content = session_->Content();
        const auto& folders = content.Folders();
        for (std::size_t index = 0; index < assetFolderButtons_.size(); ++index)
        {
            auto& button = assetFolderButtons_[index];
            if (index >= folders.size())
            {
                button.SetVisible(false);
                continue;
            }
            const auto& folder = folders[index];
            const std::string indent(
                static_cast<std::size_t>(std::max(0, folder.depth)) * 2,
                ' ');
            button.SetText(indent + folder.name);
            button.SetColor(
                folder.relativePath == content.SelectedFolder()
                    ? HologramSelected
                    : HologramIdle,
                wi::gui::IDLE);
            button.SetVisible(bottomDockOpen_ && activeDockTab_ == DockTab::Assets);
        }

        assetBreadcrumbLabel_.SetText(
            "CONTENT" +
            (content.SelectedFolder().empty()
                 ? std::string()
                 : (" / " + content.SelectedFolder())));

        auto assets = content.AssetsInFolder(content.SelectedFolder());
        if (!assetSearchFilter_.empty())
        {
            assets.erase(
                std::remove_if(
                    assets.begin(),
                    assets.end(),
                    [this](const bridge::ContentAsset& asset)
                    {
                        return asset.name.find(assetSearchFilter_) ==
                            std::string::npos;
                    }),
                assets.end());
        }

        for (std::size_t index = 0; index < assetCardButtons_.size(); ++index)
        {
            auto& button = assetCardButtons_[index];
            if (index >= assets.size())
            {
                button.SetVisible(false);
                continue;
            }
            const auto& asset = assets[index];
            button.SetText(
                asset.name + "\n" +
                bridge::ContentBrowserService::TypeLabel(asset.type));
            button.SetVisible(bottomDockOpen_ && activeDockTab_ == DockTab::Assets);
        }

        assetEmptyStateLabel_.SetVisible(
            bottomDockOpen_ &&
            activeDockTab_ == DockTab::Assets &&
            assets.empty());
    }

    void StudioRenderPath::RefreshConsole()
    {
        consoleLogLabel_.SetText(wi::backlog::getText());

        if (session_ == nullptr)
        {
            diagnosticsLabel_.SetText("NO SESSION");
            return;
        }

        diagnosticsLabel_.SetText(
            "RENDERER // " +
            std::string(wi::graphics::GetDevice()->GetAdapterName()) + "\n" +
            "RESOLUTION // " + std::to_string(GetPhysicalWidth()) + " x " +
            std::to_string(GetPhysicalHeight()) + "\n" +
            "ENTITIES // " + std::to_string(session_->Scenes().ListEntities().size()) + "\n" +
            "UNDO // " + std::to_string(session_->Commands().UndoCount()) +
            "  REDO // " + std::to_string(session_->Commands().RedoCount()));
    }

    void StudioRenderPath::SetDockTab(const DockTab tab, const bool forceOpen)
    {
        if (bottomDockOpen_ && activeDockTab_ == tab && !forceOpen)
        {
            bottomDockOpen_ = false;
        }
        else
        {
            activeDockTab_ = tab;
            bottomDockOpen_ = true;
        }

        for (auto* button : {
                 &dockTabAssetsButton_,
                 &dockTabConsoleButton_,
                 &dockTabOutputButton_,
                 &dockTabDiagnosticsButton_,
             })
        {
            button->SetColor(HologramIdle, wi::gui::IDLE);
        }
        if (bottomDockOpen_)
        {
            wi::gui::Button* activeButton = &dockTabAssetsButton_;
            switch (activeDockTab_)
            {
            case DockTab::Assets: activeButton = &dockTabAssetsButton_; break;
            case DockTab::Console: activeButton = &dockTabConsoleButton_; break;
            case DockTab::Output: activeButton = &dockTabOutputButton_; break;
            case DockTab::Diagnostics: activeButton = &dockTabDiagnosticsButton_; break;
            }
            activeButton->SetColor(HologramFocus, wi::gui::IDLE);
        }

        if (bottomDockOpen_ &&
            (activeDockTab_ == DockTab::Console || activeDockTab_ == DockTab::Output ||
             activeDockTab_ == DockTab::Diagnostics))
        {
            RefreshConsole();
        }
        if (bottomDockOpen_ && activeDockTab_ == DockTab::Assets)
        {
            RefreshAssetBrowser();
        }

        ResizeLayout();
        PersistPanelGeometry();
    }

    void StudioRenderPath::ToggleDockTab(const DockTab tab)
    {
        SetDockTab(tab, activeDockTab_ != tab || !bottomDockOpen_);
    }

    void StudioRenderPath::SetLeftPanelVisible(const bool visible)
    {
        hierarchyVisible_ = visible;
        ResizeLayout();
        PersistPanelGeometry();
    }

    void StudioRenderPath::SetRightPanelVisible(const bool visible)
    {
        inspectorVisible_ = visible;
        ResizeLayout();
        PersistPanelGeometry();
    }

    void StudioRenderPath::ToggleMenu(const Menu menu)
    {
        const Menu next = (openMenu_ == menu) ? Menu::None : menu;
        CloseAllMenus();
        openMenu_ = next;
        menuDropdownFile_.SetVisible(openMenu_ == Menu::File);
        menuDropdownEdit_.SetVisible(openMenu_ == Menu::Edit);
        menuDropdownView_.SetVisible(openMenu_ == Menu::View);
        menuDropdownWindow_.SetVisible(openMenu_ == Menu::Window);
    }

    void StudioRenderPath::CloseAllMenus()
    {
        openMenu_ = Menu::None;
        menuDropdownFile_.SetVisible(false);
        menuDropdownEdit_.SetVisible(false);
        menuDropdownView_.SetVisible(false);
        menuDropdownWindow_.SetVisible(false);
    }

    bool StudioRenderPath::IsSceneDirty() const
    {
        return session_ != nullptr &&
            session_->Commands().UndoCount() != lastSavedUndoCount_;
    }

    void StudioRenderPath::MarkSavedForDirtyTracking()
    {
        lastSavedUndoCount_ = session_ != nullptr
            ? session_->Commands().UndoCount()
            : 0;
    }

    void StudioRenderPath::RequestCloseScene()
    {
        if (IsSceneDirty())
        {
            ShowUnsavedChangesModal();
            return;
        }

        ReturnToProjectHub();
    }

    void StudioRenderPath::ShowUnsavedChangesModal()
    {
        modalVisible_ = true;
        modalWindow_.SetVisible(true);
        toolbarPanel_.SetEnabled(false);
        sceneTabsPanel_.SetEnabled(false);
        hierarchyPanel_.SetEnabled(false);
        inspectorPanel_.SetEnabled(false);
        contentPanel_.SetEnabled(false);
    }

    void StudioRenderPath::HideUnsavedChangesModal()
    {
        modalVisible_ = false;
        modalWindow_.SetVisible(false);
        toolbarPanel_.SetEnabled(true);
        sceneTabsPanel_.SetEnabled(true);
        hierarchyPanel_.SetEnabled(true);
        inspectorPanel_.SetEnabled(true);
        contentPanel_.SetEnabled(true);
    }

    void StudioRenderPath::ShowToast(
        const std::string& title,
        const std::string& detail)
    {
        toastTitleLabel_.SetText(title);
        toastDetailLabel_.SetText(detail);
        toastPanel_.SetVisible(true);
        toastTimer_ = 1.6f;
    }

    void StudioRenderPath::ResetWorkspaceLayout()
    {
        leftPanelWidth_ = 320.0f;
        rightPanelWidth_ = 360.0f;
        bottomDockHeight_ = 300.0f;
        bottomDockOpen_ = false;
        hierarchyVisible_ = true;
        inspectorVisible_ = true;
        activeDockTab_ = DockTab::Assets;
        ResizeLayout();
        PersistPanelGeometry();
    }

    void StudioRenderPath::PersistPanelGeometry()
    {
        if (session_ == nullptr)
        {
            return;
        }

        auto& projects = session_->Projects();
        projects.SetEditorPreference("layout_left_width", leftPanelWidth_);
        projects.SetEditorPreference("layout_right_width", rightPanelWidth_);
        projects.SetEditorPreference("layout_bottom_height", bottomDockHeight_);
        projects.SetEditorPreference("layout_hierarchy_visible", hierarchyVisible_);
        projects.SetEditorPreference("layout_inspector_visible", inspectorVisible_);
    }

    void StudioRenderPath::LoadPanelGeometry()
    {
        if (session_ == nullptr)
        {
            return;
        }

        auto& projects = session_->Projects();
        leftPanelWidth_ = projects.GetEditorPreference(
            "layout_left_width",
            leftPanelWidth_);
        rightPanelWidth_ = projects.GetEditorPreference(
            "layout_right_width",
            rightPanelWidth_);
        bottomDockHeight_ = projects.GetEditorPreference(
            "layout_bottom_height",
            bottomDockHeight_);
        hierarchyVisible_ = projects.GetEditorPreference(
            "layout_hierarchy_visible",
            hierarchyVisible_);
        inspectorVisible_ = projects.GetEditorPreference(
            "layout_inspector_visible",
            inspectorVisible_);
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

        // Real dirty state (undo history since the last save), not a
        // cosmetic flag: the scene tab's dot only lights up when
        // IsSceneDirty() is true.
        const bool dirty = IsSceneDirty();
        sceneTabCloseButton_.SetColor(
            dirty ? WarningAmber : HologramIdle,
            wi::gui::IDLE);
        sceneTabCloseButton_.SetTooltip(
            dirty
                ? "Unsaved changes. Close will offer to save first."
                : "Close the scene.");
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

    void StudioRenderPath::LayoutInspectorActions(const bool environment)
    {
        const float width = inspectorPanel_.GetSize().x;
        constexpr float gap = 8.0f;
        const float threeButtonWidth = (width - 40.0f) / 3.0f;
        const float twoButtonWidth = (width - 32.0f) / 2.0f;
        const float actionStart = environment
            ? std::max(578.0f, inspectorPanel_.GetSize().y - 82.0f)
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
        const auto entity = hasSession
            ? session_->Selection().SelectedEntity()
            : wi::ecs::INVALID_ENTITY;
        auto* transform = hasSession
            ? session_->Scenes().GetScene().transforms.GetComponent(entity)
            : nullptr;
        auto* weather = hasSession
            ? session_->Scenes().GetScene().weathers.GetComponent(entity)
            : nullptr;
        SyncSelectionOutline();

        const bool hasTransform = transform != nullptr;
        const bool hasWeather = weather != nullptr;
        LayoutInspectorActions(hasWeather);
        const auto setTransformVisible = [hasWeather](wi::gui::Widget& widget)
        {
            widget.SetVisible(!hasWeather);
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
                "ENVIRONMENT // " +
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
            SyncGizmoSelection();
            return;
        }

        if (!hasTransform)
        {
            inspectorLabel_.SetText("TRANSFORM // SELECT AN ENTITY");
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
        case EditorAction::TranslateTool:
            SetTransformTool(TransformTool::Translate);
            break;
        case EditorAction::RotateTool:
            SetTransformTool(TransformTool::Rotate);
            break;
        case EditorAction::ScaleTool:
            SetTransformTool(TransformTool::Scale);
            break;
        case EditorAction::ToggleGrid:
            SetGridVisible(!gridVisible_);
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
        SyncGizmoSelection();
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
        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return false;
        }

        const auto entity = session_->Selection().SelectedEntity();
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetWeatherCommand>(
                session_->Scenes().GetScene(),
                entity,
                weather));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySelectedWeatherValue(
        const WeatherField field,
        const float value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
        const auto* component =
            session_->Scenes().GetScene().weathers.GetComponent(entity);
        if (component == nullptr)
        {
            return;
        }

        auto next = bridge::CaptureWeather(*component);
        switch (field)
        {
        case WeatherField::SkyExposure:
            next.skyExposure = std::clamp(value, 0.0f, 8.0f);
            break;
        case WeatherField::AmbientIntensity:
            next.ambientIntensity = std::clamp(value, 0.0f, 8.0f);
            break;
        case WeatherField::FogStart:
            next.fogStart = std::clamp(value, 0.0f, 100000.0f);
            break;
        case WeatherField::FogDensity:
            next.fogDensity = std::clamp(value, 0.0f, 1.0f);
            break;
        case WeatherField::FogHeightStart:
            next.fogHeightStart =
                std::clamp(value, -100000.0f, 100000.0f);
            break;
        case WeatherField::FogHeightEnd:
            next.fogHeightEnd =
                std::clamp(value, -100000.0f, 100000.0f);
            break;
        case WeatherField::CloudCoverage:
            next.cloudCoverage = std::clamp(value, 0.0f, 1.0f);
            break;
        case WeatherField::CloudStartHeight:
            next.cloudStartHeight =
                std::clamp(value, 0.0f, 50000.0f);
            break;
        case WeatherField::CloudThickness:
            next.cloudThickness =
                std::clamp(value, 1.0f, 50000.0f);
            break;
        }
        CommitSelectedWeather(next);
    }

    void StudioRenderPath::ApplySelectedWeatherToggle(
        const WeatherToggle toggle,
        const bool value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
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
        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
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
        if (session_ == nullptr || !session_->Selection().HasSelection())
        {
            return;
        }

        const auto entity = session_->Selection().SelectedEntity();
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
                MarkSavedForDirtyTracking();
                sceneTabButton_.SetText(
                    wi::helper::GetFileNameFromPath(
                        session_->Projects().StartupScenePath()));
                session_->Content().Refresh(
                    session_->Projects().CurrentProject().rootPath +
                    "/Content");
                RefreshAssetBrowser();
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
        MarkSavedForDirtyTracking();
        sceneTabButton_.SetText(
            wi::helper::GetFileNameFromPath(
                session_->Projects().StartupScenePath()));
        session_->Content().Refresh(
            session_->Projects().CurrentProject().rootPath + "/Content");
        RefreshAssetBrowser();
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

        // Unsaved-changes confirmation now happens once, up front, in
        // RequestCloseScene() via the in-brand modal (Cancel/Discard/Save) -
        // matching Renegade_Studio_Workspace_Prototype_v1.0_Standalone.html.
        // A caller that reaches ReturnToProjectHub() directly (the modal's
        // own Discard/Save buttons) has already resolved that question, so
        // prompting again here would double the interruption.
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
        sceneTabsPanel_.SetVisible(!visible);
        hierarchyPanel_.SetVisible(!visible && hierarchyVisible_);
        inspectorPanel_.SetVisible(!visible && inspectorVisible_);
        contentPanel_.SetVisible(!visible);
        CloseAllMenus();
        if (visible)
        {
            HideUnsavedChangesModal();
        }

        // The frame-rate readout belongs to the authoring viewport and must
        // not appear over the Project Hub. DrawEditorGrid checks
        // projectHubVisible_ for the same reason.
        if (diagnostics_ != nullptr)
        {
            diagnostics_->active = !visible;
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

        ClearSelectionOutline();
        session_->SaveScene(scenePath);
        SyncSelectionOutline();
        MarkSavedForDirtyTracking();
        ShowToast(
            "Workspace saved",
            wi::helper::GetFileNameFromPath(scenePath));
        RefreshStatus();
        RefreshInspector();
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
                        MarkSavedForDirtyTracking();
                        sceneTabButton_.SetText(
                            wi::helper::GetFileNameFromPath(scenePath));
                        ShowToast(
                            "Workspace saved",
                            wi::helper::GetFileNameFromPath(scenePath));
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
        MarkSavedForDirtyTracking();
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
