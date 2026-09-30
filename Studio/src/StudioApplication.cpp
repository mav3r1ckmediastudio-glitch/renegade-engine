#include "DiagnosticInputFrame.h"
#include "StudioApplication.h"
#include <cctype>
#include "StudioUserPreferences.h"

#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/CharacterService.h"
#include "renegade/bridge/CreatorAssetWorkflowService.h"
#include "renegade/bridge/CreatorModelImportRecipe.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/HumanoidRetargetService.h"
#include "renegade/bridge/AnimationService.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <set>
#include <utility>

namespace
{
    namespace fs = std::filesystem;
    constexpr std::uint8_t SelectionStencilReference = 0x0F;
    constexpr wi::Color HologramIdle = wi::Color(12, 16, 19, 255);
    constexpr wi::Color HologramFocus = wi::Color(26, 31, 35, 255);
    constexpr wi::Color HologramActive = wi::Color(38, 43, 47, 255);
    constexpr wi::Color HologramText = wi::Color(244, 244, 244, 255);
    constexpr wi::Color HologramMuted = wi::Color(178, 178, 176, 255);
    constexpr wi::Color HologramBorder = wi::Color(38, 52, 61, 255);
    constexpr wi::Color HologramPanel = wi::Color(8, 12, 16, 255);
    constexpr wi::Color HologramSelected = wi::Color(44, 35, 29, 255);
    constexpr wi::Color HubBackground = wi::Color(4, 7, 10, 255);
    constexpr wi::Color HubSurface = wi::Color(8, 14, 18, 255);
    constexpr wi::Color HubSurfaceRaised = wi::Color(11, 20, 25, 255);
    constexpr wi::Color HubBorder = wi::Color(28, 68, 82, 255);
    constexpr wi::Color HubCyan = wi::Color(92, 208, 236, 255);
    constexpr wi::Color HubOrange = wi::Color(222, 91, 29, 255);
    constexpr wi::Color HubMuted = wi::Color(139, 158, 166, 255);
    constexpr wi::Color HubSelected = wi::Color(13, 35, 43, 255);
    constexpr wi::Color WarningAmber = wi::Color(255, 150, 40, 255);
    constexpr int LayoutPreferenceBits = 10;

    XMFLOAT4 RotationFromTo(
        const XMFLOAT3& source,
        const XMFLOAT3& target) noexcept
    {
        const XMVECTOR sourceVector = XMLoadFloat3(&source);
        const XMVECTOR targetVector = XMLoadFloat3(&target);
        if (XMVectorGetX(XMVector3LengthSq(sourceVector)) < 0.0001f ||
            XMVectorGetX(XMVector3LengthSq(targetVector)) < 0.0001f)
        {
            return XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        }
        const XMVECTOR from = XMVector3Normalize(sourceVector);
        const XMVECTOR to = XMVector3Normalize(targetVector);
        const float dot = XMVectorGetX(XMVector3Dot(from, to));
        if (dot >= 0.9999f)
        {
            return XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        }
        if (dot <= -0.9999f)
        {
            XMVECTOR axis = XMVector3Cross(from, XMVectorSet(1, 0, 0, 0));
            if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.0001f)
            {
                axis = XMVector3Cross(from, XMVectorSet(0, 0, 1, 0));
            }
            XMFLOAT4 rotation;
            XMStoreFloat4(
                &rotation,
                XMQuaternionRotationAxis(XMVector3Normalize(axis), XM_PI));
            return rotation;
        }

        const XMVECTOR axis = XMVector3Cross(from, to);
        XMFLOAT4 rotation;
        XMStoreFloat4(
            &rotation,
            XMQuaternionNormalize(XMVectorSet(
                XMVectorGetX(axis),
                XMVectorGetY(axis),
                XMVectorGetZ(axis),
                1.0f + dot)));
        return rotation;
    }

    wi::ecs::Entity ResolveReusableSelectionRoot(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity selected) noexcept
    {
        if (selected == wi::ecs::INVALID_ENTITY)
            return wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity current = selected;
        const std::size_t maximumDepth = scene.hierarchy.GetCount() + 1;
        for (std::size_t depth = 0;
            current != wi::ecs::INVALID_ENTITY && depth <= maximumDepth; ++depth)
        {
            const auto* metadata = scene.metadatas.GetComponent(current);
            if (metadata != nullptr && metadata->string_values.has(
                    renegade::bridge::ReusableAssetInstanceIdMetadataKey))
            {
                return current;
            }
            const auto* hierarchy = scene.hierarchy.GetComponent(current);
            if (hierarchy == nullptr || hierarchy->parentID == wi::ecs::INVALID_ENTITY ||
                hierarchy->parentID == current)
            {
                break;
            }
            current = hierarchy->parentID;
        }
        return wi::ecs::INVALID_ENTITY;
    }

    void CollectReusableSelectionObjects(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity root,
        std::vector<wi::ecs::Entity>& objects)
    {
        objects.clear();
        if (root == wi::ecs::INVALID_ENTITY)
            return;
        for (std::size_t index = 0; index < scene.objects.GetCount(); ++index)
        {
            const wi::ecs::Entity entity = scene.objects.GetEntity(index);
            if (entity == root || scene.Entity_IsDescendant(entity, root))
                objects.push_back(entity);
        }
    }

    const char* PlacementLightName(
        const wi::scene::LightComponent::LightType type) noexcept
    {
        switch (type)
        {
        case wi::scene::LightComponent::SPOT:
            return "SPOT";
        case wi::scene::LightComponent::RECTANGLE:
            return "RECTANGLE";
        case wi::scene::LightComponent::DIRECTIONAL:
            return "DIRECTIONAL";
        case wi::scene::LightComponent::POINT:
        default:
            return "POINT";
        }
    }

    renegade::studio::RenegadeStudioChrome::HierarchyCategory
    ToHierarchyCategory(
        const renegade::bridge::SceneEntityCategory category) noexcept
    {
        using BridgeCategory = renegade::bridge::SceneEntityCategory;
        using ChromeCategory =
            renegade::studio::RenegadeStudioChrome::HierarchyCategory;
        switch (category)
        {
        case BridgeCategory::Lights:
            return ChromeCategory::Lights;
        case BridgeCategory::Models:
            return ChromeCategory::Models;
        case BridgeCategory::Characters:
            return ChromeCategory::Characters;
        case BridgeCategory::Cameras:
            return ChromeCategory::Cameras;
        case BridgeCategory::Terrain:
            return ChromeCategory::Terrain;
        case BridgeCategory::Effects:
            return ChromeCategory::Effects;
        case BridgeCategory::Audio:
            return ChromeCategory::Audio;
        case BridgeCategory::Other:
        default:
            return ChromeCategory::Other;
        }
    }

    void DrawEditorLine(
        const XMFLOAT2& start,
        const XMFLOAT2& end,
        const XMFLOAT4& color)
    {
        wi::renderer::RenderableLine2D line;
        line.start = start;
        line.end = end;
        line.color_start = color;
        line.color_end = color;
        wi::renderer::DrawLine(line);
    }

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

    // Keep the temporary preview beyond the normal camera's 1000 m far plane
    // so the authored level remains invisible, but do not push it to 100 km.
    // At Y=100000 a 32-bit transform has roughly 7.8 mm granularity, which
    // visibly quantizes detailed character geometry and normals even though
    // the source mesh is intact. Y=2048 keeps sub-millimetre precision while
    // retaining render isolation from the authored scene at the origin.
    }

namespace renegade::studio
{
    struct StudioRenderPath::ProjectLoadOperation
    {
        std::string descriptorPath;
        std::string startupScenePath;
        bridge::ProjectMetadata project;
        bridge::PreparedSceneOpen preparedScene;
        bridge::MaterialTextureRestoreResult textureRestore;
        bool storyFlowNative = false;
        std::string error;
    };

    void StudioRenderPath::BindSession(bridge::StudioSession& session) noexcept
    {
        session_ = &session;
        scene = &session.Scenes().GetScene();
    }

    void StudioRenderPath::SetExitRequestHandler(std::function<void()> handler)
    {
        exitRequestHandler_ = std::move(handler);
    }

    void StudioRenderPath::RequestExit()
    {
        if (projectLoadingOverlay_.IsBlocking() &&
            projectLoadingOverlay_.CurrentPhase() != RenegadeProjectLoadingOverlay::Phase::Failed)
        {
            return;
        }
        if (session_ == nullptr)
        {
            if (exitRequestHandler_)
                exitRequestHandler_();
            return;
        }

        RequestSceneReplacement(
            [this]()
            {
                if (exitRequestHandler_)
                    exitRequestHandler_();
            });
    }



    void StudioRenderPath::BindDiagnostics(
        wi::Application::InfoDisplayer& diagnostics) noexcept
    {
        diagnostics_ = &diagnostics;
        InitializeLiveDiagnostics();
    }

