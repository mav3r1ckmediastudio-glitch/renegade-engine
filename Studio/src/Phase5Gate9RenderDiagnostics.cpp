#include "StudioApplication.h"

#include <wiProfiler.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

namespace
{
    constexpr wi::Color RenderMuted = wi::Color(178, 178, 176, 255);
    constexpr wi::Color ProfilerText = wi::Color(244, 244, 244, 255);
    constexpr wi::Color ProfilerBackground = wi::Color(8, 12, 16, 224);

    enum class DiagnosticShading : std::uint64_t
    {
        Normal = 0,
        NoAlbedo = 1,
        DiffuseOnly = 2,
        Unlit = 3,
    };

    DiagnosticShading CurrentShading() noexcept
    {
        if (wi::renderer::IsForceUnlit())
            return DiagnosticShading::Unlit;
        if (wi::renderer::IsForceDiffuseLighting())
            return DiagnosticShading::DiffuseOnly;
        if (wi::renderer::IsDisableAlbedoMaps())
            return DiagnosticShading::NoAlbedo;
        return DiagnosticShading::Normal;
    }

    void ApplyShading(const DiagnosticShading mode) noexcept
    {
        wi::renderer::SetDisableAlbedoMaps(false);
        wi::renderer::SetForceDiffuseLighting(false);
        wi::renderer::SetForceUnlit(false);

        switch (mode)
        {
        case DiagnosticShading::NoAlbedo:
            wi::renderer::SetDisableAlbedoMaps(true);
            break;
        case DiagnosticShading::DiffuseOnly:
            wi::renderer::SetForceDiffuseLighting(true);
            break;
        case DiagnosticShading::Unlit:
            wi::renderer::SetForceUnlit(true);
            break;
        case DiagnosticShading::Normal:
        default:
            break;
        }
    }

    void ResetNativeDiagnostics() noexcept
    {
        wi::renderer::SetWireframeMode(wi::renderer::WIREFRAME_DISABLED);
        ApplyShading(DiagnosticShading::Normal);
        wi::renderer::SetToDrawDebugEnvProbes(false);
        wi::renderer::SetToDrawDebugCameras(false);
        wi::renderer::SetToDrawDebugPartitionTree(false);
        wi::renderer::SetRaytraceDebugBVHVisualizerEnabled(false);
        wi::renderer::SetFreezeCullingCameraEnabled(false);
        wi::renderer::SetDebugLightCulling(false);
        wi::renderer::SetTemporalAADebugEnabled(false);
        wi::renderer::SetSurfelGIDebugEnabled(SURFEL_DEBUG_NONE);
        wi::profiler::SetEnabled(false);
    }

    class Gate9ProfilerOverlay final : public wi::gui::Widget
    {
    public:
        void Render(
            const wi::Canvas& canvas,
            const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;

            ApplyScissor(canvas, scissorRect, cmd);
            wi::profiler::SetBackgroundColor(ProfilerBackground);
            wi::profiler::SetTextColor(ProfilerText);
            wi::profiler::DrawData(
                canvas,
                translation.x + 8.0f,
                translation.y + 8.0f,
                cmd);
        }
    };

    struct Gate9Controls
    {
        wi::gui::Label viewLabel;
        renegade::studio::SceneInspectorComboBox viewMode;
        renegade::studio::SceneInspectorComboBox shadingMode;

        wi::gui::Label sceneLabel;
        renegade::studio::SceneInspectorCheckBox envProbes;
        renegade::studio::SceneInspectorCheckBox cameras;
        renegade::studio::SceneInspectorCheckBox partitionTree;
        renegade::studio::SceneInspectorCheckBox raytraceBvh;
        renegade::studio::SceneInspectorCheckBox freezeCulling;

        wi::gui::Label rendererLabel;
        renegade::studio::SceneInspectorCheckBox lightCulling;
        renegade::studio::SceneInspectorCheckBox taaDebug;
        renegade::studio::SceneInspectorComboBox surfelDebug;
        wi::gui::Label status;

        wi::gui::Label profilerLabel;
        renegade::studio::SceneInspectorCheckBox profilerEnabled;
        Gate9ProfilerOverlay profilerOverlay;

