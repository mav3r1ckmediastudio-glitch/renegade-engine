#pragma once
#include "StudioModalWindow.h"
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenColorPicker.h"
#include "renegade/bridge/ScreenLiquidMaskService.h"
#include "renegade/bridge/DialogueAssetService.h"
#include <memory>
#include <filesystem>
#include <functional>
#include <cstdlib>
#include <sstream>
#include <iomanip>
namespace renegade::studio
{
class ScreenLiquidEditor final : public StudioModalWindow, public std::enable_shared_from_this<ScreenLiquidEditor>
{
    using Accept=std::function<bool(std::optional<bridge::ScreenLiquid>,bridge::ScreenColor,std::vector<bridge::ScreenBinding>,bridge::ScreenFillDirection,std::string&)>;
    using Preview=std::function<bool(const std::optional<bridge::ScreenLiquid>&,bridge::ScreenFillDirection,bridge::ScreenColor,float,std::string&)>;
    bridge::ScreenWidget value_;
    bridge::StableId project_;
    std::string root_,status_;
    std::uint64_t generation_=0;
    RenegadeButton colour_,health_,mana_,strength_,mask_,fit_,direction_,decrease_,increase_,silence_,save_,cancel_;
    RenegadeTextInputField current_,maximum_,wave_,bubbles_,left_,right_,top_,bottom_;
    RenegadeCheckBox enabled_,animated_;
    RenegadeSlider previewValue_;
    RenegadeScreenColorPicker picker_;
    wi::Resource maskImage_;
    std::vector<wi::gui::Widget*> controls_;
    Accept accept_; Preview preview_; std::function<void()> closed_;
    bridge::ScreenBinding binding_;
    void Add(wi::gui::Widget& w,float x,float y,float width,float height)
    {w.SetPos({x,y});w.SetSize({width,height});w.SetShadowRadius(0);AddWidget(&w);controls_.push_back(&w);}
    void Button(RenegadeButton& b,const char* name,float x,float y,float width,const char* help)
    {b.Create(name);b.SetText(name);b.SetAccentOutlineStyle(true);b.SetRenderTextSize(14);b.SetTooltip(help);Add(b,x,y,width,32);}
    void Field(RenegadeTextInputField& f,const char* name,float x,float y,float width,const char* help)
    {f.Create(name);f.SetDescription("");f.SetRenderTextSize(14);f.SetTooltip(help);Add(f,x,y,width,28);f.OnInputAccepted([this](const auto&){PreviewDraft();});}
    std::vector<wi::gui::TextInputField*> Fields()
    {return {&current_,&maximum_,&wave_,&bubbles_,&left_,&right_,&top_,&bottom_,&previewValue_.valueInputField};}
    static bool Number(const std::string& text,float& out)
    {char* end=nullptr;out=std::strtof(text.c_str(),&end);return end!=text.c_str()&&!*end&&std::isfinite(out);}
    static std::string Short(std::string text,size_t n=70)
    {if(text.size()>n)text.resize(n-3),text+="...";return text;}
    void KeepDrafts()
    {for(auto* f:Fields())if(f->GetState()==wi::gui::ACTIVE){const auto text=f->GetCurrentInputValue();f->Deactivate();f->SetValue(text);}}
    bool Draft(std::optional<bridge::ScreenLiquid>& result)
    {
        if(!enabled_.GetCheck()){result.reset();return true;}
        auto liquid=*value_.liquid;float bubbles;
        if(!Number(left_.GetValue(),liquid.left)||!Number(right_.GetValue(),liquid.right)||
            !Number(top_.GetValue(),liquid.top)||!Number(bottom_.GetValue(),liquid.bottom)||
            !Number(wave_.GetValue(),liquid.wavePixels)||!Number(bubbles_.GetValue(),bubbles)||
            bubbles!=std::floor(bubbles)||bubbles<0||bubbles>32)
        {status_="Enter valid bounds, wave height and a whole bubble count (0-32).";return false;}
        liquid.left/=100;liquid.right/=100;liquid.top/=100;liquid.bottom/=100;
        liquid.bubbles=int(bubbles);liquid.animated=animated_.GetCheck();
        if(liquid.mask.empty()){status_="Choose a mask: white inside the fill shape; black or transparency outside.";return false;}
        if(!bridge::ValidateScreenLiquidNumbers(liquid))
        {status_="Bounds must be 0-100%, with left before right and top before bottom; wave height is 0-16 pixels.";return false;}
        result=std::move(liquid);return true;
    }
    void PreviewDraft()
    {
        std::optional<bridge::ScreenLiquid> draft;if(!preview_||!Draft(draft))return;
        std::string error;if(!preview_(draft,value_.fillDirection,value_.style.normal.imageTint,previewValue_.GetValue()/100.f,error))status_=error;
    }
    void Colour(bridge::ScreenColor colour)
    {for(auto* v:{&value_.style.normal,&value_.style.hover,&value_.style.pressed,&value_.style.focused,&value_.style.disabled})v->imageTint=colour;PreviewDraft();}
    void Preset(const char* attribute,bridge::ScreenColor colour)
    {
        binding_.source={bridge::ScreenBindingScope::PossessedPlayer,{},std::string(attribute)+".current"};
        binding_.denominator={bridge::ScreenBindingScope::PossessedPlayer,{},std::string(attribute)+".maximum"};
        current_.SetValue(binding_.source.attributeId);maximum_.SetValue(binding_.denominator.attributeId);Colour(colour);
    }
    void Bounds()
    {const auto& l=*value_.liquid;left_.SetValue(l.left*100);right_.SetValue(l.right*100);top_.SetValue(l.top*100);bottom_.SetValue(l.bottom*100);}
    void MaskImage()
    {
        maskImage_={};std::string absolute,error;
        if(bridge::ResolveScreenResourcePath(root_,value_.liquid->mask,absolute,error))maskImage_=wi::resourcemanager::Load(absolute);
        mask_.SetText(value_.liquid->mask.empty()?"Choose mask...":"Change mask...");
        mask_.SetTooltip("White defines the fill shape; black or transparency stays empty. Current mask: "+value_.liquid->mask);
    }
    void ChooseMask()
    {
        KeepDrafts();const auto weak=weak_from_this();const auto generation=generation_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Choose fill mask (white shape, black or transparent outside)";d.extensions={"png","bmp","jpg","jpeg"};
        wi::helper::FileDialog(d,[weak,generation](const std::string& path){if(path.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,path](std::uint64_t){
                const auto self=weak.lock();if(!self||!self->IsVisible()||self->generation_!=generation)return;
                auto liquid=*self->value_.liquid;std::string error;
                if(!bridge::ImportScreenLiquidMask(self->root_,self->project_,path,liquid,error)){self->status_=error;return;}
                self->value_.liquid=std::move(liquid);self->enabled_.SetCheck(true);self->Bounds();self->MaskImage();self->PreviewDraft();
                self->status_="Mask imported and fill bounds fitted. Save applies the change; Cancel keeps this layer unchanged.";
            });
        });
    }
    void ChooseSound(bool increase)
    {
        KeepDrafts();const auto weak=weak_from_this();const auto generation=generation_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description="Choose fill value-change sound";d.extensions={"wav","ogg"};
        wi::helper::FileDialog(d,[weak,generation,increase](const std::string& path){if(path.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,increase,path](std::uint64_t){
                const auto self=weak.lock();if(!self||!self->IsVisible()||self->generation_!=generation)return;
                std::string clip,error;if(!bridge::ImportUiClickSound(self->root_,path,clip,error)){self->status_=error;return;}
                auto& l=*self->value_.liquid;(increase?l.increaseSound:l.decreaseSound)=clip;self->Sounds();self->PreviewDraft();
                self->status_="Sound imported. Save applies it to this fill layer.";
            });
        });
    }
    void Sounds()
    {
        const auto label=[](const std::string& path){return path.empty()?std::string("none"):Short(std::filesystem::u8path(path).filename().u8string(),20);};
        decrease_.SetText("Decrease: "+label(value_.liquid->decreaseSound));increase_.SetText("Increase: "+label(value_.liquid->increaseSound));
    }
    void Direction()
    {const char* names[]={"Left to right","Right to left","Bottom to top","Top to bottom"};direction_.SetText(names[static_cast<int>(value_.fillDirection)]);}
    void Save()
    {
        KeepDrafts();std::optional<bridge::ScreenLiquid> liquid;if(!Draft(liquid))return;
        auto bindings=value_.bindings;bindings.erase(std::remove_if(bindings.begin(),bindings.end(),[](const auto& b){return b.property==bridge::ScreenBindingProperty::Fill;}),bindings.end());
        if(!current_.GetValue().empty())
        {
            binding_.property=bridge::ScreenBindingProperty::Fill;binding_.source.attributeId=current_.GetValue();
            binding_.operation=maximum_.GetValue().empty()?bridge::ScreenBindingOperation::Direct:bridge::ScreenBindingOperation::Ratio;
            binding_.denominator.attributeId=maximum_.GetValue();bindings.push_back(binding_);
        }
        std::string error;auto accept=accept_;
        if(accept&&accept(std::move(liquid),value_.style.normal.imageTint,std::move(bindings),value_.fillDirection,error))Close();else status_=error;
    }
    static void Label(const std::string& text,float x,float y,int size,wi::Color colour,wi::graphics::CommandList cmd)
    {wi::font::Params p(x,y,size);p.color=colour;p.bolden=.12f;wi::font::Draw(text,p,cmd);}