    void StudioRenderPath::Load()
    {
        setSSREnabled(false);
        setReflectionsEnabled(true);

        // Ambient occlusion grounds the deck props against the terrain, and
        // volumetric lights are what make the scene's fog react to lighting
        // instead of reading as a flat screen-space overlay.
        setAO(AO::AO_MSAO);
        setAOPower(1.4f);
        setVolumeLightsEnabled(true);

        // Gate 5 owns image-quality state per Level. Apply the persisted
        // state before RenderPath3D::Load() so the first rendered frame cannot
        // inherit arbitrary settings from the previous editor scene.
        SyncRenderSettingsFromScene(false);

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
        RefreshAssetBrowser();
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
        // w is the grid plane height. Y=0 is Renegade's creator-facing
        // world reference plane; keep only the 2 cm reverse-Z/depth epsilon.
        constexpr float gridPlaneHeight = 0.02f;
        constants.cameraPosition = XMFLOAT4(
            camera->Eye.x,
            camera->Eye.y,
            camera->Eye.z,
            gridPlaneHeight);

        // The authored editor uses ice-blue interaction lines.
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
        if (!gridVisible_ || !gridPipeline_.IsValid() || projectHubVisible_ || camera == nullptr) return;

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
        if (pathTracePreviewActive_)
        {
            selectionOutlineMask_ = {};
            selectionOutlineMaskMsaa_ = {};
            RenderPath3D_PathTracing::ResizeBuffers();
            return;
        }
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

    void StudioRenderPath::PreRender()
    {
        if (testLevelRuntime_.IsActive())
        {
            // Runtime owns the live 3D world during Test Level. Keep only the
            // lightweight 2D Studio surface alive so STOP/status remain usable.
            wi::RenderPath2D::PreRender();
            return;
        }
        wi::RenderPath3D::PreRender();
    }

    void StudioRenderPath::Render() const
    {
        if (testLevelRuntime_.IsActive())
        {
            wi::RenderPath2D::Render();
            return;
        }
        if (pathTracePreviewActive_)
        {
            RenderPath3D_PathTracing::Render();
            return;
        }
        RenderPath3D::Render();

        const auto* depthStencil = GetDepthStencil();
        if (projectHubVisible_ ||
            outlinedSelection_ == wi::ecs::INVALID_ENTITY ||
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
                RequestDiagnosticAction(action);
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
            RequestDiagnosticAction(EditorAction::ToggleGrid);
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
            SetRenderWorkspaceActive(false);
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
            HologramPanel,
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
            sceneIdentityLabel_,
            "Scene Identity Section",
            "SCENE // IDENTITY");
        sceneNameInput_.Create("Scene Entity Name");
        sceneNameInput_.SetPlaceholder("ENTITY NAME");
        sceneNameInput_.SetTooltip(
            "Creator-facing name. Reusable imported assets rename their stable top-level root, not an internal glTF node.");
        sceneNameInput_.OnInputAccepted([this](const wi::gui::EventArgs& args)
        {
            CommitSelectedSceneName(args.sValue);
        });
        inspectorPanel_.AddWidget(&sceneNameInput_);

        createSectionLabel(
            sceneLayerLabel_,
            "Scene Layer Section",
            "LAYERS // 32-BIT MASK");
        sceneLayerAllButton_.Create("Enable All Scene Layers");
        sceneLayerAllButton_.SetText("ALL");
        sceneLayerAllButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ApplySelectedLayerMask(~0u);
        });
        inspectorPanel_.AddWidget(&sceneLayerAllButton_);
        sceneLayerNoneButton_.Create("Disable All Scene Layers");
        sceneLayerNoneButton_.SetText("NONE");
        sceneLayerNoneButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            ApplySelectedLayerMask(0u);
        });
        inspectorPanel_.AddWidget(&sceneLayerNoneButton_);
        for (std::uint32_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
        {
            auto& checkbox = sceneLayerBits_[bit];
            // The CheckBox Create() label is creator-visible. Keep it compact
            // so all 32 native layer bits remain readable in the 8-column grid.
            checkbox.Create(std::to_string(bit));
            checkbox.SetTooltip(
                "Wicked layer bit " + std::to_string(bit) +
                ". Reusable assets apply the bit to the stable root and descendant render objects.");
            checkbox.OnClick([this, bit](const wi::gui::EventArgs& args)
            {
                ApplySelectedLayerBit(bit, args.bValue);
            });
            inspectorPanel_.AddWidget(&checkbox);
        }

        createSectionLabel(
            sceneMetadataLabel_,
            "Scene Metadata Section",
            "METADATA // PRESET");
        sceneMetadataPreset_.Create("Metadata Preset");
        sceneMetadataPreset_.AddItem("CUSTOM", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Custom));
        sceneMetadataPreset_.AddItem("WAYPOINT", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Waypoint));
        sceneMetadataPreset_.AddItem("PLAYER", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Player));
        sceneMetadataPreset_.AddItem("ENEMY", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Enemy));
        sceneMetadataPreset_.AddItem("NPC", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::NPC));
        sceneMetadataPreset_.AddItem("PICKUP", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Pickup));
        sceneMetadataPreset_.AddItem("VEHICLE", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::Vehicle));
        sceneMetadataPreset_.AddItem("POINT OF INTEREST", static_cast<std::uint64_t>(wi::scene::MetadataComponent::Preset::PointOfInterest));
        sceneMetadataPreset_.SetTooltip(
            "Native Wicked semantic preset. Existing typed metadata and Renegade asset identity are preserved.");
        sceneMetadataPreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedMetadataPreset(
                static_cast<wi::scene::MetadataComponent::Preset>(args.userdata));
        });
        inspectorPanel_.AddWidget(&sceneMetadataPreset_);

        createSectionLabel(
            sceneObjectLabel_,
            "Scene Object Section",
            "OBJECT // RENDER PARTICIPATION");
        const auto createObjectToggle = [this](
            SceneInspectorCheckBox& checkbox,
            const char* name,
            const char* tooltip,
            const bridge::ObjectParticipationProperty property)
        {
            checkbox.Create(name);
            checkbox.SetTooltip(tooltip);
            checkbox.OnClick([this, property](const wi::gui::EventArgs& args)
            {
                ApplySelectedObjectParticipation(property, args.bValue);
            });
            inspectorPanel_.AddWidget(&checkbox);
        };
        createObjectToggle(sceneObjectRenderable_, "Renderable: ",
            "Participate in normal scene rendering.",
            bridge::ObjectParticipationProperty::Renderable);
        createObjectToggle(sceneObjectCastShadow_, "Cast shadow: ",
            "Allow this object to cast native Wicked shadows.",
            bridge::ObjectParticipationProperty::CastShadow);
        createObjectToggle(sceneObjectForeground_, "Foreground: ",
            "Render as foreground geometry.",
            bridge::ObjectParticipationProperty::Foreground);
        createObjectToggle(sceneObjectMainCamera_, "Main camera: ",
            "Visible to the main camera.",
            bridge::ObjectParticipationProperty::VisibleInMainCamera);
        createObjectToggle(sceneObjectReflections_, "Reflections: ",
            "Visible to reflection rendering.",
            bridge::ObjectParticipationProperty::VisibleInReflections);
        createObjectToggle(sceneObjectWetmap_, "Wetmap: ",
            "Enable native Wicked wetmap participation.",
            bridge::ObjectParticipationProperty::Wetmap);

        createSectionLabel(
            playerLabel_,
            "Player Start Section",
            "PLAYER START // FIRST PERSON");
        playerCameraMode_.Create("Player Camera Mode");
        playerCameraMode_.SetText(
            "CAMERA // FIRST PERSON // SPAWN HEADING FOLLOWS ROTATION Y");
        playerCameraMode_.font.params.size = 11;
        playerCameraMode_.font.params.color = HologramMuted;
        playerCameraMode_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        playerCameraMode_.SetColor(wi::Color::Transparent());
        inspectorPanel_.AddWidget(&playerCameraMode_);

        const auto createPlayerSlider = [this](
            SceneInspectorSlider& slider,
            const char* name,
            const char* label,
            const char* tooltip,
            const PlayerField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            slider.Create(minimum, maximum, minimum, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnValueCommitted([this, field](const float value)
            {
                CommitSelectedPlayerField(field, value);
            });
            inspectorPanel_.AddWidget(&slider);
        };
        createPlayerSlider(playerCapsuleRadius_, "Player Capsule Radius",
            "CAPSULE RADIUS // M", "Wicked/Jolt character capsule radius.",
            PlayerField::CapsuleRadius, 0.1f, 1.5f, 1401.0f);
        createPlayerSlider(playerCapsuleHeight_, "Player Capsule Height",
            "CAPSULE TOTAL HEIGHT // M", "Total height including both rounded caps.",
            PlayerField::CapsuleTotalHeight, 0.4f, 4.0f, 3601.0f);
        createPlayerSlider(playerEyeHeight_, "Player Eye Height",
            "EYE HEIGHT // M", "First-person camera height above the marker feet.",
            PlayerField::EyeHeight, 0.1f, 3.5f, 3401.0f);
        createPlayerSlider(playerWalkSpeed_, "Player Walk Speed",
            "WALK SPEED // M/S", "Normal movement speed.",
            PlayerField::WalkSpeed, 0.0f, 20.0f, 2001.0f);
        createPlayerSlider(playerSprintSpeed_, "Player Sprint Speed",
            "SPRINT SPEED // M/S", "Sprint speed; never lower than walk speed.",
            PlayerField::SprintSpeed, 0.0f, 30.0f, 3001.0f);
        createPlayerSlider(playerJumpSpeed_, "Player Jump Speed",
            "JUMP SPEED // M/S", "Vertical impulse requested through Wicked character physics.",
            PlayerField::JumpSpeed, 0.0f, 20.0f, 2001.0f);
        createPlayerSlider(playerLookSensitivity_, "Player Look Sensitivity",
            "LOOK SENSITIVITY", "Multiplier for mouse and gamepad look actions.",
            PlayerField::LookSensitivity, 0.05f, 5.0f, 991.0f);
        createPlayerSlider(playerMaximumSlope_, "Player Maximum Slope",
            "MAXIMUM SLOPE // DEG", "Steepest surface accepted by Wicked/Jolt.",
            PlayerField::MaximumSlope, 0.0f, 89.0f, 891.0f);
        createPlayerSlider(playerGravityFactor_, "Player Gravity Factor",
            "GRAVITY FACTOR", "Multiplier for world gravity on the character.",
            PlayerField::GravityFactor, 0.0f, 4.0f, 801.0f);
        createPlayerSlider(playerMinimumPitch_, "Player Minimum Pitch",
            "LOOK DOWN LIMIT // DEG", "Lowest first-person camera pitch.",
            PlayerField::MinimumPitch, -89.0f, 0.0f, 891.0f);
        createPlayerSlider(playerMaximumPitch_, "Player Maximum Pitch",
            "LOOK UP LIMIT // DEG", "Highest first-person camera pitch.",
            PlayerField::MaximumPitch, 0.0f, 89.0f, 891.0f);

        CreateMaterialInspector();
        CreateS1BInspectorSections();
        CreateRenderWorkspace();

        createSectionLabel(
            cameraLabel_,
            "Camera Section",
            "CAMERA // NATIVE WICKED");
        cameraProjection_.Create("Camera Projection");
        cameraProjection_.AddItem("PERSPECTIVE", 0);
        cameraProjection_.AddItem("ORTHOGRAPHIC", 1);
        cameraProjection_.SetTooltip(
            "Choose the selected scene camera's native projection mode.");
        cameraProjection_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedCameraProjection(args.userdata == 1);
        });
        inspectorPanel_.AddWidget(&cameraProjection_);

        const auto createCameraSlider = [this](
            SceneInspectorSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const CameraField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginCameraSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewCameraSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitCameraSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createCameraSlider(cameraFieldOfView_, "Camera FOV", "FIELD OF VIEW",
            "Perspective field of view in degrees.",
            CameraField::FieldOfView, 1.0f, 179.0f, 1780.0f);
        createCameraSlider(cameraNearPlane_, "Camera Near Plane", "NEAR CLIP",
            "Geometry nearer than this distance is clipped.",
            CameraField::NearPlane, 0.001f, 10.0f, 10000.0f);
        createCameraSlider(cameraFarPlane_, "Camera Far Plane", "FAR CLIP",
            "Geometry farther than this distance is clipped.",
            CameraField::FarPlane, 10.0f, 100000.0f, 100000.0f);
        createCameraSlider(cameraFocalLength_, "Camera Focal Length", "FOCAL DISTANCE",
            "Depth-of-field focus distance.",
            CameraField::FocalLength, 0.001f, 1000.0f, 10000.0f);
        createCameraSlider(cameraApertureSize_, "Camera Aperture", "APERTURE",
            "Depth-of-field aperture strength.",
            CameraField::ApertureSize, 0.0f, 1.0f, 1000.0f);
        createCameraSlider(cameraOrthoVerticalSize_, "Camera Ortho Size", "ORTHO // VERTICAL SIZE",
            "Vertical size of the orthographic camera volume.",
            CameraField::OrthoVerticalSize, 0.01f, 10000.0f, 100000.0f);

        cameraAlignToView_.Create("Align Camera To View");
        cameraAlignToView_.SetText("ALIGN CAMERA TO VIEW");
        cameraAlignToView_.SetTooltip(
            "Move the selected scene camera to the current editor viewpoint.");
        cameraAlignToView_.OnClick([this](const wi::gui::EventArgs&)
        {
            AlignSelectedCameraToView();
        });
        inspectorPanel_.AddWidget(&cameraAlignToView_);
        cameraViewFrom_.Create("View From Camera");
        cameraViewFrom_.SetText("VIEW FROM CAMERA");
        cameraViewFrom_.SetTooltip(
            "Move the transient editor view to the selected scene camera without changing the scene.");
        cameraViewFrom_.OnClick([this](const wi::gui::EventArgs&)
        {
            ViewFromSelectedCamera();
        });
        inspectorPanel_.AddWidget(&cameraViewFrom_);

        createSectionLabel(
            decalLabel_,
            "Decal Section",
            "DECAL // NATIVE WICKED");
        decalBaseColorOnlyAlpha_.Create("Base color alpha only: ");
        decalBaseColorOnlyAlpha_.SetTooltip(
            "Use only base-colour alpha while preserving normal/surface decal detail.");
        decalBaseColorOnlyAlpha_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* decal = session_->Scenes().GetScene().decals.GetComponent(entity);
            if (decal == nullptr)
                return;
            auto state = bridge::CaptureDecal(*decal);
            state.baseColorOnlyAlpha = args.bValue;
            CommitSelectedDecal(state);
        });
        inspectorPanel_.AddWidget(&decalBaseColorOnlyAlpha_);

        decalSlopeBlend_.Create(
            0.0f, 8.0f, 0.0f, 801.0f,
            "Decal Slope Blend", "SLOPE BLEND");
        decalSlopeBlend_.SetTooltip(
            "Blend decal projection by receiving-surface slope. Zero disables slope rejection.");
        decalSlopeBlend_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* decal = session_->Scenes().GetScene().decals.GetComponent(entity);
            if (decal == nullptr)
                return;
            auto state = bridge::CaptureDecal(*decal);
            state.slopeBlendPower = value;
            CommitSelectedDecal(state);
        });
        inspectorPanel_.AddWidget(&decalSlopeBlend_);

        createSectionLabel(
            decalMaterialLabel_,
            "Decal Material Section",
            "MATERIAL // RENEGRADE CORE");
        const auto createDecalMaterialSlider = [this](
            SceneInspectorSlider& slider,
            const char* name,
            const char* label,
            const int component)
        {
            slider.Create(0.0f, 1.0f, 1.0f, 1001.0f, name, label);
            slider.OnValueCommitted([this, component](float value)
            {
                if (session_ == nullptr)
                    return;
                const auto selected = session_->Selection().SelectedEntity();
                auto& scene = session_->Scenes().GetScene();
                if (!scene.decals.Contains(selected))
                    return;
                const auto materialEntity =
                    bridge::ResolveEditableMaterialEntity(scene, selected);
                auto* material = scene.materials.GetComponent(materialEntity);
                if (material == nullptr)
                    return;
                auto state = bridge::CaptureMaterial(*material);
                if (component == 0)
                    state.baseColor.x = value;
                else if (component == 1)
                    state.baseColor.y = value;
                else if (component == 2)
                    state.baseColor.z = value;
                else
                    state.baseColor.w = value;
                (void)session_->Commands().Execute(
                    std::make_unique<bridge::SetMaterialCommand>(
                        scene, materialEntity, state));
                RefreshInspector();
                RefreshStatus();
            });
            inspectorPanel_.AddWidget(&slider);
        };
        createDecalMaterialSlider(
            decalBaseColorRed_, "Decal Material Red", "BASE COLOR // R", 0);
        createDecalMaterialSlider(
            decalBaseColorGreen_, "Decal Material Green", "BASE COLOR // G", 1);
        createDecalMaterialSlider(
            decalBaseColorBlue_, "Decal Material Blue", "BASE COLOR // B", 2);
        createDecalMaterialSlider(
            decalOpacity_, "Decal Material Opacity", "OPACITY", 3);

        decalBaseColorTexture_.Create("Decal Base Color Texture");
        decalBaseColorTexture_.SetText("SELECT DECAL TEXTURE...");
        decalBaseColorTexture_.SetTooltip(
            "Choose a local image, import it as a governed Renegade texture, and bind it to this projected decal's base-colour/alpha slot.");
        decalBaseColorTexture_.OnClick([this](const wi::gui::EventArgs&)
        {
            ChooseSelectedDecalTexture();
        });
        inspectorPanel_.AddWidget(&decalBaseColorTexture_);

        createSectionLabel(
            environmentProbeLabel_,
            "Environment Probe Section",
            "ENVIRONMENT PROBE // NATIVE WICKED");
        environmentProbeResolution_.Create("Probe Resolution");
        for (const std::uint64_t resolution :
            {32ull, 64ull, 128ull, 256ull, 512ull, 1024ull, 2048ull})
        {
            environmentProbeResolution_.AddItem(
                std::to_string(resolution), resolution);
        }
        environmentProbeResolution_.SetTooltip(
            "Cubemap face resolution. Higher values cost more GPU memory and capture time.");
        environmentProbeResolution_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.resolution = static_cast<std::uint32_t>(args.userdata);
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeResolution_);

        environmentProbeRealtime_.Create("Real-time update: ");
        environmentProbeRealtime_.SetTooltip(
            "Continuously recapture this probe using the configured interval.");
        environmentProbeRealtime_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.realTime = args.bValue;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeRealtime_);

        environmentProbeInterval_.Create(
            0.0f, 60.0f, 0.0f, 601.0f,
            "Probe Update Interval", "UPDATE INTERVAL // S");
        environmentProbeInterval_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.updateInterval = value;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeInterval_);

        environmentProbeMsaa_.Create("8x MSAA capture: ");
        environmentProbeMsaa_.SetTooltip(
            "Use Wicked's native 8-sample MSAA environment-probe capture path.");
        environmentProbeMsaa_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.msaa = args.bValue;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeMsaa_);

        environmentProbeViewDistance_.Create(
            -1.0f, 5000.0f, -1.0f, 5002.0f,
            "Probe View Distance", "VIEW DISTANCE // -1 = CAMERA");
        environmentProbeViewDistance_.OnValueCommitted([this](float value)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            auto* probe = session_->Scenes().GetScene().probes.GetComponent(entity);
            if (probe == nullptr)
                return;
            auto state = bridge::CaptureEnvironmentProbe(*probe);
            state.viewDistance = value < 0.0f ? -1.0f : value;
            CommitSelectedEnvironmentProbe(state);
        });
        inspectorPanel_.AddWidget(&environmentProbeViewDistance_);

        environmentProbeRefresh_.Create("Refresh Environment Probe");
        environmentProbeRefresh_.SetText("REFRESH PROBE");
        environmentProbeRefresh_.SetTooltip(
            "Discard the generated cubemap and force Wicked to recapture this probe.");
        environmentProbeRefresh_.OnClick([this](const wi::gui::EventArgs&)
        {
            if (session_ == nullptr)
                return;
            const auto entity = session_->Selection().SelectedEntity();
            if (bridge::RefreshEnvironmentProbe(
                    session_->Scenes().GetScene(), entity))
            {
                RefreshInspector();
                RefreshStatus();
            }
        });
        inspectorPanel_.AddWidget(&environmentProbeRefresh_);

        createSectionLabel(
            lightLabel_,
            "Light Section",
            "LIGHT // NATIVE WICKED");
        lightType_.Create("Light Type");
        lightType_.AddItem(
            "DIRECTIONAL",
            static_cast<std::uint64_t>(
                wi::scene::LightComponent::DIRECTIONAL));
        lightType_.AddItem(
            "POINT",
            static_cast<std::uint64_t>(wi::scene::LightComponent::POINT));
        lightType_.AddItem(
            "SPOT",
            static_cast<std::uint64_t>(wi::scene::LightComponent::SPOT));
        lightType_.AddItem(
            "RECTANGLE",
            static_cast<std::uint64_t>(wi::scene::LightComponent::RECTANGLE));
        lightType_.SetTooltip(
            "Wicked's four native light types. Type-specific shape controls "
            "appear below.");
        lightType_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedLightType(
                static_cast<wi::scene::LightComponent::LightType>(
                    args.userdata));
        });
        inspectorPanel_.AddWidget(&lightType_);

        const auto createLightSlider = [this](
            RenegadeSlider& input,
            const char* name,
            const char* label,
            const char* tooltip,
            const LightField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            input.Create(minimum, maximum, 0.0f, steps, name, label);
            input.SetTooltip(tooltip);
            input.OnDragStarted([this, field](const float)
            {
                BeginLightSlider(field);
            });
            input.OnValuePreview([this, field](const float value)
            {
                PreviewLightSlider(field, value);
            });
            input.OnValueCommitted([this, field](const float value)
            {
                CommitLightSlider(field, value);
            });
            inspectorPanel_.AddWidget(&input);
        };
        createLightSlider(
            lightColorRed_,
            "Light Color Red",
            "COLOUR // RED",
            "Red channel of the native light colour.",
            LightField::ColorRed,
            0.0f,
            1.0f,
            255.0f);
        createLightSlider(
            lightColorGreen_,
            "Light Color Green",
            "COLOUR // GREEN",
            "Green channel of the native light colour.",
            LightField::ColorGreen,
            0.0f,
            1.0f,
            255.0f);
        createLightSlider(
            lightColorBlue_,
            "Light Color Blue",
            "COLOUR // BLUE",
            "Blue channel of the native light colour.",
            LightField::ColorBlue,
            0.0f,
            1.0f,
            255.0f);
        createLightSlider(
            lightIntensity_,
            "Light Intensity",
            "INTENSITY",
            "Brightness in Wicked's native physical units for this type.",
            LightField::Intensity,
            0.0f,
            2000.0f,
            20000.0f);
        createLightSlider(
            lightRange_,
            "Light Range",
            "RANGE",
            "Maximum influence distance. Directional lights are scene-wide.",
            LightField::Range,
            0.0f,
            1000.0f,
            10000.0f);
        createLightSlider(
            lightOuterCone_,
            "Light Outer Cone",
            "SPOT // OUTER CONE",
            "Outer spotlight cone angle in degrees.",
            LightField::OuterCone,
            0.1f,
            89.9f,
            898.0f);
        createLightSlider(
            lightInnerCone_,
            "Light Inner Cone",
            "SPOT // INNER CONE",
            "Inner spotlight cone angle; it cannot exceed the outer cone.",
            LightField::InnerCone,
            0.0f,
            89.9f,
            899.0f);
        createLightSlider(
            lightRadius_,
            "Light Radius",
            "SOURCE // RADIUS",
            "Physical source radius; also controls directional shadow softness.",
            LightField::Radius,
            0.0f,
            10.0f,
            1000.0f);
        createLightSlider(
            lightLength_,
            "Light Length Or Width",
            "SOURCE // LENGTH / WIDTH",
            "Point capsule length, or rectangle width.",
            LightField::Length,
            0.0f,
            100.0f,
            2000.0f);
        createLightSlider(
            lightHeight_,
            "Light Height",
            "SOURCE // HEIGHT",
            "Rectangle light height.",
            LightField::Height,
            0.0f,
            100.0f,
            2000.0f);

        lightCastShadow_.Create("Cast shadows: ");
        lightCastShadow_.SetTooltip(
            "Render native Wicked shadows from this light.");
        lightCastShadow_.OnClick([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedLightToggle(LightToggle::CastShadow, args.bValue);
        });
        inspectorPanel_.AddWidget(&lightCastShadow_);

        lightVolumetrics_.Create("Volumetric beam: ");
        lightVolumetrics_.SetTooltip(
            "Enable Wicked's real shadow-aware volumetric light scattering.");
        lightVolumetrics_.OnClick([this](const wi::gui::EventArgs& args)
        {
            ApplySelectedLightToggle(LightToggle::Volumetrics, args.bValue);
        });
        inspectorPanel_.AddWidget(&lightVolumetrics_);
        createLightSlider(
            lightVolumetricBoost_,
            "Light Volumetric Boost",
            "VOLUMETRIC // BOOST",
            "Increase this light's contribution to volumetric fog.",
            LightField::VolumetricBoost,
            0.0f,
            10.0f,
            1000.0f);

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
            stars_,
            "Stars",
            "STARS",
            "Procedural star visibility in the native realistic sky.",
            WeatherField::Stars,
            0.0f,
            1.0f,
            100.0f);
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
            RequestDiagnosticAction(EditorAction::StartSunPreview);
        });
        inspectorPanel_.AddWidget(&sunPlayButton_);

        sunPauseButton_.Create("Pause Sun Preview");
        sunPauseButton_.SetText("PAUSE");
        sunPauseButton_.SetTooltip(
            "Pause the preview and commit its final time as one Undo step.");
        sunPauseButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::PauseSunPreview);
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
            RequestDiagnosticAction(EditorAction::SetOceanEnabled);
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
                RequestDiagnosticAction(EditorAction::ApplyOceanPreset);
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
            RequestDiagnosticAction(EditorAction::SetOceanResolution);
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
            RequestDiagnosticAction(EditorAction::CreateTerrain);
        });
        inspectorPanel_.AddWidget(&createTerrainButton_);

        terrainSizeReadout_.Create("Current Terrain Size");
        terrainSizeReadout_.SetText("CURRENT TERRAIN // 1.25 KM x 1.25 KM");
        terrainSizeReadout_.SetTooltip(
            "Authored finite terrain size. Expansion preserves every existing chunk.");
        inspectorPanel_.AddWidget(&terrainSizeReadout_);

        expandTerrainButton_.Create("Expand Terrain");
        expandTerrainButton_.SetText("EXPAND TERRAIN // +1 RING");
        expandTerrainButton_.SetTooltip(
            "Add one 66 m chunk ring on every side without restarting or erasing sculpting.");
        expandTerrainButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::ExpandTerrain);
        });
        inspectorPanel_.AddWidget(&expandTerrainButton_);

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
        createTerrainSlider(terrainChunkScale_, "Terrain Resolution",
            "VERTEX SPACING (M)",
            "Distance between sculptable terrain samples. 1 m is standard; "
            "larger spacing trades local detail for world coverage.",
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

        createSectionLabel(
            terrainMaterialLabel_,
            "Terrain Material Section",
            "MATERIAL // DEFAULT GRASS");
        terrainMaterialPreset_.Create("Terrain Material Preset");
        terrainMaterialPreset_.AddItem("CUSTOM", 0u);
        terrainMaterialPreset_.AddItem(
            "MEADOW // 8X",
            static_cast<std::uint64_t>(
                bridge::TerrainMaterialPreset::Meadow) + 1u);
        terrainMaterialPreset_.AddItem(
            "COARSE GRASS // 12X",
            static_cast<std::uint64_t>(
                bridge::TerrainMaterialPreset::CoarseGrass) + 1u);
        terrainMaterialPreset_.AddItem(
            "FINE GROUND COVER // 16X",
            static_cast<std::uint64_t>(
                bridge::TerrainMaterialPreset::FineGroundCover) + 1u);
        terrainMaterialPreset_.SetTooltip(
            "Change grass density live without regenerating texture files.");
        terrainMaterialPreset_.OnSelect(
            [this](const wi::gui::EventArgs& args)
        {
            if (args.userdata > 0u)
            {
                pendingTerrainMaterialPreset_ =
                    static_cast<bridge::TerrainMaterialPreset>(
                        args.userdata - 1u);
                RequestDiagnosticAction(EditorAction::ApplyTerrainMaterialPreset);
            }
        });
        inspectorPanel_.AddWidget(&terrainMaterialPreset_);

        terrainTextureScale_.Create(
            1.0f,
            bridge::DefaultGrassPackedTileCount,
            bridge::DefaultGrassTextureScale,
            31.0f,
            "Terrain Texture Scale",
            "TEXTURE SCALE");
        terrainTextureScale_.SetTooltip(
            "Visible grass repeats. Updates live and is stored in WISCENE.");
        terrainTextureScale_.OnDragStarted([this](const float)
        {
            BeginTerrainTextureScale();
        });
        terrainTextureScale_.OnValuePreview([this](const float value)
        {
            PreviewTerrainTextureScale(value);
        });
        terrainTextureScale_.OnValueCommitted([this](const float value)
        {
            CommitTerrainTextureScale(value);
        });
        inspectorPanel_.AddWidget(&terrainTextureScale_);

        terrainApplyDefaultGrassButton_.Create("Apply Default Grass");
        terrainApplyDefaultGrassButton_.SetText("APPLY DEFAULT");
        terrainApplyDefaultGrassButton_.SetTooltip(
            "Assign the bundled grass to all four terrain material regions.");
        terrainApplyDefaultGrassButton_.OnClick(
            [this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::ApplyDefaultGrass);
        });
        inspectorPanel_.AddWidget(&terrainApplyDefaultGrassButton_);

        terrainReloadMaterialButton_.Create("Reload Terrain Material");
        terrainReloadMaterialButton_.SetText("RELOAD FILES");
        terrainReloadMaterialButton_.SetTooltip(
            "Reload changed bundled texture files without rebuilding Studio.");
        terrainReloadMaterialButton_.OnClick(
            [this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::ReloadTerrainMaterial);
        });
        inspectorPanel_.AddWidget(&terrainReloadMaterialButton_);

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
            const auto update = [this, target](const float value)
            {
                *target = value;
                std::ostringstream brush;
                brush << "BRUSH // SIZE " << std::fixed
                      << std::setprecision(0) << terrainBrushRadiusValue_
                      << " // STRENGTH " << std::setprecision(2)
                      << terrainBrushStrengthValue_;
                terrainBrushReadout_.SetText(brush.str());
            };
            slider.OnValuePreview(update);
            slider.OnValueCommitted(update);
            inspectorPanel_.AddWidget(&slider);
        };
        createBrushSlider(terrainBrushRadius_, "Terrain Brush Radius", "BRUSH SIZE",
            "Radius of the brush in world units.", 1.0f, 100.0f, 12.0f, 990.0f, &terrainBrushRadiusValue_);
        createBrushSlider(terrainBrushStrength_, "Terrain Brush Strength", "STRENGTH",
            "Height change applied while dragging.", 0.05f, 5.0f, 1.0f, 990.0f, &terrainBrushStrengthValue_);
        createBrushSlider(terrainBrushFalloff_, "Terrain Brush Falloff", "FALLOFF",
            "Zero is soft; one concentrates the effect at the centre.", 0.0f, 1.0f, 0.55f, 1000.0f, &terrainBrushFalloffValue_);

        terrainBrushReadout_.Create("Terrain Brush Readout");
        terrainBrushReadout_.SetText("BRUSH // SIZE 12 // STRENGTH 1.00");
        inspectorPanel_.AddWidget(&terrainBrushReadout_);
        terrainStrokeDiagnostic_.Create("Terrain Stroke Diagnostic");
        terrainStrokeDiagnostic_.SetText("LAST STROKE // READY");
        inspectorPanel_.AddWidget(&terrainStrokeDiagnostic_);

        CreateWd01VegetationControls();

        focusButton_.Create("Focus Selected");
        focusButton_.SetText("FOCUS [F]");
        focusButton_.SetTooltip("Frame the selected entity in the viewport");
        focusButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::FocusSelection);
        });
        inspectorPanel_.AddWidget(&focusButton_);

        duplicateButton_.Create("Duplicate Selected");
        duplicateButton_.SetText("DUPLICATE");
        duplicateButton_.SetTooltip("Duplicate selected entity (Ctrl+D)");
        duplicateButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::DuplicateSelection);
        });
        inspectorPanel_.AddWidget(&duplicateButton_);

        deleteButton_.Create("Delete Selected");
        deleteButton_.SetText("DELETE");
        deleteButton_.SetTooltip("Delete selected entity (Delete)");
        deleteButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::DeleteSelection);
        });
        inspectorPanel_.AddWidget(&deleteButton_);

        undoButton_.Create("Undo Transform");
        undoButton_.SetText("UNDO");
        undoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        undoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::Undo);
        });
        inspectorPanel_.AddWidget(&undoButton_);

        redoButton_.Create("Redo Transform");
        redoButton_.SetText("REDO");
        redoButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        redoButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::Redo);
        });
        inspectorPanel_.AddWidget(&redoButton_);

        saveButton_.Create("Save Scene");
        saveButton_.SetText("SAVE");
        saveButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        saveButton_.SetTooltip("Save the current scene (Ctrl+S)");
        saveButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::SaveScene);
        });
        inspectorPanel_.AddWidget(&saveButton_);

        saveAsButton_.Create("Save Scene As");
        saveAsButton_.SetText("SAVE AS...");
        saveAsButton_.SetSize(XMFLOAT2(112.0f, 28.0f));
        saveAsButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::SaveSceneAs);
        });
        inspectorPanel_.AddWidget(&saveAsButton_);

        reopenButton_.Create("Reopen Scene");
        reopenButton_.SetText("REOPEN");
        reopenButton_.SetSize(XMFLOAT2(92.0f, 28.0f));
        reopenButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            RequestDiagnosticAction(EditorAction::ReopenScene);
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
            [this](const std::uint64_t entity, const bool frameWholeObject)
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
            if (frameWholeObject)
                FocusSelection();
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
            if (action == RenegadeStudioChrome::Action::ProjectHub ||
                action == RenegadeStudioChrome::Action::SceneWorkspace ||
                action == RenegadeStudioChrome::Action::EnvironmentWorkspace ||
                action == RenegadeStudioChrome::Action::TerrainWorkspace ||
                action == RenegadeStudioChrome::Action::RenderWorkspace)
            {
                ResetS1BInspectorDisclosure();
            }
            switch (action)
            {
            case RenegadeStudioChrome::Action::ProjectHub:
                RequestDiagnosticAction(EditorAction::ProjectHub);
                break;
            case RenegadeStudioChrome::Action::OpenScene:
                RequestDiagnosticAction(EditorAction::OpenScene);
                break;
            case RenegadeStudioChrome::Action::Save:
                RequestDiagnosticAction(EditorAction::SaveScene);
                break;
            case RenegadeStudioChrome::Action::SaveAs:
                RequestDiagnosticAction(EditorAction::SaveSceneAs);
                break;
            case RenegadeStudioChrome::Action::Reopen:
                RequestDiagnosticAction(EditorAction::ReopenScene);
                break;
            case RenegadeStudioChrome::Action::Undo:
                RequestDiagnosticAction(EditorAction::Undo);
                break;
            case RenegadeStudioChrome::Action::Redo:
                RequestDiagnosticAction(EditorAction::Redo);
                break;
            case RenegadeStudioChrome::Action::Duplicate:
                RequestDiagnosticAction(EditorAction::DuplicateSelection);
                break;
            case RenegadeStudioChrome::Action::Delete:
                RequestDiagnosticAction(EditorAction::DeleteSelection);
                break;
            case RenegadeStudioChrome::Action::CreatePointLight:
                pendingLightType_ = wi::scene::LightComponent::POINT;
                RequestDiagnosticAction(EditorAction::CreateLight);
                break;
            case RenegadeStudioChrome::Action::CreateSpotLight:
                pendingLightType_ = wi::scene::LightComponent::SPOT;
                RequestDiagnosticAction(EditorAction::CreateLight);
                break;
            case RenegadeStudioChrome::Action::CreateDirectionalLight:
                pendingLightType_ = wi::scene::LightComponent::DIRECTIONAL;
                RequestDiagnosticAction(EditorAction::CreateLight);
                break;
            case RenegadeStudioChrome::Action::CreateRectangleLight:
                pendingLightType_ = wi::scene::LightComponent::RECTANGLE;
                RequestDiagnosticAction(EditorAction::CreateLight);
                break;
            case RenegadeStudioChrome::Action::CreatePlayerStart:
                RequestDiagnosticAction(EditorAction::CreatePlayerStart);
                break;
            case RenegadeStudioChrome::Action::CreateCamera:
                RequestDiagnosticAction(EditorAction::CreateCamera);
                break;
            case RenegadeStudioChrome::Action::CreateDecal:
                RequestDiagnosticAction(EditorAction::CreateDecal);
                break;
            case RenegadeStudioChrome::Action::CreateEnvironmentProbe:
                RequestDiagnosticAction(EditorAction::CreateEnvironmentProbe);
                break;
            case RenegadeStudioChrome::Action::Focus:
                RequestDiagnosticAction(EditorAction::FocusSelection);
                break;
            case RenegadeStudioChrome::Action::ToggleGrid:
                RequestDiagnosticAction(EditorAction::ToggleGrid);
                break;
            case RenegadeStudioChrome::Action::EnvironmentWorkspace:
                RequestDiagnosticAction(EditorAction::OpenEnvironmentWorkspace);
                break;
            case RenegadeStudioChrome::Action::TerrainWorkspace:
                RequestDiagnosticAction(EditorAction::OpenTerrainWorkspace);
                break;
            case RenegadeStudioChrome::Action::RenderWorkspace:
                RequestDiagnosticAction(EditorAction::OpenRenderWorkspace);
                break;
            case RenegadeStudioChrome::Action::SceneWorkspace:
                RequestDiagnosticAction(EditorAction::OpenSceneWorkspace);
                break;
            case RenegadeStudioChrome::Action::TestLevelPlay:
                RequestDiagnosticAction(EditorAction::StartTestLevel);
                break;
            case RenegadeStudioChrome::Action::TestLevelStop:
                RequestDiagnosticAction(EditorAction::StopTestLevel);
                break;
            case RenegadeStudioChrome::Action::BuildWindowsGame:
                RequestDiagnosticAction(EditorAction::BuildWindowsGame);
                break;
            }
        });
        studioChrome_.OnDrawerChanged([this](const int tab)
        {
            if (tab >= 0)
            {
                lastDrawerTab_ = tab;
            }
            if (tab == 0)
            {
                RefreshAssetBrowser();
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
        studioChrome_.OnAssetBrowserFolderSelected(
            [this](const std::string& relativePath)
        {
            SelectAssetBrowserFolder(relativePath);
        });
        studioChrome_.OnAssetBrowserItemSelected(
            [this](const std::string& relativePath)
        {
            SelectAssetBrowserItem(relativePath);
        });
        studioChrome_.OnCreatorAssetPlaceRequested(
            [this](
                const bridge::StableId& assetId,
                const std::string& label)
        {
            BeginCreatorAssetPlacement(assetId, label);
        });
        studioChrome_.OnCreatorAssetDropped(
            [this](
                const bridge::StableId& assetId,
                const std::string& label,
                const float x,
                const float y)
        {
            DropCreatorAsset(assetId, label, x, y);
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
        // Audio is an independent top-level authoring surface. Keep the
        // accepted Inspector alive and untouched underneath it: Wicked's
        // Window::SetVisible(true) makes every child visible, so using parent
        // visibility as a z-order switch corrupts per-section Inspector state.
        // Register Audio ahead of the Inspector in Wicked's reverse render
        // order, then keep the chrome behind both authoring surfaces.
        GetGUI().RemoveWidget(&inspectorPanel_);
        GetGUI().AddWidget(&studioChrome_.AudioWorkspace());
        GetGUI().AddWidget(&inspectorPanel_);
        GetGUI().AddWidget(&studioChrome_);
    }

    void StudioRenderPath::CreateProjectHub()
    {
        projectHubPanel_.Create(
            "Renegade Project Hub",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        projectHubPanel_.SetShadowRadius(0.0f);
        GetGUI().AddWidget(&projectHubPanel_);

        hubBrandLabel_.Create("Renegade Hub Brand");
        hubBrandLabel_.SetText("RENEGADE");
        hubBrandLabel_.font.params.size = 15;
        hubBrandLabel_.font.params.bolden = 0.30f;
        hubBrandLabel_.font.params.color = HubOrange;
        hubBrandLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubBrandLabel_);

        hubTitleLabel_.Create("Renegade Project Hub Title");
        hubTitleLabel_.SetText("PROJECT HUB");
        hubTitleLabel_.font.params.size = 38;
        hubTitleLabel_.font.params.bolden = 0.28f;
        hubTitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubTitleLabel_);

        hubSubtitleLabel_.Create("Renegade Project Hub Subtitle");
        hubSubtitleLabel_.SetText(
            "PROJECT LIFECYCLE CONTROL // CREATE // OPEN // CONTINUE");
        hubSubtitleLabel_.font.params.size = 14;
        hubSubtitleLabel_.font.params.color = HubMuted;
        hubSubtitleLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubSubtitleLabel_);

        projectNameInput_.Create("New Project Name");
        projectNameInput_.SetDescription("NEW PROJECT // NAME: ");
        projectNameInput_.SetText("New Renegade Project");
        projectNameInput_.SetCancelInputEnabled(false);
        projectNameInput_.SetTooltip("Name the project, then choose its parent folder");
        projectHubPanel_.AddWidget(&projectNameInput_);

        createProjectButton_.Create("Create Renegade Project");
        createProjectButton_.SetText("CREATE NEW PROJECT");
        createProjectButton_.SetTooltip(
            "Choose a parent folder and create a project from the Proving Ground");
        createProjectButton_.SetAngularHighlightWidth(3.0f);
        createProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            CreateProject();
        });
        projectHubPanel_.AddWidget(&createProjectButton_);

        openProjectButton_.Create("Open Renegade Project");
        openProjectButton_.SetText("OPEN PROJECT...");
        openProjectButton_.SetTooltip(
            "Open an existing Renegade project descriptor (.renegade)");
        openProjectButton_.SetAngularHighlightWidth(3.0f);
        openProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenProject();
        });
        projectHubPanel_.AddWidget(&openProjectButton_);

        // Project Hub is deliberately project-level. OPEN SCENE remains an
        // editor command in the Renegade Studio chrome, but is not presented
        // as a primary startup action here.
        openSceneButton_.Create("Open Renegade Scene");
        openSceneButton_.SetText("OPEN SCENE...");
        openSceneButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenScene();
        });
        openSceneButton_.SetVisible(false);
        projectHubPanel_.AddWidget(&openSceneButton_);

        recentProjectsLabel_.Create("Recent Projects");
        recentProjectsLabel_.SetText("RECENT PROJECTS");
        recentProjectsLabel_.font.params.size = 17;
        recentProjectsLabel_.font.params.bolden = 0.22f;
        recentProjectsLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&recentProjectsLabel_);

        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            auto& button = recentProjectButtons_[index];
            button.Create("Recent Project " + std::to_string(index));
            button.SetText("");
            button.SetAngularHighlightWidth(2.0f);
            button.SetShadowRadius(1.0f);
            button.font.params.size = 14;
            button.font.params.h_align = wi::font::WIFALIGN_LEFT;
            button.font.params.v_align = wi::font::WIFALIGN_CENTER;
            button.OnClick([this, index](const wi::gui::EventArgs&)
            {
                SelectRecentProject(index);
            });
            projectHubPanel_.AddWidget(&button);
        }

        selectedProjectLabel_.Create("Selected Project");
        selectedProjectLabel_.SetText("PROJECT DETAILS");
        selectedProjectLabel_.SetFitTextEnabled(true);
        selectedProjectLabel_.font.params.size = 15;
        selectedProjectLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        selectedProjectLabel_.font.params.v_align = wi::font::WIFALIGN_TOP;
        projectHubPanel_.AddWidget(&selectedProjectLabel_);

        launchProjectButton_.Create("Launch Selected Project");
        launchProjectButton_.SetText("OPEN PROJECT");
        launchProjectButton_.SetAngularHighlightWidth(3.0f);
        launchProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            OpenSelectedRecentProject();
        });
        projectHubPanel_.AddWidget(&launchProjectButton_);

        continueProjectButton_.Create("Continue Current Project");
        continueProjectButton_.SetText("BACK TO EDITOR");
        continueProjectButton_.SetTooltip(
            "Close Project Hub and return to the currently active project");
        continueProjectButton_.SetAngularHighlightWidth(2.0f);
        continueProjectButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            SetProjectHubVisible(false);
        });
        projectHubPanel_.AddWidget(&continueProjectButton_);

        hubMessageLabel_.Create("Project Hub Message");
        hubMessageLabel_.SetText(
            "PROJECT SERVICES // ONLINE     FORMAT // RENEGADE PROJECT V1");
        hubMessageLabel_.SetFitTextEnabled(true);
        hubMessageLabel_.font.params.size = 13;
        hubMessageLabel_.font.params.color = HubMuted;
        hubMessageLabel_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        projectHubPanel_.AddWidget(&hubMessageLabel_);
        projectHubPanel_.SetVisible(false);

        projectLoadingOverlay_.Create();
        projectLoadingOverlay_.OnReturnToHub([this]()
        {
            projectLoadingOverlay_.SetVisible(false);
            RefreshProjectHub();
            SetProjectHubVisible(true);
        });
        GetGUI().AddWidget(&projectLoadingOverlay_);

        projectHubChrome_.Create();
        const auto savedIdentity = StudioUserPreferences::LoadDeveloperIdentity();
        projectHubChrome_.SetDeveloperIdentity(savedIdentity.has_value()
            ? fs::path(*savedIdentity).u8string() : std::string("DEVELOPER"));
        projectHubChrome_.SetStatusProvider([this]() { return hubMessageLabel_.GetText(); });
        projectHubChrome_.OnRecentProjectSelected([this](std::size_t index) { SelectRecentProject(index); });
        projectHubChrome_.OnAction([this](RenegadeProjectHub::Action action)
        {
            switch (action)
            {
            case RenegadeProjectHub::Action::NewProject:
                hubNewProjectMode_ = true;
                projectHubChrome_.SetNewProjectMode(true);
                hubNewProjectNameInput_.SetText("New Renegade Project");
                hubNewProjectNameInput_.SetVisible(true);
                hubNewProjectConfirmButton_.SetVisible(true);
                hubNewProjectCancelButton_.SetVisible(true);
                break;
            case RenegadeProjectHub::Action::OpenProject:
                // Defer the native browser until the next thread-safe point.
                // Calling it directly from the custom Hub input pass can race
                // the GUI/input update and was observed by the owner as a
                // dead OPEN PROJECT control.
                hubMessageLabel_.font.params.color = HologramMuted;
                hubMessageLabel_.SetText("PROJECT BROWSER // OPENING");
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this](uint64_t) { OpenProject(); });
                break;
            case RenegadeProjectHub::Action::OpenSelectedProject: OpenSelectedRecentProject(); break;
            case RenegadeProjectHub::Action::BackToEditor:
                if (session_ && session_->Projects().HasProject()) SetProjectHubVisible(false);
                break;
            case RenegadeProjectHub::Action::ExitRenegade:
                RequestExit();
                break;
            case RenegadeProjectHub::Action::CancelNewProject:
                hubNewProjectMode_ = false;
                projectHubChrome_.SetNewProjectMode(false);
                hubNewProjectNameInput_.SetVisible(false);
                hubNewProjectConfirmButton_.SetVisible(false);
                hubNewProjectCancelButton_.SetVisible(false);
                break;
            }
        });
        projectHubChrome_.SetVisible(projectHubVisible_);

        hubNewProjectNameInput_.Create("Hub New Project Name");
        hubNewProjectNameInput_.SetPlaceholder("PROJECT NAME");
        hubNewProjectNameInput_.SetText("New Renegade Project");
        hubNewProjectNameInput_.SetCancelInputEnabled(false);
        hubNewProjectNameInput_.OnInputAccepted([this](const wi::gui::EventArgs&) { CreateProject(); });
        hubNewProjectNameInput_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectNameInput_);
        hubNewProjectConfirmButton_.Create("Hub Create Project Confirm");
        hubNewProjectConfirmButton_.SetText("CREATE PROJECT");
        hubNewProjectConfirmButton_.OnClick([this](const wi::gui::EventArgs&) { CreateProject(); });
        hubNewProjectConfirmButton_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectConfirmButton_);
        hubNewProjectCancelButton_.Create("Hub Create Project Cancel");
        hubNewProjectCancelButton_.SetText("CANCEL");
        hubNewProjectCancelButton_.OnClick([this](const wi::gui::EventArgs&)
        {
            hubNewProjectMode_ = false;
            projectHubChrome_.SetNewProjectMode(false);
            hubNewProjectNameInput_.SetVisible(false);
            hubNewProjectConfirmButton_.SetVisible(false);
            hubNewProjectCancelButton_.SetVisible(false);
        });
        hubNewProjectCancelButton_.SetVisible(false);
        GetGUI().AddWidget(&hubNewProjectCancelButton_);

        // Wicked GUI renders top-level widgets back-to-front. Register the
        // authored Hub chrome after its native NEW PROJECT controls so the
        // input/CREATE/CANCEL controls render above the modal on their very
        // first visible frame instead of only being promoted after a click.
        GetGUI().AddWidget(&projectHubChrome_);
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
        theme.shadow_highlight_color = XMFLOAT3(0.28f, 0.30f, 0.32f);
        theme.shadow_highlight_spread = 0.18f;
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
            wi::Color(7, 10, 12, 255),
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
            HubBackground,
            wi::gui::WIDGET_ID_WINDOW_BASE);
        projectHubPanel_.SetShadowRadius(0.0f);
        for (auto& sprite : projectHubPanel_.sprites)
        {
            sprite.params.disableCornerRounding();
        }

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
            label.SetColor(HologramIdle);
            label.SetShadowRadius(0.0f);
            label.font.params.color = HologramText;
            label.font.params.bolden = 0.18f;
            label.font.params.shadowColor = wi::Color::Transparent();
        };
        ownLabel(inspectorLabel_);
        ownLabel(positionLabel_);
        ownLabel(rotationLabel_);
        ownLabel(scaleLabel_);
        ownLabel(playerLabel_);
        ownLabel(playerCameraMode_);
        ownLabel(cameraLabel_);
        ownLabel(decalLabel_);
        ownLabel(decalMaterialLabel_);
        ownLabel(materialLabel_);
        ownLabel(materialCoreLabel_);
        ownLabel(materialUvLabel_);
        ownLabel(materialTexturesLabel_);
        ownLabel(materialShaderSpecificLabel_);
        ownLabel(environmentProbeLabel_);
        ownLabel(lightLabel_);
        ownLabel(environmentSkyLabel_);
        ownLabel(environmentFogLabel_);
        ownLabel(environmentCloudLabel_);
        ownLabel(precipitationLabel_);
        ownLabel(sunLabel_);
        ownLabel(oceanLabel_);
        ownLabel(workspaceTitle_);
        ownLabel(statusLabel_);
        ownLabel(hierarchyLabel_);
        ownLabel(terrainLabel_);
        ownLabel(terrainMaterialLabel_);
        ownLabel(terrainSculptLabel_);
        ownLabel(terrainBrushReadout_);
        ownLabel(terrainStrokeDiagnostic_);
        ownLabel(contentLabel_);
        ownLabel(contentPlaceholder_);
        const auto ownHubLabel = [](
            wi::gui::Label& label,
            const wi::Color foreground,
            const wi::Color background = wi::Color::Transparent())
        {
            label.SetColor(background);
            label.SetShadowRadius(0.0f);
            label.font.params.color = foreground;
            label.font.params.shadowColor = wi::Color::Transparent();
        };
        ownHubLabel(hubBrandLabel_, HubOrange);
        ownHubLabel(hubTitleLabel_, HologramText);
        ownHubLabel(hubSubtitleLabel_, HubMuted);
        ownHubLabel(recentProjectsLabel_, HubCyan);
        ownHubLabel(selectedProjectLabel_, HologramText, HubSurfaceRaised);
        ownHubLabel(hubMessageLabel_, HubMuted);

        const auto styleHubButton = [](
            wi::gui::Button& button,
            const wi::Color idle,
            const wi::Color focus,
            const wi::Color active)
        {
            button.SetColor(idle, wi::gui::IDLE);
            button.SetColor(focus, wi::gui::FOCUS);
            button.SetColor(active, wi::gui::ACTIVE);
            button.SetColor(idle, wi::gui::DEACTIVATING);
            button.SetShadowRadius(1.0f);
            button.font.params.color = HologramText;
            button.font.params.shadowColor = wi::Color::Transparent();
        };
        styleHubButton(
            createProjectButton_, HubSurfaceRaised, HubOrange, HubSelected);
        styleHubButton(
            openProjectButton_, HubSurface, HubBorder, HubSelected);
        styleHubButton(
            launchProjectButton_, HubSelected, HubCyan, HubBorder);
        styleHubButton(
            continueProjectButton_, HubSurface, HubBorder, HubSelected);
        for (auto& button : recentProjectButtons_)
        {
            styleHubButton(button, HubSurface, HubBorder, HubSelected);
        }
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
        DiagnosticInputFrame diagnosticInput(diagnosticService_,
            wi::input::Down(wi::input::MOUSE_BUTTON_LEFT), wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT));
        PollTestLevel();
        if (testLevelRuntime_.IsActive())
        {
            diagnosticInput.StopAt("test_level");
            // The child Runtime is the sole 3D owner while Test Level runs.
            // RenderPath2D keeps wiGUI/chrome responsive without ticking the
            // editor scene, visibility, physics, vegetation or render graph.
            wi::RenderPath2D::Update(dt);

            if (pendingAction_ == EditorAction::StopTestLevel)
            {
                ProcessPendingAction();
                return;
            }
            pendingAction_ = EditorAction::None;

            const auto state = testLevelRuntime_.LastResult().state;
            statusLabel_.SetText(
                state == TestLevelProcessState::Running
                    ? "TEST LEVEL // RUNNING // UNSAVED SNAPSHOT"
                    : "TEST LEVEL // STARTING // UNSAVED SNAPSHOT");
            if (session_ != nullptr)
                studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }

        if (!pathTracePreviewActive_ && session_ != nullptr &&
            (!appliedRenderSettingsInitialized_ ||
                appliedRenderSettingsSceneRevision_ !=
                    session_->Scenes().Revision()))
        {
            SyncRenderSettingsFromScene(true);
        }
        if (renderWorkspaceActive_)
        {
            // The Window owns child transforms while it processes scrolling and
            // pointer state. Never relayout its children from the frame loop.
            renderWorkspacePanel_.SetVisible(
                !projectHubVisible_);
            inspectorPanel_.SetVisible(false);
            TickGate8BakeControls();
        }
        // Scene deserialization runs on Wicked's job system. Keep the current
        // document visible but immutable until its prepared replacement is
        // committed at EVENT_THREAD_SAFE_POINT. This check intentionally
        // precedes RenderPath3D::Update(), because wiGUI callbacks can author
        // scene changes from inside the base update.
        if (sceneOpenInProgress_)
        {
            diagnosticInput.StopAt("scene_loading");
            detail::ClearCreatorAssetDragPreview();
            pendingAction_ = EditorAction::None;
            return;
        }

        if (session_ != nullptr)
        {
            bridge::RefreshPrecipitationVisual(session_->Scenes().GetScene());
        }
        if (pathTracePreviewActive_)
        {
            RenderPath3D_PathTracing::Update(dt);
            RefreshPathTracePreviewStatus();
        }
        else
        {
            RenderPath3D::Update(dt);
        }

        if (inspectorRefreshPending_)
        {
            inspectorRefreshPending_ = false;
            RefreshInspector();
        }

        if (session_ == nullptr || projectHubVisible_)
        {
            diagnosticInput.StopAt("project_hub");
            detail::ClearCreatorAssetDragPreview();
            return;
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool playerStartIconConsumed = HandlePlayerStartSceneIcon(pointer);
        const bool cameraIconConsumed = HandleCameraSceneIcons(pointer);
        const bool audioIconConsumed = HandleAudioSceneIcons(pointer);
        const bool decalProbeIconConsumed = HandleDecalProbeSceneIcons(pointer);
        const bool lightIconConsumed = HandleLightSceneIcons(pointer);

        // A vegetation stroke can be released after the pointer has crossed
        // from the viewport onto native GUI or Renegade chrome. Finalize that
        // release before a UI callback, shortcut or ownership check can return
        // from this frame. HandleWd01Vegetation() applies its own focus and
        // viewport guards after completing an in-flight release, so this does
        // not let a brush begin through the UI.
        const bool vegetationConsumed =
            HandleWd01Vegetation(pointer);

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
            diagnosticInput.StopAt("editor_action");
            ProcessPendingAction();
            return;
        }

        // The Renegade-owned shell uses deliberate hit regions rather than
        // stock Wicked widgets. Never let a chrome click fall through into
        // scene selection, gizmo manipulation, or camera navigation.
        if (studioChrome_.ConsumedPointerThisFrame())
        {
            diagnosticInput.StopAt("studio_chrome");
            // Renegade chrome owns this pointer press. Cancel the persistent
            // vegetation tool so a viewport brush can never retain input
            // ownership across top-menu or bottom-drawer interaction.
            DisableWd01VegetationBrush();
            return;
        }

        if (playerStartIconConsumed || cameraIconConsumed || audioIconConsumed ||
            decalProbeIconConsumed || lightIconConsumed)
        {
            diagnosticInput.StopAt("scene_icon");
            return;
        }

        if (vegetationConsumed)
        {
            diagnosticInput.StopAt("vegetation");
            return;
        }

        if (gizmoEntity_ != session_->Selection().SelectedEntity())
        {
            SyncGizmoSelection();
            SyncSelectionOutline();
        }

        if (HandleCreatorAssetPlacement(pointer))
        {
            diagnosticInput.StopAt("asset_placement");
            return;
        }

        if (HandleLightPlacement(pointer))
        {
            diagnosticInput.StopAt("light_placement");
            return;
        }

        HandleViewportNavigation(dt, pointer);
        diagnosticInput.CameraReached(flyCameraActive_);

        if (GetGUI().HasFocus() && !gizmoDragActive_)
        {
            diagnosticInput.StopAt("native_gui_focus");
            return;
        }

        if (gizmoEntity_ != wi::ecs::INVALID_ENTITY &&
            !flyCameraActive_)
        {
            gizmo_.Update(*camera, pointer, *this);
            if (gizmo_.IsInteracting())
            {
                // The controller overwrites gizmo edits, even if inactive.
                bridge::SyncNativeCharacterPoseFromTransform(
                    session_->Scenes().GetScene(), gizmoEntity_);
            }
        }

        if (HandleTerrainSculpt(pointer))
        {
            diagnosticInput.StopAt("terrain_sculpt");
            return;
        }
        if (HandleViewportSelection(pointer))
        {
            diagnosticInput.StopAt("viewport_selection");
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
        if (testLevelRuntime_.IsActive())
        {
            wi::RenderPath2D::Compose(cmd);
            return;
        }
        if (pathTracePreviewActive_)
        {
            RenderPath3D_PathTracing::Compose(cmd);
            return;
        }
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
            outlinedSelection_ != wi::ecs::INVALID_ENTITY &&
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

        if (!projectHubVisible_ &&
            !gizmoSuppressedForCameraView_ &&
            gizmoEntity_ != wi::ecs::INVALID_ENTITY)
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
        projectHubChrome_.SetLayout(width, height);

        projectLoadingOverlay_.SetLayout(width, height);
        const XMFLOAT4 n = projectHubChrome_.NewProjectInputBounds();
        hubNewProjectNameInput_.SetPos(XMFLOAT2(n.x,n.y));
        hubNewProjectNameInput_.SetSize(XMFLOAT2(n.z-n.x,n.w-n.y));
        const XMFLOAT4 c = projectHubChrome_.NewProjectConfirmBounds();
        hubNewProjectConfirmButton_.SetPos(XMFLOAT2(c.x,c.y));
        hubNewProjectConfirmButton_.SetSize(XMFLOAT2(c.z-c.x,c.w-c.y));
        const XMFLOAT4 x = projectHubChrome_.NewProjectCancelBounds();
        hubNewProjectCancelButton_.SetPos(XMFLOAT2(x.x,x.y));
        hubNewProjectCancelButton_.SetSize(XMFLOAT2(x.z-x.x,x.w-x.y));
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
        positionEnvironmentWidget(sceneIdentityLabel_, 224.0f, 20.0f);
        positionEnvironmentWidget(sceneNameInput_, 244.0f);
        positionEnvironmentWidget(sceneLayerLabel_, 282.0f, 20.0f);
        const float layerActionWidth = (environmentFieldWidth - 8.0f) * 0.5f;
        sceneLayerAllButton_.SetPos(XMFLOAT2(12.0f, 302.0f));
        sceneLayerNoneButton_.SetPos(XMFLOAT2(20.0f + layerActionWidth, 302.0f));
        sceneLayerAllButton_.SetSize(XMFLOAT2(layerActionWidth, 28.0f));
        sceneLayerNoneButton_.SetSize(XMFLOAT2(layerActionWidth, 28.0f));
        const float layerBitGap = 4.0f;
        const float layerBitWidth =
            (environmentFieldWidth - layerBitGap * 7.0f) / 8.0f;
        for (std::size_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
        {
            const float x = 12.0f +
                static_cast<float>(bit % 8u) * (layerBitWidth + layerBitGap);
            const float y = 336.0f +
                static_cast<float>(bit / 8u) * 26.0f;
            sceneLayerBits_[bit].SetPos(XMFLOAT2(x, y));
            sceneLayerBits_[bit].SetSize(XMFLOAT2(layerBitWidth, 22.0f));
        }
        positionEnvironmentWidget(sceneMetadataLabel_, 446.0f, 20.0f);
        positionEnvironmentWidget(sceneMetadataPreset_, 466.0f);
        positionEnvironmentWidget(sceneObjectLabel_, 506.0f, 20.0f);
        const float objectToggleWidth = (environmentFieldWidth - 8.0f) * 0.5f;
        const auto layoutObjectToggle = [objectToggleWidth](
            wi::gui::Widget& widget,
            const int column,
            const float y)
        {
            widget.SetPos(XMFLOAT2(
                12.0f + static_cast<float>(column) * (objectToggleWidth + 8.0f),
                y));
            widget.SetSize(XMFLOAT2(objectToggleWidth, 28.0f));
        };
        layoutObjectToggle(sceneObjectRenderable_, 0, 526.0f);
        layoutObjectToggle(sceneObjectCastShadow_, 1, 526.0f);
        layoutObjectToggle(sceneObjectForeground_, 0, 558.0f);
        layoutObjectToggle(sceneObjectMainCamera_, 1, 558.0f);
        layoutObjectToggle(sceneObjectReflections_, 0, 590.0f);
        layoutObjectToggle(sceneObjectWetmap_, 1, 590.0f);
        positionEnvironmentWidget(playerLabel_, 224.0f, 20.0f);
        positionEnvironmentWidget(playerCameraMode_, 244.0f, 32.0f);
        positionEnvironmentWidget(playerCapsuleRadius_, 280.0f);
        positionEnvironmentWidget(playerCapsuleHeight_, 314.0f);
        positionEnvironmentWidget(playerEyeHeight_, 348.0f);
        positionEnvironmentWidget(playerWalkSpeed_, 382.0f);
        positionEnvironmentWidget(playerSprintSpeed_, 416.0f);
        positionEnvironmentWidget(playerJumpSpeed_, 450.0f);
        positionEnvironmentWidget(playerLookSensitivity_, 484.0f);
        positionEnvironmentWidget(playerMaximumSlope_, 518.0f);
        positionEnvironmentWidget(playerGravityFactor_, 552.0f);
        positionEnvironmentWidget(playerMinimumPitch_, 586.0f);
        positionEnvironmentWidget(playerMaximumPitch_, 620.0f);
        LayoutMaterialInspector(environmentFieldWidth);

        positionEnvironmentWidget(cameraLabel_, 506.0f, 20.0f);
        positionEnvironmentWidget(cameraProjection_, 526.0f);
        positionEnvironmentWidget(cameraFieldOfView_, 560.0f);
        positionEnvironmentWidget(cameraNearPlane_, 594.0f);
        positionEnvironmentWidget(cameraFarPlane_, 628.0f);
        positionEnvironmentWidget(cameraFocalLength_, 662.0f);
        positionEnvironmentWidget(cameraApertureSize_, 696.0f);
        positionEnvironmentWidget(cameraOrthoVerticalSize_, 730.0f);
        const float cameraActionWidth = (environmentFieldWidth - 8.0f) * 0.5f;
        cameraAlignToView_.SetPos(XMFLOAT2(12.0f, 764.0f));
        cameraViewFrom_.SetPos(XMFLOAT2(20.0f + cameraActionWidth, 764.0f));
        cameraAlignToView_.SetSize(XMFLOAT2(cameraActionWidth, 28.0f));
        cameraViewFrom_.SetSize(XMFLOAT2(cameraActionWidth, 28.0f));

        positionEnvironmentWidget(decalLabel_, 506.0f, 20.0f);
        positionEnvironmentWidget(decalBaseColorOnlyAlpha_, 528.0f);
        positionEnvironmentWidget(decalSlopeBlend_, 562.0f);
        positionEnvironmentWidget(decalMaterialLabel_, 596.0f, 20.0f);
        positionEnvironmentWidget(decalBaseColorRed_, 620.0f);
        positionEnvironmentWidget(decalBaseColorGreen_, 654.0f);
        positionEnvironmentWidget(decalBaseColorBlue_, 688.0f);
        positionEnvironmentWidget(decalOpacity_, 722.0f);
        positionEnvironmentWidget(decalBaseColorTexture_, 756.0f);

        positionEnvironmentWidget(environmentProbeLabel_, 506.0f, 20.0f);
        positionEnvironmentWidget(environmentProbeResolution_, 530.0f);
        positionEnvironmentWidget(environmentProbeRealtime_, 564.0f);
        positionEnvironmentWidget(environmentProbeInterval_, 598.0f);
        positionEnvironmentWidget(environmentProbeMsaa_, 632.0f);
        positionEnvironmentWidget(environmentProbeViewDistance_, 666.0f);
        positionEnvironmentWidget(environmentProbeRefresh_, 708.0f);

        positionEnvironmentWidget(lightLabel_, 630.0f, 20.0f);
        positionEnvironmentWidget(lightType_, 650.0f);
        positionEnvironmentWidget(lightColorRed_, 684.0f);
        positionEnvironmentWidget(lightColorGreen_, 718.0f);
        positionEnvironmentWidget(lightColorBlue_, 752.0f);
        positionEnvironmentWidget(lightIntensity_, 786.0f);
        positionEnvironmentWidget(lightRange_, 820.0f);
        positionEnvironmentWidget(lightOuterCone_, 854.0f);
        positionEnvironmentWidget(lightInnerCone_, 888.0f);
        positionEnvironmentWidget(lightRadius_, 922.0f);
        positionEnvironmentWidget(lightLength_, 956.0f);
        positionEnvironmentWidget(lightHeight_, 990.0f);
        positionEnvironmentWidget(lightCastShadow_, 1024.0f);
        positionEnvironmentWidget(lightVolumetrics_, 1056.0f);
        positionEnvironmentWidget(lightVolumetricBoost_, 1088.0f);
        positionEnvironmentWidget(environmentSkyLabel_, 44.0f, 20.0f);
        positionEnvironmentWidget(environmentPreset_, 64.0f);
        positionEnvironmentWidget(skyMode_, 98.0f);
        positionEnvironmentWidget(aerialPerspective_, 132.0f);
        positionEnvironmentWidget(skyExposure_, 164.0f);
        positionEnvironmentWidget(stars_, 198.0f);
        positionEnvironmentWidget(ambientIntensity_, 232.0f);
        positionEnvironmentWidget(environmentFogLabel_, 266.0f, 20.0f);
        positionEnvironmentWidget(fogStart_, 286.0f);
        positionEnvironmentWidget(fogDensity_, 320.0f);
        positionEnvironmentWidget(heightFog_, 354.0f);
        positionEnvironmentWidget(fogHeightStart_, 386.0f);
        positionEnvironmentWidget(fogHeightEnd_, 420.0f);
        positionEnvironmentWidget(environmentCloudLabel_, 454.0f, 20.0f);
        positionEnvironmentWidget(cloudCoverage_, 474.0f);
        positionEnvironmentWidget(cloudStartHeight_, 508.0f);
        positionEnvironmentWidget(cloudThickness_, 542.0f);
        positionEnvironmentWidget(cloudsCastShadow_, 576.0f);
        positionEnvironmentWidget(precipitationLabel_, 610.0f, 20.0f);
        positionEnvironmentWidget(precipitationMode_, 630.0f);
        positionEnvironmentWidget(precipitationIntensity_, 664.0f);
        positionEnvironmentWidget(precipitationFallSpeed_, 698.0f);
        positionEnvironmentWidget(precipitationParticleScale_, 732.0f);
        positionEnvironmentWidget(precipitationWindAzimuth_, 766.0f);
        positionEnvironmentWidget(precipitationWindSpeed_, 800.0f);
        positionEnvironmentWidget(precipitationTurbulence_, 834.0f);
        positionEnvironmentWidget(sunLabel_, 868.0f, 20.0f);
        positionEnvironmentWidget(sunPreset_, 888.0f);
        positionEnvironmentWidget(sunTime_, 922.0f);
        positionEnvironmentWidget(sunAzimuth_, 956.0f);
        positionEnvironmentWidget(sunElevation_, 990.0f);
        positionEnvironmentWidget(sunPreviewSpeed_, 1024.0f);
        sunPlayButton_.SetPos(XMFLOAT2(12.0f, 1058.0f));
        sunPauseButton_.SetPos(XMFLOAT2(
            20.0f + (environmentFieldWidth - 8.0f) * 0.5f,
            1058.0f));
        sunPlayButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        sunPauseButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        positionEnvironmentWidget(oceanLabel_, 1094.0f, 20.0f);
        positionEnvironmentWidget(oceanEnabled_, 1114.0f);
        positionEnvironmentWidget(oceanPreset_, 1148.0f);
        positionEnvironmentWidget(oceanResolution_, 1182.0f);
        positionEnvironmentWidget(oceanWaterHeight_, 1216.0f);
        positionEnvironmentWidget(oceanPatchLength_, 1250.0f);
        positionEnvironmentWidget(oceanWaveAmplitude_, 1284.0f);
        positionEnvironmentWidget(oceanChoppyScale_, 1318.0f);
        positionEnvironmentWidget(oceanTimeScale_, 1352.0f);
        positionEnvironmentWidget(oceanWindAzimuth_, 1386.0f);
        positionEnvironmentWidget(oceanWindSpeed_, 1420.0f);
        positionEnvironmentWidget(oceanWindDependency_, 1454.0f);
        positionEnvironmentWidget(oceanSurfaceDetail_, 1488.0f);
        positionEnvironmentWidget(oceanDisplacementTolerance_, 1522.0f);
        positionEnvironmentWidget(oceanWaterRed_, 1556.0f);
        positionEnvironmentWidget(oceanWaterGreen_, 1590.0f);
        positionEnvironmentWidget(oceanWaterBlue_, 1624.0f);
        positionEnvironmentWidget(oceanWaterOpacity_, 1658.0f);
        positionEnvironmentWidget(oceanExtinctionRed_, 1692.0f);
        positionEnvironmentWidget(oceanExtinctionGreen_, 1726.0f);
        positionEnvironmentWidget(oceanExtinctionBlue_, 1760.0f);

        positionEnvironmentWidget(terrainLabel_, 44.0f, 20.0f);
        positionEnvironmentWidget(createTerrainButton_, 64.0f);
        positionEnvironmentWidget(terrainSizeReadout_, 64.0f, 20.0f);
        positionEnvironmentWidget(expandTerrainButton_, 88.0f);
        positionEnvironmentWidget(terrainChunkScale_, 122.0f);
        positionEnvironmentWidget(terrainMinimumHeight_, 156.0f);
        positionEnvironmentWidget(terrainMaximumHeight_, 190.0f);
        positionEnvironmentWidget(terrainLowAltitudeBlend_, 224.0f);
        positionEnvironmentWidget(terrainBaseBlend_, 258.0f);
        positionEnvironmentWidget(terrainSlopeBlend_, 292.0f);
        positionEnvironmentWidget(terrainLodBias_, 326.0f);
        positionEnvironmentWidget(terrainMaterialLabel_, 370.0f, 20.0f);
        positionEnvironmentWidget(terrainMaterialPreset_, 390.0f);
        positionEnvironmentWidget(terrainTextureScale_, 424.0f);
        terrainApplyDefaultGrassButton_.SetPos(XMFLOAT2(12.0f, 458.0f));
        terrainReloadMaterialButton_.SetPos(XMFLOAT2(
            20.0f + (environmentFieldWidth - 8.0f) * 0.5f,
            458.0f));
        terrainApplyDefaultGrassButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        terrainReloadMaterialButton_.SetSize(XMFLOAT2(
            (environmentFieldWidth - 8.0f) * 0.5f,
            28.0f));
        positionEnvironmentWidget(terrainSculptLabel_, 500.0f, 20.0f);
        positionEnvironmentWidget(terrainSculptMode_, 520.0f);
        positionEnvironmentWidget(terrainBrushRadius_, 554.0f);
        positionEnvironmentWidget(terrainBrushStrength_, 588.0f);
        positionEnvironmentWidget(terrainBrushFalloff_, 622.0f);
        positionEnvironmentWidget(terrainBrushReadout_, 656.0f, 20.0f);
        positionEnvironmentWidget(terrainStrokeDiagnostic_, 676.0f, 20.0f);
        LayoutWd01VegetationControls(environmentFieldWidth);

        const bool environmentSelected =
            environmentWorkspaceActive_;
        const bool terrainSelected = terrainWorkspaceActive_;
        const bool lightSelected =
            session_ != nullptr && !environmentSelected && !terrainSelected &&
            session_->Scenes().GetScene().lights.Contains(
                session_->Selection().SelectedEntity());
        const bool playerStartSelected =
            session_ != nullptr && !environmentSelected && !terrainSelected &&
            bridge::IsPlayerStart(
                session_->Scenes().GetScene(),
                session_->Selection().SelectedEntity());
        LayoutInspectorActions(
            environmentSelected,
            terrainSelected,
            lightSelected,
            false,
            playerStartSelected);
        LayoutS1BInspectorSections();

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

        const float hubMargin = std::clamp(width * 0.025f, 24.0f, 40.0f);
        const float hubGap = 18.0f;
        const float hubHeaderHeight = 112.0f;
        const float hubStatusHeight = 34.0f;
        const float hubContentTop = hubMargin + hubHeaderHeight;
        const float hubStatusY = std::max(
            hubContentTop + 280.0f,
            height - hubMargin - hubStatusHeight);
        const bool hubCompact = width < 1080.0f;
        const float hubLeftWidth = std::clamp(width * 0.205f, 230.0f, 300.0f);
        const float hubRightWidth = hubCompact
            ? hubLeftWidth
            : std::clamp(width * 0.27f, 310.0f, 410.0f);
        const float hubMainX = hubMargin + hubLeftWidth + hubGap;
        const float hubRightX = hubCompact
            ? hubMargin
            : width - hubMargin - hubRightWidth;
        const float hubMainWidth = std::max(
            220.0f,
            (hubCompact ? width - hubMargin : hubRightX) - hubGap - hubMainX);

        projectHubPanel_.SetPos(XMFLOAT2(0.0f, 0.0f));
        projectHubPanel_.SetSize(XMFLOAT2(width, height));
        hubBrandLabel_.SetPos(XMFLOAT2(hubMargin, hubMargin));
        hubBrandLabel_.SetSize(XMFLOAT2(280.0f, 22.0f));
        hubTitleLabel_.SetPos(XMFLOAT2(hubMargin, hubMargin + 24.0f));
        hubTitleLabel_.SetSize(XMFLOAT2(520.0f, 48.0f));
        hubSubtitleLabel_.SetPos(XMFLOAT2(hubMargin, hubMargin + 75.0f));
        hubSubtitleLabel_.SetSize(XMFLOAT2(width - hubMargin * 2.0f, 26.0f));

        projectNameInput_.SetPos(XMFLOAT2(hubMargin, hubContentTop + 30.0f));
        projectNameInput_.SetSize(XMFLOAT2(hubLeftWidth, 38.0f));
        createProjectButton_.SetPos(XMFLOAT2(hubMargin, hubContentTop + 82.0f));
        createProjectButton_.SetSize(XMFLOAT2(hubLeftWidth, 46.0f));
        openProjectButton_.SetPos(XMFLOAT2(hubMargin, hubContentTop + 140.0f));
        openProjectButton_.SetSize(XMFLOAT2(hubLeftWidth, 42.0f));
        continueProjectButton_.SetPos(XMFLOAT2(hubMargin, hubContentTop + 194.0f));
        continueProjectButton_.SetSize(XMFLOAT2(hubLeftWidth, 40.0f));
        openSceneButton_.SetVisible(false);
        openSceneButton_.SetPos(XMFLOAT2(0.0f, 0.0f));
        openSceneButton_.SetSize(XMFLOAT2(0.0f, 0.0f));

        recentProjectsLabel_.SetPos(XMFLOAT2(hubMainX, hubContentTop));
        recentProjectsLabel_.SetSize(XMFLOAT2(hubMainWidth, 26.0f));
        const float hubCardsTop = hubContentTop + 38.0f;
        const std::size_t hubColumns = hubMainWidth >= 430.0f ? 2u : 1u;
        const std::size_t hubRows =
            (recentProjectButtons_.size() + hubColumns - 1u) / hubColumns;
        const float hubCardGap = 10.0f;
        const float hubCardsAvailable = std::max(
            180.0f,
            hubStatusY - hubCardsTop - 14.0f);
        const float hubCardHeight = std::clamp(
            (hubCardsAvailable - hubCardGap * static_cast<float>(hubRows - 1u)) /
                static_cast<float>(hubRows),
            38.0f,
            68.0f);
        const float hubCardWidth =
            (hubMainWidth - hubCardGap * static_cast<float>(hubColumns - 1u)) /
            static_cast<float>(hubColumns);
        for (std::size_t index = 0; index < recentProjectButtons_.size(); ++index)
        {
            const std::size_t column = index % hubColumns;
            const std::size_t row = index / hubColumns;
            recentProjectButtons_[index].SetPos(XMFLOAT2(
                hubMainX + static_cast<float>(column) * (hubCardWidth + hubCardGap),
                hubCardsTop + static_cast<float>(row) * (hubCardHeight + hubCardGap)));
            recentProjectButtons_[index].SetSize(XMFLOAT2(hubCardWidth, hubCardHeight));
        }

        const float hubDetailsY = hubCompact
            ? hubContentTop + 252.0f
            : hubContentTop;
        const float hubDetailsHeight = hubCompact
            ? std::max(110.0f, hubStatusY - hubDetailsY - 66.0f)
            : std::clamp(height * 0.36f, 210.0f, 300.0f);
        selectedProjectLabel_.SetPos(XMFLOAT2(hubRightX, hubDetailsY));
        selectedProjectLabel_.SetSize(XMFLOAT2(hubRightWidth, hubDetailsHeight));
        launchProjectButton_.SetPos(XMFLOAT2(
            hubRightX,
            hubDetailsY + hubDetailsHeight + 12.0f));
        launchProjectButton_.SetSize(XMFLOAT2(hubRightWidth, 46.0f));

        hubMessageLabel_.SetPos(XMFLOAT2(hubMargin, hubStatusY));
        hubMessageLabel_.SetSize(XMFLOAT2(
            width - hubMargin * 2.0f,
            hubStatusHeight));

        // Layout the Render window only when the Studio layout itself changes.
        // Repositioning Window children every frame breaks Wicked GUI mouse/scroll ownership.
        if (renderWorkspaceActive_)
            LayoutRenderWorkspace();

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
            diagnosticService_.Record(bridge::DiagnosticSeverity::Error, "studio", "session.unavailable", "Studio session unavailable");
            statusLabel_.SetText("ENGINEBRIDGE SESSION UNAVAILABLE");
            return;
        }

        const auto& scenes = session_->Scenes();
        if (sceneOpenInProgress_)
        {
            const std::string openingName = wi::helper::GetFileNameFromPath(
                openingScenePath_);
            statusLabel_.SetText("OPENING SCENE // " + openingName);
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }
        if (!scenes.LastError().empty())
        {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Error, "scene", "scene.error", scenes.LastError());
            statusLabel_.SetText("SCENE ERROR // " + scenes.LastError());
            studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }

        if (!session_->Documents().LastWarning().empty())
        {
            diagnosticService_.Record(bridge::DiagnosticSeverity::Warning, "scene", "scene.warning", session_->Documents().LastWarning());
            statusLabel_.SetText(
                "SCENE WARNING // " + session_->Documents().LastWarning());
            studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
            studioChrome_.SetStatusText(statusLabel_.GetText());
            return;
        }

        if (lightPlacementActive_)
        {
            statusLabel_.SetText(
                std::string("PLACE ") +
                PlacementLightName(lightPlacementType_) +
                " LIGHT // LEFT CLICK SURFACE // ESC OR RIGHT CLICK CANCEL");
            studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
            studioChrome_.SetStatusText(statusLabel_.GetText());
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
        std::string sceneName = projectName;
        if (!scenes.CurrentPath().empty())
        {
            sceneName = wi::helper::RemoveExtension(
                wi::helper::GetFileNameFromPath(scenes.CurrentPath()));
        }
        studioChrome_.SetSceneName(sceneName);
        studioChrome_.SetSceneDirty(session_->Commands().IsDirty());
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
        const auto& scene = session_->Scenes().GetScene();
        for (const auto& entity : session_->Scenes().ListEntities())
        {
            // A broken Gate 5 build could serialize Wicked's fallback Weather
            // onto the Terrain entity. Hide only the dedicated Environment
            // carrier; legacy dual-role terrain must remain discoverable.
            if (entity.entity == weatherEntity &&
                !scene.terrains.Contains(entity.entity))
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
                entity.logicalAsset,
                ToHierarchyCategory(entity.category),
            });
        }
        studioChrome_.SetHierarchyRows(std::move(chromeRows));
    }

    void StudioRenderPath::LayoutInspectorActions(
        const bool environment,
        const bool terrain,
        const bool light,
        const bool sceneCamera,
        const bool playerStart)
    {
        const float width = inspectorPanel_.GetSize().x;
        constexpr float gap = 8.0f;
        const float threeButtonWidth = (width - 40.0f) / 3.0f;
        const float twoButtonWidth = (width - 32.0f) / 2.0f;
        const float actionStart = environment
            ? std::max(1772.0f, inspectorPanel_.GetSize().y - 82.0f)
            : terrain
                ? 1340.0f
                : light
                    ? 1130.0f
                    : sceneCamera
                        ? 804.0f
                        : playerStart
                            ? 670.0f
                        : materialInspectorVisible_
                            ? std::max(630.0f, materialInspectorBottom_ + 16.0f)
                            : 630.0f;
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

    void StudioRenderPath::QueueInspectorRefresh() noexcept
    {
        inspectorRefreshPending_ = true;
    }

    void StudioRenderPath::RefreshInspector()
    {
        diagnosticService_.SetState("inspector_refresh", {{"elapsed_ms", diagnosticService_.ElapsedMs()},
            {"stage", std::string("refresh_entered")}});
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
        // Terrain generation in an older blank Level can leave Weather on the
        // Terrain entity. Terrain mode must still resolve only Terrain controls;
        // Environment owns Weather presentation in its dedicated workspace.
        auto* weather = hasSession && !terrainWorkspaceActive_
            ? session_->Scenes().GetScene().weathers.GetComponent(entity)
            : nullptr;
        auto* terrain = hasSession && !environmentWorkspaceActive_
            ? session_->Scenes().GetScene().terrains.GetComponent(entity)
            : nullptr;
        auto* light = hasSession && !environmentWorkspaceActive_ &&
            !terrainWorkspaceActive_
            ? session_->Scenes().GetScene().lights.GetComponent(entity)
            : nullptr;
        auto* authoredCamera = hasSession && !environmentWorkspaceActive_ &&
            !terrainWorkspaceActive_
            ? session_->Scenes().GetScene().cameras.GetComponent(entity)
            : nullptr;
        auto* decal = hasSession && !environmentWorkspaceActive_ &&
            !terrainWorkspaceActive_
            ? session_->Scenes().GetScene().decals.GetComponent(entity)
            : nullptr;
        auto* environmentProbe = hasSession && !environmentWorkspaceActive_ &&
            !terrainWorkspaceActive_
            ? session_->Scenes().GetScene().probes.GetComponent(entity)
            : nullptr;
        auto* decalMaterial = hasSession && decal != nullptr
            ? session_->Scenes().GetScene().materials.GetComponent(entity)
            : nullptr;
        SyncSelectionOutline();

        const bool hasTransform = transform != nullptr;
        const bool hasWeather = weather != nullptr;
        const bool hasTerrain = terrain != nullptr;
        const bool hasLight = light != nullptr;
        const bool hasCamera = authoredCamera != nullptr;
        const bool hasDecal = decal != nullptr;
        const bool hasEnvironmentProbe = environmentProbe != nullptr;
        const bool hasPlayerStart = hasSession &&
            !environmentWorkspaceActive_ && !terrainWorkspaceActive_ &&
            bridge::IsPlayerStart(
                session_->Scenes().GetScene(), selectedEntity);
        const bool sceneComponentsVisible =
            hasSession && selectedEntity != wi::ecs::INVALID_ENTITY &&
            !environmentWorkspaceActive_ && !terrainWorkspaceActive_ &&
            !hasWeather && !hasTerrain && !hasPlayerStart;
        wi::ecs::Entity sceneAuthoringRoot = wi::ecs::INVALID_ENTITY;
        bridge::SceneLayerMaskState sceneLayerState;
        bridge::ObjectParticipationState objectRenderableState;
        bridge::ObjectParticipationState objectCastShadowState;
        bridge::ObjectParticipationState objectForegroundState;
        bridge::ObjectParticipationState objectMainCameraState;
        bridge::ObjectParticipationState objectReflectionsState;
        bridge::ObjectParticipationState objectWetmapState;
        if (sceneComponentsVisible)
        {
            const auto& currentScene = session_->Scenes().GetScene();
            sceneAuthoringRoot = bridge::ResolveSceneComponentAuthoringRoot(
                currentScene, selectedEntity);
            sceneLayerState = bridge::InspectSceneLayerMask(
                currentScene, selectedEntity);
            objectRenderableState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::Renderable);
            objectCastShadowState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::CastShadow);
            objectForegroundState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::Foreground);
            objectMainCameraState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::VisibleInMainCamera);
            objectReflectionsState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::VisibleInReflections);
            objectWetmapState = bridge::InspectObjectParticipation(
                currentScene, selectedEntity,
                bridge::ObjectParticipationProperty::Wetmap);
        }
        const bool hasObjectTargets = objectRenderableState.targetCount > 0;
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
        LayoutInspectorActions(
            hasWeather, hasTerrain, hasLight,
            hasCamera || hasDecal || hasEnvironmentProbe,
            hasPlayerStart);

        sceneIdentityLabel_.SetVisible(sceneComponentsVisible);
        sceneNameInput_.SetVisible(sceneComponentsVisible);
        sceneLayerLabel_.SetVisible(sceneComponentsVisible);
        sceneLayerAllButton_.SetVisible(sceneComponentsVisible);
        sceneLayerNoneButton_.SetVisible(sceneComponentsVisible);
        for (auto& bit : sceneLayerBits_)
            bit.SetVisible(sceneComponentsVisible);
        sceneMetadataLabel_.SetVisible(sceneComponentsVisible);
        sceneMetadataPreset_.SetVisible(sceneComponentsVisible);
        sceneObjectLabel_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectRenderable_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectCastShadow_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectForeground_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectMainCamera_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectReflections_.SetVisible(sceneComponentsVisible && hasObjectTargets);
        sceneObjectWetmap_.SetVisible(sceneComponentsVisible && hasObjectTargets);

        RefreshMaterialInspector(
            sceneComponentsVisible && !hasCamera && !hasDecal &&
                !hasEnvironmentProbe && !hasLight,
            selectedEntity);
        LayoutInspectorActions(
            hasWeather, hasTerrain, hasLight,
            hasCamera || hasDecal || hasEnvironmentProbe,
            hasPlayerStart);

        const auto setPlayerVisible = [hasPlayerStart](wi::gui::Widget& widget)
        {
            widget.SetVisible(hasPlayerStart);
        };
        setPlayerVisible(playerLabel_);
        setPlayerVisible(playerCameraMode_);
        setPlayerVisible(playerCapsuleRadius_);
        setPlayerVisible(playerCapsuleHeight_);
        setPlayerVisible(playerEyeHeight_);
        setPlayerVisible(playerWalkSpeed_);
        setPlayerVisible(playerSprintSpeed_);
        setPlayerVisible(playerJumpSpeed_);
        setPlayerVisible(playerLookSensitivity_);
        setPlayerVisible(playerMaximumSlope_);
        setPlayerVisible(playerGravityFactor_);
        setPlayerVisible(playerMinimumPitch_);
        setPlayerVisible(playerMaximumPitch_);
        if (hasPlayerStart)
        {
            const auto settings = bridge::CapturePlayerControllerSettings(
                session_->Scenes().GetScene(), selectedEntity);
            playerCapsuleRadius_.SetValue(settings.capsuleRadius);
            playerCapsuleHeight_.SetValue(
                bridge::PlayerCapsuleTotalHeight(settings));
            playerEyeHeight_.SetValue(settings.eyeHeight);
            playerWalkSpeed_.SetValue(settings.walkSpeed);
            playerSprintSpeed_.SetValue(settings.sprintSpeed);
            playerJumpSpeed_.SetValue(settings.jumpSpeed);
            playerLookSensitivity_.SetValue(settings.lookSensitivity);
            playerMaximumSlope_.SetValue(settings.maximumSlopeDegrees);
            playerGravityFactor_.SetValue(settings.gravityFactor);
            playerMinimumPitch_.SetValue(
                settings.minimumPitch / XM_PI * 180.0f);
            playerMaximumPitch_.SetValue(
                settings.maximumPitch / XM_PI * 180.0f);
        }

        cameraLabel_.SetVisible(hasCamera);
        cameraProjection_.SetVisible(hasCamera);
        cameraFieldOfView_.SetVisible(hasCamera);
        cameraNearPlane_.SetVisible(hasCamera);
        cameraFarPlane_.SetVisible(hasCamera);
        cameraFocalLength_.SetVisible(hasCamera);
        cameraApertureSize_.SetVisible(hasCamera);
        cameraOrthoVerticalSize_.SetVisible(hasCamera);
        cameraAlignToView_.SetVisible(hasCamera);
        cameraViewFrom_.SetVisible(hasCamera);

        decalLabel_.SetVisible(hasDecal);
        decalBaseColorOnlyAlpha_.SetVisible(hasDecal);
        decalSlopeBlend_.SetVisible(hasDecal);
        decalMaterialLabel_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalBaseColorRed_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalBaseColorGreen_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalBaseColorBlue_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalOpacity_.SetVisible(hasDecal && decalMaterial != nullptr);
        decalBaseColorTexture_.SetVisible(hasDecal && decalMaterial != nullptr);
        if (hasDecal && decalMaterial != nullptr)
        {
            decalBaseColorTexture_.SetText(
                decalMaterial->textures[wi::scene::MaterialComponent::BASECOLORMAP]
                        .resource.IsValid()
                    ? "CHANGE DECAL TEXTURE..."
                    : "SELECT DECAL TEXTURE...");
        }
        if (hasDecal)
        {
            const auto state = bridge::CaptureDecal(*decal);
            decalBaseColorOnlyAlpha_.SetCheck(state.baseColorOnlyAlpha);
            decalSlopeBlend_.SetValue(state.slopeBlendPower);
            if (decalMaterial != nullptr)
            {
                const auto material = bridge::CaptureMaterial(*decalMaterial);
                decalBaseColorRed_.SetValue(material.baseColor.x);
                decalBaseColorGreen_.SetValue(material.baseColor.y);
                decalBaseColorBlue_.SetValue(material.baseColor.z);
                decalOpacity_.SetValue(material.baseColor.w);
            }
        }

        environmentProbeLabel_.SetVisible(hasEnvironmentProbe);
        environmentProbeResolution_.SetVisible(hasEnvironmentProbe);
        environmentProbeRealtime_.SetVisible(hasEnvironmentProbe);
        environmentProbeInterval_.SetVisible(hasEnvironmentProbe);
        environmentProbeMsaa_.SetVisible(hasEnvironmentProbe);
        environmentProbeViewDistance_.SetVisible(hasEnvironmentProbe);
        environmentProbeRefresh_.SetVisible(hasEnvironmentProbe);
        if (hasEnvironmentProbe)
        {
            const auto state = bridge::CaptureEnvironmentProbe(*environmentProbe);
            environmentProbeResolution_.SetSelectedByUserdataWithoutCallback(
                state.resolution);
            environmentProbeRealtime_.SetCheck(state.realTime);
            environmentProbeInterval_.SetValue(state.updateInterval);
            environmentProbeMsaa_.SetCheck(state.msaa);
            environmentProbeViewDistance_.SetValue(state.viewDistance);
        }

        if (hasCamera)
        {
            const auto cameraState = bridge::CaptureCamera(*authoredCamera);
            cameraProjection_.SetSelectedByUserdataWithoutCallback(
                cameraState.orthographic ? 1u : 0u);
            cameraFieldOfView_.SetValue(cameraState.fieldOfViewDegrees);
            cameraNearPlane_.SetValue(cameraState.nearPlane);
            cameraFarPlane_.SetValue(cameraState.farPlane);
            cameraFocalLength_.SetValue(cameraState.focalLength);
            cameraApertureSize_.SetValue(cameraState.apertureSize);
            cameraOrthoVerticalSize_.SetValue(cameraState.orthoVerticalSize);
            cameraFieldOfView_.SetEnabled(!cameraState.orthographic);
            cameraOrthoVerticalSize_.SetEnabled(cameraState.orthographic);
        }

        if (sceneComponentsVisible && sceneAuthoringRoot != wi::ecs::INVALID_ENTITY)
        {
            const auto& currentScene = session_->Scenes().GetScene();
            const auto* sceneName = currentScene.names.GetComponent(sceneAuthoringRoot);
            sceneNameInput_.SetValue(sceneName != nullptr ? sceneName->name : std::string{});
            sceneLayerLabel_.SetText(sceneLayerState.mixed
                ? "LAYERS // MIXED // EDITING PRESERVES OTHER BITS"
                : "LAYERS // 32-BIT MASK");
            for (std::uint32_t bit = 0; bit < sceneLayerBits_.size(); ++bit)
            {
                sceneLayerBits_[bit].SetCheck(
                    (sceneLayerState.mask & (std::uint32_t{1} << bit)) != 0u);
            }
            const auto* metadata = currentScene.metadatas.GetComponent(sceneAuthoringRoot);
            sceneMetadataPreset_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(metadata != nullptr
                    ? metadata->preset
                    : wi::scene::MetadataComponent::Preset::Custom));
            const bool objectMixed = objectRenderableState.mixed ||
                objectCastShadowState.mixed || objectForegroundState.mixed ||
                objectMainCameraState.mixed || objectReflectionsState.mixed ||
                objectWetmapState.mixed;
            sceneObjectLabel_.SetText(objectMixed
                ? "OBJECT // MIXED // WHOLE-ASSET EDIT"
                : "OBJECT // RENDER PARTICIPATION");
            sceneObjectRenderable_.SetCheck(objectRenderableState.value);
            sceneObjectCastShadow_.SetCheck(objectCastShadowState.value);
            sceneObjectForeground_.SetCheck(objectForegroundState.value);
            sceneObjectMainCamera_.SetCheck(objectMainCameraState.value);
            sceneObjectReflections_.SetCheck(objectReflectionsState.value);
            sceneObjectWetmap_.SetCheck(objectWetmapState.value);
        }

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
        if (hasPlayerStart)
        {
            rotationLabel_.SetText("ROTATION // Y CAMERA HEADING");
            rotationX_.SetVisible(false);
            rotationZ_.SetVisible(false);
            scaleLabel_.SetVisible(false);
            scaleX_.SetVisible(false);
            scaleY_.SetVisible(false);
            scaleZ_.SetVisible(false);
        }
        else
        {
            rotationLabel_.SetText("ROTATION // DEGREES");
        }

        const auto setLightVisible = [hasLight](wi::gui::Widget& widget)
        {
            widget.SetVisible(hasLight);
        };
        setLightVisible(lightLabel_);
        setLightVisible(lightType_);
        setLightVisible(lightColorRed_);
        setLightVisible(lightColorGreen_);
        setLightVisible(lightColorBlue_);
        setLightVisible(lightIntensity_);
        setLightVisible(lightRange_);
        setLightVisible(lightOuterCone_);
        setLightVisible(lightInnerCone_);
        setLightVisible(lightRadius_);
        setLightVisible(lightLength_);
        setLightVisible(lightHeight_);
        setLightVisible(lightCastShadow_);
        setLightVisible(lightVolumetrics_);
        setLightVisible(lightVolumetricBoost_);

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
        setEnvironmentVisible(stars_);
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
        setTerrainVisible(terrainSizeReadout_);
        setTerrainVisible(expandTerrainButton_);
        setTerrainVisible(terrainChunkScale_);
        setTerrainVisible(terrainMinimumHeight_);
        setTerrainVisible(terrainMaximumHeight_);
        setTerrainVisible(terrainLowAltitudeBlend_);
        setTerrainVisible(terrainBaseBlend_);
        setTerrainVisible(terrainSlopeBlend_);
        setTerrainVisible(terrainLodBias_);
        setTerrainVisible(terrainMaterialLabel_);
        setTerrainVisible(terrainMaterialPreset_);
        setTerrainVisible(terrainTextureScale_);
        setTerrainVisible(terrainApplyDefaultGrassButton_);
        setTerrainVisible(terrainReloadMaterialButton_);
        setTerrainVisible(terrainSculptLabel_);
        setTerrainVisible(terrainSculptMode_);
        setTerrainVisible(terrainBrushRadius_);
        setTerrainVisible(terrainBrushStrength_);
        setTerrainVisible(terrainBrushFalloff_);
        setTerrainVisible(terrainBrushReadout_);
        setTerrainVisible(terrainStrokeDiagnostic_);
        RefreshWd01VegetationControls(hasTerrain);

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
        duplicateButton_.SetEnabled(
            hasTransform && !hasTerrain && !hasPlayerStart);
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
            stars_.SetValue(state.stars);
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
            LayoutS1BInspectorSections();
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
            const float terrainWidthKm = bridge::TerrainWidthMeters(
                state.visibleChunkRadius,
                state.chunkScale) / 1000.0f;
            std::ostringstream terrainSize;
            terrainSize << "CURRENT TERRAIN // " << std::fixed
                        << std::setprecision(2) << terrainWidthKm
                        << " KM x " << terrainWidthKm << " KM";
            terrainSizeReadout_.SetText(terrainSize.str());
            expandTerrainButton_.SetEnabled(
                !state.centerToCamera && !state.removeDistantChunks &&
                state.visibleChunkRadius < bridge::MaximumTerrainChunkRadius);
            terrainChunkScale_.SetValue(state.chunkScale);
            terrainMinimumHeight_.SetValue(state.minimumHeight);
            terrainMaximumHeight_.SetValue(state.maximumHeight);
            terrainLowAltitudeBlend_.SetValue(state.lowAltitudeBlend);
            terrainBaseBlend_.SetValue(state.baseBlend);
            terrainSlopeBlend_.SetValue(state.slopeBlend);
            terrainLodBias_.SetValue(state.lodBias);
            const float textureScale = bridge::CaptureTerrainTextureScale(
                session_->Scenes().GetScene(),
                *terrain);
            terrainTextureScale_.SetValue(textureScale);
            int materialPreset = 0;
            for (int presetIndex = 0; presetIndex < 3; ++presetIndex)
            {
                const auto preset = static_cast<bridge::TerrainMaterialPreset>(
                    presetIndex);
                if (std::abs(
                        textureScale -
                        bridge::MakeTerrainMaterialPreset(preset)) < 0.01f)
                {
                    materialPreset = presetIndex + 1;
                    break;
                }
            }
            terrainMaterialPreset_.SetSelectedWithoutCallback(materialPreset);
            std::ostringstream brush;
            brush << "BRUSH // SIZE " << std::fixed << std::setprecision(0)
                  << terrainBrushRadiusValue_ << " // STRENGTH "
                  << std::setprecision(2) << terrainBrushStrengthValue_;
            terrainBrushReadout_.SetText(brush.str());
            SyncGizmoSelection();
            LayoutS1BInspectorSections();
            return;
        }

        if (hasLight)
        {
            const auto* name =
                session_->Scenes().GetScene().names.GetComponent(entity);
            inspectorLabel_.SetText(
                "LIGHT // " +
                (name != nullptr && !name->name.empty()
                    ? name->name
                    : "ENTITY " + std::to_string(entity)));
            const auto state = bridge::CaptureLight(*light);
            lightType_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(state.type));
            lightColorRed_.SetValue(state.color.x);
            lightColorGreen_.SetValue(state.color.y);
            lightColorBlue_.SetValue(state.color.z);
            lightIntensity_.SetValue(state.intensity);
            lightRange_.SetValue(state.range);
            lightOuterCone_.SetValue(state.outerConeDegrees);
            lightInnerCone_.SetValue(state.innerConeDegrees);
            lightRadius_.SetValue(state.radius);
            lightLength_.SetValue(state.length);
            lightHeight_.SetValue(state.height);
            lightCastShadow_.SetCheck(state.castShadow);
            lightVolumetrics_.SetCheck(state.volumetrics);
            lightVolumetricBoost_.SetValue(state.volumetricBoost);

            const bool directional = state.type ==
                wi::scene::LightComponent::DIRECTIONAL;
            const bool point = state.type ==
                wi::scene::LightComponent::POINT;
            const bool spot = state.type ==
                wi::scene::LightComponent::SPOT;
            const bool rectangle = state.type ==
                wi::scene::LightComponent::RECTANGLE;
            lightRange_.SetEnabled(!directional);
            lightOuterCone_.SetVisible(spot);
            lightInnerCone_.SetVisible(spot);
            lightRadius_.SetVisible(directional || point || spot);
            lightLength_.SetVisible(point || rectangle);
            lightHeight_.SetVisible(rectangle);
            lightVolumetricBoost_.SetEnabled(state.volumetrics);
        }

        if (!hasTransform)
        {
            if (!hasLight)
            {
                inspectorLabel_.SetText(terrainWorkspaceActive_
                    ? "TERRAIN // CREATE OR EDIT LANDSCAPE"
                    : "TRANSFORM // SELECT AN ENTITY");
            }
            translationX_.SetValue(0.0f);
            translationY_.SetValue(0.0f);
            translationZ_.SetValue(0.0f);
            rotationX_.SetValue(0.0f);
            rotationY_.SetValue(0.0f);
            rotationZ_.SetValue(0.0f);
            scaleX_.SetValue(1.0f);
            scaleY_.SetValue(1.0f);
            scaleZ_.SetValue(1.0f);
            LayoutS1BInspectorSections();
            return;
        }

        const auto* name =
            session_->Scenes().GetScene().names.GetComponent(entity);
        if (hasPlayerStart)
        {
            inspectorLabel_.SetText("PLAYER START // SPAWN + CONTROLLER");
        }
        else if (!hasLight)
        {
            inspectorLabel_.SetText(
                "TRANSFORM // " +
                (name != nullptr && !name->name.empty()
                    ? name->name
                    : "ENTITY " + std::to_string(entity)));
        }
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
        LayoutS1BInspectorSections();
        SyncGizmoSelection();
    }

    void StudioRenderPath::HandleEditorShortcuts()
    {
        if (projectHubVisible_ ||
            flyCameraActive_ ||
            GetGUI().IsTyping() ||
            pendingAction_ != EditorAction::None ||
            gizmoDragActive_ ||
            weatherSliderActive_ ||
            precipitationSliderActive_ ||
            sunSliderActive_ ||
            oceanSliderActive_ ||
            lightSliderActive_ ||
            materialSliderActive_ ||
            lightPlacementActive_ ||
            terrainSliderActive_ ||
            terrainTextureScaleActive_ ||
            terrainStrokeActive_)
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
            RequestDiagnosticAction(EditorAction::Undo);
        }
        else if (control && wi::input::Press(key('Y')))
        {
            RequestDiagnosticAction(EditorAction::Redo);
        }
        else if (control && wi::input::Press(key('D')))
        {
            RequestDiagnosticAction(EditorAction::DuplicateSelection);
        }
        else if (control && wi::input::Press(key('S')))
        {
            pendingAction_ = shift
                ? EditorAction::SaveSceneAs
                : EditorAction::SaveScene;
        }
        else if (wi::input::Press(wi::input::KEYBOARD_BUTTON_DELETE))
        {
            RequestDiagnosticAction(EditorAction::DeleteSelection);
        }
        else if (wi::input::Press(key('F')))
        {
            RequestDiagnosticAction(EditorAction::FocusSelection);
        }
        else if (wi::input::Press(key('W')))
        {
            RequestDiagnosticAction(EditorAction::TranslateTool);
        }
        else if (wi::input::Press(key('E')))
        {
            RequestDiagnosticAction(EditorAction::RotateTool);
        }
        else if (wi::input::Press(key('R')))
        {
            RequestDiagnosticAction(EditorAction::ScaleTool);
        }
        else if (wi::input::Press(key('G')))
        {
            RequestDiagnosticAction(EditorAction::ToggleGrid);
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
        TraceDiagnosticAction(pendingAction_, session_ ? "handler_entered" : "blocked.scene_unavailable");
        if (session_ == nullptr)
        {
            pendingAction_ = EditorAction::None;
            return;
        }

        const EditorAction action = pendingAction_;
        pendingAction_ = EditorAction::None;
        if (lightPlacementActive_ && action != EditorAction::CreateLight)
        {
            CancelLightPlacement();
        }
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
        case EditorAction::CreateLight:
            CreateLight(pendingLightType_);
            break;
        case EditorAction::CreatePlayerStart:
            CreatePlayerStartFromView();
            break;
        case EditorAction::CreateCamera:
            CreateCameraFromView();
            break;
        case EditorAction::CreateDecal:
            CreateDecalFromView();
            break;
        case EditorAction::CreateEnvironmentProbe:
            CreateEnvironmentProbeFromView();
            break;
        case EditorAction::OpenScene:
            OpenScene();
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
            SetRenderWorkspaceActive(false);
            SetTransformTool(TransformTool::Select);
            break;
        case EditorAction::TranslateTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetRenderWorkspaceActive(false);
            SetTransformTool(TransformTool::Translate);
            break;
        case EditorAction::RotateTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetRenderWorkspaceActive(false);
            SetTransformTool(TransformTool::Rotate);
            break;
        case EditorAction::ScaleTool:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetRenderWorkspaceActive(false);
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
        case EditorAction::OpenRenderWorkspace:
            SetRenderWorkspaceActive(true);
            break;
        case EditorAction::OpenSceneWorkspace:
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetRenderWorkspaceActive(false);
            break;
        case EditorAction::StartTestLevel:
            StartTestLevel();
            break;
        case EditorAction::StartProjectPlay:
            StartProjectPlay();
            break;
        case EditorAction::StopTestLevel:
            StopTestLevel();
            break;
        case EditorAction::BuildWindowsGame:
            RequestWindowsGameBuild();
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
        case EditorAction::ExpandTerrain:
            ExpandTerrain();
            break;
        case EditorAction::ApplyTerrainMaterialPreset:
            ApplyTerrainMaterialPreset(pendingTerrainMaterialPreset_);
            break;
        case EditorAction::ApplyDefaultGrass:
            ApplyDefaultGrass();
            break;
        case EditorAction::ReloadTerrainMaterial:
            ReloadTerrainMaterial();
            break;
        case EditorAction::None:
        default:
            break;
        }
        TraceDiagnosticAction(action, "handler_returned");
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
        if (active && renderWorkspaceActive_)
            SetRenderWorkspaceActive(false);
        if (!active && sunPreviewPlaying_)
        {
            StopSunPreview(true);
        }
        if (active && session_ != nullptr &&
            session_->Scenes().WeatherEntity() == wi::ecs::INVALID_ENTITY)
        {
            const auto environmentState = bridge::CaptureWeather(
                session_->Scenes().GetScene().weather);
            session_->Commands().Execute(
                std::make_unique<bridge::CreateEnvironmentCommand>(
                    session_->Scenes().GetScene(),
                    environmentState,
                    "Environment"));
        }
        if (active && session_ != nullptr)
        {
            auto& scene = session_->Scenes().GetScene();
            const auto weatherEntity = session_->Scenes().WeatherEntity();
            if (weatherEntity != wi::ecs::INVALID_ENTITY &&
                bridge::FindPrimarySunLight(scene) ==
                    wi::ecs::INVALID_ENTITY)
            {
                session_->Commands().Execute(
                    std::make_unique<bridge::CreateSunCommand>(
                        scene,
                        weatherEntity));
            }
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
        if (active && renderWorkspaceActive_)
            SetRenderWorkspaceActive(false);
        if (sunPreviewPlaying_)
        {
            StopSunPreview(true);
        }
        terrainWorkspaceActive_ = active && session_ != nullptr;
        if (terrainWorkspaceActive_)
        {
            environmentWorkspaceActive_ = false;
        }
        else
        {
            DisableWd01VegetationBrush();
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
        wi::primitive::AABB aggregateBounds;
        bool hasBounds = false;
        // The selected logical Character/asset root is usually a transform
        // wrapper, not a mesh. Frame its complete rendered hierarchy rather
        // than just the wrapper's origin or an arbitrary single child mesh.
        for (std::size_t index = 0;
             index < scene.objects.GetCount() && index < scene.aabb_objects.size();
             ++index)
        {
            const auto candidate = scene.objects.GetEntity(index);
            if (candidate != entity && !scene.Entity_IsDescendant(candidate, entity))
                continue;
            const auto& objectBounds = scene.aabb_objects[index];
            const XMFLOAT3 minimum = objectBounds.getMin();
            const XMFLOAT3 maximum = objectBounds.getMax();
            if (!std::isfinite(minimum.x) || !std::isfinite(minimum.y) ||
                !std::isfinite(minimum.z) || !std::isfinite(maximum.x) ||
                !std::isfinite(maximum.y) || !std::isfinite(maximum.z) ||
                minimum.x > maximum.x || minimum.y > maximum.y ||
                minimum.z > maximum.z)
            {
                continue;
            }
            aggregateBounds = hasBounds
                ? wi::primitive::AABB::Merge(aggregateBounds, objectBounds)
                : objectBounds;
            hasBounds = true;
        }
        if (hasBounds)
        {
            const XMFLOAT3 boundsCenter = aggregateBounds.getCenter();
            center = XMLoadFloat3(&boundsCenter);
            distance = std::max(2.5f, aggregateBounds.getRadius() * 2.5f);
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

    void StudioRenderPath::CreateDecalFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto transform = CaptureEditorCameraTransform();
        const XMVECTOR forward = XMVector3Rotate(
            XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
            XMLoadFloat4(&transform.rotation));
        XMFLOAT3 direction = {};
        XMStoreFloat3(&direction, XMVector3Normalize(forward));
        transform.translation.x += direction.x * 4.0f;
        transform.translation.y += direction.y * 4.0f;
        transform.translation.z += direction.z * 4.0f;
        transform.scale = XMFLOAT3(1.5f, 1.5f, 0.5f);

        auto command = std::make_unique<bridge::CreateDecalCommand>(
            session_->Scenes().GetScene(), bridge::DecalState{}, transform);
        auto* created = command.get();
        if (session_->Commands().Execute(std::move(command)))
        {
            session_->Selection().Select(created->CreatedEntity());
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::ChooseSelectedDecalTexture()
    {
        if (session_ == nullptr || !session_->Projects().HasProject() ||
            wi::jobsystem::IsBusy(decalTextureImportWorkload_))
        {
            return;
        }

        const wi::ecs::Entity decalEntity =
            session_->Selection().SelectedEntity();
        if (!session_->Scenes().GetScene().decals.Contains(decalEntity))
            return;

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Projected decal base-colour / alpha texture";
        params.extensions = {
            "png", "tga", "dds", "jpg", "jpeg", "bmp", "hdr"};
        wi::helper::FileDialog(
            params,
            [this, decalEntity](const std::string& sourcePath)
            {
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, decalEntity, sourcePath](std::uint64_t)
                    {
                        if (sourcePath.empty() || session_ == nullptr ||
                            !session_->Projects().HasProject() ||
                            wi::jobsystem::IsBusy(decalTextureImportWorkload_))
                        {
                            return;
                        }

                        auto& scene = session_->Scenes().GetScene();
                        if (!scene.decals.Contains(decalEntity))
                        {
                            studioChrome_.SetStatusText(
                                "DECAL TEXTURE // TARGET NO LONGER EXISTS");
                            return;
                        }

                        const bridge::ResourceSourceFormat format =
                            bridge::DetectResourceSourceFormat(sourcePath);
                        if (format == bridge::ResourceSourceFormat::Unknown ||
                            bridge::ClassifyResourceSourceFormat(format) !=
                                bridge::ResourceClass::Texture)
                        {
                            studioChrome_.SetStatusText(
                                "DECAL TEXTURE // UNSUPPORTED IMAGE FORMAT");
                            ShowStudioMessageBox(
                                "Choose a supported image texture (PNG, TGA, DDS, JPG/JPEG, BMP or HDR).",
                                "Select Decal Texture");
                            return;
                        }

                        struct DecalTextureImportState
                        {
                            std::string projectRoot;
                            bridge::StableId projectId;
                            wi::ecs::Entity decalEntity = wi::ecs::INVALID_ENTITY;
                            std::string sourcePath;
                            bridge::CreatorTextureImportResult imported;
                        };

                        auto state = std::make_shared<DecalTextureImportState>();
                        const auto& project =
                            session_->Projects().CurrentProject();
                        state->projectRoot = project.rootPath;
                        state->projectId = project.projectId;
                        state->decalEntity = decalEntity;
                        state->sourcePath = sourcePath;
                        studioChrome_.SetStatusText(
                            "DECAL TEXTURE // IMPORTING + REGISTERING // " +
                            fs::u8path(sourcePath).filename().generic_u8string());

                        wi::jobsystem::Execute(
                            decalTextureImportWorkload_,
                            [this, state](wi::jobsystem::JobArgs)
                            {
                                bridge::CreatorTextureWorkflowService workflow;
                                state->imported = workflow.ImportTexture(
                                    state->projectRoot,
                                    state->projectId,
                                    state->sourcePath);

                                wi::eventhandler::Subscribe_Once(
                                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                                    [this, state](std::uint64_t)
                                    {
                                        if (!state->imported.succeeded)
                                        {
                                            const std::string prefix =
                                                state->imported.committed
                                                    ? "DECAL TEXTURE // IMPORT COMMITTED // VERIFY FAILED // "
                                                    : "DECAL TEXTURE // IMPORT FAILED // ";
                                            studioChrome_.SetStatusText(
                                                prefix + state->imported.error);
                                            ShowStudioMessageBox(
                                                "Could not prepare the selected decal texture.\n\nReason: " +
                                                    state->imported.error,
                                                "Select Decal Texture");
                                            return;
                                        }

                                        if (session_ == nullptr ||
                                            !session_->Projects().HasProject() ||
                                            session_->Projects().CurrentProject().projectId !=
                                                state->projectId)
                                        {
                                            return;
                                        }

                                        auto& currentScene =
                                            session_->Scenes().GetScene();
                                        if (!currentScene.decals.Contains(
                                                state->decalEntity))
                                        {
                                            studioChrome_.SetStatusText(
                                                "DECAL TEXTURE // IMPORTED // TARGET NO LONGER EXISTS");
                                            RefreshAssetBrowser();
                                            return;
                                        }

                                        const wi::ecs::Entity materialEntity =
                                            bridge::ResolveEditableMaterialEntity(
                                                currentScene,
                                                state->decalEntity);
                                        if (materialEntity ==
                                            wi::ecs::INVALID_ENTITY)
                                        {
                                            studioChrome_.SetStatusText(
                                                "DECAL TEXTURE // IMPORTED // DECAL MATERIAL MISSING");
                                            RefreshAssetBrowser();
                                            return;
                                        }

                                        bridge::PreparedMaterialTextureAsset prepared;
                                        std::string error;
                                        if (!bridge::PrepareMaterialTextureAsset(
                                                state->projectRoot,
                                                state->projectId,
                                                state->imported.assetId,
                                                prepared,
                                                error))
                                        {
                                            studioChrome_.SetStatusText(
                                                "DECAL TEXTURE // IMPORTED // PREPARE FAILED // " +
                                                error);
                                            RefreshAssetBrowser();
                                            return;
                                        }

                                        auto command = std::make_unique<
                                            bridge::SetMaterialBaseColorTextureAssetCommand>(
                                                currentScene,
                                                materialEntity,
                                                std::move(prepared));
                                        if (!session_->Commands().Execute(
                                                std::move(command)))
                                        {
                                            studioChrome_.SetStatusText(
                                                "DECAL TEXTURE // IMPORTED // ASSIGN FAILED");
                                            RefreshAssetBrowser();
                                            return;
                                        }

                                        studioChrome_.SetSceneDirty(
                                            session_->Commands().IsDirty());
                                        RefreshAssetBrowser();
                                        RefreshInspector();
                                        RefreshStatus();
                                        studioChrome_.SetStatusText(
                                            "DECAL TEXTURE // GOVERNED + ASSIGNED // " +
                                            fs::u8path(state->sourcePath)
                                                .filename().generic_u8string());
                                    });
                            });
                    });
            });
    }

    void StudioRenderPath::CreateEnvironmentProbeFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto transform = CaptureEditorCameraTransform();
        const XMVECTOR forward = XMVector3Rotate(
            XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
            XMLoadFloat4(&transform.rotation));
        XMFLOAT3 direction = {};
        XMStoreFloat3(&direction, XMVector3Normalize(forward));
        transform.translation.x += direction.x * 5.0f;
        transform.translation.y += direction.y * 5.0f;
        transform.translation.z += direction.z * 5.0f;
        transform.rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        transform.scale = XMFLOAT3(5.0f, 5.0f, 5.0f);

        auto command = std::make_unique<bridge::CreateEnvironmentProbeCommand>(
            session_->Scenes().GetScene(),
            bridge::EnvironmentProbeState{}, transform);
        auto* created = command.get();
        if (session_->Commands().Execute(std::move(command)))
        {
            session_->Selection().Select(created->CreatedEntity());
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
    }

    bool StudioRenderPath::CommitSelectedDecal(
        const bridge::DecalState& state)
    {
        if (session_ == nullptr)
            return false;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.decals.Contains(entity))
            return false;
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetDecalCommand>(scene, entity, state));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    bool StudioRenderPath::CommitSelectedEnvironmentProbe(
        const bridge::EnvironmentProbeState& state)
    {
        if (session_ == nullptr)
            return false;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.probes.Contains(entity))
            return false;
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetEnvironmentProbeCommand>(
                scene, entity, state));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::CreateLight(
        const wi::scene::LightComponent::LightType type)
    {
        if (session_ == nullptr || camera == nullptr)
        {
            return;
        }

        StopSunPreview(true);
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);

        lightPlacementActive_ = false;
        if (type != wi::scene::LightComponent::DIRECTIONAL)
        {
            lightPlacementType_ = type;
            lightPlacementActive_ = true;
            ClearSelectionOutline();
            RefreshStatus();
            return;
        }

        XMFLOAT3 position = camera->Eye;
        position.x += camera->At.x * 5.0f;
        position.y += camera->At.y * 5.0f;
        position.z += camera->At.z * 5.0f;

        PlaceLight(type, position);
    }

    void StudioRenderPath::PlaceLight(
        const wi::scene::LightComponent::LightType type,
        const XMFLOAT3& position,
        const XMFLOAT4& rotation)
    {
        if (session_ == nullptr)
        {
            return;
        }

        ClearSelectionOutline();
        auto command = std::make_unique<bridge::CreateLightCommand>(
            session_->Scenes().GetScene(),
            type,
            position,
            rotation);
        auto* createCommand = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            SyncSelectionOutline();
            return;
        }

        lightPlacementActive_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        session_->Selection().Select(createCommand->CreatedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CancelLightPlacement()
    {
        if (!lightPlacementActive_)
        {
            return;
        }
        lightPlacementActive_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        RefreshStatus();
    }

    void StudioRenderPath::BeginCreatorAssetPlacement(
        const bridge::StableId& assetId,
        const std::string& label)
    {
        if (session_ == nullptr || !bridge::IsValidStableId(assetId))
            return;
        CancelLightPlacement();
        creatorAssetPlacementActive_ = true;
        creatorAssetPlacementId_ = assetId;
        creatorAssetPlacementLabel_ = label;
        creatorAssetDropPending_ = false;
        studioChrome_.SetActiveBottomTab(-1, true);
        studioChrome_.SetStatusText(
            "PLACE ASSET // CLICK A SURFACE // ESC OR RMB TO CANCEL");
    }

    void StudioRenderPath::DropCreatorAsset(
        const bridge::StableId& assetId,
        const std::string& label,
        const float screenX,
        const float screenY)
    {
        if (detail::CreatorAssetDragPreviewOwnsDrop(assetId))
        {
            // The live cursor instance is committed by the Studio update in
            // this exact release frame. Do not create a second placement path.
            return;
        }

        // Chrome has already consumed the release event by the time Studio
        // reaches its drag-preview update. Preserve the stable asset identity
        // and release point so a background preparation that is still finishing
        // can complete the same drop instead of cancelling it as "not ready".
        detail::QueueCreatorAssetDrop(assetId, label, screenX, screenY);
    }

    void StudioRenderPath::CancelCreatorAssetPlacement()
    {
        if (!creatorAssetPlacementActive_)
            return;
        creatorAssetPlacementActive_ = false;
        creatorAssetPlacementId_.clear();
        creatorAssetPlacementLabel_.clear();
        creatorAssetDropPending_ = false;
        detail::ClearCreatorAssetDragPreview();
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        studioChrome_.SetStatusText("PLACE ASSET // CANCELLED");
    }

    bool StudioRenderPath::HandleCreatorAssetPlacement(
        const XMFLOAT4& pointer)
    {
        if (!creatorAssetPlacementActive_ || session_ == nullptr)
            return false;

        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE) ||
            wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
        {
            CancelCreatorAssetPlacement();
            return true;
        }

        if (flyCameraActive_ ||
            GetGUI().HasFocus() ||
            !IsPointerOverViewport(pointer))
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return true;
        }

        auto& scene = session_->Scenes().GetScene();
        const auto ray = wi::renderer::GetPickRay(
            static_cast<long>(pointer.x),
            static_cast<long>(pointer.y),
            *this,
            *camera);
        const auto picked = wi::scene::Pick(
            ray,
            wi::enums::FILTER_OBJECT_ALL | wi::enums::FILTER_TERRAIN,
            ~0u,
            scene);
        XMFLOAT3 surfacePosition = picked.position;
        bool hasSurface = picked.entity != wi::ecs::INVALID_ENTITY;
        if (!hasSurface && std::abs(ray.direction.y) > 0.0001f)
        {
            const float distance = -ray.origin.y / ray.direction.y;
            if (distance >= ray.TMin && distance <= ray.TMax)
            {
                surfacePosition = XMFLOAT3(
                    ray.origin.x + ray.direction.x * distance,
                    0.0f,
                    ray.origin.z + ray.direction.z * distance);
                hasSurface = true;
            }
        }
        if (!hasSurface)
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return true;
        }

        wi::input::SetCursor(wi::input::CURSOR_CROSS);
        wi::renderer::DrawSphere(
            wi::primitive::Sphere(surfacePosition, 0.16f),
            XMFLOAT4(1.0f, 0.36f, 0.06f, 0.9f),
            false);
        if (!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            return true;

        const auto& project = session_->Projects().CurrentProject();
        bridge::CreatorAssetWorkflowService workflow;
        auto prepared = workflow.PrepareModelPlacement(
            project.rootPath,
            project.projectId,
            creatorAssetPlacementId_);
        if (!prepared.IsReady())
        {
            studioChrome_.SetStatusText(
                "PLACE ASSET // PREPARE FAILED // " +
                prepared.Result().error);
            return true;
        }

        const wi::scene::Scene* preparedScene = prepared.PeekScene();
        const float scale = bridge::HasCreatorAuthoredTransform(*preparedScene)
            ? 1.0f
            : bridge::ImportService::ResolveScaleFactor(
                bridge::ModelScaleMode::Automatic,
                *preparedScene);
        const bridge::ModelBounds bounds =
            bridge::ImportService::MeasureModelBounds(*preparedScene);
        XMFLOAT3 position = surfacePosition;
        if (bounds.valid)
        {
            position.y = bridge::ImportService::ResolveGroundedPlacementY(
                surfacePosition.y,
                bounds,
                scale);
        }

        const bridge::StableId assetId = creatorAssetPlacementId_;
        auto command = std::make_unique<bridge::PlaceReusableModelCommand>(
            scene,
            prepared.ReleaseScene(),
            assetId,
            position,
            scale,
            creatorAssetPlacementLabel_);
        auto* placed = command.get();
        if (!session_->Commands().Execute(std::move(command)))
        {
            studioChrome_.SetStatusText("PLACE ASSET // FAILED");
            return true;
        }

        const std::string label = creatorAssetPlacementLabel_;
        creatorAssetPlacementActive_ = false;
        creatorAssetPlacementId_.clear();
        creatorAssetPlacementLabel_.clear();
        creatorAssetDropPending_ = false;
        wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
        session_->Selection().Select(placed->PlacedEntity());
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        studioChrome_.SetStatusText(
            "PLACE ASSET // " + label +
            " // SURFACE GROUNDED // STABLE RASSET INSTANCE");
        return true;
    }

    bool StudioRenderPath::HandleLightPlacement(const XMFLOAT4& pointer)
    {
        if (!lightPlacementActive_ || session_ == nullptr)
        {
            return false;
        }

        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE) ||
            wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))
        {
            CancelLightPlacement();
            return true;
        }

        if (flyCameraActive_ || GetGUI().HasFocus() ||
            !IsPointerOverViewport(pointer))
        {
            wi::input::SetCursor(wi::input::CURSOR_DEFAULT);
            return false;
        }

        const auto ray = wi::renderer::GetPickRay(
            static_cast<long>(pointer.x),
            static_cast<long>(pointer.y),
            *this,
            *camera);
        const auto picked = wi::scene::Pick(
            ray,
            wi::enums::FILTER_OBJECT_ALL,
            ~0u,
            session_->Scenes().GetScene());
        if (picked.entity == wi::ecs::INVALID_ENTITY)
        {
            wi::input::SetCursor(wi::input::CURSOR_NOTALLOWED);
            return true;
        }

        wi::input::SetCursor(wi::input::CURSOR_CROSS);
        wi::renderer::DrawSphere(
            wi::primitive::Sphere(picked.position, 0.18f),
            XMFLOAT4(0.20f, 0.92f, 1.0f, 0.90f),
            false);
        wi::renderer::RenderableLine normal;
        normal.start = picked.position;
        XMStoreFloat3(
            &normal.end,
            XMLoadFloat3(&picked.position) +
                XMLoadFloat3(&picked.normal) * 0.8f);
        normal.color_start = XMFLOAT4(0.20f, 0.92f, 1.0f, 0.95f);
        normal.color_end = XMFLOAT4(1.0f, 0.55f, 0.15f, 0.95f);
        wi::renderer::DrawLine(normal, false);

        if (!wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            return true;
        }

        XMFLOAT4 rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        if (lightPlacementType_ == wi::scene::LightComponent::SPOT)
        {
            rotation = RotationFromTo(XMFLOAT3(0, 1, 0), picked.normal);
        }
        else if (lightPlacementType_ == wi::scene::LightComponent::RECTANGLE)
        {
            rotation = RotationFromTo(XMFLOAT3(0, 0, -1), picked.normal);
        }
        PlaceLight(lightPlacementType_, picked.position, rotation);
        return true;
    }

    bool StudioRenderPath::ProjectEditorPoint(
        const XMFLOAT3& world,
        XMFLOAT2& screen) const noexcept
    {
        if (camera == nullptr)
        {
            return false;
        }
        const XMVECTOR clip = XMVector4Transform(
            XMVectorSet(world.x, world.y, world.z, 1.0f),
            camera->GetViewProjection());
        const float w = XMVectorGetW(clip);
        if (w <= 0.001f)
        {
            return false;
        }
        const XMVECTOR ndc = clip / w;
        const float z = XMVectorGetZ(ndc);
        if (z < 0.0f || z > 1.0f)
        {
            return false;
        }
        screen.x = (XMVectorGetX(ndc) * 0.5f + 0.5f) * GetLogicalWidth();
        screen.y = (-XMVectorGetY(ndc) * 0.5f + 0.5f) * GetLogicalHeight();
        return IsPointerOverViewport(XMFLOAT4(screen.x, screen.y, 0, 0));
    }