        renegade::studio::SceneInspectorButton reset;

        bool identityCaptured = false;
        std::string projectIdentity;
        std::string sceneIdentity;

        ~Gate9Controls()
        {
            // Studio process teardown must not leave any renderer-owned global
            // diagnostic switch or the native profiler enabled.
            ResetNativeDiagnostics();
        }
    };

    std::unique_ptr<Gate9Controls> gate9;

    Gate9Controls& Controls()
    {
        if (!gate9)
            gate9 = std::make_unique<Gate9Controls>();
        return *gate9;
    }

    void LayoutProfilerOverlay(
        Gate9ProfilerOverlay& overlay,
        const XMFLOAT4& viewportBounds)
    {
        constexpr float Margin = 12.0f;
        constexpr float PreferredWidth = 460.0f;
        constexpr float PreferredHeight = 620.0f;

        const float availableWidth = std::max(
            160.0f,
            viewportBounds.z - viewportBounds.x - Margin * 2.0f);
        const float availableHeight = std::max(
            120.0f,
            viewportBounds.w - viewportBounds.y - Margin * 2.0f);

        overlay.SetPos(XMFLOAT2(
            viewportBounds.x + Margin,
            viewportBounds.y + Margin));
        overlay.SetSize(XMFLOAT2(
            std::min(PreferredWidth, availableWidth),
            std::min(PreferredHeight, availableHeight)));
    }
}

