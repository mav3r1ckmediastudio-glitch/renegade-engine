// Night Sky authoring within the shared Environment inspector.
// Existing Environment and Time of Day authoring are not duplicated.
#include "StudioApplication.h"
#include "EnvironmentSectionLayout.h"
#include <algorithm>
#include <cmath>

namespace renegade::studio
{
    void StudioRenderPath::SetNightFieldValue(bridge::NightSkyState& s,
                                               NightField field, float v) noexcept
    {
        switch (field)
        {
        case NightField::Gradient: s.gradient = v; break;
        case NightField::ZenithR: s.zenith.x = v; break;
        case NightField::ZenithG: s.zenith.y = v; break;
        case NightField::ZenithB: s.zenith.z = v; break;
        case NightField::HorizonR: s.horizon.x = v; break;
        case NightField::HorizonG: s.horizon.y = v; break;
        case NightField::HorizonB: s.horizon.z = v; break;
        case NightField::HorizonGlow: s.horizonGlow = v; break;
        case NightField::AuroraIntensity: s.aurora = v; break;
        case NightField::AuroraCoverage: s.auroraCoverage = v; break;
        case NightField::AuroraSpeed: s.auroraSpeed = v; break;
        case NightField::Count: break;
        }
        s = bridge::SanitizeNightSky(s);
    }

    void StudioRenderPath::BeginNightSlider(const NightField field)
    {
        nightSliderActive_ = false;
        if (!session_) return;
        const auto entity = EditableWeatherEntity();
        if (entity == wi::ecs::INVALID_ENTITY) return;
        auto& scene = session_->Scenes().GetScene();
        if (!scene.weathers.Contains(entity)) return;
        nightSliderEntity_ = entity;
        nightSliderField_ = field;
        nightSliderBefore_ = bridge::CaptureNightSky(scene, entity);
        nightSliderAfter_ = nightSliderBefore_;
        nightSliderActive_ = true;
    }

    void StudioRenderPath::PreviewNightSlider(NightField field, float value)
    {
        if (!nightSliderActive_ || nightSliderField_ != field || !session_) return;
        auto& scene = session_->Scenes().GetScene();
        if (!scene.weathers.Contains(nightSliderEntity_))
        {
            nightSliderActive_ = false;
            return;
        }
        nightSliderAfter_ = nightSliderBefore_;
        SetNightFieldValue(nightSliderAfter_, field, value);
        bridge::ApplyNightSkyRuntime(scene, nightSliderEntity_, nightSliderAfter_);
    }