bool StudioRenderPath::HandleAudioSceneIcons(
    const XMFLOAT4& pointer)
{
    if (session_ == nullptr || camera == nullptr || projectHubVisible_)
    {
        return false;
    }

    auto& scene = session_->Scenes().GetScene();
    const auto selected = session_->Selection().SelectedEntity();
    const bool canSelect = !lightPlacementActive_ && !flyCameraActive_ &&
        !GetGUI().HasFocus() && !gizmo_.IsInteracting() &&
        IsPointerOverViewport(pointer) &&
        wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
    wi::ecs::Entity best = wi::ecs::INVALID_ENTITY;
    float bestDistanceSquared = 24.0f * 24.0f;

    const auto drawCircle = [this](
        const XMFLOAT2& center,
        const float radius,
        const XMFLOAT4& color)
    {
        constexpr int Segments = 18;
        for (int segment = 0; segment < Segments; ++segment)
        {
            const float a0 = XM_2PI * static_cast<float>(segment) / Segments;
            const float a1 = XM_2PI * static_cast<float>(segment + 1) / Segments;
            DrawEditorLine(
                XMFLOAT2(center.x + std::cos(a0) * radius,
                    center.y + std::sin(a0) * radius),
                XMFLOAT2(center.x + std::cos(a1) * radius,
                    center.y + std::sin(a1) * radius),
                color);
        }
    };

    for (std::size_t index = 0; index < scene.sounds.GetCount(); ++index)
    {
        const auto entity = scene.sounds.GetEntity(index);
        if (!bridge::IsRenegadeSoundSource(scene, entity) ||
            !session_->Scenes().IsHierarchyVisible(entity))
        {
            continue;
        }
        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            continue;
        const auto source = bridge::CaptureSoundSource(scene, entity);
        const XMFLOAT3 position = transform->GetPosition();
        XMFLOAT2 center = {};
        if (!ProjectEditorPoint(position, center))
            continue;

        XMFLOAT4 baseColor;
        switch (source.bus)
        {
        case bridge::AudioBus::Music:
            baseColor = XMFLOAT4(0.78f, 0.42f, 1.0f, 0.96f);
            break;
        case bridge::AudioBus::Ambience:
            baseColor = XMFLOAT4(0.25f, 0.92f, 0.62f, 0.96f);
            break;
        case bridge::AudioBus::Voice:
            baseColor = XMFLOAT4(0.35f, 0.78f, 1.0f, 0.96f);
            break;
        case bridge::AudioBus::SoundEffect:
        default:
            baseColor = XMFLOAT4(1.0f, 0.55f, 0.15f, 0.96f);
            break;
        }
        const float dx = pointer.x - center.x;
        const float dy = pointer.y - center.y;
        const float distanceSquared = dx * dx + dy * dy;
        const bool hovered = distanceSquared <= 24.0f * 24.0f;
        const XMFLOAT4 color = entity == selected
            ? XMFLOAT4(1.0f, 0.88f, 0.42f, 1.0f)
            : hovered
                ? XMFLOAT4(0.70f, 0.96f, 1.0f, 1.0f)
                : baseColor;

        // Bus-specific editor-only glyphs remain a constant readable size.
        if (source.bus == bridge::AudioBus::SoundEffect)
        {
            DrawEditorLine(XMFLOAT2(center.x - 10, center.y - 5),
                XMFLOAT2(center.x - 4, center.y - 5), color);
            DrawEditorLine(XMFLOAT2(center.x - 10, center.y + 5),
                XMFLOAT2(center.x - 4, center.y + 5), color);
            DrawEditorLine(XMFLOAT2(center.x - 10, center.y - 5),
                XMFLOAT2(center.x - 10, center.y + 5), color);
            DrawEditorLine(XMFLOAT2(center.x - 4, center.y - 5),
                XMFLOAT2(center.x + 4, center.y - 11), color);
            DrawEditorLine(XMFLOAT2(center.x - 4, center.y + 5),
                XMFLOAT2(center.x + 4, center.y + 11), color);
            DrawEditorLine(XMFLOAT2(center.x + 4, center.y - 11),
                XMFLOAT2(center.x + 4, center.y + 11), color);
            drawCircle(XMFLOAT2(center.x + 4, center.y), 14.0f, color);
        }
        else if (source.bus == bridge::AudioBus::Music)
        {
            drawCircle(XMFLOAT2(center.x - 5, center.y + 8), 5.0f, color);
            DrawEditorLine(XMFLOAT2(center.x, center.y + 8),
                XMFLOAT2(center.x, center.y - 12), color);
            DrawEditorLine(XMFLOAT2(center.x, center.y - 12),
                XMFLOAT2(center.x + 11, center.y - 9), color);
            DrawEditorLine(XMFLOAT2(center.x + 11, center.y - 9),
                XMFLOAT2(center.x + 11, center.y + 2), color);
            drawCircle(XMFLOAT2(center.x + 6, center.y + 3), 5.0f, color);
        }
        else if (source.bus == bridge::AudioBus::Ambience)
        {
            drawCircle(center, 6.0f, color);
            drawCircle(center, 12.0f, color);
            drawCircle(center, 18.0f, color);
        }
        else
        {
            DrawEditorLine(XMFLOAT2(center.x - 6, center.y - 10),
                XMFLOAT2(center.x + 6, center.y - 10), color);
            DrawEditorLine(XMFLOAT2(center.x - 6, center.y - 10),
                XMFLOAT2(center.x - 6, center.y + 3), color);
            DrawEditorLine(XMFLOAT2(center.x + 6, center.y - 10),
                XMFLOAT2(center.x + 6, center.y + 3), color);
            drawCircle(XMFLOAT2(center.x, center.y + 3), 6.0f, color);
            DrawEditorLine(XMFLOAT2(center.x, center.y + 9),
                XMFLOAT2(center.x, center.y + 16), color);
        }

        if (canSelect && distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            best = entity;
        }
    }

    if (canSelect && best != wi::ecs::INVALID_ENTITY)
    {
        session_->Selection().Select(best);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        SyncGizmoSelection();
        SyncSelectionOutline();
        return true;
    }
    return false;
}

