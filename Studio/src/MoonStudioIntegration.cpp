#include "StudioApplication.h"
#include <algorithm>
#include <cmath>

namespace renegade::studio
{
    namespace
    {
        void SetMoonField(bridge::MoonState& s, std::size_t i, float v)
        {
            switch(i)
            {
            case 0:s.phase=v;break;
            case 1:s.diameter=v;break;
            case 2:s.brightness=v;break;
            case 3:s.azimuth=v;break;
            case 4:s.elevation=v;break;
            }
            s=bridge::SanitizeMoon(s);
        }
    }
    void StudioRenderPath::CreateMoonControls()
    {
        moonEnabled_.Create("Moon enabled: ");
        moonEnabled_.SetTooltip("Draw a lunar disc in the realistic sky. Legacy scenes remain unchanged until enabled.");
        moonEnabled_.OnClick([this](const wi::gui::EventArgs& args)
        {
            if(!session_) return;
            const auto e=EditableWeatherEntity();
            auto& scene=session_->Scenes().GetScene();
            if(!scene.weathers.Contains(e)) return;
            auto s=bridge::CaptureMoon(scene,e); s.enabled=args.bValue;
            session_->Commands().Execute(std::make_unique<bridge::SetMoonCommand>(scene,e,s));
            RefreshInspector();RefreshStatus();
        });
        inspectorPanel_.AddWidget(&moonEnabled_);
        moonPhasePreset_.Create("Moon phase");
        moonPhasePreset_.AddItem("CUSTOM",0);
        const char* phases[]={"NEW MOON","WAXING CRESCENT","FIRST QUARTER","WAXING GIBBOUS",
            "FULL MOON","WANING GIBBOUS","LAST QUARTER","WANING CRESCENT"};
        for(std::size_t i=0;i<8;++i) moonPhasePreset_.AddItem(phases[i],i+1);
        moonPhasePreset_.SetTooltip("Choose a fixed phase. The phase slider allows any value between presets.");
        moonPhasePreset_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if(!session_ || args.userdata==0) return;
            const auto e=EditableWeatherEntity();
            auto& scene=session_->Scenes().GetScene();
            if(!scene.weathers.Contains(e)) return;
            auto s=bridge::CaptureMoon(scene,e);
            s.phase=float(args.userdata-1)/8.0f;
            session_->Commands().Execute(std::make_unique<bridge::SetMoonCommand>(scene,e,s));
            RefreshInspector();RefreshStatus();
        });
        inspectorPanel_.AddWidget(&moonPhasePreset_);
        moonMovement_.Create("Moon movement");
        moonMovement_.AddItem("FOLLOW TIME OF DAY",0);
        moonMovement_.AddItem("FIXED POSITION",1);
        moonMovement_.SetTooltip("Automatic orbit follows opposite the sun. Selected phase stays fixed. Heading offsets the automatic path.");
        moonMovement_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            if(!session_) return;
            const auto e=EditableWeatherEntity();
            auto& scene=session_->Scenes().GetScene();
            if(!scene.weathers.Contains(e)) return;
            auto s=bridge::CaptureMoon(scene,e);s.automatic=args.userdata==0;
            session_->Commands().Execute(std::make_unique<bridge::SetMoonCommand>(scene,e,s));
            RefreshInspector();RefreshStatus();
        });
        inspectorPanel_.AddWidget(&moonMovement_);
        const char* names[]={"Moon phase","Moon apparent size","Moon brightness","Moon heading","Moon elevation"};
        const char* labels[]={"PHASE","SIZE (DEGREES)","BRIGHTNESS","HEADING / ORBIT OFFSET","FIXED ELEVATION"};
        const char* tips[]={
            "0 = new, 0.25 = first quarter, 0.5 = full, 0.75 = last quarter, 1 = new. Fixed phase; one Undo step.",
            "Angular diameter in degrees. Real-world scale is about 0.5 degrees; larger values give a cinematic moon.",
            "Brightness of the visible disc. This first version does not create a moonlight or shadows.",
            "Compass heading in fixed mode, or heading offset of the automatic Time of Day orbit.",
            "Elevation above the horizon in fixed mode. Negative values place the moon below the horizon."};
        const float minimum[]={0,.1f,0,-180,-90}, maximum[]={1,12,4,180,90};
        for(std::size_t i=0;i<moonSliders_.size();++i)
        {
            auto& slider=moonSliders_[i];
            slider.Create(minimum[i],maximum[i],0,720,names[i],labels[i]);
            slider.SetTooltip(tips[i]);
            slider.OnDragStarted([this](float)
            {
                moonSliderActive_=false;
                if(!session_) return;
                auto& scene=session_->Scenes().GetScene();
                moonSliderEntity_=EditableWeatherEntity();
                if(!scene.weathers.Contains(moonSliderEntity_)) return;
                moonSliderBefore_=bridge::CaptureMoon(scene,moonSliderEntity_);
                moonSliderAfter_=moonSliderBefore_;
                moonSliderActive_=true;
            });
            slider.OnValuePreview([this,i](float v){PreviewMoonSlider(i,v);});
            slider.OnValueCommitted([this,i](float v){CommitMoonSlider(i,v);});
            inspectorPanel_.AddWidget(&slider);
        }
    }
    void StudioRenderPath::RefreshMoonControls(bool visible,bool procedural)
    {
        moonEnabled_.SetVisible(visible);
        moonEnabled_.SetEnabled(procedural);
        if(!session_ || !visible) return;
        const auto s=bridge::CaptureMoon(session_->Scenes().GetScene(),EditableWeatherEntity());
        moonEnabled_.SetCheck(s.enabled);
        int preset=0;
        for(int i=0;i<8;++i)
            if(std::abs(s.phase-i/8.0f)<.0001f) preset=i+1;
        if(std::abs(s.phase-1)<.0001f) preset=1;
        moonPhasePreset_.SetSelectedWithoutCallback(preset);
        moonMovement_.SetSelectedWithoutCallback(s.automatic?0:1);
        moonPhasePreset_.SetEnabled(procedural && s.enabled);
        moonMovement_.SetEnabled(procedural && s.enabled);
        const float values[]={s.phase,s.diameter,s.brightness,s.azimuth,s.elevation};
        for(std::size_t i=0;i<moonSliders_.size();++i)
        {
            moonSliders_[i].SetValue(values[i]);
            moonSliders_[i].SetEnabled(procedural && s.enabled && (i!=4 || !s.automatic));
        }
    }
    void StudioRenderPath::PreviewMoonSlider(std::size_t i,float v)
    {
        if(!moonSliderActive_ || !session_) return;
        auto& scene=session_->Scenes().GetScene();
        if(!scene.weathers.Contains(moonSliderEntity_)) {moonSliderActive_=false;return;}
        moonSliderAfter_=moonSliderBefore_;
        SetMoonField(moonSliderAfter_,i,v);
        bridge::ApplyMoonRuntime(scene,moonSliderEntity_,moonSliderAfter_);
    }
    void StudioRenderPath::CommitMoonSlider(std::size_t i,float v)
    {
        if(!session_) return;
        auto& scene=session_->Scenes().GetScene();
        if(!moonSliderActive_)
        {
            moonSliderEntity_=EditableWeatherEntity();
            if(!scene.weathers.Contains(moonSliderEntity_)) return;
            moonSliderBefore_=bridge::CaptureMoon(scene,moonSliderEntity_);
        }
        if(scene.weathers.Contains(moonSliderEntity_))
        {
            moonSliderAfter_=moonSliderBefore_;
            SetMoonField(moonSliderAfter_,i,v);
            bridge::ApplyMoonRuntime(scene,moonSliderEntity_,moonSliderBefore_);
            session_->Commands().Execute(std::make_unique<bridge::SetMoonCommand>(scene,moonSliderEntity_,moonSliderAfter_));
        }
        moonSliderActive_=false;
        moonSliderEntity_=wi::ecs::INVALID_ENTITY;
        RefreshInspector();RefreshStatus();
    }
}
