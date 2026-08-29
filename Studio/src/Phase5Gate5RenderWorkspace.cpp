#include "StudioApplication.h"

#include <algorithm>
#include <memory>

namespace
{
    constexpr wi::Color RenderPanel = wi::Color(8, 12, 16, 255);
    constexpr wi::Color RenderMuted = wi::Color(178, 178, 176, 255);
}

namespace renegade::studio
{
    void StudioRenderPath::CreateRenderWorkspace()
    {
        renderWorkspacePanel_.Create(
            "Render Workspace",
            wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
        renderWorkspacePanel_.SetShadowRadius(0.0f);
        renderWorkspacePanel_.SetColor(wi::Color::Transparent());
        renderWorkspacePanel_.SetColor(
            RenderPanel,
            wi::gui::WIDGET_ID_WINDOW_BASE);
        renderWorkspacePanel_.scrollbar_vertical.SetVisible(true);
        renderWorkspacePanel_.SetVisible(false);
        GetGUI().AddWidget(&renderWorkspacePanel_);

        renderWorkspaceTitle_.Create("Render Workspace Title");
        renderWorkspaceTitle_.SetText("RENDER // NATIVE WICKED");
        renderWorkspaceTitle_.font.params.size = 16;
        renderWorkspaceTitle_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        renderWorkspaceTitle_.SetColor(wi::Color::Transparent());
        renderWorkspacePanel_.AddWidget(&renderWorkspaceTitle_);

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
            renderImageLabel_,
            "Render Image Section",
            "IMAGE // TONEMAPPING");
        renderTonemap_.Create("Render Tonemap");
        renderTonemap_.AddItem(
            "ACES",
            static_cast<std::uint64_t>(bridge::RenderTonemap::ACES));
        renderTonemap_.AddItem(
            "REINHARD",
            static_cast<std::uint64_t>(bridge::RenderTonemap::Reinhard));
        renderTonemap_.AddItem(
            "UCHIMURA",
            static_cast<std::uint64_t>(bridge::RenderTonemap::Uchimura));
        renderTonemap_.SetTooltip(
            "Native Wicked tonemapper used identically by Studio and Runtime.");
        renderTonemap_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
            state.tonemap = static_cast<bridge::RenderTonemap>(args.userdata);
            CommitRenderSettings(state);
        });
        renderWorkspacePanel_.AddWidget(&renderTonemap_);

        const auto createSlider = [this](
            SceneInspectorSlider& slider,
            const char* name,
            const char* label,
            const char* tooltip,
            const RenderField field,
            const float minimum,
            const float maximum,
            const float steps)
        {
            slider.Create(minimum, maximum, 0.0f, steps, name, label);
            slider.SetTooltip(tooltip);
            slider.OnDragStarted([this, field](const float)
            {
                BeginRenderSlider(field);
            });
            slider.OnValuePreview([this, field](const float value)
            {
                PreviewRenderSlider(field, value);
            });
            slider.OnValueCommitted([this, field](const float value)
            {
                CommitRenderSlider(field, value);
            });
            renderWorkspacePanel_.AddWidget(&slider);
        };

        createSlider(
            renderExposure_, "Render Exposure", "EXPOSURE",
            "Native scene exposure.", RenderField::Exposure,
            0.0f, 16.0f, 1600.0f);
        createSlider(
            renderBrightness_, "Render Brightness", "BRIGHTNESS",
            "Native post-process brightness offset.", RenderField::Brightness,
            -2.0f, 2.0f, 800.0f);
        createSlider(
            renderContrast_, "Render Contrast", "CONTRAST",
            "Native post-process contrast.", RenderField::Contrast,
            0.0f, 4.0f, 800.0f);
        createSlider(
            renderSaturation_, "Render Saturation", "SATURATION",
            "Native post-process colour saturation.", RenderField::Saturation,
            0.0f, 4.0f, 800.0f);
        createSlider(
            renderHdrCalibration_, "Render HDR Calibration", "HDR CALIBRATION",
            "Wicked render-path HDR calibration value. This does not switch OS HDR output mode.",
            RenderField::HdrCalibration, 0.01f, 16.0f, 1600.0f);

        createSection(
            renderBloomLabel_,
            "Render Bloom Section",
            "BLOOM // AUTO EXPOSURE");

        const auto createToggle = [this](
            SceneInspectorCheckBox& checkbox,
            const char* name,
            const char* tooltip,
            const RenderToggle toggle)
        {
            checkbox.Create(name);
            checkbox.SetTooltip(tooltip);
            checkbox.OnClick([this, toggle](const wi::gui::EventArgs& args)
            {
                ApplyRenderToggle(toggle, args.bValue);
            });
            renderWorkspacePanel_.AddWidget(&checkbox);
        };

        createToggle(
            renderBloomEnabled_, "Bloom enabled: ",
            "Run Wicked's native bloom pass.", RenderToggle::Bloom);
        createSlider(
            renderBloomThreshold_, "Render Bloom Threshold", "BLOOM THRESHOLD",
            "Brightness threshold for native bloom response.",
            RenderField::BloomThreshold, 0.0f, 64.0f, 1280.0f);
        createToggle(
            renderEyeAdaptationEnabled_, "Auto exposure: ",
            "Enable Wicked eye adaptation / automatic exposure.",
            RenderToggle::EyeAdaptation);
        createSlider(
            renderEyeAdaptationKey_, "Render Eye Adaptation Key", "AUTO EXPOSURE // KEY",
            "Native luminance key used by Wicked eye adaptation.",
            RenderField::EyeAdaptationKey, 0.001f, 4.0f, 1000.0f);
        createSlider(
            renderEyeAdaptationRate_, "Render Eye Adaptation Rate", "AUTO EXPOSURE // RATE",
            "How quickly eye adaptation responds to luminance changes.",
            RenderField::EyeAdaptationRate, 0.0f, 16.0f, 1600.0f);

        createSection(
            renderAntiAliasingLabel_,
            "Render Anti Aliasing Section",
            "ANTI-ALIASING // NATIVE WICKED");
        renderAntiAliasing_.Create("Render Anti Aliasing");
        renderAntiAliasing_.AddItem("OFF", static_cast<std::uint64_t>(bridge::AntiAliasingMode::Off));
        renderAntiAliasing_.AddItem("FXAA", static_cast<std::uint64_t>(bridge::AntiAliasingMode::FXAA));
        renderAntiAliasing_.AddItem("TAA", static_cast<std::uint64_t>(bridge::AntiAliasingMode::TAA));
        renderAntiAliasing_.AddItem("MSAA 2X", static_cast<std::uint64_t>(bridge::AntiAliasingMode::MSAA2X));
        renderAntiAliasing_.AddItem("MSAA 4X", static_cast<std::uint64_t>(bridge::AntiAliasingMode::MSAA4X));
        renderAntiAliasing_.AddItem("MSAA 8X", static_cast<std::uint64_t>(bridge::AntiAliasingMode::MSAA8X));
        renderAntiAliasing_.SetTooltip(
            "One governed AA mode. Selecting a mode clears incompatible FXAA/TAA/MSAA state.");
        renderAntiAliasing_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
            state.antiAliasing = static_cast<bridge::AntiAliasingMode>(args.userdata);
            CommitRenderSettings(state);
        });
        renderWorkspacePanel_.AddWidget(&renderAntiAliasing_);

        createSection(
            renderPostFxLabel_,
            "Render Post FX Section",
            "POST FX // CAMERA IMAGE");
        createToggle(
            renderDepthOfFieldEnabled_, "Depth of field: ",
            "Run Wicked depth of field. Focal distance/aperture remain authored on scene cameras.",
            RenderToggle::DepthOfField);
        createSlider(
            renderDepthOfFieldStrength_, "Render DOF Strength", "DEPTH OF FIELD // STRENGTH",
            "Native depth-of-field strength.", RenderField::DepthOfFieldStrength,
            0.0f, 100.0f, 1000.0f);
        createToggle(
            renderMotionBlurEnabled_, "Motion blur: ",
            "Run Wicked's native motion-blur pass.", RenderToggle::MotionBlur);
        createSlider(
            renderMotionBlurStrength_, "Render Motion Blur Strength", "MOTION BLUR // STRENGTH",
            "Native motion-blur strength.", RenderField::MotionBlurStrength,
            0.0f, 500.0f, 1000.0f);
        createToggle(
            renderSharpenEnabled_, "Sharpen: ",
            "Run Wicked's native sharpen filter.", RenderToggle::Sharpen);
        createSlider(
            renderSharpenAmount_, "Render Sharpen Amount", "SHARPEN // AMOUNT",
            "Native sharpen filter amount.", RenderField::SharpenAmount,
            0.0f, 4.0f, 800.0f);
        createToggle(
            renderChromaticAberrationEnabled_, "Chromatic aberration: ",
            "Run Wicked's native chromatic-aberration pass.",
            RenderToggle::ChromaticAberration);
        createSlider(
            renderChromaticAberrationAmount_, "Render Chromatic Aberration Amount",
            "CHROMATIC ABERRATION // AMOUNT",
            "Native chromatic-aberration amount.",
            RenderField::ChromaticAberrationAmount, 0.0f, 64.0f, 1280.0f);
        createToggle(
            renderDitherEnabled_, "Dither: ",
            "Enable Wicked's final-image dithering.", RenderToggle::Dither);

        RefreshRenderWorkspace();
        LayoutRenderWorkspace();
    }

    void StudioRenderPath::LayoutRenderWorkspace()
    {
        const XMFLOAT4 bounds = studioChrome_.ViewportBounds();
        const float panelWidth = std::max(280.0f, studioChrome_.InspectorWidth());
        const float panelHeight = std::max(120.0f, bounds.w - bounds.y);
        renderWorkspacePanel_.SetPos(XMFLOAT2(bounds.z, bounds.y));
        renderWorkspacePanel_.SetSize(XMFLOAT2(panelWidth, panelHeight));

        const float fieldWidth = std::max(180.0f, panelWidth - 28.0f);
        float y = 10.0f;
        const auto title = [fieldWidth, &y](wi::gui::Widget& widget)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, 24.0f));
            y += 30.0f;
        };
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

        title(renderWorkspaceTitle_);
        section(renderImageLabel_);
        full(renderTonemap_);
        full(renderExposure_);
        full(renderBrightness_);
        full(renderContrast_);
        full(renderSaturation_);
        full(renderHdrCalibration_);

        section(renderBloomLabel_);
        full(renderBloomEnabled_);
        full(renderBloomThreshold_);
        full(renderEyeAdaptationEnabled_);
        full(renderEyeAdaptationKey_);
        full(renderEyeAdaptationRate_);

        section(renderAntiAliasingLabel_);
        full(renderAntiAliasing_);

        section(renderPostFxLabel_);
        full(renderDepthOfFieldEnabled_);
        full(renderDepthOfFieldStrength_);
        full(renderMotionBlurEnabled_);
        full(renderMotionBlurStrength_);
        full(renderSharpenEnabled_);
        full(renderSharpenAmount_);
        full(renderChromaticAberrationEnabled_);
        full(renderChromaticAberrationAmount_);
        full(renderDitherEnabled_);
    }

    void StudioRenderPath::RefreshRenderWorkspace()
    {
        if (session_ == nullptr)
            return;

        const auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
        renderTonemap_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(state.tonemap));
        renderExposure_.SetValue(state.exposure);
        renderBrightness_.SetValue(state.brightness);
        renderContrast_.SetValue(state.contrast);
        renderSaturation_.SetValue(state.saturation);
        renderHdrCalibration_.SetValue(state.hdrCalibration);
        renderBloomEnabled_.SetCheck(state.bloomEnabled);
        renderBloomThreshold_.SetValue(state.bloomThreshold);
        renderEyeAdaptationEnabled_.SetCheck(state.eyeAdaptationEnabled);
        renderEyeAdaptationKey_.SetValue(state.eyeAdaptationKey);
        renderEyeAdaptationRate_.SetValue(state.eyeAdaptationRate);
        renderAntiAliasing_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(state.antiAliasing));
        renderDepthOfFieldEnabled_.SetCheck(state.depthOfFieldEnabled);
        renderDepthOfFieldStrength_.SetValue(state.depthOfFieldStrength);
        renderMotionBlurEnabled_.SetCheck(state.motionBlurEnabled);
        renderMotionBlurStrength_.SetValue(state.motionBlurStrength);
        renderSharpenEnabled_.SetCheck(state.sharpenEnabled);
        renderSharpenAmount_.SetValue(state.sharpenAmount);
        renderChromaticAberrationEnabled_.SetCheck(state.chromaticAberrationEnabled);
        renderChromaticAberrationAmount_.SetValue(state.chromaticAberrationAmount);
        renderDitherEnabled_.SetCheck(state.ditherEnabled);

        renderBloomThreshold_.SetEnabled(state.bloomEnabled);
        renderEyeAdaptationKey_.SetEnabled(state.eyeAdaptationEnabled);
        renderEyeAdaptationRate_.SetEnabled(state.eyeAdaptationEnabled);
        renderDepthOfFieldStrength_.SetEnabled(state.depthOfFieldEnabled);
        renderMotionBlurStrength_.SetEnabled(state.motionBlurEnabled);
        renderSharpenAmount_.SetEnabled(state.sharpenEnabled);
        renderChromaticAberrationAmount_.SetEnabled(
            state.chromaticAberrationEnabled);
    }

    void StudioRenderPath::SetRenderWorkspaceActive(const bool active)
    {
        if (renderWorkspaceActive_ == active)
        {
            if (active)
            {
                LayoutRenderWorkspace();
                RefreshRenderWorkspace();
            }
            return;
        }

        renderWorkspaceActive_ = active && session_ != nullptr;
        if (renderWorkspaceActive_)
        {
            if (sunPreviewPlaying_)
                StopSunPreview(true);
            environmentWorkspaceActive_ = false;
            terrainWorkspaceActive_ = false;
            studioChrome_.SetEnvironmentWorkspaceActive(false);
            studioChrome_.SetTerrainWorkspaceActive(false);
            studioChrome_.SetRenderWorkspaceActive(true);
            inspectorPanel_.SetVisible(false);
            renderWorkspacePanel_.SetVisible(!projectHubVisible_);
            ClearSelectionOutline();
            RefreshRenderWorkspace();
            LayoutRenderWorkspace();
            studioChrome_.SetStatusText("RENDER // NATIVE WICKED");
        }
        else
        {
            studioChrome_.SetRenderWorkspaceActive(false);
            renderWorkspacePanel_.SetVisible(false);
            if (!projectHubVisible_)
                inspectorPanel_.SetVisible(true);
            RefreshInspector();
        }
        RefreshStatus();
    }

    void StudioRenderPath::ApplyRenderSettingsState(
        const bridge::RenderSettingsState& state,
        const bool resizeBuffersForMSAA)
    {
        const auto safe = bridge::SanitizeRenderSettings(state);
        bridge::ApplyRenderSettingsToPath(*this, safe, resizeBuffersForMSAA);
        appliedRenderSettings_ = safe;
        appliedRenderSettingsInitialized_ = true;
    }

    void StudioRenderPath::SyncRenderSettingsFromScene(
        const bool resizeBuffersForMSAA)
    {
        if (session_ == nullptr || renderSliderActive_)
            return;
        const auto authored = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
        if (appliedRenderSettingsInitialized_ &&
            !bridge::HasRenderSettingsChange(appliedRenderSettings_, authored))
        {
            return;
        }
        ApplyRenderSettingsState(authored, resizeBuffersForMSAA);
        if (renderWorkspaceActive_)
            RefreshRenderWorkspace();
    }

    bool StudioRenderPath::CommitRenderSettings(
        const bridge::RenderSettingsState& state)
    {
        if (session_ == nullptr)
            return false;
        auto& scene = session_->Scenes().GetScene();
        const auto safe = bridge::SanitizeRenderSettings(state);
        const auto callback = [this](const bridge::RenderSettingsState& applied)
        {
            ApplyRenderSettingsState(applied, true);
            if (renderWorkspaceActive_)
                RefreshRenderWorkspace();
            RefreshStatus();
        };
        return session_->Commands().Execute(
            std::make_unique<bridge::SetRenderSettingsCommand>(
                scene,
                safe,
                callback));
    }

    void StudioRenderPath::ApplyRenderToggle(
        const RenderToggle toggle,
        const bool value)
    {
        if (session_ == nullptr)
            return;
        auto state = bridge::CaptureRenderSettings(session_->Scenes().GetScene());
        switch (toggle)
        {
        case RenderToggle::Bloom:
            state.bloomEnabled = value;
            break;
        case RenderToggle::EyeAdaptation:
            state.eyeAdaptationEnabled = value;
            break;
        case RenderToggle::DepthOfField:
            state.depthOfFieldEnabled = value;
            break;
        case RenderToggle::MotionBlur:
            state.motionBlurEnabled = value;
            break;
        case RenderToggle::Sharpen:
            state.sharpenEnabled = value;
            break;
        case RenderToggle::ChromaticAberration:
            state.chromaticAberrationEnabled = value;
            break;
        case RenderToggle::Dither:
            state.ditherEnabled = value;
            break;
        }
        CommitRenderSettings(state);
    }

    void StudioRenderPath::BeginRenderSlider(const RenderField field)
    {
        if (session_ == nullptr)
            return;
        renderSliderActive_ = true;
        renderSliderField_ = field;
        renderSliderHadCarrierBefore_ =
            bridge::FindRenderSettingsCarrier(session_->Scenes().GetScene()) !=
            wi::ecs::INVALID_ENTITY;
        renderSliderBefore_ = bridge::CaptureRenderSettings(
            session_->Scenes().GetScene());
        renderSliderAfter_ = renderSliderBefore_;
    }

    void StudioRenderPath::PreviewRenderSlider(
        const RenderField field,
        const float value)
    {
        if (session_ == nullptr)
            return;
        if (!renderSliderActive_ || renderSliderField_ != field)
            BeginRenderSlider(field);
        renderSliderAfter_ = renderSliderBefore_;
        SetRenderFieldValue(renderSliderAfter_, field, value);
        renderSliderAfter_ = bridge::SanitizeRenderSettings(renderSliderAfter_);
        ApplyRenderSettingsState(renderSliderAfter_, true);
    }

    void StudioRenderPath::CommitRenderSlider(
        const RenderField field,
        const float value)
    {
        if (session_ == nullptr)
            return;
        if (!renderSliderActive_ || renderSliderField_ != field)
            BeginRenderSlider(field);
        SetRenderFieldValue(renderSliderAfter_, field, value);
        renderSliderAfter_ = bridge::SanitizeRenderSettings(renderSliderAfter_);

        auto& scene = session_->Scenes().GetScene();
        const auto before = renderSliderBefore_;
        const auto after = renderSliderAfter_;
        const bool hadCarrierBefore = renderSliderHadCarrierBefore_;
        renderSliderActive_ = false;

        const auto callback = [this](const bridge::RenderSettingsState& applied)
        {
            ApplyRenderSettingsState(applied, true);
            if (renderWorkspaceActive_)
                RefreshRenderWorkspace();
            RefreshStatus();
        };
        (void)session_->Commands().Execute(
            std::make_unique<bridge::SetRenderSettingsCommand>(
                scene,
                before,
                after,
                hadCarrierBefore,
                callback));
        SyncRenderSettingsFromScene(true);
    }

    void StudioRenderPath::SetRenderFieldValue(
        bridge::RenderSettingsState& state,
        const RenderField field,
        const float value) noexcept
    {
        switch (field)
        {
        case RenderField::Exposure:
            state.exposure = value;
            break;
        case RenderField::Brightness:
            state.brightness = value;
            break;
        case RenderField::Contrast:
            state.contrast = value;
            break;
        case RenderField::Saturation:
            state.saturation = value;
            break;
        case RenderField::HdrCalibration:
            state.hdrCalibration = value;
            break;
        case RenderField::BloomThreshold:
            state.bloomThreshold = value;
            break;
        case RenderField::EyeAdaptationKey:
            state.eyeAdaptationKey = value;
            break;
        case RenderField::EyeAdaptationRate:
            state.eyeAdaptationRate = value;
            break;
        case RenderField::DepthOfFieldStrength:
            state.depthOfFieldStrength = value;
            break;
        case RenderField::MotionBlurStrength:
            state.motionBlurStrength = value;
            break;
        case RenderField::SharpenAmount:
            state.sharpenAmount = value;
            break;
        case RenderField::ChromaticAberrationAmount:
            state.chromaticAberrationAmount = value;
            break;
        }
    }
}