bool StudioRenderPath::HandleDecalProbeSceneIcons(
    const XMFLOAT4& pointer)
{
    if (session_ == nullptr || camera == nullptr || projectHubVisible_)
    {
        return false;
    }

    auto& scene = session_->Scenes().GetScene();
    const bool canSelect = !lightPlacementActive_ && !flyCameraActive_ &&
        !GetGUI().HasFocus() && !gizmo_.IsInteracting() &&
        IsPointerOverViewport(pointer) &&
        wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
    wi::ecs::Entity best = wi::ecs::INVALID_ENTITY;
    float bestDistanceSquared = 22.0f * 22.0f;
    const auto selected = session_->Selection().SelectedEntity();

    const auto drawVolume = [this](
        const wi::scene::TransformComponent& transform,
        const XMFLOAT4& color)
    {
        constexpr XMFLOAT3 local[8] = {
            XMFLOAT3(-1, -1, -1), XMFLOAT3(1, -1, -1),
            XMFLOAT3(1, 1, -1), XMFLOAT3(-1, 1, -1),
            XMFLOAT3(-1, -1, 1), XMFLOAT3(1, -1, 1),
            XMFLOAT3(1, 1, 1), XMFLOAT3(-1, 1, 1)};
        constexpr int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}};
        XMFLOAT2 projected[8] = {};
        bool visible[8] = {};
        const XMMATRIX world = transform.GetWorldMatrix();
        for (int index = 0; index < 8; ++index)
        {
            XMFLOAT3 worldPoint = {};
            XMStoreFloat3(
                &worldPoint,
                XMVector3TransformCoord(XMLoadFloat3(&local[index]), world));
            visible[index] = ProjectEditorPoint(worldPoint, projected[index]);
        }
        for (const auto& edge : edges)
        {
            if (visible[edge[0]] && visible[edge[1]])
                DrawEditorLine(projected[edge[0]], projected[edge[1]], color);
        }
    };

    const auto drawEntity = [&](
        const wi::ecs::Entity entity,
        const bool probe)
    {
        if (!session_->Scenes().IsHierarchyVisible(entity))
            return;
        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            return;
        XMFLOAT2 center = {};
        if (!ProjectEditorPoint(transform->GetPosition(), center))
            return;
        const XMFLOAT4 color = probe
            ? XMFLOAT4(0.20f, 0.92f, 1.0f, 0.95f)
            : XMFLOAT4(1.0f, 0.48f, 0.10f, 0.95f);
        constexpr float radius = 8.0f;
        if (probe)
        {
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y - radius),
                XMFLOAT2(center.x + radius, center.y - radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius, center.y - radius),
                XMFLOAT2(center.x + radius, center.y + radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius, center.y + radius),
                XMFLOAT2(center.x - radius, center.y + radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y + radius),
                XMFLOAT2(center.x - radius, center.y - radius), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius, center.y),
                XMFLOAT2(center.x + radius, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x, center.y - radius),
                XMFLOAT2(center.x, center.y + radius), color);
        }
        else
        {
            DrawEditorLine(
                XMFLOAT2(center.x, center.y - radius - 3.0f),
                XMFLOAT2(center.x + radius + 3.0f, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x + radius + 3.0f, center.y),
                XMFLOAT2(center.x, center.y + radius + 3.0f), color);
            DrawEditorLine(
                XMFLOAT2(center.x, center.y + radius + 3.0f),
                XMFLOAT2(center.x - radius - 3.0f, center.y), color);
            DrawEditorLine(
                XMFLOAT2(center.x - radius - 3.0f, center.y),
                XMFLOAT2(center.x, center.y - radius - 3.0f), color);
        }
        if (selected == entity)
        {
            drawVolume(
                *transform,
                XMFLOAT4(color.x, color.y, color.z, 0.72f));
        }
        if (canSelect)
        {
            const float dx = pointer.x - center.x;
            const float dy = pointer.y - center.y;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                best = entity;
            }
        }
    };

    for (std::size_t index = 0; index < scene.decals.GetCount(); ++index)
        drawEntity(scene.decals.GetEntity(index), false);
    for (std::size_t index = 0; index < scene.probes.GetCount(); ++index)
        drawEntity(scene.probes.GetEntity(index), true);

    if (canSelect && best != wi::ecs::INVALID_ENTITY)
    {
        session_->Selection().Select(best);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
    }
    return false;
}

