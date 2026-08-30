#include "StudioApplication.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
    constexpr wi::Color RenderMuted = wi::Color(178, 178, 176, 255);
}

namespace renegade::studio
{
    void StudioRenderPath::CreateGate7RenderControls()
    {
        const auto createSection = [this](wi::gui::Label& label, const char* name, const char* text)
        {
            label.Create(name);
            label.SetText(text);
            label.font.params.size = 13;
            label.font.params.color = RenderMuted;
            label.font.params.h_align = wi::font::WIFALIGN_LEFT;
            label.SetColor(wi::Color::Transparent());
            renderWorkspacePanel_.AddWidget(&label);
        };
        const auto createToggle = [this](SceneInspectorCheckBox& checkbox, const char* name, const char* tooltip,
                                         auto mutate)
        {
            checkbox.Create(name);
            checkbox.SetTooltip(tooltip);
            checkbox.OnClick([this, mutate](const wi::gui::EventArgs& args)
            {
                if (session_ == nullptr)
                    return;
                auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
                mutate(state, args.bValue);
                CommitRenderSettings(state);
            });
            renderWorkspacePanel_.AddWidget(&checkbox);
        };
        const auto createRange = [this](SceneInspectorSlider& slider, const char* name, const char* label,
                                        const char* tooltip, const RenderField field,
                                        const float minimum, const float maximum, const float steps)
        {
            slider.Create(minimum, maximum, minimum, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnDragStarted([this, field](const float) { BeginRenderSlider(field); });
            slider.OnValuePreview([this, field](const float value) { PreviewRenderSlider(field, value); });
            slider.OnValueCommitted([this, field](const float value) { CommitRenderSlider(field, value); });
            renderWorkspacePanel_.AddWidget(&slider);
        };

        createSection(renderRayTracingLabel_, "Render Ray Tracing Section", "RAY TRACING // REALTIME");
        renderRayTracingCapability_.Create("Render Ray Tracing Capability");
        renderRayTracingCapability_.font.params.size = 12;
        renderRayTracingCapability_.font.params.color = RenderMuted;
        renderRayTracingCapability_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        renderRayTracingCapability_.SetColor(wi::Color::Transparent());
        renderWorkspacePanel_.AddWidget(&renderRayTracingCapability_);

        createToggle(renderRaytracedShadowsEnabled_, "Ray-traced shadows: ",
            "Replace shadow-map lighting with Wicked hardware ray-traced shadows when supported.",
            [](bridge::RenderSettingsState& state, bool value) { state.raytracedShadowsEnabled = value; });
        createToggle(renderRaytracedReflectionsEnabled_, "Ray-traced reflections: ",
            "Enable Wicked hardware ray-traced reflections.",
            [](bridge::RenderSettingsState& state, bool value) { state.raytracedReflectionsEnabled = value; });
        createRange(renderRaytracedReflectionsRange_, "Render RT Reflection Range", "RT REFLECTIONS // RANGE",
            "Maximum ray distance for Wicked ray-traced reflections.",
            RenderField::RaytracedReflectionsRange, 1.0f, 10000.0f, 1000.0f);
        renderRaytracedReflectionsQuality_.Create("Render RT Reflection Quality");
        renderRaytracedReflectionsQuality_.AddItem("LOW", static_cast<std::uint64_t>(bridge::RenderQuality::Low));
        renderRaytracedReflectionsQuality_.AddItem("MEDIUM", static_cast<std::uint64_t>(bridge::RenderQuality::Medium));
        renderRaytracedReflectionsQuality_.AddItem("HIGH", static_cast<std::uint64_t>(bridge::RenderQuality::High));
        renderRaytracedReflectionsQuality_.SetTooltip("Native Wicked ray-traced reflection quality.");
        renderRaytracedReflectionsQuality_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
            state.raytracedReflectionsQuality = static_cast<bridge::RenderQuality>(args.userdata);
            CommitRenderSettings(state);
        });
        renderWorkspacePanel_.AddWidget(&renderRaytracedReflectionsQuality_);

