#include "StudioApplication.h"

namespace renegade::studio
{
    namespace
    {
        void SetMeteorField(bridge::ShootingStarsState& s,std::size_t i,float v)
        {
            if(i==0)s.frequency=v;
            if(i==1)s.brightness=v;
            if(i==2)s.trailLength=v;
            s=bridge::SanitizeShootingStars(s);
        }
    }
    void StudioRenderPath::CreateShootingStarsControls()
    {
        shootingStarsEnabled_.Create("Shooting stars: ");
        shootingStarsEnabled_.SetTooltip("Occasional night-sky meteors, independent of star density. Extra controls appear when enabled.");
        shootingStarsEnabled_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if(!session_)return;
            auto& scene=session_->Scenes().GetScene();
            const auto e=EditableWeatherEntity();
            if(!scene.weathers.Contains(e))return;
            auto s=bridge::CaptureShootingStars(scene,e);s.enabled=args.bValue;
            session_->Commands().Execute(std::make_unique<bridge::SetShootingStarsCommand>(scene,e,s));
            RefreshInspector();RefreshStatus();
        });
        inspectorPanel_.AddWidget(&shootingStarsEnabled_);
        const char* names[]={"Shooting star frequency","Shooting star brightness","Shooting star trail length"};
        const char* labels[]={"SIGHTINGS / MINUTE","STREAK BRIGHTNESS","TRAIL LENGTH (DEGREES)"};
        const char* tips[]={
            "Average across the whole sky, not just your current view. 0.1 is rare; 120 is a meteor shower. Hidden in daylight.",
            "Brightness of meteor heads and fading trails. Zero suppresses the effect without changing native stars.",
            "Angular trail length. Each meteor moves quickly and fades in under a second."};
        const float lo[]={.1f,0,1},hi[]={120,4,25};
        for(std::size_t i=0;i<shootingStarsSliders_.size();++i)
        {
            auto& slider=shootingStarsSliders_[i];
            slider.Create(lo[i],hi[i],0,1200,names[i],labels[i]);
            slider.SetTooltip(tips[i]);
            slider.OnDragStarted([this](float)
            {
                meteorSliderActive_=false;
                if(!session_)return;
                auto& scene=session_->Scenes().GetScene();
                meteorSliderEntity_=EditableWeatherEntity();
                if(!scene.weathers.Contains(meteorSliderEntity_))return;
                meteorSliderBefore_=bridge::CaptureShootingStars(scene,meteorSliderEntity_);
                meteorSliderAfter_=meteorSliderBefore_;
                meteorSliderActive_=true;
            });
            slider.OnValuePreview([this,i](float v){PreviewShootingStarsSlider(i,v);});
            slider.OnValueCommitted([this,i](float v){CommitShootingStarsSlider(i,v);});
            inspectorPanel_.AddWidget(&slider);
        }
        shootingStarsPreview_.Create("Preview shooting star");
        shootingStarsPreview_.SetText("PREVIEW SHOOTING STAR");
        shootingStarsPreview_.SetTooltip("Look toward open night sky to preview one streak. This does not change saved settings or Undo history.");
        shootingStarsPreview_.OnClick([this](const wi::gui::EventArgs&)
        {
            if(session_)bridge::PreviewShootingStar(session_->Scenes().GetScene(),EditableWeatherEntity(),frameCB.time);
        });
        inspectorPanel_.AddWidget(&shootingStarsPreview_);
    }
    void StudioRenderPath::RefreshShootingStarsControls(bool visible,bool procedural)
    {
        shootingStarsEnabled_.SetVisible(visible);
        shootingStarsEnabled_.SetEnabled(procedural);
        for(auto& s:shootingStarsSliders_)s.SetVisible(false);
        shootingStarsPreview_.SetVisible(false);
        if(!visible || !session_)return;
        const auto s=bridge::CaptureShootingStars(session_->Scenes().GetScene(),EditableWeatherEntity());
        shootingStarsEnabled_.SetCheck(s.enabled);
        const float values[]={s.frequency,s.brightness,s.trailLength};
        for(std::size_t i=0;i<shootingStarsSliders_.size();++i)
        {
            shootingStarsSliders_[i].SetValue(values[i]);
            shootingStarsSliders_[i].SetEnabled(procedural && s.enabled);
        }
        shootingStarsPreview_.SetEnabled(procedural && s.enabled && s.brightness>0);
    }
    void StudioRenderPath::PreviewShootingStarsSlider(std::size_t i,float v)
    {
        if(!meteorSliderActive_ || !session_)return;
        auto& scene=session_->Scenes().GetScene();
        if(!scene.weathers.Contains(meteorSliderEntity_)){meteorSliderActive_=false;return;}
        meteorSliderAfter_=meteorSliderBefore_;
        SetMeteorField(meteorSliderAfter_,i,v);
        bridge::ApplyShootingStarsRuntime(scene,meteorSliderEntity_,meteorSliderAfter_);
    }
    void StudioRenderPath::CommitShootingStarsSlider(std::size_t i,float v)
    {
        if(!session_)return;
        auto& scene=session_->Scenes().GetScene();
        if(!meteorSliderActive_)
        {
            meteorSliderEntity_=EditableWeatherEntity();
            if(!scene.weathers.Contains(meteorSliderEntity_))return;
            meteorSliderBefore_=bridge::CaptureShootingStars(scene,meteorSliderEntity_);
        }
        if(scene.weathers.Contains(meteorSliderEntity_))
        {
            meteorSliderAfter_=meteorSliderBefore_;
            SetMeteorField(meteorSliderAfter_,i,v);
            bridge::ApplyShootingStarsRuntime(scene,meteorSliderEntity_,meteorSliderBefore_);
            session_->Commands().Execute(std::make_unique<bridge::SetShootingStarsCommand>(scene,meteorSliderEntity_,meteorSliderAfter_));
        }
        meteorSliderActive_=false;
        meteorSliderEntity_=wi::ecs::INVALID_ENTITY;
        RefreshInspector();RefreshStatus();
    }
}
