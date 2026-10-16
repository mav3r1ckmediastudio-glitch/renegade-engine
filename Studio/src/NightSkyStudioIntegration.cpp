// Night Sky authoring — isolated Renegade Studio workspace, sharing one Environment entity.
// Existing Environment and Time of Day authoring are not duplicated.
#include "StudioApplication.h"
#include <algorithm>

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
        if (!session_ || !nightSkyWorkspaceActive_) return;
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
}