    void StudioRenderPath::CommitNightSlider(NightField field, float value)
    {
        if (!nightSliderActive_) BeginNightSlider(field);
        if (!nightSliderActive_ || nightSliderField_ != field || !session_) return;
        auto& scene = session_->Scenes().GetScene();
        if (scene.weathers.Contains(nightSliderEntity_))
        {
            nightSliderAfter_ = nightSliderBefore_;
            SetNightFieldValue(nightSliderAfter_, field, value);
            // Restore preview state before handing a single atomic change to Undo/Redo.
            bridge::ApplyNightSkyRuntime(scene, nightSliderEntity_, nightSliderBefore_);
            session_->Commands().Execute(
                std::make_unique<bridge::SetNightSkyCommand>(scene, nightSliderEntity_,
                                                             nightSliderAfter_));
        }
        nightSliderActive_ = false;
        nightSliderEntity_ = wi::ecs::INVALID_ENTITY;
        RefreshInspector();
        RefreshStatus();
    }
    // One flow layout owns every Environment child. Disclosure is session-only
    // editor state; scene values and command callbacks retain their owners.
    void StudioRenderPath::LayoutEnvironmentSections()
    {
        const bool visible = session_ && !terrainWorkspaceActive_ && !renderWorkspaceActive_ &&
            EditableWeatherEntity() != wi::ecs::INVALID_ENTITY;
        const float width = std::max(1.0f, inspectorPanel_.GetSize().x - 24.0f);
        const std::array<const char*, 9> titles = {"TIME OF DAY / SUN", "REALISTIC SKY", "CLOUDS", "STARS", "FOG", "OCEAN", "WEATHER / PRECIPITATION", "NIGHT ATMOSPHERE", "AURORA"};
        const std::array<std::vector<wi::gui::Widget*>, 9> children = {{
            {&sunPreset_, &sunTime_, &sunAzimuth_, &sunElevation_, &sunPreviewSpeed_, &sunPlayButton_, &sunPauseButton_},
            {&environmentPreset_, &skyMode_, &aerialPerspective_, &skyExposure_, &ambientIntensity_},
            {&cloudCoverage_, &cloudStartHeight_, &cloudThickness_, &cloudsCastShadow_},
            {&stars_},
            {&fogStart_, &fogDensity_, &heightFog_, &fogHeightStart_, &fogHeightEnd_},
            {&oceanEnabled_, &oceanPreset_, &oceanResolution_, &oceanWaterHeight_, &oceanPatchLength_, &oceanWaveAmplitude_, &oceanChoppyScale_, &oceanTimeScale_, &oceanWindAzimuth_, &oceanWindSpeed_, &oceanWindDependency_, &oceanSurfaceDetail_, &oceanDisplacementTolerance_, &oceanWaterRed_, &oceanWaterGreen_, &oceanWaterBlue_, &oceanWaterOpacity_, &oceanExtinctionRed_, &oceanExtinctionGreen_, &oceanExtinctionBlue_},
            {&precipitationMode_, &precipitationIntensity_, &precipitationFallSpeed_, &precipitationParticleScale_, &precipitationWindAzimuth_, &precipitationWindSpeed_, &precipitationTurbulence_},
            {&nightSkySliders_[0], &nightZenithColour_, &nightHorizonColour_, &nightSkySliders_[7]},
            {&nightAuroraEnabled_, &nightSkySliders_[8], &nightSkySliders_[9], &nightSkySliders_[10]}
        }};
        if (!environmentSectionsCreated_)
        {
            for (std::size_t i = 0; i < environmentSectionHeaders_.size(); ++i)
            {
                auto& header = environmentSectionHeaders_[i];
                header.Create(std::string("Environment disclosure ") + titles[i]);
                header.SetTooltip("Expand or collapse this Environment group.");
                header.OnClick([this, i](const wi::gui::EventArgs&)
                {
                    environmentSectionExpanded_[i] = !environmentSectionExpanded_[i];
                    QueueInspectorRefresh();
                });
                inspectorPanel_.AddWidget(&header);
            }
            CreateNightColourEditor();
            environmentSectionsCreated_ = true;
        }
        // Colour channels are authored through the native hue/saturation picker.
        for (std::size_t i = 1; i <= 6; ++i) nightSkySliders_[i].SetVisible(false);
        const bool procedural = visible && session_->Scenes().GetScene().weathers.GetComponent(EditableWeatherEntity())->IsRealisticSky();
        nightZenithColour_.SetEnabled(procedural);
        nightHorizonColour_.SetEnabled(procedural);
        if (nightColourOpen_ && (!visible || EditableWeatherEntity() != nightColourEntity_))
            FinishNightColour(false);
        float y = 44.0f;
        for (std::size_t i = 0; i < children.size(); ++i)
        {
            auto& header = environmentSectionHeaders_[i];
            header.SetVisible(visible);
            header.SetText(std::string(environmentSectionExpanded_[i] ? "-  " : "+  ") + titles[i]);
            const auto bounds = MeasureEnvironmentSection(y, environmentSectionExpanded_[i], children[i].size());
            header.SetPos(XMFLOAT2(12.0f, bounds.headerTop));
            header.SetSize(XMFLOAT2(width, 28.0f));
            y = bounds.contentTop;
            for (auto* child : children[i])
            {
                child->SetVisible(visible && environmentSectionExpanded_[i]);
                if (visible && environmentSectionExpanded_[i])
                {
                    child->SetPos(XMFLOAT2(12.0f, y));
                    child->SetSize(XMFLOAT2(width, 28.0f));
                    y += 34.0f;
                }
            }
            y = bounds.nextTop;
        }
        if (visible)
        {
            const float half = (width - 8.0f) * 0.5f;
            const float third = (width - 16.0f) / 3.0f;
            undoButton_.SetPos(XMFLOAT2(12.0f, y + 8.0f));
            redoButton_.SetPos(XMFLOAT2(20.0f + half, y + 8.0f));
            undoButton_.SetSize(XMFLOAT2(half, 28.0f));
            redoButton_.SetSize(XMFLOAT2(half, 28.0f));
            std::array<wi::gui::Widget*, 3> saves = {&saveButton_, &saveAsButton_, &reopenButton_};
            for (std::size_t i = 0; i < saves.size(); ++i)
            {
                saves[i]->SetPos(XMFLOAT2(12.0f + i * (third + 8.0f), y + 48.0f));
                saves[i]->SetSize(XMFLOAT2(third, 28.0f));
            }
        }
    }

}

namespace renegade::studio
{
    namespace
    {
        float NightSRGBToLinear(float v)
        {
            return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
        }
        std::uint8_t NightLinearToSRGB(float v)
        {
            const float s = v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
            return static_cast<std::uint8_t>(std::round(std::clamp(s, 0.0f, 1.0f) * 255.0f));
        }
    }