public:
    ~ScreenLiquidEditor(){RemoveWidgets();}
    void Create()
    {
        StudioModalWindow::Create("Liquid fill component",WindowControls::CLOSE|WindowControls::MOVE);SetSize({940,820});SetControlSize(32);SetShadowRadius(0);
        enabled_.Create("Use liquid fill effect");enabled_.SetRenderTextSize(14);enabled_.SetTooltip("Enable a masked fill on this Progress layer. Turn off to return to an ordinary Progress element.");Add(enabled_,28,44,20,24);enabled_.OnClick([this](const auto&){PreviewDraft();});
        Button(mask_,"Choose mask...",28,88,396,"Choose the shape containing the fill; its bounds are fitted automatically.");mask_.OnClick([this](const auto&){ChooseMask();});
        Field(left_,"Fill area left (%)",28,440,185,"Left edge of the usable fill area, as a percentage of mask width.");Field(top_,"Fill area top (%)",238,440,185,"Top edge of the usable fill area, as a percentage of mask height.");
        Field(right_,"Fill area right (%)",28,500,185,"Right edge of the usable fill area. Must exceed Left.");Field(bottom_,"Fill area bottom (%)",238,500,185,"Bottom edge of the usable fill area. Must exceed Top.");
        Button(fit_,"Fit area to mask",28,548,185,"Find the bounds of the visible mask shape automatically.");fit_.OnClick([this](const auto&){KeepDrafts();std::string error;auto l=*value_.liquid;if(!bridge::InspectScreenLiquidMask(root_,l.mask,l,error)){status_=error;return;}value_.liquid=l;Bounds();PreviewDraft();});
        Button(direction_,"Fill direction",238,548,185,"Choose which edge the fill grows from. All four directions work with any mask.");direction_.OnClick([this](const auto&){value_.fillDirection=static_cast<bridge::ScreenFillDirection>((static_cast<int>(value_.fillDirection)+1)%4);Direction();PreviewDraft();});
        Button(colour_,"Choose colour...",480,44,185,"Tint this fill layer without tinting separate frame/artwork layers.");colour_.OnClick([this](const auto&){KeepDrafts();picker_.Open(root_,"Fill colour",value_.style.normal.imageTint,[this](auto c){Colour(c);});});
        Button(health_,"Health",480,96,120,"Bind this layer to the current player's current and maximum health.");health_.OnClick([this](const auto&){Preset("health",{250,60,42,255});});
        Button(mana_,"Mana",610,96,120,"Bind this layer to current and maximum mana.");mana_.OnClick([this](const auto&){Preset("mana",{55,130,255,255});});
        Button(strength_,"Strength",740,96,140,"Bind this layer to current and maximum strength.");strength_.OnClick([this](const auto&){Preset("strength",{90,220,65,255});});
        animated_.Create("Animate surface and bubbles");animated_.SetRenderTextSize(14);animated_.SetTooltip("Animate the effect independently of the bound fill value.");Add(animated_,480,155,20,24);animated_.OnClick([this](const auto&){PreviewDraft();});
        Field(wave_,"Wave height",480,210,185,"Surface wave height in source-image pixels; 0 gives a flat surface.");Field(bubbles_,"Bubble count",695,210,185,"Number of animated bubbles, from 0 to 32. Use 0 for a clean gauge.");
        Button(decrease_,"Decrease sound...",480,270,185,"Optional cue when this layer's value decreases.");decrease_.OnClick([this](const auto&){ChooseSound(false);});
        Button(increase_,"Increase sound...",695,270,185,"Optional cue when this layer's value increases.");increase_.OnClick([this](const auto&){ChooseSound(true);});
        Button(silence_,"Clear sounds",740,315,140,"Remove this layer's value-change sound assignments.");silence_.OnClick([this](const auto&){value_.liquid->increaseSound.clear();value_.liquid->decreaseSound.clear();Sounds();PreviewDraft();});
        Field(current_,"Current value attribute",480,382,400,"Attribute driving the fill. Leave blank to use the Progress layer's authored fill value.");Field(maximum_,"Maximum value attribute",480,452,400,"Divide Current by this attribute. Leave blank when Current is already between 0 and 1.");
        previewValue_.Create(0,100,100,100,"Preview fill","Preview fill (%)");previewValue_.SetRenderTextSizes(14,14);previewValue_.SetTooltip("Audition a fill level without changing saved gameplay values.");Add(previewValue_,480,525,400,42);
        previewValue_.OnValuePreview([this](float){PreviewDraft();});previewValue_.OnValueCommitted([this](float){PreviewDraft();});
        Button(save_,"Save",0,0,116,"Apply this layer's settings as one Undo operation.");PinFooter(save_,260,12,true);save_.OnClick([this](const auto&){Save();});
        Button(cancel_,"Cancel",0,0,116,"Discard settings and reset the temporary renderer preview.");PinFooter(cancel_,132,12,true);cancel_.OnClick([this](const auto&){Close();});
        OnClose([this](const auto&){Close();});SetVisible(false);
    }
    void Open(bridge::ScreenWidget widget,std::string root,bridge::StableId project,Accept accept,std::function<void()> closed,Preview preview)
    {
        Close();value_=std::move(widget);root_=std::move(root);project_=std::move(project);accept_=std::move(accept);closed_=std::move(closed);status_.clear();
        enabled_.SetCheck(value_.liquid.has_value());if(!value_.liquid)value_.liquid.emplace();
        binding_={};binding_.property=bridge::ScreenBindingProperty::Fill;
        bool bound=false;for(const auto& b:value_.bindings)if(b.property==bridge::ScreenBindingProperty::Fill){binding_=b;bound=true;break;}
        current_.SetValue(bound?binding_.source.attributeId:"");maximum_.SetValue(bound&&binding_.operation==bridge::ScreenBindingOperation::Ratio?binding_.denominator.attributeId:"");
        if(bound&&binding_.source.attributeId.rfind("library.unassigned.",0)==0){current_.SetValue("");maximum_.SetValue("");status_="Choose a binding for this project, or leave Current blank for a manually authored value.";}
        wave_.SetValue(value_.liquid->wavePixels);bubbles_.SetValue(value_.liquid->bubbles);animated_.SetCheck(value_.liquid->animated);previewValue_.SetValue(value_.fill*100);preview_=std::move(preview);
        Bounds();Direction();Sounds();MaskImage();SetVisible(true);
    }
    void Close()
    {++generation_;picker_.Close();for(auto* f:Fields())f->Deactivate();previewValue_.Deactivate();SetVisible(false);accept_={};preview_={};maskImage_={};if(closed_){auto cb=std::move(closed_);cb();}}
    void Update(const wi::Canvas& canvas,float dt)override
    {
        if(!IsVisible())return;
        if(picker_.IsOpen()){const auto p=GetPos(),s=GetSize();picker_.SetLayout(p.x+(s.x-400)*.5f,p.y+50,400,std::min(500.f,s.y-90));picker_.Update(canvas,dt);return;}
        if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {const auto pointer=wi::input::GetPointer();for(auto* f:Fields()){const auto p=f->GetPos(),s=f->GetSize();if(f->GetState()==wi::gui::ACTIVE&&(pointer.x<p.x||pointer.x>=p.x+s.x||pointer.y<p.y||pointer.y>=p.y+s.y)){const auto text=f->GetCurrentInputValue();f->Deactivate();f->SetValue(text);}}}
        StudioModalWindow::Update(canvas,dt);
    }
    void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override
    {
        if(!IsVisible())return;const auto p=GetPos(),s=GetSize();ApplyScissor(canvas,scissorRect,cmd);
        theme::ButtonRect(p.x,p.y,s.x,s.y,theme::Border,cmd,8);theme::ButtonRect(p.x+1,p.y+1,s.x-2,s.y-2,theme::Canvas,cmd,7);theme::ButtonRect(p.x+1,p.y+1,s.x-2,38,theme::Panel,cmd,6);Label("LIQUID FILL",p.x+18,p.y+9,18,theme::Text,cmd);
        auto bodyClip=scissorRect;bodyClip.top=std::max(bodyClip.top,int(p.y+38));bodyClip.bottom=std::min(bodyClip.bottom,int(p.y+s.y-76));ApplyScissor(canvas,bodyClip,cmd);const auto o=ContentOrigin();
        const auto label=[&](const char* text,float x,float y){Label(text,o.x+x,o.y+y,14,theme::Accent,cmd);};
        label("SHAPE / MASK",28,16);label("COLOUR / BEHAVIOUR",480,16);
        Label(Short(value_.liquid->mask.empty()?"Choose white-on-black or transparent mask artwork.":std::filesystem::u8path(value_.liquid->mask).filename().u8string(),55),o.x+28,o.y+130,12,theme::Muted,cmd);
        theme::ButtonRect(o.x+28,o.y+166,396,232,theme::Panel,cmd,4);
        if(maskImage_.IsValid())
        {
            const auto& t=maskImage_.GetTexture();const auto& d=t.GetDesc();const float scale=std::min(396.f/d.width,232.f/d.height);const float w=d.width*scale,h=d.height*scale,x=o.x+28+(396-w)*.5f,y=o.y+166+(232-h)*.5f;
            wi::image::Params image(x,y,w,h);image.blendFlag=wi::enums::BLENDMODE_ALPHA;wi::image::Draw(&t,image,cmd);
        }
        label("LEFT (%)",28,416);label("TOP (%)",238,416);label("RIGHT (%)",28,476);label("BOTTOM (%)",238,476);
        label("WAVE HEIGHT",480,186);label("BUBBLES",695,186);label("VALUE-CHANGE SOUNDS (OPTIONAL)",480,246);label("CURRENT ATTRIBUTE (BLANK = MANUAL)",480,356);label("MAXIMUM ATTRIBUTE (OPTIONAL)",480,426);
        const auto c=value_.style.normal.imageTint;theme::ButtonRect(o.x+695,o.y+44,185,32,wi::Color(c.red,c.green,c.blue,c.alpha),cmd,4);
        Label("Keep glass, frames and other artwork on separate Image layers.",o.x+28,o.y+610,13,theme::Muted,cmd);
        for(auto* w:controls_){ApplyScissor(canvas,w==&save_||w==&cancel_?scissorRect:bodyClip,cmd);w->Render(canvas,cmd);}
        ApplyScissor(canvas,scissorRect,cmd);Label(Short(status_,115),p.x+18,p.y+s.y-24,12,theme::Muted,cmd);
        const auto cp=closeButton.GetPos(),cz=closeButton.GetSize();theme::DrawButton(cp.x,cp.y,cz.x,cz.y,"X",{true,closeButton.GetState()==wi::gui::FOCUS,false},14,cmd);
        for(auto* w:controls_){ApplyScissor(canvas,scissorRect,cmd);w->RenderTooltip(canvas,cmd);}picker_.Render(canvas,cmd);
    }
};
}
