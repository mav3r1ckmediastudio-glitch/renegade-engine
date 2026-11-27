#pragma once
#include "StudioModalWindow.h"
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenColorPicker.h"
#include "renegade/bridge/ScreenClickSound.h"
#include "renegade/bridge/DialogueAssetService.h"
#include <memory>
#include <filesystem>
#include <functional>
#include <cstdlib>
namespace renegade::studio
{
    class ScreenLiquidEditor final : public StudioModalWindow,public std::enable_shared_from_this<ScreenLiquidEditor>
    {
        bridge::ScreenWidget value_;
        std::string root_,status_;
        std::uint64_t generation_=0;
        RenegadeButton colour_,health_,mana_,strength_,decrease_,increase_,silence_,save_,cancel_;
        RenegadeTextInputField current_,maximum_,wave_,bubbles_;
        RenegadeCheckBox animated_;
        RenegadeSlider previewValue_;
        std::function<void(bridge::ScreenColor,float)> preview_;
        RenegadeScreenColorPicker picker_;
        std::vector<wi::gui::Widget*> controls_;
        std::function<bool(bridge::ScreenLiquid,bridge::ScreenColor,std::vector<bridge::ScreenBinding>,std::string&)> accept_;
        std::function<void()> closed_;
        bridge::ScreenBinding binding_;
        void Add(wi::gui::Widget& w,float x,float y,float width,float height)
        {w.SetPos({x,y});w.SetSize({width,height});w.SetShadowRadius(0);AddWidget(&w);controls_.push_back(&w);}
        void Button(RenegadeButton& b,const char* name,float x,float y,float width)
        {b.Create(name);b.SetText(name);b.SetAccentOutlineStyle(true);b.SetRenderTextSize(14);Add(b,x,y,width,32);}
        void Field(RenegadeTextInputField& f,const char* name,float x,float y,float width)
        {f.Create(name);f.SetDescription("");f.SetRenderTextSize(14);Add(f,x,y,width,28);}
        void Preset(const char* attribute,bridge::ScreenColor colour)
        {
            binding_.source={bridge::ScreenBindingScope::PossessedPlayer,{},std::string(attribute)+".current"};
            binding_.denominator={bridge::ScreenBindingScope::PossessedPlayer,{},std::string(attribute)+".maximum"};
            current_.SetValue(binding_.source.attributeId);maximum_.SetValue(binding_.denominator.attributeId);Colour(colour);
        }
        void Colour(bridge::ScreenColor colour)
        {for(auto* v:{&value_.style.normal,&value_.style.hover,&value_.style.pressed,&value_.style.focused,&value_.style.disabled})v->imageTint=colour;
            if(preview_)preview_(colour,previewValue_.GetValue()/100.f);}
        void ChooseSound(bool increase)
        {
            const auto weak=weak_from_this();const auto generation=generation_;
            wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Choose liquid value-change sound";d.extensions={"wav","ogg"};
            wi::helper::FileDialog(d,[weak,generation,increase](const std::string& path){
                if(path.empty())return;
                wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,increase,path](std::uint64_t){
                    const auto self=weak.lock();if(!self||!self->IsVisible()||self->generation_!=generation)return;
                    std::string clip,error;
                    if(!bridge::ImportUiClickSound(self->root_,path,clip,error)){self->status_=error;return;}
                    auto& liquid=*self->value_.liquid;
                    (increase?liquid.increaseSound:liquid.decreaseSound)=clip;self->SoundLabels();
                    self->status_="Sound imported. Save applies it to this prefab instance.";
                });
            });
        }
        void SoundLabels()
        {
            const auto label=[](const std::string& path){return path.empty()?std::string("none"):std::filesystem::u8path(path).filename().u8string();};
            decrease_.SetText("Decrease: "+label(value_.liquid->decreaseSound));increase_.SetText("Increase: "+label(value_.liquid->increaseSound));
            decrease_.SetTooltip(value_.liquid->decreaseSound);increase_.SetTooltip(value_.liquid->increaseSound);
        }
        void Save()
        {
            auto liquid=*value_.liquid;
            const auto number=[](const std::string& text,float& out){char* end=nullptr;out=std::strtof(text.c_str(),&end);return end!=text.c_str()&&!*end&&std::isfinite(out);};
            float wave=0,bubbles=0;
            if(!number(wave_.GetValue(),wave)||!number(bubbles_.GetValue(),bubbles)||bubbles!=std::floor(bubbles))
            {status_="Enter a finite wave size and a whole bubble count.";return;}
            if(bubbles<0||bubbles>32){status_="Bubble count must be 0 to 32.";return;}
            liquid.wavePixels=wave;liquid.bubbles=int(bubbles);liquid.animated=animated_.GetCheck();
            binding_.property=bridge::ScreenBindingProperty::Fill;binding_.source.attributeId=current_.GetValue();
            binding_.operation=maximum_.GetValue().empty()?bridge::ScreenBindingOperation::Direct:bridge::ScreenBindingOperation::Ratio;
            binding_.denominator.attributeId=maximum_.GetValue();
            auto bindings=value_.bindings;
            bindings.erase(std::remove_if(bindings.begin(),bindings.end(),[](const auto& b){return b.property==bridge::ScreenBindingProperty::Fill;}),bindings.end());
            bindings.push_back(binding_);std::string error;auto accept=accept_;
            if(accept&&accept(std::move(liquid),value_.style.normal.imageTint,std::move(bindings),error))Close();
            else status_=error;
        }
        static void Label(const std::string& text,float x,float y,int size,wi::Color colour,wi::graphics::CommandList cmd)
        {wi::font::Params p(x,y,size);p.color=colour;p.bolden=.12f;wi::font::Draw(text,p,cmd);}
    public:
        ~ScreenLiquidEditor(){RemoveWidgets();}
        void Create()
        {
            StudioModalWindow::Create("Liquid prefab settings",WindowControls::CLOSE|WindowControls::MOVE);SetSize({680,650});SetControlSize(32);SetShadowRadius(0);
            Button(colour_,"Choose liquid colour...",30,46,280);colour_.OnClick([this](const auto&){picker_.Open(root_,"Liquid colour",value_.style.normal.imageTint,[this](auto c){Colour(c);});});
            Button(health_,"Health",30,100,185);health_.OnClick([this](const auto&){Preset("health",{250,60,42,255});});
            Button(mana_,"Mana",230,100,185);mana_.OnClick([this](const auto&){Preset("mana",{55,130,255,255});});
            Button(strength_,"Strength",430,100,185);strength_.OnClick([this](const auto&){Preset("strength",{90,220,65,255});});
            Field(current_,"Current value attribute",30,174,585);Field(maximum_,"Maximum value attribute",30,244,585);
            animated_.Create("Animate surface and bubbles");animated_.SetRenderTextSize(14);Add(animated_,30,300,20,24);
            Field(wave_,"Wave height in source pixels",30,365,185);Field(bubbles_,"Bubble count",230,365,185);
            Button(decrease_,"Decrease sound...",30,425,185);decrease_.OnClick([this](const auto&){ChooseSound(false);});
            Button(increase_,"Increase sound...",230,425,185);increase_.OnClick([this](const auto&){ChooseSound(true);});
            Button(silence_,"Clear sounds",430,425,185);silence_.OnClick([this](const auto&){value_.liquid->increaseSound.clear();value_.liquid->decreaseSound.clear();SoundLabels();});
            previewValue_.Create(0,100,100,100,"Preview liquid level","Preview fill (%)");
            previewValue_.SetRenderTextSizes(14,14);Add(previewValue_,30,480,585,42);
            const auto preview=[this](float value){if(preview_)preview_(value_.style.normal.imageTint,value/100.f);};
            previewValue_.OnValuePreview(preview);previewValue_.OnValueCommitted(preview);
            Button(save_,"Save",0,0,116);PinFooter(save_,260,12,true);save_.OnClick([this](const auto&){Save();});
            Button(cancel_,"Cancel",0,0,116);PinFooter(cancel_,132,12,true);cancel_.OnClick([this](const auto&){Close();});
            OnClose([this](const auto&){Close();});SetVisible(false);
        }
        void Open(bridge::ScreenWidget widget,std::string root,decltype(accept_) accept,std::function<void()> closed,std::function<void(bridge::ScreenColor,float)> preview)
        {
            Close();value_=std::move(widget);root_=std::move(root);accept_=std::move(accept);closed_=std::move(closed);status_.clear();
            binding_={};binding_.property=bridge::ScreenBindingProperty::Fill;
            binding_.source.attributeId="health.current";binding_.denominator.attributeId="health.maximum";binding_.operation=bridge::ScreenBindingOperation::Ratio;
            for(const auto& b:value_.bindings)if(b.property==bridge::ScreenBindingProperty::Fill){binding_=b;break;}
            current_.SetValue(binding_.source.attributeId);maximum_.SetValue(binding_.operation==bridge::ScreenBindingOperation::Ratio?binding_.denominator.attributeId:"");
            if(binding_.source.attributeId.rfind("library.unassigned.",0)==0)
            {current_.SetValue("");maximum_.SetValue("");status_="Choose Health, Mana or Strength, or enter your own attribute names.";}
            wave_.SetValue(value_.liquid->wavePixels);bubbles_.SetValue(value_.liquid->bubbles);animated_.SetCheck(value_.liquid->animated);previewValue_.SetValue(value_.fill*100);preview_=std::move(preview);SoundLabels();SetVisible(true);
        }
        void Close()
        {++generation_;picker_.Close();previewValue_.valueInputField.Deactivate();previewValue_.Deactivate();for(auto* f:{&current_,&maximum_,&wave_,&bubbles_})if(f->GetState()==wi::gui::ACTIVE)f->Deactivate();SetVisible(false);accept_={};preview_={};if(closed_){auto cb=std::move(closed_);cb();}}
        void Update(const wi::Canvas& canvas,float dt)override
        {
            if(!IsVisible())return;
            if(picker_.IsOpen())
            {const auto p=GetPos(),s=GetSize();picker_.SetLayout(p.x+(s.x-400)*.5f,p.y+50,400,std::min(500.f,s.y-90));picker_.Update(canvas,dt);return;}
            if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
            {
                const auto pointer=wi::input::GetPointer();
                for(auto* f:{&current_,&maximum_,&wave_,&bubbles_})
                {const auto p=f->GetPos(),s=f->GetSize();if(f->GetState()==wi::gui::ACTIVE&&
                    (pointer.x<p.x||pointer.x>=p.x+s.x||pointer.y<p.y||pointer.y>=p.y+s.y))
                    {const auto text=f->GetCurrentInputValue();f->Deactivate();f->SetValue(text);}}
            }
            StudioModalWindow::Update(canvas,dt);
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override
        {
            if(!IsVisible())return;const auto p=GetPos(),s=GetSize();ApplyScissor(canvas,scissorRect,cmd);
            theme::ButtonRect(p.x,p.y,s.x,s.y,theme::Border,cmd,8);theme::ButtonRect(p.x+1,p.y+1,s.x-2,s.y-2,theme::Canvas,cmd,7);
            theme::ButtonRect(p.x+1,p.y+1,s.x-2,38,theme::Panel,cmd,6);Label("LIQUID PREFAB",p.x+18,p.y+9,18,theme::Text,cmd);
            const auto origin=ContentOrigin();
            Label("LIQUID COLOUR",origin.x+30,origin.y+20,14,theme::Accent,cmd);
            Label("CURRENT VALUE ATTRIBUTE",origin.x+30,origin.y+148,14,theme::Accent,cmd);
            Label("MAXIMUM ATTRIBUTE (BLANK FOR A 0-1 VALUE)",origin.x+30,origin.y+218,14,theme::Accent,cmd);
            Label("WAVE HEIGHT",origin.x+30,origin.y+340,14,theme::Accent,cmd);Label("BUBBLES",origin.x+230,origin.y+340,14,theme::Accent,cmd);
            Label("VALUE-CHANGE SOUNDS (OPTIONAL)",origin.x+30,origin.y+400,14,theme::Accent,cmd);
            const auto colour=value_.style.normal.imageTint;theme::ButtonRect(origin.x+330,origin.y+46,285,32,wi::Color(colour.red,colour.green,colour.blue,colour.alpha),cmd,4);
            for(auto* w:controls_){ApplyScissor(canvas,scissorRect,cmd);w->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);Label(status_,p.x+18,p.y+s.y-24,12,theme::Muted,cmd);
            const auto c=closeButton.GetPos(),z=closeButton.GetSize();theme::DrawButton(c.x,c.y,z.x,z.y,"X",{true,closeButton.GetState()==wi::gui::FOCUS,false},14,cmd);
            picker_.Render(canvas,cmd);
        }
    };
}
