#pragma once
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include <functional>
namespace renegade::studio
{
    class RenegadeScreenAnimationInspector final : public wi::gui::Widget
    {
    public:
        void Create()
        {
            SetName("Screen animation and transition timeline");
            target_.Create("Animation or Screen transition target");
            target_.OnClick([this](const wi::gui::EventArgs&){
                const auto* w=session_?session_->FindWidget(id_):nullptr;
                targetMode_=(targetMode_+1)%(w&&w->kind==bridge::ScreenWidgetKind::Button?5:3);
                selected_=0;preview_={};LoadTrack();});
            inherit_.Create("Restore destination transition default");
            inherit_.SetText("BUTTON // INHERIT SCREEN DEFAULTS");
            inherit_.OnClick([this](const wi::gui::EventArgs&){
                if(!session_)return;std::string error;
                if(!session_->UpdateWidgetTransitionOverride(id_,std::nullopt,error)){status_=error;return;}
                LoadTrack();if(changed_)changed_();});
            track_.Create("Animation track");property_.Create("Animation property");
            easing_.Create("Animation easing");loop_.Create("Animation loop");
            delay_.Create("Animation delay");delay_.SetDescription("DELAY SEC  ");
            keys_.Create("Animation keys");keys_.SetDescription("TIME:VALUE;...  ");
            apply_.Create("Apply animation");apply_.SetText("APPLY / ADD TRACK");
            remove_.Create("Remove animation");remove_.SetText("REMOVE THIS TRACK");
            preset_.Create("Animation preset");preset_.SetText("PRESET: FADE / SLIDE / PULSE / SCALE");
            play_.Create("Preview animation");play_.SetText("PLAY ANIMATION PREVIEW");
            stop_.Create("Stop animation preview");stop_.SetText("DESIGN / RESET");
            seek_.Create("Seek animation time");seek_.SetDescription("PREVIEW TIME SEC  ");seek_.SetValue("0");
            seekButton_.Create("Seek animation preview");seekButton_.SetText("SEEK TO TIME // PAUSE");
            motion_.Create("Reduced motion preview");motion_.SetText("REDUCED MOTION: OFF");
            for(auto* input:{&delay_,&keys_,&seek_})input->SetCancelInputEnabled(false);
            track_.OnClick([this](const wi::gui::EventArgs&){
                if(!session_)return;selected_=(selected_+1)%(Tracks().size()+1);LoadTrack();});
            property_.OnClick([this](const wi::gui::EventArgs&){
                draft_.property=static_cast<bridge::ScreenAnimationProperty>((int(draft_.property)+1)%(targetMode_?4:5));Labels();});
            easing_.OnClick([this](const wi::gui::EventArgs&){
                draft_.easing=static_cast<bridge::ScreenAnimationEasing>((int(draft_.easing)+1)%5);Labels();});
            loop_.OnClick([this](const wi::gui::EventArgs&){
                draft_.loop=static_cast<bridge::ScreenAnimationLoop>((int(draft_.loop)+1)%3);Labels();});
            apply_.OnClick([this](const wi::gui::EventArgs&){Apply(false);});
            remove_.OnClick([this](const wi::gui::EventArgs&){Apply(true);});
            preset_.OnClick([this](const wi::gui::EventArgs&){Preset();});
            play_.OnClick([this](const wi::gui::EventArgs&){
                preview_.active=true;preview_.playing=!preview_.playing;
                play_.SetText(preview_.playing?"PAUSE ANIMATION PREVIEW":"RESUME ANIMATION PREVIEW");});
            stop_.OnClick([this](const wi::gui::EventArgs&){ResetPreview();});
            seekButton_.OnClick([this](const wi::gui::EventArgs&){Seek();});
            seek_.OnInputAccepted([this](const wi::gui::EventArgs&){Seek();});
            motion_.OnClick([this](const wi::gui::EventArgs&){
                preview_.reducedMotion=!preview_.reducedMotion;
                motion_.SetText(preview_.reducedMotion?"REDUCED MOTION: ON":"REDUCED MOTION: OFF");});
            Labels();
        }
        void Bind(bridge::ScreenAuthoringSession* session,bridge::StableId id,std::function<void()> changed)
        {
            const bool different=session_!=session||id_!=id;
            session_=session;id_=std::move(id);changed_=std::move(changed);
            if(different){selected_=0;targetMode_=0;preview_={};}
            LoadTrack();
        }
        void ResetPreview()
        {preview_={};seek_.SetValue("0");play_.SetText("PLAY ANIMATION PREVIEW");motion_.SetText("REDUCED MOTION: OFF");}
        bridge::ScreenAnimationPreview Preview() const noexcept{return preview_;}
        bridge::ScreenTransitionFrame TransitionPreview() const
        {
            if(!targetMode_||!preview_.active)return {};
            const auto tracks=Tracks();
            return bridge::EvaluateScreenTransition(tracks,preview_.reducedMotion?
                bridge::ScreenTransitionDuration(tracks):preview_.seconds);
        }
        void Tick(float dt)
        {
            if(preview_.active&&preview_.playing&&std::isfinite(dt)&&dt>0)
            {preview_.seconds+=dt;if(preview_.seconds>1200)preview_.seconds=0;}
        }
        void SetActive(bool active)
        {SetVisible(active);if(!active)for(auto* c:AllControls())c->SetVisible(false);}
        void SetLayout(float x,float y,float width,float height)
        {x_=x;y_=y;width_=width;height_=height;SetPos(XMFLOAT2(x,y));SetSize(XMFLOAT2(width,height));}
        void Update(const wi::Canvas& canvas,float dt) override
        {
            Widget::Update(canvas,dt);if(!IsVisible()||!session_||!session_->IsLoaded())return;
            if(revision_!=session_->Revision())LoadTrack();
            const auto active=Controls();
            for(auto* c:AllControls())if(std::find(active.begin(),active.end(),c)==active.end()&&c->IsVisible())c->SetVisible(false);
            const float step=std::min(28.0f,std::max(22.0f,(height_-44.0f)/float(active.size())));
            float y=y_;for(auto* c:active)
            {
                c->SetEnabled(session_->FindWidget(id_)!=nullptr);
                if(!c->IsVisible())c->SetVisible(true);
                c->SetPos(XMFLOAT2(x_,y));c->SetSize(XMFLOAT2(width_,std::min(24.0f,step-3.0f)));
                y+=step;c->Update(canvas,dt);
            }
        }
        void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override
        {
            if(!IsVisible()||!session_||!session_->IsLoaded())return;
            for(auto* c:const_cast<RenegadeScreenAnimationInspector*>(this)->Controls())
            {ApplyScissor(canvas,scissorRect,cmd);c->Render(canvas,cmd);}
            ApplyScissor(canvas,scissorRect,cmd);
            const auto count=float(const_cast<RenegadeScreenAnimationInspector*>(this)->Controls().size());
            const float step=std::min(28.0f,std::max(22.0f,(height_-44.0f)/count));
            wi::font::Params t;t.posX=x_;t.posY=y_+count*step+2;t.size=10;t.h_wrap=width_;
            t.color=wi::Color(145,170,181,255);
            wi::font::Draw(preview_.active?"ANIMATION PREVIEW // "+std::to_string(preview_.seconds)+" SEC\n"+status_:status_,t,cmd);
        }
    private:
        bool Entering() const noexcept{return targetMode_==1||targetMode_==3;}
        std::vector<bridge::ScreenAnimationTrack> Tracks() const
        {
            if(!session_||!session_->IsLoaded())return {};
            const auto* w=session_->FindWidget(id_);if(!w)return {};
            if(!targetMode_)return w->animations;
            if(targetMode_<3)return Entering()?session_->Document().transitions.enter:session_->Document().transitions.exit;
            if(!w->transitionOverride)return {};
            return Entering()?w->transitionOverride->enter:w->transitionOverride->exit;
        }
        bool Store(std::vector<bridge::ScreenAnimationTrack> tracks,std::string& error)
        {
            if(!session_)return false;
            if(!targetMode_)return session_->UpdateWidgetAnimations(id_,std::move(tracks),error);
            if(targetMode_<3)
            {
                auto pair=session_->Document().transitions;
                (Entering()?pair.enter:pair.exit)=std::move(tracks);
                return session_->UpdateScreenTransitions(std::move(pair),error);
            }
            const auto* w=session_->FindWidget(id_);if(!w)return false;
            auto pair=w->transitionOverride.value_or(bridge::ScreenTransitionPair{});
            (Entering()?pair.enter:pair.exit)=std::move(tracks);
            return session_->UpdateWidgetTransitionOverride(id_,std::move(pair),error);
        }
        template<class T> bool Number(const std::string& value,T& out)
        {std::istringstream s(value);s.imbue(std::locale::classic());s>>std::noskipws>>out;
            return !s.fail()&&s.peek()==std::char_traits<char>::eof()&&std::isfinite(out);}
        void Seek()
        {
            double v=0;if(!Number(seek_.GetValue(),v)||v<0||v>1200)
            {status_="Preview time must be 0..1200 seconds.";return;}
            preview_.active=true;preview_.playing=false;preview_.seconds=v;play_.SetText("RESUME ANIMATION PREVIEW");
        }
        void Labels()
        {
            const char* targets[]={"WIDGET ANIMATION","SCREEN ENTER DEFAULT","SCREEN EXIT DEFAULT","BUTTON ENTER OVERRIDE","BUTTON EXIT OVERRIDE"};
            target_.SetText(std::string("TARGET: ")+targets[targetMode_]);
            track_.SetText("TRACK "+std::to_string(selected_+1)+" // NEXT / NEW");
            property_.SetText(std::string("PROPERTY: ")+bridge::ScreenAnimationPropertyName(draft_.property));
            const char* eases[]={"LINEAR","SMOOTH","EASE IN","EASE OUT","STEP"};
            const char* loops[]={"ONCE","REPEAT","PING PONG"};
            easing_.SetText(std::string("EASING: ")+eases[int(draft_.easing)]);
            loop_.SetText(std::string("LOOP: ")+loops[int(draft_.loop)]);
            preset_.SetText(targetMode_?"PRESET: FADE / SLIDE Y / SLIDE X / SCALE":"PRESET: FADE / SLIDE / PULSE / SCALE");
        }
        void LoadTrack()
        {
            if(!session_||!session_->IsLoaded())return;
            revision_=session_->Revision();const auto* w=session_->FindWidget(id_);if(!w)return;
            if(targetMode_>=3&&w->kind!=bridge::ScreenWidgetKind::Button)targetMode_=0;
            const auto tracks=Tracks();if(selected_>tracks.size())selected_=tracks.size();
            draft_=selected_<tracks.size()?tracks[selected_]:bridge::ScreenAnimationTrack{};
            if(draft_.keys.empty())draft_.keys=targetMode_&&!Entering()?
                std::vector<bridge::ScreenAnimationKey>{{0,1},{.4,0}}:std::vector<bridge::ScreenAnimationKey>{{0,0},{.4,1}};
            delay_.SetValue(std::to_string(draft_.delay));keys_.SetValue(bridge::AnimationKeysText(draft_.keys));
            Labels();status_=targetMode_?
                "Transitions finish once within 30 sec. Enter ends / exit starts at authored values. Empty phase cuts. Button override owns BOTH phases; Inherit restores defaults.":
                "Keys: seconds:value separated by ;. Preview never runs gameplay actions.";
        }
        void Apply(bool remove)
        {
            if(!session_)return;auto tracks=Tracks();
            if(remove){if(selected_>=tracks.size())return;tracks.erase(tracks.begin()+selected_);}
            else
            {
                if(!Number(delay_.GetValue(),draft_.delay)||!bridge::ParseAnimationKeys(keys_.GetValue(),draft_.keys))
                {status_="Invalid delay or keys. Example: 0:0;0.4:1";return;}
                if(targetMode_)draft_.loop=bridge::ScreenAnimationLoop::Once;
                if(selected_<tracks.size())tracks[selected_]=draft_;else tracks.push_back(draft_);
            }
            std::string error;if(!Store(std::move(tracks),error)){status_=error;return;}
            LoadTrack();if(changed_)changed_();
        }
        void Preset()
        {
            if(!session_)return;
            bridge::ScreenAnimationTrack t;const auto n=presetIndex_++%4;
            const bool enter=!targetMode_||Entering();
            if(n==0)t.keys=enter?std::vector<bridge::ScreenAnimationKey>{{0,0},{.4,1}}:std::vector<bridge::ScreenAnimationKey>{{0,1},{.4,0}};
            if(n==1){t.property=bridge::ScreenAnimationProperty::OffsetY;t.keys=enter?std::vector<bridge::ScreenAnimationKey>{{0,40},{.4,0}}:std::vector<bridge::ScreenAnimationKey>{{0,0},{.4,40}};}
            if(n==2)
            {
                if(!targetMode_){t.loop=bridge::ScreenAnimationLoop::PingPong;t.keys={{0,1},{.7,.35f}};}
                else{t.property=bridge::ScreenAnimationProperty::OffsetX;t.keys=enter?std::vector<bridge::ScreenAnimationKey>{{0,40},{.4,0}}:std::vector<bridge::ScreenAnimationKey>{{0,0},{.4,-40}};}
            }
            if(n==3){t.property=bridge::ScreenAnimationProperty::Scale;t.keys=enter?std::vector<bridge::ScreenAnimationKey>{{0,.8f},{.35,1}}:std::vector<bridge::ScreenAnimationKey>{{0,1},{.35,.8f}};}
            auto tracks=Tracks();const auto it=std::find_if(tracks.begin(),tracks.end(),[&](const auto& v){return v.property==t.property;});
            if(it!=tracks.end()){selected_=std::size_t(it-tracks.begin());*it=t;}
            else{selected_=tracks.size();tracks.push_back(t);}
            std::string error;if(!Store(std::move(tracks),error)){status_=error;return;}
            LoadTrack();preview_.active=true;preview_.playing=true;preview_.seconds=0;
            play_.SetText("PAUSE ANIMATION PREVIEW");if(changed_)changed_();
        }
        std::vector<wi::gui::Widget*> Controls()
        {
            std::vector<wi::gui::Widget*> controls={&target_,&track_,&property_,&easing_};
            if(!targetMode_)controls.push_back(&loop_);
            controls.insert(controls.end(),{&delay_,&keys_,&apply_,&remove_,&preset_,&play_,&stop_,&seek_,&seekButton_,&motion_});
            if(targetMode_>=3)controls.push_back(&inherit_);
            return controls;
        }
        std::vector<wi::gui::Widget*> AllControls()
        {return {&target_,&inherit_,&track_,&property_,&easing_,&loop_,&delay_,&keys_,&apply_,&remove_,&preset_,&play_,&stop_,&seek_,&seekButton_,&motion_};}
        bridge::ScreenAuthoringSession* session_=nullptr;bridge::StableId id_;
        std::function<void()> changed_;std::uint64_t revision_=0;std::size_t selected_=0,presetIndex_=0;
        int targetMode_=0;
        bridge::ScreenAnimationTrack draft_;bridge::ScreenAnimationPreview preview_;
        RenegadeButton target_,inherit_,track_,property_,easing_,loop_,apply_,remove_,preset_,play_,stop_,seekButton_,motion_;
        RenegadeTextInputField delay_,keys_,seek_;float x_=0,y_=0,width_=320,height_=420;std::string status_;
    };
}