bool StudioRenderPath::HandlePlayerStartSceneIcon(
    const XMFLOAT4& pointer)
{
    if (session_ == nullptr || camera == nullptr || projectHubVisible_)
    {
        return false;
    }

    auto& scene = session_->Scenes().GetScene();
    const auto resolved = bridge::ResolvePlayerStart(scene);
    if (resolved.resolution != bridge::PlayerStartResolution::Success ||
        !session_->Scenes().IsHierarchyVisible(resolved.start.entity))
    {
        return false;
    }
    const auto* transform = scene.transforms.GetComponent(resolved.start.entity);
    if (transform == nullptr)
        return false;

    const XMFLOAT3 feet = transform->GetPosition();
    const XMFLOAT3 euler = wi::math::QuaternionToRollPitchYaw(
        resolved.start.transform.rotation);
    const XMVECTOR forwardVector = XMVectorSet(
        std::sin(euler.y), 0.0f, std::cos(euler.y), 0.0f);
    const XMVECTOR rightVector = XMVectorSet(
        std::cos(euler.y), 0.0f, -std::sin(euler.y), 0.0f);
    const XMVECTOR origin = XMLoadFloat3(&feet) + XMVectorSet(0, 0.035f, 0, 0);
    const auto worldPoint = [&](const float forward, const float right,
        const float up = 0.0f)
    {
        XMFLOAT3 point;
        XMStoreFloat3(&point,
            origin + forwardVector * forward + rightVector * right +
                XMVectorSet(0, up, 0, 0));
        return point;
    };

    // Ground-plane silhouette follows the supplied arrow asset proportions:
    // 2.4 m long, 0.72 m wide, with its tip aligned to Runtime +Z forward.
    constexpr XMFLOAT2 Arrow[7] = {
        XMFLOAT2(1.20f, 0.0f),
        XMFLOAT2(0.28f, 0.36f),
        XMFLOAT2(0.28f, 0.15f),
        XMFLOAT2(-1.20f, 0.15f),
        XMFLOAT2(-1.20f, -0.15f),
        XMFLOAT2(0.28f, -0.15f),
        XMFLOAT2(0.28f, -0.36f),
    };
    XMFLOAT2 projected[7] = {};
    bool visible[7] = {};
    for (int index = 0; index < 7; ++index)
        visible[index] = ProjectEditorPoint(
            worldPoint(Arrow[index].x, Arrow[index].y), projected[index]);

    XMFLOAT2 center = {};
    if (!ProjectEditorPoint(feet, center))
        return false;
    const float dx = pointer.x - center.x;
    const float dy = pointer.y - center.y;
    const bool hovered = dx * dx + dy * dy <= 28.0f * 28.0f;
    const bool selected = session_->Selection().SelectedEntity() ==
        resolved.start.entity;
    const XMFLOAT4 color = selected
        ? XMFLOAT4(1.0f, 0.55f, 0.15f, 1.0f)
        : hovered
            ? XMFLOAT4(0.58f, 0.95f, 1.0f, 1.0f)
            : XMFLOAT4(0.20f, 0.84f, 1.0f, 0.95f);
    for (int index = 0; index < 7; ++index)
    {
        const int next = (index + 1) % 7;
        if (visible[index] && visible[next])
            DrawEditorLine(projected[index], projected[next], color);
    }

    // The arrow is always present. Selecting it adds the real configured
    // capsule as a wire guide without creating a renderable Runtime mesh.
    if (selected)
    {
        const auto settings = resolved.start.settings;
        const float radius = settings.capsuleRadius;
        const float totalHeight = bridge::PlayerCapsuleTotalHeight(settings);
        constexpr int Segments = 20;
        for (int ring = 0; ring < 2; ++ring)
        {
            const float height = ring == 0 ? radius : totalHeight - radius;
            for (int segment = 0; segment < Segments; ++segment)
            {
                const float a0 = XM_2PI * static_cast<float>(segment) / Segments;
                const float a1 = XM_2PI * static_cast<float>(segment + 1) / Segments;
                XMFLOAT2 p0 = {}, p1 = {};
                if (ProjectEditorPoint(
                        worldPoint(std::cos(a0) * radius,
                            std::sin(a0) * radius, height), p0) &&
                    ProjectEditorPoint(
                        worldPoint(std::cos(a1) * radius,
                            std::sin(a1) * radius, height), p1))
                {
                    DrawEditorLine(p0, p1, XMFLOAT4(color.x, color.y, color.z, 0.72f));
                }
            }
        }
        for (const float side : {-radius, radius})
        {
            XMFLOAT2 bottom = {}, top = {};
            if (ProjectEditorPoint(worldPoint(0, side, radius), bottom) &&
                ProjectEditorPoint(worldPoint(0, side, totalHeight - radius), top))
            {
                DrawEditorLine(bottom, top, XMFLOAT4(color.x, color.y, color.z, 0.72f));
            }
        }
    }

    const bool selectRequested = hovered && !flyCameraActive_ &&
        !GetGUI().HasFocus() && !gizmo_.IsInteracting() &&
        IsPointerOverViewport(pointer) &&
        wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
    if (selectRequested)
    {
        session_->Selection().Select(resolved.start.entity);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        SyncGizmoSelection();
        SyncSelectionOutline();
        return true;
    }
    return false;
}

