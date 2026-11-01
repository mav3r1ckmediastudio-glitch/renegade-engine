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
        case NightField::MilkyWay: s.milkyWay = v; break;
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
        const std::array<const char*, 11> titles = {"TIME OF DAY / SUN", "REALISTIC SKY", "CLOUDS", "STARS", "FOG", "OCEAN", "WEATHER / PRESETS & PRECIPITATION", "LIGHTNING & THUNDER", "NIGHT ATMOSPHERE", "AURORA", "MOON"};
        std::array<std::vector<wi::gui::Widget*>, 11> children = {{
            {&sunPreset_, &sunTime_, &sunAzimuth_, &sunElevation_, &sunPreviewSpeed_, &sunPlayButton_, &sunPauseButton_},
            {&skyMode_, &aerialPerspective_, &skyExposure_, &ambientIntensity_},
            {&cloudFormation_, &cloudCoverage_, &cloudMovementSpeed_, &cloudStartHeight_, &cloudThickness_, &cloudsCastShadow_},
            {&stars_, &nightSkySliders_[11], &shootingStarsEnabled_},
            {&fogStart_, &fogDensity_, &heightFog_, &fogHeightStart_, &fogHeightEnd_},
            {&oceanEnabled_, &oceanPreset_, &oceanResolution_, &oceanWaterHeight_, &oceanPatchLength_, &oceanWaveAmplitude_, &oceanChoppyScale_, &oceanTimeScale_, &oceanWindAzimuth_, &oceanWindSpeed_, &oceanWindDependency_, &oceanSurfaceDetail_, &oceanDisplacementTolerance_, &oceanWaterRed_, &oceanWaterGreen_, &oceanWaterBlue_, &oceanWaterOpacity_, &oceanExtinctionRed_, &oceanExtinctionGreen_, &oceanExtinctionBlue_},
            {&environmentPreset_, &weatherTransitionDuration_, &weatherApplyNowButton_, &weatherStartTransitionButton_, &weatherCancelTransitionButton_, &precipitationMode_, &precipitationIntensity_, &precipitationFallSpeed_, &precipitationParticleScale_, &precipitationWindAzimuth_, &precipitationWindSpeed_, &precipitationTurbulence_},
            {&lightningMode_, &sheetLightningFrequency_, &forkLightningFrequency_, &lightningGlow_, &lightningBranches_, &lightningThunder_, &thunderVolume_, &lightningDelay_, &lightningSoundSet_},
            {&nightSkySliders_[0], &nightZenithColour_, &nightHorizonColour_, &nightSkySliders_[7]},
            {&nightAuroraEnabled_, &nightSkySliders_[8], &nightSkySliders_[9], &nightSkySliders_[10]},
            {&moonEnabled_, &moonPhasePreset_, &moonSliders_[0], &moonMovement_, &moonSliders_[1], &moonSliders_[2], &moonSliders_[3], &moonSliders_[4]}
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
            CreateMoonControls();
            CreateShootingStarsControls();
            environmentSectionsCreated_ = true;
        }
        // Colour channels are authored through the native hue/saturation picker.
        for (std::size_t i = 1; i <= 6; ++i) nightSkySliders_[i].SetVisible(false);
        const bool procedural = visible && session_->Scenes().GetScene().weathers.GetComponent(EditableWeatherEntity())->IsRealisticSky();
        RefreshMoonControls(visible, procedural);
        RefreshShootingStarsControls(visible, procedural);
        if(visible && bridge::CaptureShootingStars(session_->Scenes().GetScene(),EditableWeatherEntity()).enabled)
        {
            for(auto& slider:shootingStarsSliders_)children[3].push_back(&slider);
            children[3].push_back(&shootingStarsPreview_);
        }
        nightZenithColour_.SetEnabled(procedural);
        nightHorizonColour_.SetEnabled(procedural);
        if (nightColourOpen_ && (!procedural || EditableWeatherEntity() != nightColourEntity_))
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
                0.1f * NightSRGBToLinear(rgb.x),
                0.1f * NightSRGBToLinear(rgb.y),
                0.1f * NightSRGBToLinear(rgb.z));
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
            NightLinearToSRGB(nightColourValue_.x / 0.1f),
            NightLinearToSRGB(nightColourValue_.y / 0.1f),
            NightLinearToSRGB(nightColourValue_.z / 0.1f)));
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

namespace renegade::studio
{
    void StudioRenderPath::CommitLightning(const bridge::LightningAuthoringState& state)
    {
        if(!session_) return;
        auto& scene=session_->Scenes().GetScene();
        const auto entity=EditableWeatherEntity();
        if(entity==wi::ecs::INVALID_ENTITY)return;
        session_->Commands().Execute(std::make_unique<bridge::SetLightningCommand>(scene,entity,state));
        RefreshInspector();RefreshStatus();
    }