        createToggle(renderRaytracedDiffuseEnabled_, "Ray-traced diffuse: ",
            "Enable Wicked single-bounce ray-traced diffuse lighting.",
            [](bridge::RenderSettingsState& state, bool value) { state.raytracedDiffuseEnabled = value; });
        createRange(renderRaytracedDiffuseRange_, "Render RT Diffuse Range", "RT DIFFUSE // RANGE",
            "Maximum ray distance for Wicked ray-traced diffuse lighting.",
            RenderField::RaytracedDiffuseRange, 1.0f, 100.0f, 1000.0f);
        renderRaytracedDiffuseQuality_.Create("Render RT Diffuse Quality");
        renderRaytracedDiffuseQuality_.AddItem("LOW", static_cast<std::uint64_t>(bridge::RenderQuality::Low));
        renderRaytracedDiffuseQuality_.AddItem("MEDIUM", static_cast<std::uint64_t>(bridge::RenderQuality::Medium));
        renderRaytracedDiffuseQuality_.AddItem("HIGH", static_cast<std::uint64_t>(bridge::RenderQuality::High));
        renderRaytracedDiffuseQuality_.SetTooltip("Native Wicked ray-traced diffuse quality.");
        renderRaytracedDiffuseQuality_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if (session_ == nullptr)
                return;
            auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
            state.raytracedDiffuseQuality = static_cast<bridge::RenderQuality>(args.userdata);
            CommitRenderSettings(state);
        });
        renderWorkspacePanel_.AddWidget(&renderRaytracedDiffuseQuality_);
        createToggle(renderSurfelGiEnabled_, "Surfel GI: ",
            "Enable Wicked ray-traced diffuse GI using its surface cache.",
            [](bridge::RenderSettingsState& state, bool value) { state.surfelGiEnabled = value; });

        createSection(renderPathTraceLabel_, "Render Path Trace Section", "PATH TRACE // REFERENCE PREVIEW");
        renderPathTraceStatus_.Create("Render Path Trace Status");
        renderPathTraceStatus_.font.params.size = 12;
        renderPathTraceStatus_.font.params.color = RenderMuted;
        renderPathTraceStatus_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        renderPathTraceStatus_.SetColor(wi::Color::Transparent());
        renderWorkspacePanel_.AddWidget(&renderPathTraceStatus_);

        renderPathTraceToggle_.Create("Render Path Trace Toggle");
        renderPathTraceToggle_.SetText("START PATH TRACE PREVIEW");
        renderPathTraceToggle_.SetTooltip(
            "Use Wicked's native progressive path tracer as a Studio reference preview. On GPUs without hardware ray tracing the fallback can be very slow.");
        renderPathTraceToggle_.OnClick([this](const wi::gui::EventArgs&) {
            SetPathTracePreviewActive(!pathTracePreviewActive_);
        });
        renderWorkspacePanel_.AddWidget(&renderPathTraceToggle_);

        renderPathTraceSamples_.Create(1.0f, 2048.0f, 1024.0f, 2047.0f,
            "Render Path Trace Samples", "TARGET SAMPLES");
        renderPathTraceSamples_.SetTooltip("Target progressive samples per pixel (1-2048).");
        renderPathTraceSamples_.OnValueCommitted([this](const float value)
        {
            pathTraceTargetSamples_ = std::clamp(static_cast<int>(std::lround(value)), 1, 2048);
            setTargetSampleCount(pathTraceTargetSamples_);
            if (pathTracePreviewActive_)
                RestartPathTracePreview();
        });
        renderWorkspacePanel_.AddWidget(&renderPathTraceSamples_);

        renderPathTraceBounces_.Create(1.0f, 10.0f, 1.0f, 9.0f,
            "Render Path Trace Bounces", "RAYTRACE BOUNCES");
        renderPathTraceBounces_.SetTooltip("Wicked path-trace light-bounce count (1-10).");
        renderPathTraceBounces_.OnValueCommitted([this](const float value)
        {
            pathTraceBounceCount_ = std::clamp(
                static_cast<std::uint32_t>(std::lround(value)), 1u, 10u);
            if (pathTracePreviewActive_)
            {
                wi::renderer::SetRaytraceBounceCount(pathTraceBounceCount_);
                RestartPathTracePreview();
            }
        });
        renderWorkspacePanel_.AddWidget(&renderPathTraceBounces_);

        renderPathTraceRestart_.Create("Render Path Trace Restart");
        renderPathTraceRestart_.SetText("RESTART ACCUMULATION");
        renderPathTraceRestart_.SetTooltip("Discard accumulated samples and restart the reference preview.");
        renderPathTraceRestart_.OnClick([this](const wi::gui::EventArgs&) { RestartPathTracePreview(); });
        renderWorkspacePanel_.AddWidget(&renderPathTraceRestart_);
    }

    void StudioRenderPath::LayoutGate7RenderControls(const float fieldWidth, float& y)
    {
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

        section(renderRayTracingLabel_);
        full(renderRayTracingCapability_);
        full(renderRaytracedShadowsEnabled_);
        full(renderRaytracedReflectionsEnabled_);
        full(renderRaytracedReflectionsRange_);
        full(renderRaytracedReflectionsQuality_);
        full(renderRaytracedDiffuseEnabled_);
        full(renderRaytracedDiffuseRange_);
        full(renderRaytracedDiffuseQuality_);
        full(renderSurfelGiEnabled_);

        section(renderPathTraceLabel_);
        full(renderPathTraceStatus_);
        full(renderPathTraceToggle_);
        full(renderPathTraceSamples_);
        full(renderPathTraceBounces_);
        full(renderPathTraceRestart_);
    }

    void StudioRenderPath::RefreshGate7RenderControls(const bridge::RenderSettingsState& state)
    {
        const bool hardware = bridge::IsHardwareRayTracingAvailable();
        renderRayTracingCapability_.SetText(
            hardware ? "HARDWARE RAY TRACING // AVAILABLE" : "HARDWARE RAY TRACING // UNAVAILABLE");

        if (!hardware && state.ambientOcclusion == bridge::RenderAmbientOcclusion::RTAO)
        {
            renderAmbientOcclusion_.SetSelectedByUserdataWithoutCallback(
                static_cast<std::uint64_t>(bridge::RenderAmbientOcclusion::Off));
        }

        renderRaytracedShadowsEnabled_.SetCheck(state.raytracedShadowsEnabled);
        renderRaytracedReflectionsEnabled_.SetCheck(state.raytracedReflectionsEnabled);
        renderRaytracedReflectionsRange_.SetValue(state.raytracedReflectionsRange);
        renderRaytracedReflectionsQuality_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(state.raytracedReflectionsQuality));
        renderRaytracedDiffuseEnabled_.SetCheck(state.raytracedDiffuseEnabled);
        renderRaytracedDiffuseRange_.SetValue(state.raytracedDiffuseRange);
        renderRaytracedDiffuseQuality_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(state.raytracedDiffuseQuality));
        renderSurfelGiEnabled_.SetCheck(state.surfelGiEnabled);

        renderRaytracedShadowsEnabled_.SetEnabled(hardware);
        renderRaytracedReflectionsEnabled_.SetEnabled(hardware);
        renderRaytracedReflectionsRange_.SetEnabled(hardware && state.raytracedReflectionsEnabled);
        renderRaytracedReflectionsQuality_.SetEnabled(hardware && state.raytracedReflectionsEnabled);
        renderRaytracedDiffuseEnabled_.SetEnabled(hardware);
        renderRaytracedDiffuseRange_.SetEnabled(hardware && state.raytracedDiffuseEnabled);
        renderRaytracedDiffuseQuality_.SetEnabled(hardware && state.raytracedDiffuseEnabled);
        renderSurfelGiEnabled_.SetEnabled(true);

        renderPathTraceSamples_.SetValue(static_cast<float>(pathTraceTargetSamples_));
        renderPathTraceBounces_.SetValue(static_cast<float>(pathTraceBounceCount_));
        renderPathTraceToggle_.SetText(
            pathTracePreviewActive_ ? "RETURN TO REALTIME" : "START PATH TRACE PREVIEW");
        renderPathTraceRestart_.SetEnabled(pathTracePreviewActive_);
        RefreshPathTracePreviewStatus();
    }

    void StudioRenderPath::SetPathTracePreviewActive(const bool active)
    {
        if (pathTracePreviewActive_ == active)
            return;

        if (active)
        {
            pathTracePreviousBounceCount_ = wi::renderer::GetRaytraceBounceCount();
            pathTracePreviewActive_ = true;
            setTargetSampleCount(pathTraceTargetSamples_);
            wi::renderer::SetRaytraceBounceCount(pathTraceBounceCount_);
            resetProgress();
            ResizeBuffers();
            studioChrome_.SetStatusText("RENDER // PATH TRACE REFERENCE PREVIEW");
        }
        else
        {
            pathTracePreviewActive_ = false;
            resetProgress();
            wi::renderer::SetRaytraceBounceCount(pathTracePreviousBounceCount_);
            ResizeBuffers();
            if (session_ != nullptr)
                ApplyRenderSettingsState(bridge::CaptureRenderSettings(session_->Scenes().GetScene()), true);
            studioChrome_.SetStatusText("RENDER // NATIVE WICKED");
        }
        RefreshGate7RenderControls(
            session_ != nullptr ? bridge::CaptureRenderSettings(session_->Scenes().GetScene())
                                : bridge::DefaultRenderSettings());
    }

    void StudioRenderPath::RestartPathTracePreview()
    {
        if (!pathTracePreviewActive_)
            return;
        setTargetSampleCount(pathTraceTargetSamples_);
        wi::renderer::SetRaytraceBounceCount(pathTraceBounceCount_);
        resetProgress();
        RefreshPathTracePreviewStatus();
    }

    void StudioRenderPath::RefreshPathTracePreviewStatus()
    {
        const bool hardware = bridge::IsHardwareRayTracingAvailable();
        if (!pathTracePreviewActive_)
        {
            renderPathTraceStatus_.SetText(
                hardware ? "REFERENCE PREVIEW // IDLE // HARDWARE RT"
                         : "REFERENCE PREVIEW // IDLE // SOFTWARE FALLBACK WILL BE SLOW");
            return;
        }

        const int sample = std::max(0, getCurrentSampleCount());
        const float progress = std::clamp(getProgress(), 0.0f, 1.0f) * 100.0f;
        std::ostringstream text;
        text << "PATH TRACE // " << sample << "/" << pathTraceTargetSamples_
             << " SAMPLES // " << std::fixed << std::setprecision(1) << progress << "%";
        if (isDenoiserAvailable())
        {
            text << " // DENOISER "
                 << std::fixed << std::setprecision(0)
                 << std::clamp(getDenoiserProgress(), 0.0f, 1.0f) * 100.0f << "%";
        }
        else
        {
            text << " // DENOISER UNAVAILABLE";
        }
        renderPathTraceStatus_.SetText(text.str());
    }
}