bool StudioRenderPath::HandleCameraSceneIcons(
    const XMFLOAT4& pointer)
{
    if (session_ == nullptr || camera == nullptr || projectHubVisible_)
    {
        return false;
    }

    auto& scene = session_->Scenes().GetScene();
    const bool selectRequested =
        !flyCameraActive_ && !GetGUI().HasFocus() &&
        !gizmo_.IsInteracting() &&
        IsPointerOverViewport(pointer) &&
        wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);

    for (std::size_t index = 0; index < scene.cameras.GetCount(); ++index)
    {
        const wi::ecs::Entity entity = scene.cameras.GetEntity(index);
        if (!session_->Scenes().IsHierarchyVisible(entity))
            continue;

        const auto* transform = scene.transforms.GetComponent(entity);
        if (transform == nullptr)
            continue;

        wi::scene::CameraComponent authoredCamera = scene.cameras[index];
        authoredCamera.TransformCamera(*transform);
        authoredCamera.UpdateCamera();

        XMFLOAT3 eye = {};
        XMFLOAT3 ahead = {};
        const XMVECTOR eyeVector = authoredCamera.GetEye();
        const XMVECTOR aheadVector = XMVectorAdd(
            eyeVector,
            XMVectorScale(authoredCamera.GetAt(), 1.0f));
        XMStoreFloat3(&eye, eyeVector);
        XMStoreFloat3(&ahead, aheadVector);

        XMFLOAT2 center = {};
        if (!ProjectEditorPoint(eye, center))
            continue;

        XMFLOAT2 direction = XMFLOAT2(1.0f, 0.0f);
        XMFLOAT2 aheadScreen = {};
        if (ProjectEditorPoint(ahead, aheadScreen))
        {
            const float dx = aheadScreen.x - center.x;
            const float dy = aheadScreen.y - center.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length > 0.001f)
            {
                direction.x = dx / length;
                direction.y = dy / length;
            }
        }
        const XMFLOAT2 perpendicular(-direction.y, direction.x);

        const bool selected =
            session_->Selection().SelectedEntity() == entity;
        const float pointerDx = pointer.x - center.x;
        const float pointerDy = pointer.y - center.y;
        const bool hovered =
            pointerDx * pointerDx + pointerDy * pointerDy <= 18.0f * 18.0f;
        const XMFLOAT4 color = selected
            ? XMFLOAT4(0.58f, 0.95f, 1.0f, 1.0f)
            : hovered
                ? XMFLOAT4(1.0f, 0.68f, 0.30f, 1.0f)
                : XMFLOAT4(0.20f, 0.84f, 1.0f, 0.92f);

        const auto point = [&](const float forward, const float side)
        {
            return XMFLOAT2(
                center.x + direction.x * forward + perpendicular.x * side,
                center.y + direction.y * forward + perpendicular.y * side);
        };

        const XMFLOAT2 backLeft = point(-7.0f, 5.0f);
        const XMFLOAT2 backRight = point(-7.0f, -5.0f);
        const XMFLOAT2 frontLeft = point(6.0f, 5.0f);
        const XMFLOAT2 frontRight = point(6.0f, -5.0f);
        const XMFLOAT2 lensLeft = point(12.0f, 7.0f);
        const XMFLOAT2 lensRight = point(12.0f, -7.0f);
        const XMFLOAT2 facingEnd = point(27.0f, 0.0f);

        DrawEditorLine(backLeft, frontLeft, color);
        DrawEditorLine(frontLeft, frontRight, color);
        DrawEditorLine(frontRight, backRight, color);
        DrawEditorLine(backRight, backLeft, color);
        DrawEditorLine(frontLeft, lensLeft, color);
        DrawEditorLine(frontRight, lensRight, color);
        DrawEditorLine(lensLeft, lensRight, color);
        DrawEditorLine(point(12.0f, 0.0f), facingEnd, color);
        DrawEditorLine(
            facingEnd,
            XMFLOAT2(
                facingEnd.x - direction.x * 6.0f + perpendicular.x * 4.0f,
                facingEnd.y - direction.y * 6.0f + perpendicular.y * 4.0f),
            color);
        DrawEditorLine(
            facingEnd,
            XMFLOAT2(
                facingEnd.x - direction.x * 6.0f - perpendicular.x * 4.0f,
                facingEnd.y - direction.y * 6.0f - perpendicular.y * 4.0f),
            color);

        if (hovered && selectRequested)
        {
            session_->Selection().Select(entity);
            gizmoSuppressedForCameraView_ = false;
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
            SyncGizmoSelection();
            SyncSelectionOutline();
            return true;
        }
    }
    return false;
}

    bool StudioRenderPath::HandleLightSceneIcons(
        const XMFLOAT4& pointer)
    {
        if (session_ == nullptr || camera == nullptr || projectHubVisible_)
        {
            return false;
        }

        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        wi::ecs::Entity hit = wi::ecs::INVALID_ENTITY;
        float bestDistanceSquared = 22.0f * 22.0f;
        const bool canSelect = !lightPlacementActive_ &&
            !flyCameraActive_ && !GetGUI().HasFocus() &&
            !gizmo_.IsInteracting() &&
            IsPointerOverViewport(pointer) &&
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);

        for (std::size_t index = 0; index < scene.lights.GetCount(); ++index)
        {
            const auto entity = scene.lights.GetEntity(index);
            const auto& light = scene.lights[index];
            if (!session_->Scenes().IsHierarchyVisible(entity))
            {
                continue;
            }
            const auto* transform = scene.transforms.GetComponent(entity);
            if (transform == nullptr)
            {
                continue;
            }

            const XMFLOAT3 position = transform->GetPosition();
            XMFLOAT2 center;
            if (!ProjectEditorPoint(position, center))
            {
                continue;
            }
            const float dx = pointer.x - center.x;
            const float dy = pointer.y - center.y;
            const float distanceSquared = dx * dx + dy * dy;
            const bool hovered = distanceSquared <= 22.0f * 22.0f;
            const XMFLOAT4 color = entity == selected
                ? XMFLOAT4(1.0f, 0.55f, 0.15f, 1.0f)
                : hovered
                    ? XMFLOAT4(0.58f, 0.95f, 1.0f, 1.0f)
                    : XMFLOAT4(0.20f, 0.84f, 1.0f, 0.92f);

            const auto drawCircle = [&](const float radius)
            {
                constexpr int Segments = 16;
                for (int segment = 0; segment < Segments; ++segment)
                {
                    const float angle0 = static_cast<float>(segment) /
                        static_cast<float>(Segments) * XM_2PI;
                    const float angle1 = static_cast<float>(segment + 1) /
                        static_cast<float>(Segments) * XM_2PI;
                    DrawEditorLine(
                        XMFLOAT2(center.x + std::cos(angle0) * radius,
                            center.y + std::sin(angle0) * radius),
                        XMFLOAT2(center.x + std::cos(angle1) * radius,
                            center.y + std::sin(angle1) * radius),
                        color);
                }
            };

            XMFLOAT3 directionTarget = position;
            directionTarget.x += light.direction.x;
            directionTarget.y += light.direction.y;
            directionTarget.z += light.direction.z;
            XMFLOAT2 projectedDirection;
            XMFLOAT2 arrow = XMFLOAT2(0.0f, 1.0f);
            if (ProjectEditorPoint(directionTarget, projectedDirection))
            {
                arrow = XMFLOAT2(
                    projectedDirection.x - center.x,
                    projectedDirection.y - center.y);
                const float length = std::sqrt(
                    arrow.x * arrow.x + arrow.y * arrow.y);
                if (length > 0.001f)
                {
                    arrow.x /= length;
                    arrow.y /= length;
                }
            }
            const XMFLOAT2 perpendicular = XMFLOAT2(-arrow.y, arrow.x);

            switch (light.GetType())
            {
            case wi::scene::LightComponent::POINT:
                drawCircle(7.0f);
                for (int rayIndex = 0; rayIndex < 4; ++rayIndex)
                {
                    const float angle =
                        static_cast<float>(rayIndex) * XM_PIDIV2;
                    DrawEditorLine(
                        XMFLOAT2(center.x + std::cos(angle) * 10.0f,
                            center.y + std::sin(angle) * 10.0f),
                        XMFLOAT2(center.x + std::cos(angle) * 15.0f,
                            center.y + std::sin(angle) * 15.0f),
                        color);
                }
                break;
            case wi::scene::LightComponent::SPOT:
            {
                const XMFLOAT2 tip = XMFLOAT2(
                    center.x + arrow.x * 15.0f,
                    center.y + arrow.y * 15.0f);
                const XMFLOAT2 baseLeft = XMFLOAT2(
                    center.x - arrow.x * 7.0f + perpendicular.x * 8.0f,
                    center.y - arrow.y * 7.0f + perpendicular.y * 8.0f);
                const XMFLOAT2 baseRight = XMFLOAT2(
                    center.x - arrow.x * 7.0f - perpendicular.x * 8.0f,
                    center.y - arrow.y * 7.0f - perpendicular.y * 8.0f);
                DrawEditorLine(baseLeft, baseRight, color);
                DrawEditorLine(baseLeft, tip, color);
                DrawEditorLine(baseRight, tip, color);
                DrawEditorLine(center, tip, color);
                break;
            }
            case wi::scene::LightComponent::RECTANGLE:
            {
                constexpr float HalfWidth = 10.0f;
                constexpr float HalfHeight = 7.0f;
                const XMFLOAT2 topLeft(
                    center.x - HalfWidth,
                    center.y - HalfHeight);
                const XMFLOAT2 topRight(
                    center.x + HalfWidth,
                    center.y - HalfHeight);
                const XMFLOAT2 bottomLeft(
                    center.x - HalfWidth,
                    center.y + HalfHeight);
                const XMFLOAT2 bottomRight(
                    center.x + HalfWidth,
                    center.y + HalfHeight);
                DrawEditorLine(topLeft, topRight, color);
                DrawEditorLine(topRight, bottomRight, color);
                DrawEditorLine(bottomRight, bottomLeft, color);
                DrawEditorLine(bottomLeft, topLeft, color);
                DrawEditorLine(topLeft, bottomRight, color);
                DrawEditorLine(topRight, bottomLeft, color);
                break;
            }
            case wi::scene::LightComponent::DIRECTIONAL:
            default:
                drawCircle(8.0f);
                for (int rayIndex = 0; rayIndex < 8; ++rayIndex)
                {
                    const float angle =
                        static_cast<float>(rayIndex) / 8.0f * XM_2PI;
                    DrawEditorLine(
                        XMFLOAT2(center.x + std::cos(angle) * 11.0f,
                            center.y + std::sin(angle) * 11.0f),
                        XMFLOAT2(center.x + std::cos(angle) * 16.0f,
                            center.y + std::sin(angle) * 16.0f),
                        color);
                }
                break;
            }

            if (light.GetType() != wi::scene::LightComponent::POINT)
            {
                const XMFLOAT2 arrowEnd = XMFLOAT2(
                    center.x + arrow.x * 22.0f,
                    center.y + arrow.y * 22.0f);
                DrawEditorLine(center, arrowEnd, color);
                DrawEditorLine(
                    arrowEnd,
                    XMFLOAT2(
                        arrowEnd.x - arrow.x * 6.0f + perpendicular.x * 4.0f,
                        arrowEnd.y - arrow.y * 6.0f + perpendicular.y * 4.0f),
                    color);
                DrawEditorLine(
                    arrowEnd,
                    XMFLOAT2(
                        arrowEnd.x - arrow.x * 6.0f - perpendicular.x * 4.0f,
                        arrowEnd.y - arrow.y * 6.0f - perpendicular.y * 4.0f),
                    color);
            }

            if (canSelect && hovered && distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                hit = entity;
            }
        }

        if (hit == wi::ecs::INVALID_ENTITY)
        {
            return false;
        }
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);
        session_->Selection().Select(hit);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
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
            gizmoSuppressedForCameraView_ = false;
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
                const auto finishStarted =
                    std::chrono::steady_clock::now();
                auto after = bridge::CaptureTerrainSculpt(scene, *terrain);
                const std::size_t affectedChunks =
                    bridge::RetainChangedTerrainSculpt(
                        terrainStrokeBefore_,
                        after);
                if (affectedChunks > 0)
                {
                    bridge::RefreshTerrainSculptPhysics(
                        scene,
                        *terrain,
                        after);
                    session_->Commands().RecordExecuted(
                        std::make_unique<bridge::SculptTerrainCommand>(
                            scene,
                            terrainStrokeEntity_,
                            std::move(terrainStrokeBefore_),
                            std::move(after)));
                }
                const auto elapsed = std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - finishStarted);
                terrainStrokeDiagnostic_.SetText(
                    "LAST STROKE // " +
                    std::to_string(affectedChunks) +
                    " TILES // " +
                    std::to_string(elapsed.count()) +
                    " MS");
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
        auto& scene = session_->Scenes().GetScene();
        const auto picked = wi::scene::Pick(
            pickRay,
            wi::enums::FILTER_OBJECT_ALL,
            ~0u,
            scene);
        wi::ecs::Entity selection = picked.entity;
        if (selection != wi::ecs::INVALID_ENTITY)
        {
            const wi::ecs::Entity reusableRoot =
                ResolveReusableSelectionRoot(scene, selection);
            if (reusableRoot != wi::ecs::INVALID_ENTITY)
                selection = reusableRoot;
        }
        const auto current = session_->Selection().SelectedEntity();
        if (selection == current && !environmentWorkspaceActive_)
        {
            return false;
        }

        environmentWorkspaceActive_ = false;
        studioChrome_.SetEnvironmentWorkspaceActive(false);
        terrainWorkspaceActive_ = false;
        studioChrome_.SetTerrainWorkspaceActive(false);

        if (selection == wi::ecs::INVALID_ENTITY ||
            !session_->Scenes().IsHierarchyVisible(selection))
        {
            session_->Selection().Clear();
        }
        else
        {
            session_->Selection().Select(selection);
        }

        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        return true;
    }

    void StudioRenderPath::RefreshProjectHub()
    {
        if (!session_) return;
        const auto& projects = session_->Projects().RecentProjects();
        if (projects.empty()) selectedRecentProject_ = -1;
        else if (selectedRecentProject_ < 0 || static_cast<std::size_t>(selectedRecentProject_) >= projects.size())
            selectedRecentProject_ = 0;

        for (auto& button : recentProjectButtons_) button.SetVisible(false);
        launchProjectButton_.SetEnabled(selectedRecentProject_ >= 0);
        continueProjectButton_.SetVisible(false);

        std::vector<RenegadeProjectHub::ProjectEntry> entries;
        entries.reserve(projects.size());
        for (const auto& recent : projects)
        {
            RenegadeProjectHub::ProjectEntry entry;
            entry.name = recent.name;
            entry.descriptorPath = recent.descriptorPath;
            bridge::ProjectMetadata meta;
            std::string error;
            entry.descriptorValid = session_->Projects().InspectProject(recent.descriptorPath, meta, error);
            if (entry.descriptorValid)
            {
                if (!meta.name.empty()) entry.name = meta.name;
                entry.rootPath = meta.rootPath;
                entry.startupScene = meta.startupScene;
                entry.startupFlow = meta.startupFlow;
                entry.formatVersion = meta.formatVersion;
            }
            entries.push_back(std::move(entry));
        }
        projectHubChrome_.SetProjects(std::move(entries), selectedRecentProject_);
        if (session_->Projects().HasProject())
            projectHubChrome_.SetCurrentProject(session_->Projects().CurrentProject().name, true);
        else
            projectHubChrome_.SetCurrentProject({}, false);
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
        case WeatherField::Stars:
            weather.stars = std::clamp(value, 0.0f, 1.0f);
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

    void StudioRenderPath::CommitSelectedSceneName(const std::string& name)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        const auto target = bridge::ResolveSceneComponentAuthoringRoot(scene, selected);
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetSceneNameCommand>(scene, selected, name));
        if (changed && target != wi::ecs::INVALID_ENTITY)
            session_->Selection().Select(target);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedLayerBit(
        const std::uint32_t bit,
        const bool enabled)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetSceneLayerBitCommand>(
                scene, selected, bit, enabled));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedLayerMask(const std::uint32_t mask)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetSceneLayerMaskCommand>(
                scene, selected, mask));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedMetadataPreset(
        const wi::scene::MetadataComponent::Preset preset)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetMetadataPresetCommand>(
                scene, selected, preset));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplySelectedObjectParticipation(
        const bridge::ObjectParticipationProperty property,
        const bool value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto selected = session_->Selection().SelectedEntity();
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetObjectParticipationCommand>(
                scene, selected, property, value));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::CommitSelectedPlayerField(
        const PlayerField field,
        const float value)
    {
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto entity = session_->Selection().SelectedEntity();
        if (!bridge::IsPlayerStart(scene, entity))
            return;

        auto settings = bridge::CapturePlayerControllerSettings(scene, entity);
        switch (field)
        {
        case PlayerField::CapsuleRadius:
            settings.capsuleRadius = value;
            break;
        case PlayerField::CapsuleTotalHeight:
            settings.capsuleHeight = std::max(
                0.01f, (value - settings.capsuleRadius * 2.0f) * 0.5f);
            break;
        case PlayerField::EyeHeight:
            settings.eyeHeight = value;
            break;
        case PlayerField::WalkSpeed:
            settings.walkSpeed = value;
            break;
        case PlayerField::SprintSpeed:
            settings.sprintSpeed = value;
            break;
        case PlayerField::JumpSpeed:
            settings.jumpSpeed = value;
            break;
        case PlayerField::LookSensitivity:
            settings.lookSensitivity = value;
            break;
        case PlayerField::MaximumSlope:
            settings.maximumSlopeDegrees = value;
            break;
        case PlayerField::GravityFactor:
            settings.gravityFactor = value;
            break;
        case PlayerField::MinimumPitch:
            settings.minimumPitch = wi::math::DegreesToRadians(value);
            break;
        case PlayerField::MaximumPitch:
            settings.maximumPitch = wi::math::DegreesToRadians(value);
            break;
        }

        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetPlayerControllerSettingsCommand>(
                scene, entity, settings));
        RefreshInspector();
        RefreshStatus();
    }


    bridge::TransformState StudioRenderPath::CaptureEditorCameraTransform() const
    {
        bridge::TransformState state;
        if (camera == nullptr)
            return state;
        wi::scene::TransformComponent transform;
        transform.MatrixTransform(camera->GetInvView());
        transform.UpdateTransform();
        return bridge::CaptureTransform(transform);
    }

    void StudioRenderPath::CreateCameraFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto command = std::make_unique<bridge::CreateCameraCommand>(
            session_->Scenes().GetScene(),
            bridge::CaptureCamera(*camera),
            CaptureEditorCameraTransform(),
            camera->width,
            camera->height);
        auto* created = command.get();
        if (session_->Commands().Execute(std::move(command)))
        {
            session_->Selection().Select(created->CreatedEntity());
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::CreatePlayerStartFromView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        auto& scene = session_->Scenes().GetScene();
        const auto existing = bridge::ResolvePlayerStart(scene);
        if (existing.resolution != bridge::PlayerStartResolution::Missing)
        {
            studioChrome_.SetStatusText(
                "PLAYER START // LEVEL ALREADY HAS ONE");
            return;
        }

        auto playerStartTransform = CaptureEditorCameraTransform();
        // The editor camera represents eye position; Player Start represents
        // capsule feet. This makes Test Level begin from the view the creator
        // was composing instead of spawning the capsule 1.65 metres above it.
        playerStartTransform.translation.y -= 1.65f;
        const XMFLOAT3 cameraEuler = wi::math::QuaternionToRollPitchYaw(
            playerStartTransform.rotation);
        XMStoreFloat4(
            &playerStartTransform.rotation,
            XMQuaternionRotationRollPitchYaw(0.0f, cameraEuler.y, 0.0f));
        auto command = std::make_unique<bridge::CreatePlayerStartCommand>(
            scene,
            playerStartTransform);
        auto* created = command.get();
        if (session_->Commands().Execute(std::move(command)))
        {
            session_->Selection().Select(created->CreatedEntity());
            SetEnvironmentWorkspaceActive(false);
            SetTerrainWorkspaceActive(false);
            SetRenderWorkspaceActive(false);
            RefreshHierarchy();
            RefreshInspector();
            RefreshStatus();
            studioChrome_.SetStatusText(
                "PLAYER START // CREATED // POSITION WITH GIZMO");
        }
    }

    bool StudioRenderPath::CommitSelectedCamera(
        const bridge::CameraState& cameraState)
    {
        if (session_ == nullptr)
            return false;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.cameras.Contains(entity))
            return false;
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetCameraCommand>(
                scene, entity, cameraState));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySelectedCameraProjection(
        const bool orthographic)
    {
        if (session_ == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto* authoredCamera =
            session_->Scenes().GetScene().cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        auto state = bridge::CaptureCamera(*authoredCamera);
        state.orthographic = orthographic;
        CommitSelectedCamera(state);
    }

    void StudioRenderPath::SetCameraFieldValue(
        bridge::CameraState& cameraState,
        const CameraField field,
        const float value) noexcept
    {
        switch (field)
        {
        case CameraField::FieldOfView:
            cameraState.fieldOfViewDegrees = value;
            break;
        case CameraField::NearPlane:
            cameraState.nearPlane = value;
            break;
        case CameraField::FarPlane:
            cameraState.farPlane = value;
            break;
        case CameraField::FocalLength:
            cameraState.focalLength = value;
            break;
        case CameraField::ApertureSize:
            cameraState.apertureSize = value;
            break;
        case CameraField::OrthoVerticalSize:
            cameraState.orthoVerticalSize = value;
            break;
        }
        cameraState = bridge::SanitizeCameraState(cameraState);
    }

    void StudioRenderPath::BeginCameraSlider(const CameraField field)
    {
        cameraSliderActive_ = false;
        if (session_ == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto* authoredCamera =
            session_->Scenes().GetScene().cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        cameraSliderActive_ = true;
        cameraSliderField_ = field;
        cameraSliderEntity_ = entity;
        cameraSliderBefore_ = bridge::CaptureCamera(*authoredCamera);
        cameraSliderAfter_ = cameraSliderBefore_;
    }

    void StudioRenderPath::PreviewCameraSlider(
        const CameraField field,
        const float value)
    {
        if (!cameraSliderActive_ || cameraSliderField_ != field ||
            session_ == nullptr)
            return;
        cameraSliderAfter_ = cameraSliderBefore_;
        SetCameraFieldValue(cameraSliderAfter_, field, value);
        auto* authoredCamera = session_->Scenes().GetScene().cameras.GetComponent(
            cameraSliderEntity_);
        if (authoredCamera != nullptr)
            bridge::ApplyCamera(*authoredCamera, cameraSliderAfter_);
    }

    void StudioRenderPath::CommitCameraSlider(
        const CameraField field,
        const float value)
    {
        if (!cameraSliderActive_ || cameraSliderField_ != field ||
            session_ == nullptr)
            return;
        SetCameraFieldValue(cameraSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        auto* authoredCamera = scene.cameras.GetComponent(cameraSliderEntity_);
        if (authoredCamera != nullptr)
            bridge::ApplyCamera(*authoredCamera, cameraSliderBefore_);
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetCameraCommand>(
                scene,
                cameraSliderEntity_,
                cameraSliderBefore_,
                cameraSliderAfter_));
        cameraSliderActive_ = false;
        cameraSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::AlignSelectedCameraToView()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        auto* authoredCamera = scene.cameras.GetComponent(entity);
        if (authoredCamera == nullptr)
            return;
        if (session_->Commands().Execute(
                std::make_unique<bridge::SetTransformCommand>(
                    scene, entity, CaptureEditorCameraTransform())))
        {
            if (auto* transform = scene.transforms.GetComponent(entity))
            {
                authoredCamera->TransformCamera(*transform);
                authoredCamera->UpdateCamera();
            }
            gizmoSuppressedForCameraView_ = true;
            RefreshInspector();
            RefreshStatus();
        }
    }

    void StudioRenderPath::ViewFromSelectedCamera()
    {
        if (session_ == nullptr || camera == nullptr)
            return;
        const auto entity = session_->Selection().SelectedEntity();
        const auto& scene = session_->Scenes().GetScene();
        const auto* authoredCamera = scene.cameras.GetComponent(entity);
        const auto* transform = scene.transforms.GetComponent(entity);
        if (authoredCamera == nullptr || transform == nullptr)
            return;
        bridge::ApplyCamera(*camera, bridge::CaptureCamera(*authoredCamera));
        camera->TransformCamera(*transform);
        camera->UpdateCamera();
        gizmoSuppressedForCameraView_ = true;
        RefreshStatus();
    }

    bool StudioRenderPath::CommitSelectedLight(
        const bridge::LightState& light)
    {
        if (session_ == nullptr)
        {
            return false;
        }
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.lights.Contains(entity))
        {
            return false;
        }
        const bool changed = session_->Commands().Execute(
            std::make_unique<bridge::SetLightCommand>(
                scene,
                entity,
                light));
        RefreshInspector();
        RefreshStatus();
        return changed;
    }

    void StudioRenderPath::ApplySelectedLightType(
        const wi::scene::LightComponent::LightType type)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* light =
            session_->Scenes().GetScene().lights.GetComponent(entity);
        if (light == nullptr)
        {
            return;
        }
        auto state = bridge::CaptureLight(*light);
        state.type = type;
        CommitSelectedLight(state);
    }

    void StudioRenderPath::ApplySelectedLightToggle(
        const LightToggle toggle,
        const bool value)
    {
        StopSunPreview(true);
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* light =
            session_->Scenes().GetScene().lights.GetComponent(entity);
        if (light == nullptr)
        {
            return;
        }
        auto state = bridge::CaptureLight(*light);
        switch (toggle)
        {
        case LightToggle::CastShadow:
            state.castShadow = value;
            break;
        case LightToggle::Volumetrics:
            state.volumetrics = value;
            break;
        }
        CommitSelectedLight(state);
    }

    void StudioRenderPath::SetLightFieldValue(
        bridge::LightState& light,
        const LightField field,
        const float value) noexcept
    {
        switch (field)
        {
        case LightField::ColorRed:
            light.color.x = std::clamp(value, 0.0f, 1.0f);
            break;
        case LightField::ColorGreen:
            light.color.y = std::clamp(value, 0.0f, 1.0f);
            break;
        case LightField::ColorBlue:
            light.color.z = std::clamp(value, 0.0f, 1.0f);
            break;
        case LightField::Intensity:
            light.intensity = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::Range:
            light.range = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::OuterCone:
            light.outerConeDegrees = std::clamp(value, 0.1f, 89.9f);
            light.innerConeDegrees = std::min(
                light.innerConeDegrees,
                light.outerConeDegrees);
            break;
        case LightField::InnerCone:
            light.innerConeDegrees = std::clamp(
                value,
                0.0f,
                light.outerConeDegrees);
            break;
        case LightField::Radius:
            light.radius = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::Length:
            light.length = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::Height:
            light.height = std::clamp(value, 0.0f, 100000.0f);
            break;
        case LightField::VolumetricBoost:
            light.volumetricBoost = std::clamp(value, 0.0f, 10.0f);
            break;
        }
    }

    void StudioRenderPath::BeginLightSlider(const LightField field)
    {
        StopSunPreview(true);
        lightSliderActive_ = false;
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        const auto* light =
            session_->Scenes().GetScene().lights.GetComponent(entity);
        if (light == nullptr)
        {
            return;
        }
        lightSliderActive_ = true;
        lightSliderField_ = field;
        lightSliderEntity_ = entity;
        lightSliderBefore_ = bridge::CaptureLight(*light);
        lightSliderAfter_ = lightSliderBefore_;
    }

    void StudioRenderPath::PreviewLightSlider(
        const LightField field,
        const float value)
    {
        if (!lightSliderActive_ || lightSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        auto* light = session_->Scenes().GetScene().lights.GetComponent(
            lightSliderEntity_);
        if (light == nullptr)
        {
            return;
        }
        lightSliderAfter_ = lightSliderBefore_;
        SetLightFieldValue(lightSliderAfter_, field, value);
        bridge::ApplyLight(*light, lightSliderAfter_);
    }

    void StudioRenderPath::CommitLightSlider(
        const LightField field,
        const float value)
    {
        if (!lightSliderActive_ || lightSliderField_ != field ||
            session_ == nullptr)
        {
            return;
        }
        SetLightFieldValue(lightSliderAfter_, field, value);
        auto& scene = session_->Scenes().GetScene();
        auto* light = scene.lights.GetComponent(lightSliderEntity_);
        if (light != nullptr)
        {
            bridge::ApplyLight(*light, lightSliderBefore_);
            session_->Commands().Execute(
                std::make_unique<bridge::SetLightCommand>(
                    scene,
                    lightSliderEntity_,
                    lightSliderBefore_,
                    lightSliderAfter_));
        }
        lightSliderActive_ = false;
        lightSliderEntity_ = wi::ecs::INVALID_ENTITY;
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
        // Wicked creates Weather on the Terrain entity when generation starts
        // in a blank scene. Establish Renegade's dedicated Environment carrier
        // first so the two Inspector workspaces never acquire the same owner.
        if (scene.weathers.GetCount() == 0)
        {
            const auto environmentState = bridge::CaptureWeather(
                scene.weather);
            if (!session_->Commands().Execute(
                    std::make_unique<bridge::CreateEnvironmentCommand>(
                        scene,
                        environmentState,
                        "Environment")))
            {
                return;
            }
        }
        if (bridge::FindPrimarySunLight(scene) == wi::ecs::INVALID_ENTITY)
        {
            if (!session_->Commands().Execute(
                    std::make_unique<bridge::CreateSunCommand>(
                        scene,
                        session_->Scenes().WeatherEntity())))
            {
                return;
            }
        }
        if (scene.terrains.GetCount() > 0)
        {
            session_->Selection().Select(scene.terrains.GetEntity(0));
            RefreshHierarchy();
            RefreshInspector();
            return;
        }
        auto command = std::make_unique<bridge::CreateTerrainCommand>(
            scene,
            bridge::TerrainState{},
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

    void StudioRenderPath::ExpandTerrain()
    {
        if (session_ == nullptr)
        {
            return;
        }
        const auto entity = session_->Selection().SelectedEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.terrains.Contains(entity))
        {
            return;
        }
        session_->Commands().Execute(
            std::make_unique<bridge::ExpandTerrainCommand>(scene, entity));
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

    void StudioRenderPath::SetTerrainFieldValue(
        bridge::TerrainState& terrain,
        const TerrainField field,
        const float value) noexcept
    {
        switch (field)
        {
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

    void StudioRenderPath::ApplyTerrainMaterialPreset(
        const bridge::TerrainMaterialPreset preset)
    {
        if (session_ == nullptr ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        const auto entity = scene.terrains.GetEntity(0);
        const auto* terrain = scene.terrains.GetComponent(entity);
        if (terrain == nullptr)
        {
            return;
        }
        auto before = bridge::CaptureTerrainMaterial(scene, *terrain);
        auto after = before;
        bridge::SetTerrainTextureScale(
            after,
            bridge::MakeTerrainMaterialPreset(preset));
        session_->Commands().Execute(
            std::make_unique<bridge::SetTerrainMaterialCommand>(
                scene,
                entity,
                std::move(before),
                std::move(after)));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::BeginTerrainTextureScale()
    {
        terrainTextureScaleActive_ = false;
        if (session_ == nullptr ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        terrainMaterialEntity_ = scene.terrains.GetEntity(0);
        const auto* terrain = scene.terrains.GetComponent(
            terrainMaterialEntity_);
        if (terrain == nullptr)
        {
            terrainMaterialEntity_ = wi::ecs::INVALID_ENTITY;
            return;
        }
        terrainMaterialBefore_ = bridge::CaptureTerrainMaterial(scene, *terrain);
        terrainMaterialAfter_ = terrainMaterialBefore_;
        terrainTextureScaleActive_ = true;
    }

    void StudioRenderPath::PreviewTerrainTextureScale(const float value)
    {
        if (!terrainTextureScaleActive_ || session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* terrain = scene.terrains.GetComponent(terrainMaterialEntity_);
        if (terrain == nullptr)
        {
            return;
        }
        terrainMaterialAfter_ = terrainMaterialBefore_;
        bridge::SetTerrainTextureScale(terrainMaterialAfter_, value);
        bridge::ApplyTerrainMaterial(
            scene,
            *terrain,
            terrainMaterialAfter_,
            true);
    }

    void StudioRenderPath::CommitTerrainTextureScale(const float value)
    {
        if (!terrainTextureScaleActive_ || session_ == nullptr)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* terrain = scene.terrains.GetComponent(terrainMaterialEntity_);
        if (terrain != nullptr)
        {
            bridge::SetTerrainTextureScale(terrainMaterialAfter_, value);
            const float before = terrainMaterialBefore_.slots[0].texMulAdd.x;
            const float after = terrainMaterialAfter_.slots[0].texMulAdd.x;
            if (std::abs(before - after) > 0.00001f)
            {
                bridge::ApplyTerrainMaterial(
                    scene,
                    *terrain,
                    terrainMaterialAfter_,
                    true);
                session_->Commands().RecordExecuted(
                    std::make_unique<bridge::SetTerrainMaterialCommand>(
                        scene,
                        terrainMaterialEntity_,
                        std::move(terrainMaterialBefore_),
                        std::move(terrainMaterialAfter_)));
            }
        }
        terrainTextureScaleActive_ = false;
        terrainMaterialEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ApplyDefaultGrass()
    {
        if (session_ == nullptr ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        const auto entity = scene.terrains.GetEntity(0);
        const auto* terrain = scene.terrains.GetComponent(entity);
        if (terrain == nullptr)
        {
            return;
        }
        auto before = bridge::CaptureTerrainMaterial(scene, *terrain);
        auto after = bridge::MakeDefaultGrassMaterial(
            bridge::DefaultGrassTextureScale);
        session_->Commands().Execute(
            std::make_unique<bridge::SetTerrainMaterialCommand>(
                scene,
                entity,
                std::move(before),
                std::move(after)));
        RefreshInspector();
        RefreshStatus();
    }

    void StudioRenderPath::ReloadTerrainMaterial()
    {
        if (session_ == nullptr ||
            session_->Scenes().GetScene().terrains.GetCount() == 0)
        {
            return;
        }
        auto& scene = session_->Scenes().GetScene();
        auto* terrain = scene.terrains.GetComponent(
            scene.terrains.GetEntity(0));
        if (terrain != nullptr)
        {
            bridge::ReloadDefaultTerrainMaterial(scene, *terrain);
            terrainStrokeDiagnostic_.SetText("MATERIAL // FILES RELOADED");
        }
        RefreshInspector();
        RefreshStatus();
    }

























    std::string StudioRenderPath::ResolveTestLevelRuntimePath() const
    {
        // fs::current_path() is not reliable here: common Windows file-open
        // dialogs (Open Project, Open Scene, etc.) are documented to change
        // the calling process's working directory as a side effect, and
        // once that happens every candidate below silently resolves against
        // the wrong root - RenegadeRuntime.exe still exists exactly where
        // it always did, but this lookup would no longer find it. Anchor to
        // this process's own executable path instead, which cannot drift.
        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
        {
            return {};
        }
        const fs::path workingDirectory = fs::path(modulePath).parent_path();

        std::vector<fs::path> candidates = {
            workingDirectory / "Runtime" / "RenegadeRuntime.exe",
            workingDirectory / "RenegadeRuntime.exe",
        };

        const fs::path configuration = workingDirectory.filename();
        const fs::path buildRoot = workingDirectory.parent_path().parent_path();
        if (!configuration.empty() && !buildRoot.empty())
        {
            candidates.push_back(
                buildRoot / "Runtime" / configuration / "RenegadeRuntime.exe");
        }

        for (const auto& candidate : candidates)
        {
            std::error_code pathError;
            if (fs::is_regular_file(candidate, pathError) && !pathError)
            {
                return candidate.lexically_normal().generic_u8string();
            }
        }
        return {};
    }

    std::string StudioRenderPath::TestLevelBackendArgument() const
    {
        const auto* device = wi::graphics::GetDevice();
        if (device != nullptr && std::string(device->GetTag()) == "[Vulkan]")
        {
            return "vulkan";
        }
        return "dx12";
    }

    void StudioRenderPath::ShowStudioMessageBox(
        const std::string& message,
        const std::string& caption)
    {
        // Native modal dialogs stop Wicked's input update loop. Reporting
        // failures through the backlog keeps the Studio window interactive,
        // while the status line still identifies the failed operation.
        wi::backlog::post(
            caption + " // " + message,
            wi::backlog::LogLevel::Error);
        const std::size_t firstLine = message.find('\n');
        studioChrome_.SetStatusText(
            caption + " // " +
            message.substr(0, firstLine == std::string::npos
                ? message.size()
                : firstLine));
        wi::input::ClearForNextFrame();
        wi::input::HidePointer(false);
        wi::input::ResetCursors();
        studioChrome_.ResetTransientPointerState();
    }

    void StudioRenderPath::StartTestLevel()
    {
        projectPreviewActive_ = false;
        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            ShowStudioMessageBox(
                "Open or create a Renegade project before starting Test Level.",
                "Test Level");
            return;
        }
        if (testLevelRuntime_.IsActive())
        {
            return;
        }

        bridge::TestLevelSnapshotService snapshotService(
            session_->Scenes(),
            session_->Commands(),
            &session_->Scripts());
        bridge::TestLevelSnapshot snapshot;
        std::string error;
        ClearSelectionOutline();
        const bool snapshotCreated = snapshotService.Create(
            session_->Projects().CurrentProject(),
            snapshot,
            error);
        SyncSelectionOutline();
        if (!snapshotCreated)
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            studioChrome_.SetStatusText("TEST LEVEL // SNAPSHOT FAILED");
            ShowStudioMessageBox(
                "Renegade could not create the Test Level snapshot.\n\n" +
                    error,
                "Test Level");
            return;
        }

        const std::string runtimePath = ResolveTestLevelRuntimePath();
        if (runtimePath.empty())
        {
            std::string cleanupError;
            snapshotService.Cleanup(snapshot, cleanupError);
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            studioChrome_.SetStatusText("TEST LEVEL // RUNTIME NOT FOUND");

            std::string message =
                "RenegadeRuntime.exe was not found beside this Studio build.";
            if (!cleanupError.empty())
            {
                message += "\n\nSnapshot cleanup warning: " + cleanupError;
            }
            ShowStudioMessageBox(message, "Test Level");
            return;
        }

        TestLevelLaunchOptions options;
        options.executablePath = runtimePath;
        options.workingDirectory =
            fs::u8path(runtimePath).parent_path().generic_u8string();
        options.arguments = {
            TestLevelBackendArgument(),
            "--project",
            snapshot.descriptorPath,
        };
        options.startupTimeout = std::chrono::milliseconds(60000);

        if (!testLevelRuntime_.Launch(
                std::move(options),
                std::move(snapshot),
                error))
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            studioChrome_.SetStatusText("TEST LEVEL // LAUNCH FAILED");
            ShowStudioMessageBox(
                "Renegade could not launch Test Level.\n\n" + error,
                "Test Level");
            return;
        }

        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Starting);
        studioChrome_.SetStatusText(
            "TEST LEVEL // STARTING // UNSAVED SNAPSHOT");
    }

    void StudioRenderPath::StartProjectPlay()
    {
        projectPreviewActive_ = false;
        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            ShowStudioMessageBox(
                "Open or create a Renegade project before previewing Story Flow.",
                "Story Flow Preview");
            return;
        }
        if (testLevelRuntime_.IsActive())
            return;

        const std::string runtimePath = ResolveTestLevelRuntimePath();
        if (runtimePath.empty())
        {
            studioChrome_.SetStatusText("STORY FLOW PREVIEW // RUNTIME NOT FOUND");
            ShowStudioMessageBox(
                "RenegadeRuntime.exe was not found beside this Studio build.",
                "Story Flow Preview");
            return;
        }

        const auto& project = session_->Projects().CurrentProject();
        TestLevelLaunchOptions options;
        options.executablePath = runtimePath;
        options.workingDirectory =
            fs::u8path(runtimePath).parent_path().generic_u8string();
        options.arguments = {
            TestLevelBackendArgument(),
            "--project",
            project.descriptorPath,
        };
        options.startupTimeout = std::chrono::milliseconds(60000);
        options.ownsSnapshot = false;
        projectPreviewActive_ = true;

        bridge::TestLevelSnapshot noSnapshot;
        std::string error;
        if (!testLevelRuntime_.Launch(
                std::move(options), std::move(noSnapshot), error))
        {
            projectPreviewActive_ = false;
            studioChrome_.SetStatusText("STORY FLOW PREVIEW // LAUNCH FAILED");
            ShowStudioMessageBox(
                "Renegade could not launch Story Flow Preview.\n\n" + error,
                "Story Flow Preview");
            return;
        }

        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Starting);
        studioChrome_.SetStatusText("STORY FLOW PREVIEW // STARTING");
    }

    void StudioRenderPath::PollTestLevel()
    {
        if (!testLevelRuntime_.IsActive())
        {
            return;
        }

        const TestLevelProcessResult result = testLevelRuntime_.Poll();
        if (result.state == TestLevelProcessState::Running)
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Running);
            studioChrome_.SetStatusText(projectPreviewActive_
                ? "STORY FLOW PREVIEW // RUNNING"
                : "TEST LEVEL // RUNNING // UNSAVED SNAPSHOT");
            return;
        }
        if (!result.finished)
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Starting);
            return;
        }

        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Idle);
        if (result.succeeded)
        {
            studioChrome_.SetStatusText(projectPreviewActive_
                ? "STORY FLOW PREVIEW // COMPLETED"
                : "TEST LEVEL // COMPLETED");
            projectPreviewActive_ = false;
            return;
        }

        const bool wasProjectPreview = projectPreviewActive_;
        studioChrome_.SetStatusText(wasProjectPreview
            ? "STORY FLOW PREVIEW // FAILED"
            : "TEST LEVEL // FAILED");
        projectPreviewActive_ = false;
        std::string message = result.message.empty()
            ? (wasProjectPreview
                ? "The Story Flow Preview Runtime stopped before it became ready."
                : "The Test Level Runtime stopped before it became ready.")
            : result.message;
        if (!result.warning.empty())
        {
            message += "\n\nWarning: " + result.warning;
        }
        ShowStudioMessageBox(message,
            wasProjectPreview ? "Story Flow Preview" : "Test Level");
    }

    void StudioRenderPath::StopTestLevel()
    {
        if (!testLevelRuntime_.IsActive())
        {
            studioChrome_.SetTestLevelState(
                RenegadeStudioChrome::TestLevelState::Idle);
            return;
        }

        const bool wasProjectPreview = projectPreviewActive_;
        const TestLevelProcessResult result = testLevelRuntime_.Stop();
        projectPreviewActive_ = false;
        studioChrome_.SetTestLevelState(
            RenegadeStudioChrome::TestLevelState::Idle);
        studioChrome_.SetStatusText(wasProjectPreview
            ? "STORY FLOW PREVIEW // STOPPED"
            : (result.cleanupSucceeded
                ? "TEST LEVEL // STOPPED // SNAPSHOT CLEAN"
                : "TEST LEVEL // STOPPED // CLEANUP WARNING"));
        if (!result.warning.empty())
        {
            ShowStudioMessageBox(result.warning,
                wasProjectPreview ? "Story Flow Preview" : "Test Level");
        }
    }

    void StudioRenderPath::RestoreGovernedMaterialTextures()
    {
        if (session_ == nullptr || !session_->Projects().HasProject())
            return;

        const auto& project = session_->Projects().CurrentProject();
        const auto restored = bridge::RestoreMaterialTextureBindings(
            session_->Scenes().GetScene(), project.rootPath, project.projectId);
        if (!restored.succeeded)
        {
            studioChrome_.SetStatusText(
                "TEXTURE BINDING // RESTORE WARNING // " + restored.error);
        }
        else if (restored.restored > 0)
        {
            studioChrome_.SetStatusText(
                "TEXTURE BINDING // RESTORED " +
                std::to_string(restored.restored) +
                " GOVERNED MATERIAL TEXTURE");
        }
    }

    void StudioRenderPath::RequestProjectHubFromStoryFlow()
    {
        RequestDiagnosticAction(EditorAction::ProjectHub);
    }

    void StudioRenderPath::RequestAssetBrowserFromStoryFlow()
    {
        RefreshAssetBrowser();
    }

    void StudioRenderPath::RequestProjectPlayFromStoryFlow()
    {
        RequestDiagnosticAction(EditorAction::StartProjectPlay);
    }

    void StudioRenderPath::RequestWindowsGameBuild()
    {
        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            SetWindowsGameBuildStatus(
                "BUILD FAILED // AN ACTIVE RENEGADE PROJECT IS REQUIRED");
            return;
        }
        if (windowsGameBuildPreparationActive_ || windowsGameBuildRequested_)
        {
            SetWindowsGameBuildStatus(
                "BUILD WINDOWS GAME // ALREADY QUEUED");
            return;
        }

        windowsGameBuildPreparationActive_ = true;
        SetWindowsGameBuildStatus(
            "BUILD WINDOWS GAME // SAVING DIRTY PROJECT DOCUMENTS");
        StopSunPreview(true);

        const auto finishScenePreparation = [this](const bool saved)
        {
            windowsGameBuildPreparationActive_ = false;
            if (!saved)
            {
                const std::string detail = session_ == nullptr
                    ? std::string{}
                    : session_->Scenes().LastError();
                SetWindowsGameBuildStatus(
                    detail.empty()
                        ? "BUILD FAILED // SCENE SAVE WAS CANCELLED OR FAILED"
                        : "BUILD FAILED // SCENE SAVE FAILED // " + detail);
                return;
            }

            windowsGameBuildRequested_ = true;
            SetWindowsGameBuildStatus(
                "BUILD WINDOWS GAME // QUEUED // SCENE SAVED");
        };

        if (!session_->Commands().IsDirty())
        {
            finishScenePreparation(true);
            return;
        }

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            SaveSceneAs(finishScenePreparation);
            return;
        }
        SaveSceneAfterTransientCleanup(scenePath, finishScenePreparation);
    }

    bool StudioRenderPath::ConsumeWindowsGameBuildRequest() noexcept
    {
        return std::exchange(windowsGameBuildRequested_, false);
    }

    void StudioRenderPath::SetWindowsGameBuildStatus(std::string message)
    {
        studioChrome_.SetStatusText(std::move(message));
        studioChrome_.SetActiveBottomTab(2, true);
    }

    void StudioRenderPath::RefreshAssetBrowser()
    {
        std::vector<RenegadeStudioChrome::AssetFolderRow> folders;
        std::vector<RenegadeStudioChrome::AssetCard> assets;

        if (session_ == nullptr || !session_->Projects().HasProject())
        {
            studioChrome_.SetAssetBrowserData(
                std::move(folders),
                std::move(assets),
                "NO PROJECT");
            return;
        }

        const auto snapshot = assetBrowserService_.Scan(
            session_->Projects().CurrentProject().rootPath,
            assetBrowserCurrentFolder_);
        if (!snapshot.succeeded)
        {
            studioChrome_.SetAssetBrowserData(
                std::move(folders),
                std::move(assets),
                "CONTENT UNAVAILABLE");
            studioChrome_.SetStatusText(
                "ASSET BROWSER // " + snapshot.error);
            return;
        }

        assetBrowserCurrentFolder_ = snapshot.currentFolder;
        folders.reserve(snapshot.folders.size());
        for (const auto& folder : snapshot.folders)
        {
            RenegadeStudioChrome::AssetFolderRow row;
            row.name = folder.name;
            row.relativePath = folder.projectRelativePath;
            row.depth = static_cast<int>(folder.depth);
            row.selected = folder.selected;
            folders.push_back(std::move(row));
        }

        assets.reserve(snapshot.assets.size());
        for (const auto& asset : snapshot.assets)
        {
            RenegadeStudioChrome::AssetCard card;
            card.name = asset.name;
            card.relativePath = asset.projectRelativePath;
            card.typeLabel =
                bridge::AssetBrowserService::TypeLabel(asset.type);
            card.directory = asset.directory;
            if (!asset.directory)
            {
                fs::path thumbnailPath =
                    fs::u8path(session_->Projects().CurrentProject().rootPath) /
                    fs::u8path(asset.projectRelativePath);
                thumbnailPath.replace_extension(".thumbnail.png");
                if (fs::exists(thumbnailPath))
                    card.thumbnail = wi::resourcemanager::Load(
                        thumbnailPath.generic_u8string());
            }
            assets.push_back(std::move(card));
        }

        studioChrome_.SetAssetBrowserData(
            std::move(folders),
            std::move(assets),
            snapshot.currentFolder);
        studioChrome_.SetStatusText(
            "ASSET BROWSER // " + snapshot.currentFolder);
    }

    void StudioRenderPath::SelectAssetBrowserFolder(
        const std::string& relativePath)
    {
        if (relativePath.empty())
        {
            return;
        }
        assetBrowserCurrentFolder_ = relativePath;
        RefreshAssetBrowser();
    }

    void StudioRenderPath::SelectAssetBrowserItem(
        const std::string& relativePath)
    {
        if (session_ == nullptr || relativePath.empty())
        {
            return;
        }

        const fs::path absolute =
            fs::u8path(session_->Projects().CurrentProject().rootPath) /
            fs::u8path(relativePath);
        if (fs::is_directory(absolute))
        {
            SelectAssetBrowserFolder(relativePath);
            return;
        }

        // V1 deliberately stops at real project browsing and selection.
        // Type-specific open/place/apply and drag payloads are the next slice.
        studioChrome_.SetStatusText(
            "ASSET SELECTED // " + relativePath);
    }

    void StudioRenderPath::CreateProject()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const std::string projectName = hubNewProjectNameInput_.GetText();
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
                RequestSceneReplacement(
                    [this, parentDirectory, projectName]()
                    {
                        if (!session_->Projects().CreateStoryFlowProject(
                                parentDirectory,
                                projectName))
                        {
                            hubMessageLabel_.font.params.color = WarningAmber;
                            hubMessageLabel_.SetText(
                                "PROJECT CREATE FAILED // " +
                                session_->Projects().LastError());
                            return;
                        }

                        ClearSelectionOutline();
                        if (!session_->CommitPendingProjectWithoutScene())
                        {
                            hubMessageLabel_.font.params.color = WarningAmber;
                            hubMessageLabel_.SetText(
                                "PROJECT HOME FAILED // " +
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
                if (descriptorPath.empty())
                    return;
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, descriptorPath](uint64_t)
                    {
                        RequestSceneReplacement(
                            [this, descriptorPath]()
                            {
                                hubMessageLabel_.font.params.color = HologramMuted;
                                hubMessageLabel_.SetText(
                                    "PROJECT OPENING // " +
                                    wi::helper::GetFileNameFromPath(descriptorPath));
                                OpenProjectDescriptor(descriptorPath);
                            });
                    });
            });
    }

    void StudioRenderPath::RequestSceneReplacement(
        std::function<void()> continuation)
    {
        if (session_ == nullptr || !continuation)
        {
            return;
        }

        // A running preview is a real pending scene edit. Commit it first so
        // the dirty-state question includes what the creator can currently
        // see in the viewport.
        StopSunPreview(true);
        if (!session_->Commands().IsDirty())
        {
            continuation();
            return;
        }

        const std::string currentPath = session_->Scenes().CurrentPath();
        const std::string sceneName = currentPath.empty()
            ? "the current scene"
            : "\"" + wi::helper::GetFileNameFromPath(currentPath) + "\"";
        const auto result = wi::helper::messageBoxCustom(
            "Do you want to save changes to " + sceneName + "?",
            "Unsaved changes",
            "YesNoCancel");
        if (result == wi::helper::MessageBoxResult::No ||
            result == wi::helper::MessageBoxResult::OK)
        {
            continuation();
            return;
        }
        if (result != wi::helper::MessageBoxResult::Yes)
        {
            return;
        }
        if (currentPath.empty())
        {
            SaveSceneAs(
                [continuation = std::move(continuation)](const bool saved)
                {
                    if (saved)
                    {
                        continuation();
                    }
                });
            return;
        }

        SaveSceneAfterTransientCleanup(
            currentPath,
            [continuation = std::move(continuation)](const bool saved)
            {
                if (saved)
                {
                    continuation();
                }
            });
    }

    void StudioRenderPath::OpenScene()
    {
        if (session_ == nullptr || sceneOpenInProgress_)
        {
            return;
        }

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::OPEN;
        params.description = "Renegade Scene (.wiscene)";
        params.extensions.push_back("wiscene");
        wi::helper::FileDialog(
            params,
            [this](const std::string& scenePath)
            {
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, scenePath](uint64_t)
                    {
                        RequestSceneReplacement(
                            [this, scenePath]()
                            {
                                BeginOpenScene(scenePath);
                            });
                    });
            });
    }

    void StudioRenderPath::BeginOpenScene(const std::string& scenePath)
    {
        if (session_ == nullptr || sceneOpenInProgress_)
        {
            return;
        }

        sceneOpenInProgress_ = true;
        openingScenePath_ = scenePath;
        sceneOpenWorkload_.priority = wi::jobsystem::Priority::Low;
        if (projectHubVisible_)
        {
            hubMessageLabel_.font.params.color = HologramMuted;
            hubMessageLabel_.SetText(
                "SCENE OPENING // " +
                wi::helper::GetFileNameFromPath(scenePath));
        }
        RefreshStatus();

        auto prepared = std::make_shared<bridge::PreparedSceneOpen>();
        wi::jobsystem::Execute(
            sceneOpenWorkload_,
            [this, scenePath, prepared](wi::jobsystem::JobArgs)
            {
                *prepared = session_->Documents().PrepareOpen(scenePath);
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, prepared](uint64_t)
                    {
                        CompleteOpenScene(std::move(*prepared));
                    });
            });
    }

    void StudioRenderPath::CompleteOpenScene(
        bridge::PreparedSceneOpen prepared)
    {
        sceneOpenInProgress_ = false;
        openingScenePath_.clear();

        if (session_ == nullptr)
        {
            return;
        }

        ClearSelectionOutline();
        if (!session_->Documents().CommitPreparedOpen(std::move(prepared)))
        {
            SyncSelectionOutline();
            if (projectHubVisible_)
            {
                hubMessageLabel_.font.params.color = WarningAmber;
                hubMessageLabel_.SetText(
                    "SCENE OPEN FAILED // " +
                    session_->Scenes().LastError());
            }
            RefreshStatus();
            RefreshInspector();
            return;
        }

        AdoptOpenedSceneCamera();
        RestoreGovernedMaterialTextures();
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        SetProjectHubVisible(false);
    }

    void StudioRenderPath::AdoptOpenedSceneCamera()
    {
        if (session_ == nullptr || camera == nullptr)
        {
            return;
        }

        const auto cameraEntity = session_->Documents().LastOpenedCamera();
        const auto& openedScene = session_->Scenes().GetScene();
        const auto* openedCamera =
            openedScene.cameras.GetComponent(cameraEntity);
        if (openedCamera == nullptr)
        {
            // No authored camera in the opened document (common for
            // terrain-only scenes that were never given an explicit
            // Camera entity). Wicked's terrain chunk streaming
            // (wi::terrain::Terrain::Generation_Update, run every real
            // frame from RenderPath3D::Update) evicts and permanently
            // discards any chunk whose distance from the *current*
            // editor camera exceeds its removal radius. Leaving the
            // camera wherever it happened to be from the previous
            // document means a freshly opened terrain scene has its
            // just-loaded, correctly-deserialized chunks evicted and
            // silently replaced with fresh procedural generation before
            // the user ever sees them - which reads as "the terrain
            // didn't save" even though the archive round-trip was
            // correct. Recenter over the terrain's own saved chunk
            // position instead of leaving the stale camera in place.
            AdoptOpenedSceneTerrainFallbackCamera(openedScene);
            return;
        }

        camera->Eye = openedCamera->Eye;
        camera->At = openedCamera->At;
        camera->Up = openedCamera->Up;
        camera->fov = openedCamera->fov;
        camera->zNearP = openedCamera->zNearP;
        camera->zFarP = openedCamera->zFarP;
        camera->focal_length = openedCamera->focal_length;
        camera->aperture_size = openedCamera->aperture_size;
        camera->aperture_shape = openedCamera->aperture_shape;
        camera->width = static_cast<float>(GetInternalResolution().x);
        camera->height = static_cast<float>(GetInternalResolution().y);

        const auto* openedTransform =
            openedScene.transforms.GetComponent(cameraEntity);
        if (openedTransform != nullptr)
        {
            editorCameraTransform_ = *openedTransform;
            camera->TransformCamera(editorCameraTransform_);
        }
        camera->UpdateCamera();
    }

    void StudioRenderPath::AdoptOpenedSceneTerrainFallbackCamera(
        const wi::scene::Scene& openedScene)
    {
        if (camera == nullptr || openedScene.terrains.GetCount() == 0)
        {
            return;
        }

        const wi::terrain::Terrain& terrain = openedScene.terrains[0];
        if (terrain.chunks.empty())
        {
            return;
        }

        // Prefer the chunk at the terrain's own saved center; fall back
        // to whichever loaded chunk is closest to it if that exact
        // coordinate was not generated/saved.
        auto best = terrain.chunks.find(terrain.center_chunk);
        if (best == terrain.chunks.end())
        {
            best = terrain.chunks.begin();
            int bestDist =
                std::max(
                    std::abs(terrain.center_chunk.x - best->first.x),
                    std::abs(terrain.center_chunk.z - best->first.z));
            for (auto it = terrain.chunks.begin();
                 it != terrain.chunks.end();
                 ++it)
            {
                const int dist = std::max(
                    std::abs(terrain.center_chunk.x - it->first.x),
                    std::abs(terrain.center_chunk.z - it->first.z));
                if (dist < bestDist)
                {
                    bestDist = dist;
                    best = it;
                }
            }
        }
        if (best == terrain.chunks.end())
        {
            return;
        }

        const XMFLOAT3 targetPosition = best->second.sphere.center;
        const float radius = std::max(best->second.sphere.radius, 1.0f);
        const XMVECTOR at = XMLoadFloat3(&targetPosition);
        const XMVECTOR eye =
            at +
            XMVectorSet(0.0f, radius * 1.5f, -radius * 2.5f, 0.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
        editorCameraTransform_.ClearTransform();
        editorCameraTransform_.MatrixTransform(
            XMMatrixInverse(nullptr, view));
        editorCameraTransform_.UpdateTransform();
        camera->TransformCamera(editorCameraTransform_);
        camera->width = static_cast<float>(GetInternalResolution().x);
        camera->height = static_cast<float>(GetInternalResolution().y);
        camera->UpdateCamera();
    }

    void StudioRenderPath::BeginProjectLoad(
        const std::string& descriptorPath)
    {
        if (session_ == nullptr || descriptorPath.empty() ||
            projectLoadingOverlay_.IsBlocking() ||
            wi::jobsystem::IsBusy(projectLoadWorkload_))
        {
            return;
        }

        projectLoadingOverlay_.Begin(
            wi::helper::GetFileNameFromPath(descriptorPath));
        projectHubChrome_.SetVisible(false);
        hubNewProjectNameInput_.SetVisible(false);
        hubNewProjectConfirmButton_.SetVisible(false);
        hubNewProjectCancelButton_.SetVisible(false);

        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::ValidatingProject);
        if (!session_->Projects().OpenProject(descriptorPath))
        {
            projectLoadingOverlay_.Fail(
                "Project validation failed: " + session_->Projects().LastError());
            return;
        }

        auto operation = std::make_shared<ProjectLoadOperation>();
        operation->descriptorPath = descriptorPath;
        operation->startupScenePath = session_->Projects().StartupScenePath();
        operation->project = session_->Projects().PendingProject();
        operation->storyFlowNative = operation->startupScenePath.empty();
        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::PreparingScene);

        projectLoadWorkload_.priority = wi::jobsystem::Priority::Low;
        wi::jobsystem::Execute(
            projectLoadWorkload_,
            [this, operation](wi::jobsystem::JobArgs)
            {
                if (operation->storyFlowNative)
                {
                    std::string resolvedFlow;
                    bridge::FlowDocument flow;
                    if (!bridge::ResolveStoryFlowDocumentPath(
                            operation->project.rootPath,
                            operation->project.projectId,
                            operation->project.startupFlowId,
                            operation->project.startupFlow,
                            resolvedFlow,
                            operation->error) ||
                        !bridge::ReadFlowDocument(
                            resolvedFlow,
                            operation->project.projectId,
                            flow,
                            operation->error))
                    {
                        operation->error =
                            "The Story Flow project home could not be prepared: " +
                            operation->error;
                    }
                }
                else
                {
                    operation->preparedScene = session_->Documents().PrepareOpen(
                        operation->startupScenePath);
                    if (!operation->preparedScene.IsReady())
                    {
                        operation->error = operation->preparedScene.Error().empty()
                            ? "The startup scene could not be prepared."
                            : operation->preparedScene.Error();
                    }
                    else
                    {
                        auto* candidate =
                            operation->preparedScene.MutablePreparedScene();
                        if (candidate != nullptr)
                        {
                            projectLoadingOverlay_.SetPhase(
                                RenegadeProjectLoadingOverlay::Phase::RestoringAssets,
                                0, 0);
                            operation->textureRestore =
                                bridge::RestoreMaterialTextureBindings(
                                    *candidate,
                                    operation->project.rootPath,
                                    operation->project.projectId,
                                    {},
                                    [this](const std::size_t completed,
                                        const std::size_t total)
                                    {
                                        projectLoadingOverlay_.SetPhase(
                                            RenegadeProjectLoadingOverlay::Phase::RestoringAssets,
                                            completed, total);
                                    });
                        }
                    }
                }

                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, operation](std::uint64_t)
                    {
                        CompleteProjectLoad(operation);
                    });
            });
    }

    void StudioRenderPath::CompleteProjectLoad(
        std::shared_ptr<ProjectLoadOperation> operation)
    {
        if (session_ == nullptr || !operation)
            return;

        if (!operation->error.empty())
        {
            session_->Projects().DiscardPendingProject();
            projectLoadingOverlay_.Fail(operation->error);
            return;
        }

        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::Finalising);
        ClearSelectionOutline();
        const bool adopted = operation->storyFlowNative
            ? session_->CommitPendingProjectWithoutScene()
            : session_->CommitPendingProjectScene(
                std::move(operation->preparedScene));
        if (!adopted)
        {
            SyncSelectionOutline();
            projectLoadingOverlay_.Fail(
                session_->Scenes().LastError().empty()
                    ? "The prepared project could not be adopted."
                    : session_->Scenes().LastError());
            return;
        }

        if (!operation->storyFlowNative)
        {
            AdoptOpenedSceneCamera();
        }
        SetEnvironmentWorkspaceActive(false);
        SetTerrainWorkspaceActive(false);
        workspaceTitle_.SetText(
            "RENEGADE STUDIO // " +
            session_->Projects().CurrentProject().name);
        hubMessageLabel_.font.params.color =
            operation->storyFlowNative || operation->textureRestore.succeeded
            ? HologramMuted : WarningAmber;
        hubMessageLabel_.SetText(
            operation->storyFlowNative || operation->textureRestore.succeeded
            ? "PROJECT ONLINE // " + session_->Projects().CurrentProject().descriptorPath
            : "PROJECT ONLINE // GOVERNED RESOURCE WARNING // " +
                operation->textureRestore.error);
        selectedRecentProject_ = -1;
        RefreshProjectHub();
        RefreshAssetBrowser();
        RefreshHierarchy();
        RefreshInspector();
        RefreshStatus();
        SetProjectHubVisible(false);
        projectLoadingOverlay_.SetPhase(
            RenegadeProjectLoadingOverlay::Phase::Ready);
    }

    void StudioRenderPath::OpenProjectDescriptor(
        const std::string& descriptorPath)
    {
        BeginProjectLoad(descriptorPath);
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

        const std::string descriptorPath = recent[index].descriptorPath;
        RequestSceneReplacement(
            [this, descriptorPath]()
            {
                OpenProjectDescriptor(descriptorPath);
            });
    }

    void StudioRenderPath::ReturnToProjectHub()
    {
        if (session_ == nullptr)
        {
            return;
        }

        RequestSceneReplacement(
            [this]()
            {
                selectedRecentProject_ = -1;
                hubMessageLabel_.font.params.color = HologramMuted;
                hubMessageLabel_.SetText(
                    "PROJECT HUB ONLINE // SELECT AN OPERATION");
                RefreshProjectHub();
                SetProjectHubVisible(true);
            });
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
        projectHubPanel_.SetVisible(false);
        projectHubChrome_.SetVisible(visible);
        if (!visible)
        {
            hubNewProjectMode_ = false;
            projectHubChrome_.SetNewProjectMode(false);
        }
        hubNewProjectNameInput_.SetVisible(visible && hubNewProjectMode_);
        hubNewProjectConfirmButton_.SetVisible(visible && hubNewProjectMode_);
        hubNewProjectCancelButton_.SetVisible(visible && hubNewProjectMode_);
        // Stock workspace surfaces stay hidden. RenegadeStudioChrome owns the
        // shell, while the opaque Inspector host schedules the functional
        // controls rendered by Renegade subclasses.
        toolbarPanel_.SetVisible(false);
        hierarchyPanel_.SetVisible(false);
        inspectorPanel_.SetVisible(!visible);
        hierarchySearch_.SetVisible(!visible);
        contentPanel_.SetVisible(false);
        studioChrome_.SetVisible(!visible);

        // The stock overlay collides with Renegade's owned shell. Live FPS is
        // rendered by RenegadeStudioChrome's status bar instead and therefore
        // hides automatically with the rest of the workspace on Project Hub.
        if (diagnostics_ != nullptr)
        {
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
        gizmoSuppressedForCameraView_ = false;
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
        if (session_ != nullptr)
        {
            auto& scene = session_->Scenes().GetScene();
            const std::size_t count = std::min(
                outlinedEntities_.size(), outlinedEntityPreviousStencils_.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                auto* object = scene.objects.GetComponent(outlinedEntities_[index]);
                if (object != nullptr)
                    object->SetUserStencilRef(outlinedEntityPreviousStencils_[index]);
            }
        }
        outlinedSelection_ = wi::ecs::INVALID_ENTITY;
        outlinedEntities_.clear();
        outlinedEntityPreviousStencils_.clear();
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
        if (selected == outlinedSelection_)
            return;

        ClearSelectionOutline();
        if (session_ == nullptr || selected == wi::ecs::INVALID_ENTITY)
            return;

        auto& scene = session_->Scenes().GetScene();
        const wi::ecs::Entity reusableRoot =
            ResolveReusableSelectionRoot(scene, selected);
        if (reusableRoot == selected)
        {
            std::vector<wi::ecs::Entity> renderObjects;
            CollectReusableSelectionObjects(scene, reusableRoot, renderObjects);
            for (const wi::ecs::Entity entity : renderObjects)
            {
                auto* object = scene.objects.GetComponent(entity);
                if (object == nullptr)
                    continue;
                outlinedEntities_.push_back(entity);
                outlinedEntityPreviousStencils_.push_back(object->userStencilRef);
                object->SetUserStencilRef(SelectionStencilReference);
            }
            if (!outlinedEntities_.empty())
                outlinedSelection_ = selected;
            return;
        }

        auto* object = scene.objects.GetComponent(selected);
        if (object == nullptr)
            return;
        outlinedSelection_ = selected;
        outlinedEntities_.push_back(selected);
        outlinedEntityPreviousStencils_.push_back(object->userStencilRef);
        object->SetUserStencilRef(SelectionStencilReference);
    }

    void StudioRenderPath::SaveSceneAfterTransientCleanup(
        const std::string& scenePath,
        std::function<void(bool)> completion)
    {
        if (session_ == nullptr)
        {
            if (completion)
                completion(false);
            return;
        }

        if (detail::CreatorAssetDragPreviewBlocksSave())
        {
            detail::ClearCreatorAssetDragPreview();
            studioChrome_.SetStatusText(
                "SAVE // WAITING FOR TRANSIENT ASSET PREVIEW CLEANUP");
            wi::eventhandler::Subscribe_Once(
                wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                [this, scenePath, completion](std::uint64_t)
                {
                    SaveSceneAfterTransientCleanup(scenePath, completion);
                });
            return;
        }

        ClearSelectionOutline();
        const bool saved = session_->SaveScene(scenePath);
        SyncSelectionOutline();
        RefreshStatus();
        RefreshInspector();
        if (completion)
            completion(saved);
    }

    void StudioRenderPath::SaveScene()
    {
        if (session_ == nullptr)
            return;

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            SaveSceneAs();
            return;
        }

        StopSunPreview(true);
        SaveSceneAfterTransientCleanup(scenePath);
    }

    void StudioRenderPath::SaveSceneAs(
        std::function<void(bool)> completion)
    {
        if (session_ == nullptr)
        {
            if (completion)
                completion(false);
            return;
        }

        StopSunPreview(true);

        wi::helper::FileDialogParams params;
        params.type = wi::helper::FileDialogParams::SAVE;
        params.description = "Renegade Scene (.wiscene)";
        params.extensions.push_back("wiscene");
        wi::helper::FileDialog(
            params,
            [this, completion](const std::string& selectedPath)
            {
                const std::string scenePath =
                    wi::helper::ForceExtension(selectedPath, "wiscene");
                wi::eventhandler::Subscribe_Once(
                    wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                    [this, scenePath, completion](std::uint64_t)
                    {
                        SaveSceneAfterTransientCleanup(
                            scenePath,
                            completion);
                    });
            },
            [completion]()
            {
                if (completion)
                    completion(false);
            });
    }

    void StudioRenderPath::ReopenScene()
    {
        if (session_ == nullptr)
        {
            return;
        }

        const std::string scenePath = session_->Scenes().CurrentPath();
        if (scenePath.empty())
        {
            session_->ReloadScene();
            RefreshStatus();
            return;
        }

        RequestSceneReplacement(
            [this, scenePath]()
            {
                BeginOpenScene(scenePath);
            });
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

    void StudioApplication::SetExitRequestHandler(std::function<void()> handler)
    {
        renderer_.SetExitRequestHandler(std::move(handler));
    }

    void StudioApplication::RequestExit()
    {
        renderer_.RequestExit();
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
        infoDisplay.fpsinfo = false;
        infoDisplay.size = 14;

        session_.Projects().Initialize("Saved/RenegadeStudio.ini");
        PrepareProvingGround();

        renderer_.BindSession(session_);
        renderer_.BindDiagnostics(infoDisplay);
        renderer_.init(canvas);
        renderer_.Load();

        storyFlowIntegration_.OnScreenEditorOpen(
            [this](const StoryFlowScreenEditorHandoff& handoff)
            {
                if (!session_.Projects().HasProject()) return;
                const auto& project = session_.Projects().CurrentProject();
                std::string error;
                if (!screenEditorRenderer_.OpenScreen(
                        handoff, project.rootPath, project.projectId, error))
                {
                    wi::backlog::post(
                        "Renegade Screen Editor: " + error,
                        wi::backlog::LogLevel::Error);
                    return;
                }
                storyFlowIntegration_.RequestScreenEditor();
            });
        screenEditorRenderer_.OnReturnRequested([this]()
        {
            storyFlowIntegration_.RequestStoryFlow();
        });
        ActivatePath(&renderer_);
    }
}