namespace renegade::studio
{
    void StudioRenderPath::CreateGate9DiagnosticsControls()
    {
        auto& controls = Controls();

        const auto createSection = [this](
            wi::gui::Label& label,
            const char* name,
            const char* text)
        {
            label.Create(name);
            label.SetText(text);
            label.font.params.size = 13;
            label.font.params.color = RenderMuted;
            label.font.params.h_align = wi::font::WIFALIGN_LEFT;
            label.SetColor(wi::Color::Transparent());
            renderWorkspacePanel_.AddWidget(&label);
        };

        createSection(
            controls.viewLabel,
            "Render Diagnostics View Section",
            "DIAGNOSTICS // VIEW");

        controls.viewMode.Create("Render Diagnostics View Mode");
        controls.viewMode.AddItem(
            "SHADED",
            static_cast<std::uint64_t>(wi::renderer::WIREFRAME_DISABLED));
        controls.viewMode.AddItem(
            "WIREFRAME OVERLAY",
            static_cast<std::uint64_t>(wi::renderer::WIREFRAME_OVERLAY));
        controls.viewMode.AddItem(
            "WIREFRAME ONLY",
            static_cast<std::uint64_t>(wi::renderer::WIREFRAME_ONLY));
        controls.viewMode.SetTooltip(
            "Studio-only native Wicked wireframe diagnostic. Never serialized or used by Runtime.");
        controls.viewMode.OnSelect([this](const wi::gui::EventArgs& args)
        {
            wi::renderer::SetWireframeMode(
                static_cast<wi::renderer::WIREFRAME_MODE>(args.userdata));
            RefreshGate9DiagnosticsControls();
        });
        renderWorkspacePanel_.AddWidget(&controls.viewMode);

        controls.shadingMode.Create("Render Diagnostics Shading Mode");
        controls.shadingMode.AddItem(
            "NORMAL",
            static_cast<std::uint64_t>(DiagnosticShading::Normal));
        controls.shadingMode.AddItem(
            "NO ALBEDO",
            static_cast<std::uint64_t>(DiagnosticShading::NoAlbedo));
        controls.shadingMode.AddItem(
            "DIFFUSE ONLY",
            static_cast<std::uint64_t>(DiagnosticShading::DiffuseOnly));
        controls.shadingMode.AddItem(
            "UNLIT",
            static_cast<std::uint64_t>(DiagnosticShading::Unlit));
        controls.shadingMode.SetTooltip(
            "Mutually-exclusive native Wicked shading inspection mode.");
        controls.shadingMode.OnSelect([this](const wi::gui::EventArgs& args)
        {
            ApplyShading(static_cast<DiagnosticShading>(args.userdata));
            RefreshGate9DiagnosticsControls();
        });
        renderWorkspacePanel_.AddWidget(&controls.shadingMode);

        createSection(
            controls.sceneLabel,
            "Render Diagnostics Scene Section",
            "DIAGNOSTICS // SCENE");

        const auto addToggle = [this](
            SceneInspectorCheckBox& checkbox,
            const char* name,
            const char* tooltip,
            auto callback)
        {
            checkbox.Create(name);
            checkbox.SetTooltip(tooltip);
            checkbox.OnClick([this, callback](const wi::gui::EventArgs& args)
            {
                callback(args.bValue);
                RefreshGate9DiagnosticsControls();
            });
            renderWorkspacePanel_.AddWidget(&checkbox);
        };

        addToggle(
            controls.envProbes,
            "Environment probes: ",
            "Show Wicked environment-probe debug visualizers.",
            [](const bool enabled)
            {
                wi::renderer::SetToDrawDebugEnvProbes(enabled);
            });
        addToggle(
            controls.cameras,
            "Camera frusta: ",
            "Show Wicked camera/frustum debug visualizers.",
            [](const bool enabled)
            {
                wi::renderer::SetToDrawDebugCameras(enabled);
            });
        addToggle(
            controls.partitionTree,
            "Spatial partition: ",
            "Show Wicked's scene spatial-partition debug tree.",
            [](const bool enabled)
            {
                wi::renderer::SetToDrawDebugPartitionTree(enabled);
            });
        addToggle(
            controls.raytraceBvh,
            "Raytrace BVH: ",
            "Show Wicked's ray-tracing BVH visualizer. Availability depends on the live renderer/device path.",
            [](const bool enabled)
            {
                wi::renderer::SetRaytraceDebugBVHVisualizerEnabled(enabled);
            });
        addToggle(
            controls.freezeCulling,
            "Freeze culling camera: ",
            "Freeze the culling camera so visibility decisions can be inspected while moving the editor camera.",
            [](const bool enabled)
            {
                wi::renderer::SetFreezeCullingCameraEnabled(enabled);
            });

        createSection(
            controls.rendererLabel,
            "Render Diagnostics Renderer Section",
            "DIAGNOSTICS // RENDERER");

        addToggle(
            controls.lightCulling,
            "Light-culling heatmap: ",
            "Toggle Wicked's screen-space light-culling debug heatmap.",
            [](const bool enabled)
            {
                wi::renderer::SetDebugLightCulling(enabled);
            });
        addToggle(
            controls.taaDebug,
            "TAA jitter/history: ",
            "Disable TAA history blending so camera subpixel jitter is visible. Enabled only while the authored AA mode is TAA.",
            [](const bool enabled)
            {
                wi::renderer::SetTemporalAADebugEnabled(enabled);
            });

        controls.surfelDebug.Create("Render Diagnostics Surfel GI");
        controls.surfelDebug.AddItem(
            "SURFEL // NONE",
            static_cast<std::uint64_t>(SURFEL_DEBUG_NONE));
        controls.surfelDebug.AddItem(
            "SURFEL // NORMAL",
            static_cast<std::uint64_t>(SURFEL_DEBUG_NORMAL));
        controls.surfelDebug.AddItem(
            "SURFEL // COLOR",
            static_cast<std::uint64_t>(SURFEL_DEBUG_COLOR));
        controls.surfelDebug.AddItem(
            "SURFEL // POINT",
            static_cast<std::uint64_t>(SURFEL_DEBUG_POINT));
        controls.surfelDebug.AddItem(
            "SURFEL // RANDOM",
            static_cast<std::uint64_t>(SURFEL_DEBUG_RANDOM));
        controls.surfelDebug.AddItem(
            "SURFEL // HEATMAP",
            static_cast<std::uint64_t>(SURFEL_DEBUG_HEATMAP));
        controls.surfelDebug.AddItem(
            "SURFEL // INCONSISTENCY",
            static_cast<std::uint64_t>(SURFEL_DEBUG_INCONSISTENCY));
        controls.surfelDebug.SetTooltip(
            "Native Wicked Surfel GI visualization. Enabled only while Surfel GI is active.");
        controls.surfelDebug.OnSelect([this](const wi::gui::EventArgs& args)
        {
            wi::renderer::SetSurfelGIDebugEnabled(
                static_cast<SURFEL_DEBUG>(args.userdata));
            RefreshGate9DiagnosticsControls();
        });
        renderWorkspacePanel_.AddWidget(&controls.surfelDebug);

        controls.status.Create("Render Diagnostics Status");
        controls.status.font.params.size = 12;
        controls.status.font.params.color = RenderMuted;
        controls.status.font.params.h_align = wi::font::WIFALIGN_LEFT;
        controls.status.SetColor(wi::Color::Transparent());
        controls.status.SetWrapEnabled(true);
        renderWorkspacePanel_.AddWidget(&controls.status);

        createSection(
            controls.profilerLabel,
            "Render Diagnostics Profiler Section",
            "DIAGNOSTICS // PROFILER");

        controls.profilerEnabled.Create("CPU/GPU profiler overlay: ");
        controls.profilerEnabled.SetTooltip(
            "Show Wicked's native CPU/GPU profiler data in a Renegade-owned Scene viewport overlay. Never serialized or enabled in Runtime.");
        controls.profilerEnabled.OnClick([this](const wi::gui::EventArgs& args)
        {
            wi::profiler::SetEnabled(args.bValue);
            Controls().profilerOverlay.SetVisible(args.bValue);
            RefreshGate9DiagnosticsControls();
        });
        renderWorkspacePanel_.AddWidget(&controls.profilerEnabled);

        controls.profilerOverlay.SetVisible(false);
        controls.profilerOverlay.SetSize(XMFLOAT2(460.0f, 620.0f));
        auto& gui = GetGUI();
        gui.AddWidget(&controls.profilerOverlay);
        // Wicked renders top-level GUI widgets in reverse registration order.
        // Re-register the Renegade chrome last so it renders first; the profiler
        // can then draw over the Scene viewport while remaining clipped away
        // from the owned sidebars/status chrome.
        gui.RemoveWidget(&studioChrome_);
        gui.AddWidget(&studioChrome_);

        controls.reset.Create("Render Diagnostics Reset");
        controls.reset.SetText("RESET DIAGNOSTICS");
        controls.reset.SetTooltip(
            "Return all Gate 9 Studio-only renderer diagnostics to their clean defaults.");
        controls.reset.OnClick([this](const wi::gui::EventArgs&)
        {
            ResetGate9Diagnostics();
            studioChrome_.SetStatusText("RENDER // DIAGNOSTICS RESET");
            RefreshGate9DiagnosticsControls();
        });
        renderWorkspacePanel_.AddWidget(&controls.reset);

        ResetGate9Diagnostics();
        RefreshGate9DiagnosticsControls();
    }

