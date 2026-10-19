#pragma once
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include <functional>
#include <sstream>

namespace renegade::studio
{
    class RenegadeScreenCompositionInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Screen composition Inspector");
            input_.Create("Screen UI input policy");cursor_.Create("Screen cursor policy");
            block_.Create("Screen block gameplay input");pause_.Create("Screen pause gameplay");
            underlay_.Create("Screen HUD underlay visibility");
            underlay_.OnClick([this](const wi::gui::EventArgs&){draft_.showHudUnderlay=!draft_.showHudUnderlay;Labels();});
            focus_.Create("Screen take keyboard focus");apply_.Create("Apply screen composition");
            reset_.Create("Reset screen composition");order_.Create("Screen composition order");
            order_.SetDescription("LAYER ORDER  ");order_.SetCancelInputEnabled(false);
            input_.OnClick([this](const wi::gui::EventArgs&){
                draft_.uiInput=static_cast<bridge::ScreenUiInputPolicy>((static_cast<int>(draft_.uiInput)+1)%3);
                if(draft_.uiInput==bridge::ScreenUiInputPolicy::None)draft_.takeKeyboardFocus=false;
                if(draft_.uiInput==bridge::ScreenUiInputPolicy::Modal)draft_.blockGameplayInput=true;
                Labels();});
            cursor_.OnClick([this](const wi::gui::EventArgs&){
                draft_.cursor=static_cast<bridge::ScreenCursorPolicy>((static_cast<int>(draft_.cursor)+1)%3);Labels();});
            block_.OnClick([this](const wi::gui::EventArgs&){draft_.blockGameplayInput=!draft_.blockGameplayInput;Labels();});
            pause_.OnClick([this](const wi::gui::EventArgs&){draft_.pauseGameplay=!draft_.pauseGameplay;
                if(draft_.pauseGameplay)draft_.blockGameplayInput=true;Labels();});
            focus_.OnClick([this](const wi::gui::EventArgs&){draft_.takeKeyboardFocus=!draft_.takeKeyboardFocus;Labels();});
            apply_.SetText("APPLY SCREEN POLICY");reset_.SetText("USE USAGE DEFAULTS");
            apply_.OnClick([this](const wi::gui::EventArgs&){Apply(false);});
            reset_.OnClick([this](const wi::gui::EventArgs&){Apply(true);});
        }
        void Bind(bridge::ScreenAuthoringSession* session,std::function<void()> changed)
        {session_=session;changed_=std::move(changed);Load();}
        void SetActive(bool active)
        {SetVisible(active);if(!active)for(auto* control:Controls())control->SetVisible(false);}
        void SetLayout(float x,float y,float width,float height)
        {x_=x;y_=y;width_=width;height_=height;SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height));}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);
            if(!IsVisible() || !session_ || !session_->IsLoaded())return;
            if(revision_!=session_->Revision())Load();
            float y=y_;
            for(auto* control:Controls())
            {control->SetVisible(true);control->SetPos(XMFLOAT2(x_,y));control->SetSize(XMFLOAT2(width_,28));y+=35;
                control->Update(canvas,dt);}
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible() || !session_ || !session_->IsLoaded())return;
            for(auto* control:const_cast<RenegadeScreenCompositionInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);control->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);
            wi::font::Params text;text.posX=x_;text.posY=y_+height_-48;text.size=9;
            text.h_wrap=width_;text.color=wi::Color(145,170,181,255);wi::font::Draw(status_,text,cmd);
        }
    private:
        void Load()
        {
            if(!session_ || !session_->IsLoaded())return;
            revision_=session_->Revision();draft_=bridge::EffectiveScreenCompositionPolicy(session_->Document());
            order_.SetValue(std::to_string(draft_.order));Labels();
            status_=session_->Document().composition?"Authored Screen policy. Flow remains the route owner.":
                "Using defaults for this Screen usage. Apply creates an explicit override.";
        }
        void Labels()
        {
            const char* inputs[]={"NONE","CONTROLS","MODAL"};const char* cursors[]={"INHERIT","SHOW","HIDE"};
            input_.SetText(std::string("UI INPUT: ")+inputs[static_cast<int>(draft_.uiInput)]);
            cursor_.SetText(std::string("CURSOR: ")+cursors[static_cast<int>(draft_.cursor)]);
            block_.SetText(draft_.blockGameplayInput?"BLOCK GAMEPLAY INPUT: YES":"BLOCK GAMEPLAY INPUT: NO");
            pause_.SetText(draft_.pauseGameplay?"PAUSE GAMEPLAY: YES":"PAUSE GAMEPLAY: NO");
            focus_.SetText(draft_.takeKeyboardFocus?"TAKE KEYBOARD FOCUS: YES":"TAKE KEYBOARD FOCUS: NO");
            underlay_.SetText(draft_.showHudUnderlay?"SHOW HUD UNDERLAY: YES":"SHOW HUD UNDERLAY: NO");
            focus_.SetEnabled(draft_.uiInput!=bridge::ScreenUiInputPolicy::None);
        }
        void Apply(bool reset)
        {
            if(!session_ || !session_->IsLoaded())return;
            auto policy=draft_;
            if(!reset)
            {
                std::istringstream value(order_.GetValue());value.imbue(std::locale::classic());value>>std::noskipws>>policy.order;
                if(value.fail() || value.peek()!=std::char_traits<char>::eof())
                {status_="Layer order must be an integer from -1024 to 1024.";return;}
            }
            std::string error;
            if(!session_->UpdateScreenComposition(reset?std::optional<bridge::ScreenCompositionPolicy>{}:policy,error))
            {status_=error;return;}
            Load();if(changed_)changed_();
        }
        std::vector<wi::gui::Widget*> Controls()
        {return {&input_,&cursor_,&block_,&pause_,&focus_,&underlay_,&order_,&apply_,&reset_};}
        bridge::ScreenAuthoringSession* session_=nullptr;
        std::function<void()> changed_;
        bridge::ScreenCompositionPolicy draft_;
        std::uint64_t revision_=0;
        float x_=0,y_=0,width_=320,height_=500;
        std::string status_;
        RenegadeButton input_,cursor_,block_,pause_,focus_,underlay_,apply_,reset_;
        RenegadeTextInputField order_;
    };
}
