#pragma once
#include "StudioModalWindow.h"
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenColorPicker.h"
#include "renegade/bridge/HurtFeedbackService.h"
#include "renegade/bridge/WorldNavigationService.h"
#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/screen/ScreenRenderer.h"
#include <memory>
#include <cstdlib>
namespace renegade::studio
{
class ScreenHurtEditor final:public StudioModalWindow,public std::enable_shared_from_this<ScreenHurtEditor>
{
    bridge::ScreenHurtFeedback config_;std::string root_,status_;bridge::StableId project_,previewId_;
    unsigned generation_=0;bool previewLoaded_=false,audio_=false;
    std::function<bool(bridge::ScreenHurtFeedback,std::string&)> accept_;std::function<void()> closed_;
    std::vector<std::unique_ptr<RenegadeTextInputField>> numbers_;
    std::vector<std::unique_ptr<RenegadeCheckBox>> flags_;
    std::vector<std::unique_ptr<RenegadeButton>> resources_;
    std::vector<wi::gui::Widget*> controls_;
    RenegadeButton save_,cancel_,front_,left_,right_,back_,zone_,generic_,clearSounds_,colour_,lowColour_,sound_;
    RenegadeCheckBox lowSample_;
    RenegadeScreenColorPicker picker_;
    wi::RenderPath2D previewPath_;screen::ScreenRenderer renderer_;bridge::PlayerHurtFeedbackSession preview_;
    void Add(wi::gui::Widget& w,float x,float y,float width,float height){w.SetPos({x,y});w.SetSize({width,height});w.SetShadowRadius(0);AddWidget(&w);controls_.push_back(&w);}
    void Button(RenegadeButton& b,const std::string& text,float x,float y,float width,const char* tip){b.Create(text);b.SetRenderTextSize(13);b.SetAccentOutlineStyle(true);b.SetTooltip(tip);Add(b,x,y,width,32);}
    void Drafts(){for(auto& field:numbers_)if(field->GetState()==wi::gui::ACTIVE){const auto text=field->GetCurrentInputValue();field->Deactivate();field->SetValue(text);}}
    bool Capture(){for(size_t i=0;i<numbers_.size();++i){const auto text=numbers_[i]->GetValue();char* end=nullptr;const float n=std::strtof(text.c_str(),&end);
            if(end==text.c_str()||*end||!std::isfinite(n)){status_="Enter a valid number for "+std::string(bridge::HurtNumbers()[i].label);return false;}
            config_.*(bridge::HurtNumbers()[i].member)=n;}
        for(size_t i=0;i<flags_.size();++i)config_.*(bridge::HurtFlags()[i].member)=flags_[i]->GetCheck();
        return bridge::ValidateHurtFeedback(config_,status_);
    }
    void Preview(){
        if(!IsVisible()||!Capture())return;
        if(previewLoaded_)renderer_.Reset(previewPath_);previewLoaded_=false;
        auto hurt=bridge::MakePlayerHurtWidget(440,248);hurt.hurt=config_;previewId_=hurt.id;
        bridge::ScreenWidget backing;backing.id=bridge::GenerateStableId();backing.name="Neutral preview";backing.kind=bridge::ScreenWidgetKind::Image;backing.enabled=false;
        backing.rect={0,0,440,248};backing.style=hurt.style;backing.style.normal.background={45,61,74,255};
        bridge::ScreenDocument doc;doc.schemaVersion=3;doc.designWidth=440;doc.designHeight=248;doc.usage=bridge::ScreenUsage::Hud;
        doc.envelope=bridge::CreateDocumentEnvelope(project_,bridge::RuntimeScreenDocumentType,"Content/UI/HurtPreview.renegade-screen","hurt-preview");doc.widgets={backing,hurt};
        std::string error;if(!renderer_.Load(doc,root_,previewPath_,{},error)){status_=error;return;}
        renderer_.SetInputEnabled(false);previewLoaded_=true;
    }
    void Hit(std::optional<float> bearing){Drafts();if(!Capture())return;preview_.Damage(25,100,bearing);}
    void Choose(size_t i){
        Drafts();if(!Capture())return;const auto field=bridge::HurtResources()[i];const auto weak=weak_from_this();const auto generation=generation_;
        wi::helper::FileDialogParams d;d.type=wi::helper::FileDialogParams::OPEN;d.description=field.label;
        d.extensions=field.audio?std::vector<std::string>{"wav","ogg"}:std::vector<std::string>{"png","jpg","jpeg","dds","tga","webp"};
        wi::helper::FileDialog(d,[weak,generation,i](const std::string& path){if(path.empty())return;
            wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[weak,generation,i,path](std::uint64_t){
                const auto self=weak.lock();if(!self||!self->IsVisible()||self->generation_!=generation)return;
                const auto& f=bridge::HurtResources()[i];std::string output,error;
                const bool ok=f.audio?bridge::ImportUiClickSound(self->root_,path,output,error):bridge::ImportScreenNavigationImage(self->root_,self->project_,path,output,error);
                if(!ok){self->status_=error;return;}self->config_.*(f.member)=output;self->Preview();self->status_="Imported. Save applies this draft.";
            });});
    }
    void Pick(bool low){Drafts();if(!Capture())return;const auto rgba=low?config_.lowColour:config_.colour;
        picker_.Open(root_,low?"Low health tint":"Hit / direction tint",{uint8_t(rgba>>24),uint8_t(rgba>>16),uint8_t(rgba>>8),uint8_t(rgba)},[this,low](auto c){
            const auto value=(std::uint32_t(c.red)<<24)|(std::uint32_t(c.green)<<16)|(std::uint32_t(c.blue)<<8)|c.alpha;
            (low?config_.lowColour:config_.colour)=value;Preview();});}
    static void Label(const std::string& text,float x,float y,int size,wi::Color colour,wi::graphics::CommandList cmd){wi::font::Params p(x,y,size);p.color=colour;p.bolden=.1f;wi::font::Draw(text,p,cmd);}
public:
    ~ScreenHurtEditor(){renderer_.Reset(previewPath_);RemoveWidgets();}
    void Create(){
        StudioModalWindow::Create("Player hurt feedback",wi::gui::Window::WindowControls::CLOSE|wi::gui::Window::WindowControls::MOVE);SetSize({1100,900});SetControlSize(32);SetShadowRadius(0);
        for(size_t i=0;i<bridge::HurtFlags().size();++i){auto f=std::make_unique<RenegadeCheckBox>();const auto& field=bridge::HurtFlags()[i];f->Create(field.label);f->SetRenderTextSize(13);
            f->SetTooltip("Independent effect switch. Save applies the same setting used by C++ and Lua.");Add(*f,24+(i%2)*270.f,48+(i/2)*42.f,20,22);f->OnClick([this](const auto&){Preview();});flags_.push_back(std::move(f));}
        Button(front_,"Front",584,300,80,"Preview an accepted hit from ahead.");front_.OnClick([this](const auto&){Hit(0);});
        Button(left_,"Left",672,300,80,"Preview a hit from the left.");left_.OnClick([this](const auto&){Hit(-90);});
        Button(right_,"Right",760,300,80,"Preview a hit from the right.");right_.OnClick([this](const auto&){Hit(90);});
        Button(back_,"Behind",848,300,80,"Preview a hit from behind.");back_.OnClick([this](const auto&){Hit(180);});
        Button(zone_,"Zone",936,300,88,"Preview damage with no attacker direction.");zone_.OnClick([this](const auto&){Hit({});});
        for(size_t i=0;i<bridge::HurtNumbers().size();++i){const auto& field=bridge::HurtNumbers()[i];auto f=std::make_unique<RenegadeTextInputField>();f->Create(field.label);f->SetDescription("");f->SetRenderTextSize(14);
            const auto tip=std::string(field.label)+": "+std::to_string(field.minimum)+" to "+std::to_string(field.maximum)+".";
            f->SetTooltip(tip);Add(*f,24+(i%2)*270.f,270+(i/2)*58.f,242,28);f->OnInputAccepted([this](const auto&){Preview();});numbers_.push_back(std::move(f));}
        for(size_t i=0;i<bridge::HurtResources().size();++i){auto b=std::make_unique<RenegadeButton>();const auto& f=bridge::HurtResources()[i];
            Button(*b,std::string(f.label)+"...",584,370+i*44.f,440,f.audio?"Choose a project-owned WAV/OGG. Empty means silent.":"Replace this effect's procedural default with your own alpha artwork. White tint preserves its colours.");
            b->OnClick([this,i](const auto&){Choose(i);});resources_.push_back(std::move(b));}
        Button(colour_,"Hit / direction tint...",584,650,212,"Tint generic and custom hit artwork.");colour_.OnClick([this](const auto&){Pick(false);});
        Button(lowColour_,"Low health tint...",812,650,212,"Tint the independently styled low-health overlay.");lowColour_.OnClick([this](const auto&){Pick(true);});
        Button(generic_,"Use generic visuals",584,696,212,"Clear all four artwork overrides, preserving sounds and numeric settings.");generic_.OnClick([this](const auto&){config_.hitImage.clear();config_.edgeImage.clear();config_.directionImage.clear();config_.lowImage.clear();Preview();});
        Button(clearSounds_,"Clear sounds",812,696,212,"Clear both custom sound clips.");clearSounds_.OnClick([this](const auto&){config_.painSound.clear();config_.heartbeatSound.clear();Preview();});
        lowSample_.Create("Low health sample");lowSample_.SetRenderTextSize(13);lowSample_.SetTooltip("Preview at 15% health. This does not change gameplay or the saved HUD.");Add(lowSample_,584,750,20,22);
        Button(sound_,"Audition selected sounds",812,744,212,"Play selected pain/heartbeat clips in this editor sample. No health is changed.");sound_.OnClick([this](const auto&){audio_=true;Hit(0);});
        Button(save_,"Save",0,0,116,"Apply all settings in one Undo operation.");PinFooter(save_,260,12,true);save_.OnClick([this](const auto&){Drafts();if(!Capture())return;std::string error;if(accept_&&accept_(config_,error))Close();else status_=error;});
        Button(cancel_,"Cancel",0,0,116,"Discard the settings draft.");PinFooter(cancel_,132,12,true);cancel_.OnClick([this](const auto&){Close();});OnClose([this](const auto&){Close();});SetVisible(false);
    }
    void Open(bridge::ScreenHurtFeedback c,std::string root,bridge::StableId project,std::function<bool(bridge::ScreenHurtFeedback,std::string&)> accept,std::function<void()> closed){
        Close();config_=std::move(c);root_=std::move(root);project_=std::move(project);accept_=std::move(accept);closed_=std::move(closed);audio_=false;preview_.Reset();lowSample_.SetCheck(false);
        for(size_t i=0;i<numbers_.size();++i)numbers_[i]->SetValue(config_.*(bridge::HurtNumbers()[i].member));
        for(size_t i=0;i<flags_.size();++i)flags_[i]->SetCheck(config_.*(bridge::HurtFlags()[i].member));
        status_.clear();SetVisible(true);Preview();Hit(0);
    }
    void Close(){++generation_;for(auto& f:numbers_)f->Deactivate();renderer_.Reset(previewPath_);previewLoaded_=false;picker_.Close();SetVisible(false);accept_={};if(closed_){auto cb=std::move(closed_);cb();}}
    void Update(const wi::Canvas& canvas,float dt)override{
        if(!IsVisible())return;if(picker_.IsOpen()){const auto p=GetPos(),s=GetSize();picker_.SetLayout(p.x+(s.x-400)*.5f,p.y+50,400,std::min(500.f,s.y-90));picker_.Update(canvas,dt);return;}
        StudioModalWindow::Update(canvas,dt);
        if(previewLoaded_){preview_.Tick(dt,true,true,lowSample_.GetCheck()?15:100,100,0);renderer_.PreviewHurtFeedback(previewId_,preview_.Frame(),audio_);
            const auto o=ContentOrigin();renderer_.SetViewport({o.x+584,o.y+38,440,248});
            auto clip=scissorRect;clip.top=std::max(clip.top,int(GetPos().y+38));clip.bottom=std::min(clip.bottom,int(GetPos().y+GetSize().y-76));
            renderer_.SetClipViewport({float(clip.left),float(clip.top),float(clip.right-clip.left),float(clip.bottom-clip.top)});renderer_.ApplyLayout(previewPath_);previewPath_.GetGUI().Update(canvas,dt);}
    }
    void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override{
        if(!IsVisible())return;const auto p=GetPos(),s=GetSize();ApplyScissor(canvas,scissorRect,cmd);
        theme::ButtonRect(p.x,p.y,s.x,s.y,theme::Border,cmd,8);theme::ButtonRect(p.x+1,p.y+1,s.x-2,s.y-2,theme::Canvas,cmd,7);
        theme::ButtonRect(p.x+1,p.y+1,s.x-2,38,theme::Panel,cmd,6);Label("PLAYER HURT FEEDBACK",p.x+18,p.y+9,18,theme::Text,cmd);
        auto clip=scissorRect;clip.top=std::max(clip.top,int(p.y+38));clip.bottom=std::min(clip.bottom,int(p.y+s.y-76));ApplyScissor(canvas,clip,cmd);const auto o=ContentOrigin();
        Label("EFFECTS / INDEPENDENT SWITCHES",o.x+24,o.y+16,13,theme::Accent,cmd);Label("PLAYER VIEW SAMPLE",o.x+584,o.y+16,13,theme::Accent,cmd);
        Label("CUSTOM ARTWORK / SOUNDS",o.x+584,o.y+346,13,theme::Accent,cmd);
        for(size_t i=0;i<numbers_.size();++i)Label(bridge::HurtNumbers()[i].label,o.x+24+(i%2)*270.f,o.y+247+(i/2)*58.f,13,theme::Accent,cmd);
        for(auto* c:controls_){ApplyScissor(canvas,c==&save_||c==&cancel_?scissorRect:clip,cmd);c->Render(canvas,cmd);}
        if(previewLoaded_)previewPath_.GetGUI().Render(canvas,cmd);
        ApplyScissor(canvas,scissorRect,cmd);Label(status_.empty()?"Real damage drives gameplay. Healing and loading never create hits. Shake is off by default.":status_.substr(0,145),p.x+18,p.y+s.y-24,12,theme::Muted,cmd);
        const auto cp=closeButton.GetPos(),cs=closeButton.GetSize();theme::DrawButton(cp.x,cp.y,cs.x,cs.y,"X",{true,closeButton.GetState()==wi::gui::FOCUS,false},14,cmd);
        for(auto* c:controls_)c->RenderTooltip(canvas,cmd);picker_.Render(canvas,cmd);
    }
};
}