    void StudioRenderPath::CreateNightColourEditor()
    {
        nightZenithColour_.Create("Zenith colour");
        nightHorizonColour_.Create("Horizon colour");
        nightZenithColour_.SetTooltip("Choose the night zenith colour. Disabled for texture skyboxes.");
        nightHorizonColour_.SetTooltip("Choose the night horizon colour. Disabled for texture skyboxes.");
        nightZenithColour_.OnClick([this](const wi::gui::EventArgs&) { OpenNightColour(false); });
        nightHorizonColour_.OnClick([this](const wi::gui::EventArgs&) { OpenNightColour(true); });
        inspectorPanel_.AddWidget(&nightZenithColour_);
        inspectorPanel_.AddWidget(&nightHorizonColour_);
        nightColourPicker_.Create("Night atmosphere colour");
        nightColourPicker_.SetSize(XMFLOAT2(340, 340));
        nightColourPicker_.SetVisible(false);
        nightColourPicker_.OnClose([this](const wi::gui::EventArgs&) { FinishNightColour(false); });
        nightColourPicker_.OnColorChanged([this](const wi::gui::EventArgs& args)
        {
            if (!nightColourOpen_ || !session_) return;
            auto& scene = session_->Scenes().GetScene();
            if (!scene.weathers.Contains(nightColourEntity_)) { FinishNightColour(false); return; }
            const auto rgb = args.color.toFloat3();
            nightColourValue_ = XMFLOAT3(
                std::min(0.1f, NightSRGBToLinear(rgb.x)),
                std::min(0.1f, NightSRGBToLinear(rgb.y)),
                std::min(0.1f, NightSRGBToLinear(rgb.z)));
            auto preview = bridge::CaptureNightSky(scene, nightColourEntity_);
            (nightColourIsHorizon_ ? preview.horizon : preview.zenith) = nightColourValue_;
            bridge::ApplyNightSkyRuntime(scene, nightColourEntity_, preview);
        });
        GetGUI().AddWidget(&nightColourPicker_);
        nightColourApply_.Create("Apply night colour");
        nightColourApply_.SetText("APPLY COLOUR");
        nightColourCancel_.Create("Cancel night colour");
        nightColourCancel_.SetText("CANCEL");
        nightColourApply_.OnClick([this](const wi::gui::EventArgs&) { FinishNightColour(true); });
        nightColourCancel_.OnClick([this](const wi::gui::EventArgs&) { FinishNightColour(false); });
        for (auto* button : {&nightColourApply_, &nightColourCancel_})
        {
            button->SetVisible(false);
            GetGUI().AddWidget(button);
        }
    }

    void StudioRenderPath::OpenNightColour(const bool horizon)
    {
        if (!session_) return;
        if (nightColourOpen_) FinishNightColour(false);
        const auto entity = EditableWeatherEntity();
        auto& scene = session_->Scenes().GetScene();
        if (!scene.weathers.Contains(entity)) return;
        const auto state = bridge::CaptureNightSky(scene, entity);
        nightColourEntity_ = entity;
        nightColourIsHorizon_ = horizon;
        nightColourValue_ = horizon ? state.horizon : state.zenith;
        nightColourPicker_.SetText(horizon ? "Horizon colour" : "Zenith colour");
        nightColourPicker_.SetPickColor(wi::Color(
            NightLinearToSRGB(nightColourValue_.x),
            NightLinearToSRGB(nightColourValue_.y),
            NightLinearToSRGB(nightColourValue_.z)));
        const float x = std::max(16.0f, inspectorPanel_.GetPos().x - 356.0f);
        nightColourPicker_.SetPos(XMFLOAT2(x, 100));
        nightColourApply_.SetPos(XMFLOAT2(x, 448));
        nightColourCancel_.SetPos(XMFLOAT2(x + 174, 448));
        nightColourApply_.SetSize(XMFLOAT2(166, 28));
        nightColourCancel_.SetSize(XMFLOAT2(166, 28));
        nightColourPicker_.alphaSlider.SetVisible(false);
        nightColourPicker_.SetVisible(true);
        nightColourApply_.SetVisible(true);
        nightColourCancel_.SetVisible(true);
        nightColourOpen_ = true;
    }

    void StudioRenderPath::FinishNightColour(const bool apply)
    {
        if (nightColourOpen_ && session_)
        {
            auto& scene = session_->Scenes().GetScene();
            if (scene.weathers.Contains(nightColourEntity_))
            {
                auto next = bridge::CaptureNightSky(scene, nightColourEntity_);
                // Restore the transient preview before capturing command history.
                bridge::ApplyNightSkyRuntime(scene, nightColourEntity_, next);
                if (apply)
                {
                    (nightColourIsHorizon_ ? next.horizon : next.zenith) = nightColourValue_;
                    session_->Commands().Execute(std::make_unique<bridge::SetNightSkyCommand>(
                        scene, nightColourEntity_, next));
                }
            }
        }
        nightColourOpen_ = false;
        nightColourEntity_ = wi::ecs::INVALID_ENTITY;
        nightColourPicker_.SetVisible(false);
        nightColourApply_.SetVisible(false);
        nightColourCancel_.SetVisible(false);
        QueueInspectorRefresh();
    }
}
