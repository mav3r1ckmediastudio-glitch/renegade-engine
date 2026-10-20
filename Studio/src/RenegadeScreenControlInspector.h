#pragma once
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenLiteralTextInput.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
namespace renegade::studio
{
    class RenegadeScreenControlInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("UI control values");
            value_.Create("Initial UI control value");value_.SetDescription("INITIAL VALUE  ");
            minimum_.Create("Slider minimum");minimum_.SetDescription("SLIDER MINIMUM  ");
            maximum_.Create("Slider maximum");maximum_.SetDescription("SLIDER MAXIMUM  ");
            step_.Create("Slider step");step_.SetDescription("SLIDER STEP  ");
            choices_.Create("Selector choices");choices_.SetDescription("CHOICES WITH |  ");
            limit_.Create("Text input limit");limit_.SetDescription("TEXT BYTE LIMIT  ");
            for(auto* input:{&value_,&minimum_,&maximum_,&step_,&choices_,&limit_})input->SetCancelInputEnabled(false);
            apply_.Create("Apply UI control");apply_.SetText("APPLY CONTROL SETTINGS");
            earlier_.Create("Control focus earlier");earlier_.SetText("FOCUS EARLIER");
            later_.Create("Control focus later");later_.SetText("FOCUS LATER");
            apply_.OnClick([this](const wi::gui::EventArgs&){Apply();});
            earlier_.OnClick([this](const wi::gui::EventArgs&){Move(-1);});
            later_.OnClick([this](const wi::gui::EventArgs&){Move(1);});
        }
        void Bind(bridge::ScreenAuthoringSession* session,bridge::StableId id,std::function<void()> changed)
        {session_=session;id_=std::move(id);changed_=std::move(changed);Load();}
        void SetActive(bool active){SetVisible(active);if(!active)for(auto* c:Controls())c->SetVisible(false);}
        void SetLayout(float x,float y,float width,float height)
        {x_=x;y_=y;width_=width;SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height));}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);if(!IsVisible()||!session_||!session_->IsLoaded())return;
            if(revision_!=session_->Revision())Load();const auto* w=session_->FindWidget(id_);
            if(!w||!bridge::IsScreenValueControl(w->kind))return;
            float y=y_;for(auto* c:Controls())
            {c->SetVisible(true);c->SetEnabled(true);c->SetPos(XMFLOAT2(x_,y));c->SetSize(XMFLOAT2(width_,28));y+=35;c->Update(canvas,dt);}
            const bool slider=w->kind==bridge::ScreenWidgetKind::Slider;
            minimum_.SetEnabled(slider);maximum_.SetEnabled(slider);step_.SetEnabled(slider);
            choices_.SetEnabled(w->kind==bridge::ScreenWidgetKind::Selector);
            limit_.SetEnabled(w->kind==bridge::ScreenWidgetKind::TextInput);
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible()||!session_||!session_->IsLoaded())return;
            for(auto* c:const_cast<RenegadeScreenControlInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);c->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);wi::font::Params t;
            t.posX=x_;t.posY=y_+330;t.size=10;t.h_wrap=width_;t.color=wi::Color(145,170,181,255);
            wi::font::Draw(status_,t,cmd);
        }
    private:
        template<class T> bool Number(const std::string& text,T& value)
        {std::istringstream s(text);s.imbue(std::locale::classic());s>>std::noskipws>>value;
            return !s.fail()&&s.peek()==std::char_traits<char>::eof()&&std::isfinite(double(value));}
        void Load()
        {
            if(!session_||!session_->IsLoaded())return;revision_=session_->Revision();
            const auto* w=session_->FindWidget(id_);if(!w||!bridge::IsScreenValueControl(w->kind))return;
            value_.SetLiteral(w->kind==bridge::ScreenWidgetKind::TextInput||w->kind==bridge::ScreenWidgetKind::Toggle);
            choices_.SetLiteral(true);
            const auto& c=w->control;
            if(const auto* number=std::get_if<double>(&c.value))value_.SetValue(bridge::ScreenControlNumber(*number));
            else if(const auto* boolean=std::get_if<bool>(&c.value))value_.SetValue(*boolean?"true":"false");
            else value_.SetValue(std::get<std::string>(c.value));
            minimum_.SetValue(bridge::ScreenControlNumber(c.minimum));maximum_.SetValue(bridge::ScreenControlNumber(c.maximum));
            step_.SetValue(bridge::ScreenControlNumber(c.step));limit_.SetValue(std::to_string(c.textLimit));
            std::string choices;for(const auto& option:c.options){if(!choices.empty())choices+='|';choices+=option;}
            choices_.SetValue(choices);status_="Initial local UI state. Runtime edits stay in the UI instance. No gameplay, audio or graphics setting is changed implicitly. Toggle: true/false. Selector: zero-based index.";
        }
        void Apply()
        {
            if(!session_)return;const auto* w=session_->FindWidget(id_);if(!w)return;
            auto c=w->control;
            if(w->kind==bridge::ScreenWidgetKind::Toggle)
            {
                const auto v=value_.GetValue();if(v!="true"&&v!="false"){status_="Toggle initial value must be true or false.";return;}c.value=v=="true";
            }
            else if(w->kind==bridge::ScreenWidgetKind::TextInput)
            {
                unsigned limit=0;if(!Number(limit_.GetValue(),limit)){status_="Invalid text byte limit.";return;}
                c.textLimit=limit;c.value=value_.GetValue();
            }
            else
            {
                double value=0;if(!Number(value_.GetValue(),value)){status_="Invalid initial numeric value.";return;}c.value=value;
                if(w->kind==bridge::ScreenWidgetKind::Slider&&
                    (!Number(minimum_.GetValue(),c.minimum)||!Number(maximum_.GetValue(),c.maximum)||!Number(step_.GetValue(),c.step)))
                {status_="Invalid Slider range or step.";return;}
                if(w->kind==bridge::ScreenWidgetKind::Selector)
                {
                    const auto choices=choices_.GetValue();
                    if(choices.empty()||choices.front()=='|'||choices.back()=='|')
                    {status_="Selector choices cannot be empty.";return;}
                    c.options.clear();std::istringstream values(choices);std::string option;
                    while(std::getline(values,option,'|'))c.options.push_back(option);
                }
            }
            std::string error;
            if(!session_->UpdateWidgetControl(id_,std::move(c),error)){status_=error;return;}
            Load();if(changed_)changed_();
        }
        void Move(int direction)
        {
            if(!session_)return;std::string error;
            const bool ok=direction<0?session_->MoveButtonFocusEarlier(id_,error):session_->MoveButtonFocusLater(id_,error);
            if(!ok){status_=error;return;}Load();if(changed_)changed_();
        }
        std::vector<wi::gui::Widget*> Controls()
        {return {&value_,&minimum_,&maximum_,&step_,&choices_,&limit_,&apply_,&earlier_,&later_};}
        bridge::ScreenAuthoringSession* session_=nullptr;bridge::StableId id_;std::function<void()> changed_;
        std::uint64_t revision_=0;float x_=0,y_=0,width_=320;std::string status_;
        RenegadeScreenLiteralTextInput value_,minimum_,maximum_,step_,choices_,limit_;RenegadeButton apply_,earlier_,later_;
    };
}
