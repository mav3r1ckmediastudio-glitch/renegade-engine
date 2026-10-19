#pragma once
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenLiteralTextInput.h"
#include "RenegadeScreenChoicePicker.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
namespace renegade::studio
{
    class RenegadeScreenInteractionInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Ordered local UI interactions");
            step_.Create("Select local action step");event_.Create("Local UI trigger");
            kind_.Create("Local UI action kind");target_.Create("Local UI action target");
            type_.Create("Local UI action value type");value_.Create("Local UI action value");
            condition_.Create("Local UI condition source");comparison_.Create("Local UI condition comparison");
            conditionType_.Create("Local UI condition value type");conditionValue_.Create("Local UI condition value");
            add_.Create("Append local UI step");add_.SetText("APPEND STEP");
            replace_.Create("Replace local UI step");replace_.SetText("REPLACE STEP");
            remove_.Create("Remove local UI step");remove_.SetText("REMOVE STEP");
            up_.Create("Earlier local UI step");up_.SetText("MOVE EARLIER");
            down_.Create("Later local UI step");down_.SetText("MOVE LATER");
            dispatch_.Create("Switch between local-only and Flow-after-local button activation");
            value_.SetCancelInputEnabled(false);conditionValue_.SetCancelInputEnabled(false);
            step_.OnClick([this](const wi::gui::EventArgs&){const auto* w=Selected();if(w&&!w->interactions.empty()){index_=(index_+1)%w->interactions.size();LoadDraft();}});
            kind_.OnClick([this](const wi::gui::EventArgs&){kindIndex_=(kindIndex_+1)%4;Labels();});
            target_.OnClick([this](const wi::gui::EventArgs&){Pick(false);});
            type_.OnClick([this](const wi::gui::EventArgs&){typeIndex_=(typeIndex_+1)%3;Labels();});
            condition_.OnClick([this](const wi::gui::EventArgs&){Pick(true);});
            comparison_.OnClick([this](const wi::gui::EventArgs&){comparisonIndex_=(comparisonIndex_+1)%6;Labels();});
            conditionType_.OnClick([this](const wi::gui::EventArgs&){conditionTypeIndex_=(conditionTypeIndex_+1)%3;Labels();});
            add_.OnClick([this](const wi::gui::EventArgs&){Apply(true);});
            replace_.OnClick([this](const wi::gui::EventArgs&){Apply(false);});
            remove_.OnClick([this](const wi::gui::EventArgs&){Edit(-2);});
            up_.OnClick([this](const wi::gui::EventArgs&){Edit(-1);});
            down_.OnClick([this](const wi::gui::EventArgs&){Edit(1);});
            dispatch_.OnClick([this](const wi::gui::EventArgs&)
            {
                const auto* w=Selected();if(!w||w->kind!=bridge::ScreenWidgetKind::Button)return;
                std::string error;
                if(!session_->SetButtonLocalOnly(id_,!w->localOnly,error))
                {status_="Flow dispatch change rejected: "+error;return;}
                LoadDraft();if(changed_)changed_();
            });
        }
        void Bind(bridge::ScreenAuthoringSession* session,bridge::StableId id,std::function<void()> changed)
        {if(id_!=id)index_=0;session_=session;id_=std::move(id);changed_=std::move(changed);LoadDraft();}
        bool HasOpenPopup() const noexcept{return picker_.IsOpen();}
        void ClosePopup(){picker_.Close();}
        void SetActive(bool active){SetVisible(active);if(!active){picker_.Close();for(auto* c:Controls())c->SetVisible(false);}}
        void SetLayout(float x,float y,float width,float height)
        {x_=x;y_=y;width_=width;SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height));picker_.SetLayout(x,y,width,height);}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);if(!IsVisible()||!session_||!session_->IsLoaded())return;
            if(revision_!=session_->Revision())LoadDraft();if(picker_.IsOpen()){picker_.Update();return;}
            const auto* w=Selected();const bool ready=w&&bridge::IsScreenInteractive(w->kind);
            const bool existing=ready&&index_<w->interactions.size();const auto controls=Controls();const float cell=(width_-6)*.5f;
            for(std::size_t i=0;i<controls.size();++i)
            {
                auto* c=controls[i];bool enabled=ready;
                if(c==&event_)enabled=false;
                if(c==&dispatch_)enabled=ready&&w->kind==bridge::ScreenWidgetKind::Button;
                if(c==&replace_||c==&remove_||c==&up_||c==&down_)enabled=existing;
                if(c==&comparison_||c==&conditionType_||c==&conditionValue_)enabled=ready&&!conditionId_.empty();
                c->SetVisible(true);c->SetEnabled(enabled);
                c->SetPos(XMFLOAT2(x_+(i%2)*(cell+6),y_+float(i/2)*34));c->SetSize(XMFLOAT2(cell,28));c->Update(canvas,dt);
            }
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible()||!session_||!session_->IsLoaded())return;
            for(auto* c:const_cast<RenegadeScreenInteractionInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);c->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);wi::font::Params t;t.posX=x_;t.posY=y_+280;t.size=10;t.h_wrap=width_;t.color=wi::Color(170,190,200,255);
            wi::font::Draw(status_,t,cmd);picker_.Render(cmd);
        }
    private:
        const bridge::ScreenWidget* Selected() const{return session_?session_->FindWidget(id_):nullptr;}
        std::string Name(const bridge::StableId& id) const
        {const auto* w=session_?session_->FindWidget(id):nullptr;return w?w->name:"MISSING";}
        void Labels()
        {
            const auto* w=Selected();step_.SetText(w&&!w->interactions.empty()?
                "STEP "+std::to_string(index_+1)+"/"+std::to_string(w->interactions.size()):"NO STEPS");
            event_.SetText(w&&w->kind==bridge::ScreenWidgetKind::Button?"ON ACTIVATE":"ON VALUE CHANGE");
            dispatch_.SetText(w&&w->kind==bridge::ScreenWidgetKind::Button?
                (w->localOnly?"DISPATCH: LOCAL ONLY":"DISPATCH: LOCAL + FLOW"):"FLOW DISPATCH: N/A");
            const char* kinds[]={"SET VISIBLE","SET ENABLED","SET UI VALUE","FOCUS"};
            const char* types[]={"NUMBER","BOOLEAN","TEXT"};const char* comparisons[]={"==","!=","<","<=",">",">="};
            kind_.SetText(kinds[kindIndex_]);target_.SetText("TO: "+Name(targetId_));
            type_.SetText(std::string("VALUE: ")+types[typeIndex_]);
            condition_.SetText(conditionId_.empty()?"ALWAYS // NO CONDITION":"IF: "+Name(conditionId_));
            comparison_.SetText(std::string("COMPARE: ")+comparisons[comparisonIndex_]);
            conditionType_.SetText(std::string("IF VALUE: ")+types[conditionTypeIndex_]);
            value_.SetLiteral(typeIndex_!=0);conditionValue_.SetLiteral(conditionTypeIndex_!=0);
        }
        void ValueToField(const bridge::GameplayAttributeValue& value,int& type,RenegadeScreenLiteralTextInput& field)
        {
            type=int(value.index());if(type==0)field.SetValue(bridge::ScreenControlNumber(std::get<double>(value)));
            else if(type==1)field.SetValue(std::get<bool>(value)?"true":"false");else field.SetValue(std::get<std::string>(value));
        }
        bool Parse(int type,const std::string& text,bridge::GameplayAttributeValue& value)
        {
            if(type==0){double n=0;std::istringstream in(text);in.imbue(std::locale::classic());in>>std::noskipws>>n;
                if(in.fail()||in.peek()!=std::char_traits<char>::eof()||!std::isfinite(n))return false;value=n;}
            else if(type==1){if(text!="true"&&text!="false")return false;value=text=="true";}
            else{if(text.size()>4096)return false;value=text;}return true;
        }
        void LoadDraft()
        {
            if(!session_||!session_->IsLoaded())return;revision_=session_->Revision();const auto* w=Selected();if(!w)return;
            if(index_>=w->interactions.size())index_=0;
            if(w->interactions.empty()){kindIndex_=0;targetId_=id_;typeIndex_=1;value_.SetValue("true");conditionId_.clear();comparisonIndex_=0;conditionTypeIndex_=1;conditionValue_.SetValue("true");}
            else
            {
                const auto& action=w->interactions[index_];kindIndex_=int(action.kind);targetId_=action.targetId;
                ValueToField(action.value,typeIndex_,value_);
                conditionId_=action.condition?action.condition->widgetId:"";
                comparisonIndex_=action.condition?int(action.condition->comparison):0;
                if(action.condition)ValueToField(action.condition->value,conditionTypeIndex_,conditionValue_);
            }
            status_=std::string("Bounded local steps run in order. Failure keeps previous UI state. ")+
                (w->kind==bridge::ScreenWidgetKind::Button?
                    (w->localOnly?"Local-only Button never dispatches Flow.":"Button Flow action follows local steps."):
                    "Value controls never dispatch Flow.");
            Labels();
        }
        void Pick(bool condition)
        {
            if(!session_)return;std::vector<RenegadeScreenChoicePicker::Choice> choices;
            if(condition)choices.push_back({"ALWAYS // NO CONDITION",""});
            for(const auto& w:session_->Document().widgets)
                if(!condition||bridge::IsScreenValueControl(w.kind))choices.push_back({w.name+" // "+std::string(bridge::ScreenWidgetKindName(w.kind)),w.id});
            picker_.Open(condition?"CONDITION UI VALUE SOURCE":"LOCAL ACTION TARGET",std::move(choices),"",[this,condition](const std::string& id){
                if(condition)conditionId_=id;else targetId_=id;Labels();
            });
        }
        void Commit(std::vector<bridge::ScreenLocalAction> actions)
        {
            std::string error;if(!session_->UpdateWidgetInteractions(id_,std::move(actions),error)){status_=error;return;}
            LoadDraft();if(changed_)changed_();
        }
        void Apply(bool append)
        {
            const auto* w=Selected();if(!w)return;bridge::ScreenLocalAction action;
            action.event=w->kind==bridge::ScreenWidgetKind::Button?bridge::ScreenInteractionEvent::Activate:bridge::ScreenInteractionEvent::ValueChanged;
            action.kind=static_cast<bridge::ScreenLocalActionKind>(kindIndex_);action.targetId=targetId_;
            if(!Parse(typeIndex_,value_.GetValue(),action.value)){status_="Invalid action value for selected type.";return;}
            if(!conditionId_.empty())
            {
                bridge::ScreenLocalCondition condition;condition.widgetId=conditionId_;condition.comparison=static_cast<bridge::ScreenLocalComparison>(comparisonIndex_);
                if(!Parse(conditionTypeIndex_,conditionValue_.GetValue(),condition.value)){status_="Invalid condition value.";return;}action.condition=std::move(condition);
            }
            auto actions=w->interactions;
            if(append){if(actions.size()>=32){status_="Maximum 32 steps.";return;}actions.push_back(std::move(action));index_=actions.size()-1;}
            else{if(index_>=actions.size())return;actions[index_]=std::move(action);}
            Commit(std::move(actions));
        }
        void Edit(int operation)
        {
            const auto* w=Selected();if(!w||index_>=w->interactions.size())return;auto actions=w->interactions;
            if(operation==-2)actions.erase(actions.begin()+index_);
            else{const auto next=static_cast<int>(index_)+operation;if(next<0||next>=int(actions.size()))return;std::swap(actions[index_],actions[next]);index_=static_cast<unsigned>(next);}
            Commit(std::move(actions));
        }
        std::vector<wi::gui::Widget*> Controls()
        {return {&step_,&event_,&kind_,&target_,&type_,&value_,&condition_,&comparison_,&conditionType_,&conditionValue_,&add_,&replace_,&remove_,&up_,&down_,&dispatch_};}
        bridge::ScreenAuthoringSession* session_=nullptr;bridge::StableId id_,targetId_,conditionId_;std::function<void()> changed_;
        std::uint64_t revision_=0;unsigned index_=0;int kindIndex_=0,typeIndex_=1,conditionTypeIndex_=1,comparisonIndex_=0;
        RenegadeButton step_,event_,kind_,target_,type_,condition_,comparison_,conditionType_,add_,replace_,remove_,up_,down_,dispatch_;
        RenegadeScreenLiteralTextInput value_,conditionValue_;RenegadeScreenChoicePicker picker_;
        float x_=0,y_=0,width_=320;std::string status_;
    };
}