    void StudioRenderPath::CreateLightningControls()
    {
        lightningMode_.Create("Lightning Effects");
        lightningMode_.AddItem("OFF",0);
        lightningMode_.AddItem("IN-CLOUD (SHEET)",1);
        lightningMode_.AddItem("CLOUD-TO-GROUND (FORK)",2);
        lightningMode_.AddItem("BOTH",3);
        lightningMode_.SetTooltip("Choose the built-in, owner-approved sheet and fork lightning effects. Works without rain.");
        lightningMode_.OnSelect([this](const wi::gui::EventArgs& args){
            if(!session_)return;
            auto state=bridge::CaptureLightning(session_->Scenes().GetScene(),EditableWeatherEntity());
            state.mode=static_cast<bridge::LightningMode>(args.userdata);CommitLightning(state);
        });
        inspectorPanel_.AddWidget(&lightningMode_);
        auto slider=[this](SceneInspectorSlider& s,const char* name,const char* label,const char* help,
                           float lo,float hi,float steps,int which)
        {
            s.Create(lo,hi,0.0f,steps,name,label);
            s.SetTooltip(help);
            s.OnValueCommitted([this,which](float value){
                if(!session_)return;
                auto state=bridge::CaptureLightning(session_->Scenes().GetScene(),EditableWeatherEntity());
                switch(which){
                    case 0:state.sheetPerMinute=value;break;
                    case 1:state.forkPerMinute=value;break;
                    case 2:state.sheetIntensity=value;break;
                    case 3:state.forkBranchDensity=value;break;
                    case 4:state.thunderVolume=value;break;
                }
                CommitLightning(state);
            });
            inspectorPanel_.AddWidget(&s);
        };
        slider(sheetLightningFrequency_,"Sheet Lightning Frequency","SHEET / MIN",
            "Average in-cloud flashes per minute (0 disables sheet events).",0,30,300,0);
        slider(forkLightningFrequency_,"Fork Lightning Frequency","FORK / MIN",
            "Average visible cloud-to-ground strikes per minute (0 disables fork strikes).",0,30,300,1);
        slider(lightningGlow_,"Cloud Lightning Brightness","CLOUD GLOW",
            "Scattered in-cloud illumination; default preserves the approved appearance.",0,.25f,250,2);
        slider(lightningBranches_,"Fork Branch Density","BRANCHES",
            "0 means no side branches, 1 keeps the approved 3-5 branches, 2 gives more.",0,2,200,3);
        lightningThunder_.Create("Play thunder: ");
        lightningThunder_.SetTooltip("Play spatial thunder for lightning events. Does not control the flashes.");
        lightningThunder_.OnClick([this](const wi::gui::EventArgs& args){
            if(!session_)return;
            auto state=bridge::CaptureLightning(session_->Scenes().GetScene(),EditableWeatherEntity());
            state.thunder=args.bValue;CommitLightning(state);
        });
        inspectorPanel_.AddWidget(&lightningThunder_);
        slider(thunderVolume_,"Thunder Volume","VOLUME",
            "Thunder level before spatial attenuation; 0 mutes.",0,1,100,4);
        lightningDelay_.Create("Realistic sound delay: ");
        lightningDelay_.SetTooltip("Thunder arrives after lightning based on distance at 343 metres per second.");
        lightningDelay_.OnClick([this](const wi::gui::EventArgs& args){
            if(!session_)return;
            auto state=bridge::CaptureLightning(session_->Scenes().GetScene(),EditableWeatherEntity());
            state.distanceDelay=args.bValue;CommitLightning(state);
        });
        inspectorPanel_.AddWidget(&lightningDelay_);
        lightningSoundSet_.Create("Thunder Audio");
        const char* options[]={"FOUR DEFAULT SOUNDS","ALL NINE - VARIATION",
          "Lightning 01","Lightning 02","Booming 01","Distant 02",
          "Booming 02","Booming 03","Distant 03","Distant 04","Lightning 03"};
        for(unsigned i=0;i<11;++i)lightningSoundSet_.AddItem(options[i],i);
        lightningSoundSet_.SetTooltip("Select the standard four-sound rotation, all nine, or any individual sound from the Thunder library.");
        lightningSoundSet_.OnSelect([this](const wi::gui::EventArgs& args){
            if(!session_)return;
            auto state=bridge::CaptureLightning(session_->Scenes().GetScene(),EditableWeatherEntity());
            state.thunderSoundSet=static_cast<int>(args.userdata);CommitLightning(state);
        });
        inspectorPanel_.AddWidget(&lightningSoundSet_);
    }

    void StudioRenderPath::RefreshLightningControls()
    {
        if(!session_)return;
        const auto& scene=session_->Scenes().GetScene();
        const auto entity=EditableWeatherEntity();
        if(!scene.weathers.Contains(entity))return;
        const auto s=bridge::CaptureLightning(scene,entity);
        lightningMode_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(s.mode));
        sheetLightningFrequency_.SetValue(s.sheetPerMinute);
        forkLightningFrequency_.SetValue(s.forkPerMinute);
        lightningGlow_.SetValue(s.sheetIntensity);
        lightningBranches_.SetValue(s.forkBranchDensity);
        lightningThunder_.SetCheck(s.thunder);
        thunderVolume_.SetValue(s.thunderVolume);
        lightningDelay_.SetCheck(s.distanceDelay);
        lightningSoundSet_.SetSelectedByUserdataWithoutCallback(static_cast<std::uint64_t>(s.thunderSoundSet));
        const bool sheet=s.mode==bridge::LightningMode::Sheet || s.mode==bridge::LightningMode::Both;
        const bool fork=s.mode==bridge::LightningMode::Fork || s.mode==bridge::LightningMode::Both;
        sheetLightningFrequency_.SetEnabled(sheet);
        lightningGlow_.SetEnabled(sheet||fork);
        forkLightningFrequency_.SetEnabled(fork);
        lightningBranches_.SetEnabled(fork);
        lightningThunder_.SetEnabled(sheet||fork);
        thunderVolume_.SetEnabled((sheet||fork)&&s.thunder);
        lightningDelay_.SetEnabled((sheet||fork)&&s.thunder);
        lightningSoundSet_.SetEnabled((sheet||fork)&&s.thunder);
    }
}
