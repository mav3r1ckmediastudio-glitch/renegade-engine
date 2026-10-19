#pragma once
#include "RenegadeStudioChrome.h"
#include "RenegadeScreenLiteralTextInput.h"
#include "ScreenRuntimeLiveProbe.h"
#include <algorithm>
#include <functional>
#include "renegade/bridge/ScreenSimulation.h"
#include "renegade/bridge/ScreenThemeResolver.h"
namespace renegade::studio
{
    class RenegadeScreenSimulationInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Isolated UI simulation");
            toggle_.Create("Toggle isolated UI simulation");live_.Create("Inspect active Runtime session, read-only");source_.Create("Simulation source");
            type_.Create("Simulation sample type");value_.Create("Simulation sample value");
            value_.SetDescription("SAMPLE VALUE  ");value_.SetCancelInputEnabled(false);
            apply_.Create("Apply isolated sample");apply_.SetText("SET EXPLICIT SAMPLE");
            clear_.Create("Clear isolated sample");clear_.SetText("MAKE SOURCE UNAVAILABLE");
            reset_.Create("Reset all isolated samples");reset_.SetText("CLEAR ALL SAMPLES");
            toggle_.OnClick([this](const wi::gui::EventArgs&){active_=!active_;if(active_)liveEnabled_=false;++revision_;Labels();});
            live_.OnClick([this](const wi::gui::EventArgs&){
                liveEnabled_=!liveEnabled_;active_=false;livePollAccum_=0;
                if(liveEnabled_)PollLive();else LoadSample();
                ++revision_;Labels();});
            source_.OnClick([this](const wi::gui::EventArgs&){if(!sources_.empty())index_=(index_+1)%sources_.size();LoadSample();});
            type_.OnClick([this](const wi::gui::EventArgs&){typeIndex_=(typeIndex_+1)%3;Labels();});
            apply_.OnClick([this](const wi::gui::EventArgs&){Apply();});
            clear_.OnClick([this](const wi::gui::EventArgs&){
                if(index_<sources_.size())simulation_.ClearSample(sources_[index_]);LoadSample();});
            reset_.OnClick([this](const wi::gui::EventArgs&){simulation_.Reset();LoadSample();});
            Labels();
        }
        void Bind(bridge::ScreenAuthoringSession* session,std::string root)
        {
            if(session_!=session){simulation_.Reset();active_=false;liveEnabled_=false;}
            session_=session;root_=std::move(root);Refresh();
        }
        void Clear(){simulation_.Reset();active_=false;liveEnabled_=false;session_=nullptr;sources_.clear();++revision_;}
        void SetLiveRuntimeProbe(std::function<std::string()> probe){runtimePeer_=std::move(probe);}
        bool Active() const noexcept{return active_||liveEnabled_;}
        bool Live() const noexcept{return liveEnabled_;}
        bool Evaluate(const bridge::ScreenDocument& document,bridge::ScreenBindingFrame& frame,std::string& error) const
        {
            if(!liveEnabled_)return simulation_.Evaluate(document,frame,error);
            return bridge::BuildScreenBindingFrame(document,
                liveValuesAvailable_?liveBinding_.snapshot:bridge::ScreenBindingSnapshot{},
                liveValuesAvailable_?liveBinding_.context:bridge::ScreenBindingContext{},frame,error);
        }
        void TickLive(float dt)
        {
            if(!liveEnabled_||!session_||!session_->IsLoaded())return;
            livePollAccum_+=std::max(0.0f,dt);
            if(livePollAccum_>=0.5f){livePollAccum_=0;PollLive();}
        }
        const bridge::ScreenSimulation& Simulation() const noexcept{return simulation_;}
        std::uint64_t Revision() const noexcept{return revision_+simulation_.Revision();}
        void SetFrameStatus(const bridge::ScreenBindingFrame& frame)
        {
            if(liveEnabled_)return;
            status_="ISOLATED SAMPLES // NO GAMEPLAY WRITES\n"+
                std::string(frame.ready?"Required bindings ready. ":"Required bindings unresolved. ")+
                std::to_string(frame.issues.size())+" binding issue(s).";
            if(!frame.issues.empty())status_+="\n"+frame.issues.front().message;
        }
        void SetActive(bool active){SetVisible(active);if(!active)for(auto* c:Controls())c->SetVisible(false);}
        void SetLayout(float x,float y,float width,float height)
        {x_=x;y_=y;width_=width;SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height));}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);if(!IsVisible()||!session_||!session_->IsLoaded())return;
            if(documentRevision_!=session_->Revision())Refresh();
            float y=y_;for(auto* c:Controls())
            {
                const bool editableSample=c==&source_||c==&type_||c==&value_||
                    c==&apply_||c==&clear_||c==&reset_;
                // Disable before processing input, not after, to prevent a
                // live/foreign project snapshot from being edited as a sample.
                c->SetEnabled(!editableSample||(!liveEnabled_&&!sources_.empty()));
                c->SetVisible(true);c->SetPos(XMFLOAT2(x_,y));
                c->SetSize(XMFLOAT2(width_,28));y+=35;c->Update(canvas,dt);
            }
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible()||!session_||!session_->IsLoaded())return;
            for(auto* c:const_cast<RenegadeScreenSimulationInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);c->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);wi::font::Params t;
            t.posX=x_;t.posY=y_+300;t.size=10;t.h_wrap=width_;t.color=wi::Color(145,170,181,255);
            wi::font::Draw(status_,t,cmd);
        }
    private:
        void PollLive()
        {
            const auto json=runtimePeer_?runtimePeer_():std::string{};
            liveStatus_=ProbeScreenRuntimeLive(json,
                session_&&session_->IsLoaded()?session_->Document().envelope.projectId:std::string{});
            std::string error;
            liveValuesAvailable_=ReadScreenRuntimeLiveBinding(json,liveStatus_,liveBinding_,error);
            if(!liveValuesAvailable_)liveBinding_={};
            status_=liveStatus_.detail+"\n"+(liveValuesAvailable_?
                "ACTUAL RUNTIME VALUES // READ ONLY":error);
            ++revision_;
            Labels();
        }
        void Labels()
        {
            toggle_.SetText(active_?"SIMULATE: ISOLATED SAMPLES // ON":"DESIGN // SIMULATION OFF");
            if(liveEnabled_)live_.SetText(liveStatus_.connected?
                (liveStatus_.gameplayFresh&&liveStatus_.projectMatched?
                    "LIVE TEST: MATCHED RUNTIME":"LIVE TEST: CONNECTED / NOT LIVE"):
                "LIVE TEST: DISCONNECTED");
            else live_.SetText("LIVE TEST: CONNECT TO RUNTIME");
            value_.SetLiteral(typeIndex_!=0);
            const char* types[]={"NUMBER","BOOLEAN","TEXT"};type_.SetText(std::string("TYPE: ")+types[typeIndex_]);
            if(sources_.empty())source_.SetText("NO AUTHORED BINDING SOURCES");
            else
            {
                const auto& s=sources_[index_];const char* scope[]={"PLAYER","CHARACTER","PROJECT"};
                source_.SetText(std::to_string(index_+1)+"/"+std::to_string(sources_.size())+" "+
                    scope[int(s.scope)]+": "+s.attributeId+(s.targetId.empty()?"":" ["+s.targetId.substr(0,8)+"]"));
            }
        }
        void Refresh()
        {
            if(!session_||!session_->IsLoaded())return;documentRevision_=session_->Revision();
            bridge::ScreenDocument effective;std::string error;
            if(!bridge::ResolveScreenTheme(root_,session_->Document(),effective,error))
            {sources_.clear();status_=error;Labels();return;}
            sources_=bridge::ScreenSimulation::Sources(effective);
            if(index_>=sources_.size())index_=0;
            LoadSample();
            if(liveEnabled_)PollLive();
        }
        void LoadSample()
        {
            if(index_<sources_.size())
            {
                const auto* sample=simulation_.Sample(sources_[index_]);
                if(sample)
                {
                    typeIndex_=int(sample->value.index());
                    if(typeIndex_==0)value_.SetValue(std::to_string(std::get<double>(sample->value)));
                    else if(typeIndex_==1)value_.SetValue(std::get<bool>(sample->value)?"true":"false");
                    else value_.SetValue(std::get<std::string>(sample->value));
                    status_="Explicit sample set. This is not a live Runtime value.";
                }
                else{value_.SetValue("");status_="Source UNAVAILABLE until you set an explicit sample.";}
            }
            else status_="Author a binding on DATA first. Simulation never guesses gameplay sources.";
            Labels();
        }
        void Apply()
        {
            if(index_>=sources_.size())return;
            bridge::GameplayAttributeValue value;const auto text=value_.GetValue();std::string error;
            if(typeIndex_==0)
            {
                double n=0;std::istringstream s(text);s.imbue(std::locale::classic());s>>std::noskipws>>n;
                if(s.fail()||s.peek()!=std::char_traits<char>::eof()||!std::isfinite(n))
                {status_="Enter a finite sample number.";return;}value=n;
            }
            else if(typeIndex_==1)
            {if(text!="true"&&text!="false"){status_="Boolean sample must be true or false.";return;}value=text=="true";}
            else value=text;
            if(!simulation_.SetSample(sources_[index_],std::move(value),error)){status_=error;return;}
            active_=true;liveEnabled_=false;++revision_;LoadSample();
        }
        std::vector<wi::gui::Widget*> Controls(){return {&toggle_,&live_,&source_,&type_,&value_,&apply_,&clear_,&reset_};}
        bridge::ScreenAuthoringSession* session_=nullptr;bridge::ScreenSimulation simulation_;
        std::vector<bridge::ScreenBindingSource> sources_;std::size_t index_=0;int typeIndex_=0;
        std::uint64_t documentRevision_=0,revision_=0;bool active_=false,liveEnabled_=false;float livePollAccum_=0;
        bridge::ScreenLiveBinding liveBinding_;bool liveValuesAvailable_=false;
        ScreenRuntimeLiveStatus liveStatus_;std::function<std::string()> runtimePeer_;
        RenegadeButton toggle_,live_,source_,type_,apply_,clear_,reset_;RenegadeScreenLiteralTextInput value_;
        float x_=0,y_=0,width_=320;std::string status_,root_;
    };
}