    void StudioRenderPath::LayoutGate9DiagnosticsControls(
        const float fieldWidth,
        float& y)
    {
        auto& controls = Controls();
        const auto section = [fieldWidth, &y](wi::gui::Widget& widget)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, 20.0f));
            y += 24.0f;
        };
        const auto full = [fieldWidth, &y](wi::gui::Widget& widget)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, 28.0f));
            y += 32.0f;
        };

        section(controls.viewLabel);
        full(controls.viewMode);
        full(controls.shadingMode);
        section(controls.sceneLabel);
        full(controls.envProbes);
        full(controls.cameras);
        full(controls.partitionTree);
        full(controls.raytraceBvh);
        full(controls.freezeCulling);
        section(controls.rendererLabel);
        full(controls.lightCulling);
        full(controls.taaDebug);
        full(controls.surfelDebug);
        controls.status.SetPos(XMFLOAT2(12.0f, y));
        controls.status.SetSize(XMFLOAT2(fieldWidth, 42.0f));
        y += 46.0f;
        section(controls.profilerLabel);
        full(controls.profilerEnabled);
        full(controls.reset);

        LayoutProfilerOverlay(controls.profilerOverlay, viewportBounds_);
    }

    void StudioRenderPath::RefreshGate9DiagnosticsControls()
    {
        auto& controls = Controls();

        // Renderer debug flags are process-global in Wicked. Track the governed
        // project/scene identity and fail closed if Studio replaces either one
        // while this render path remains alive.
        std::string projectIdentity;
        std::string sceneIdentity;
        if (session_ != nullptr)
        {
            if (session_->Projects().HasProject())
            {
                const auto& project = session_->Projects().CurrentProject();
                projectIdentity = !project.projectId.empty()
                    ? project.projectId
                    : project.descriptorPath;
            }
            sceneIdentity = session_->Scenes().CurrentPath();
        }

        if (!controls.identityCaptured)
        {
            controls.identityCaptured = true;
            controls.projectIdentity = projectIdentity;
            controls.sceneIdentity = sceneIdentity;
        }
        else if (
            controls.projectIdentity != projectIdentity ||
            controls.sceneIdentity != sceneIdentity)
        {
            ResetGate9Diagnostics();
            controls.projectIdentity = projectIdentity;
            controls.sceneIdentity = sceneIdentity;
        }

        if (pathTracePreviewActive_)
            ResetGate9Diagnostics();

        const bool interactive = renderWorkspaceActive_ && !pathTracePreviewActive_;
        controls.viewMode.SetEnabled(interactive);
        controls.shadingMode.SetEnabled(interactive);
        controls.envProbes.SetEnabled(interactive);
        controls.cameras.SetEnabled(interactive);
        controls.partitionTree.SetEnabled(interactive);
        controls.raytraceBvh.SetEnabled(interactive);
        controls.freezeCulling.SetEnabled(interactive);
        controls.lightCulling.SetEnabled(interactive);
        controls.profilerEnabled.SetEnabled(interactive);
        controls.reset.SetEnabled(interactive);

        const bool taa = session_ != nullptr &&
            bridge::CaptureRenderSettings(session_->Scenes().GetScene()).antiAliasing ==
                bridge::AntiAliasingMode::TAA;
        if (!taa && wi::renderer::GetTemporalAADebugEnabled())
            wi::renderer::SetTemporalAADebugEnabled(false);
        controls.taaDebug.SetEnabled(interactive && taa);

        const bool surfel = wi::renderer::GetSurfelGIEnabled();
        if (!surfel && wi::renderer::GetSurfelGIDebugEnabled() != SURFEL_DEBUG_NONE)
            wi::renderer::SetSurfelGIDebugEnabled(SURFEL_DEBUG_NONE);
        controls.surfelDebug.SetEnabled(interactive && surfel);

        const bool profiler = wi::profiler::IsEnabled();
        controls.profilerEnabled.SetCheck(profiler);
        controls.profilerOverlay.SetVisible(interactive && profiler);
        LayoutProfilerOverlay(controls.profilerOverlay, viewportBounds_);

        controls.viewMode.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(wi::renderer::GetWireframeMode()));
        controls.shadingMode.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(CurrentShading()));
        controls.envProbes.SetCheck(wi::renderer::GetToDrawDebugEnvProbes());
        controls.cameras.SetCheck(wi::renderer::GetToDrawDebugCameras());
        controls.partitionTree.SetCheck(wi::renderer::GetToDrawDebugPartitionTree());
        controls.raytraceBvh.SetCheck(
            wi::renderer::GetRaytraceDebugBVHVisualizerEnabled());
        controls.freezeCulling.SetCheck(
            wi::renderer::GetFreezeCullingCameraEnabled());
        controls.lightCulling.SetCheck(wi::renderer::GetDebugLightCulling());
        controls.taaDebug.SetCheck(wi::renderer::GetTemporalAADebugEnabled());
        controls.surfelDebug.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(wi::renderer::GetSurfelGIDebugEnabled()));

        if (pathTracePreviewActive_)
        {
            controls.status.SetText(
                "DIAGNOSTICS DISABLED // RETURN TO REALTIME REFERENCE VIEW");
        }
        else if (!taa && !surfel)
        {
            controls.status.SetText(
                "REALTIME // TAA AND SURFEL-SPECIFIC DEBUG CONTROLS ARE CONTEXT-GATED");
        }
        else
        {
            controls.status.SetText("REALTIME // STUDIO-ONLY // NEVER SAVED TO SCENE");
        }
    }

    void StudioRenderPath::ResetGate9Diagnostics()
    {
        ResetNativeDiagnostics();
        if (gate9)
            gate9->profilerOverlay.SetVisible(false);
    }
}
