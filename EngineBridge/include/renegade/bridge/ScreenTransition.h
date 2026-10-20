#pragma once
#include "renegade/bridge/ScreenAnimation.h"

namespace renegade::bridge
{
    struct ScreenTransitionPair
    {
        std::vector<ScreenAnimationTrack> enter;
        std::vector<ScreenAnimationTrack> exit;
    };
    struct ScreenTransitionFrame
    {
        float opacity=1,offsetX=0,offsetY=0,scale=1;
    };
    inline bool ValidateScreenTransition(const std::vector<ScreenAnimationTrack>& tracks,
        bool entering,std::string& error)
    {
        if(!ValidateScreenAnimations(tracks,false,error))return false;
        for(const auto& t:tracks)
        {
            if(t.loop!=ScreenAnimationLoop::Once||t.delay+t.keys.back().time>30)
            {error="Screen transitions must finish once within 30 seconds.";return false;}
            // The steady authored presentation is always the identity transform.
            const auto endpoint=entering?t.keys.back().value:t.keys.front().value;
            const auto identity=(t.property==ScreenAnimationProperty::Opacity||
                t.property==ScreenAnimationProperty::Scale)?1.0f:0.0f;
            if(endpoint!=identity||(t.property==ScreenAnimationProperty::Scale&&t.keys.front().value<=0))
            {error="Transitions must join the authored presentation without a geometry or opacity discontinuity.";return false;}
        }
        return true;
    }
    inline double ScreenTransitionDuration(const std::vector<ScreenAnimationTrack>& tracks) noexcept
    {
        double duration=0;
        for(const auto& t:tracks)if(!t.keys.empty())duration=std::max(duration,t.delay+t.keys.back().time);
        return duration;
    }
    inline ScreenTransitionFrame EvaluateScreenTransition(
        const std::vector<ScreenAnimationTrack>& tracks,double seconds)
    {
        ScreenTransitionFrame result;
        for(const auto& t:tracks)
        {
            const auto value=EvaluateScreenAnimation(t,seconds);
            switch(t.property)
            {
            case ScreenAnimationProperty::Opacity:result.opacity=value;break;
            case ScreenAnimationProperty::OffsetX:result.offsetX=value;break;
            case ScreenAnimationProperty::OffsetY:result.offsetY=value;break;
            case ScreenAnimationProperty::Scale:result.scale=value;break;
            default:break;
            }
        }
        return result;
    }
    enum class ScreenTransitionPhase { Active, Entering, Exiting, Exited };
    // One instance-relative UI clock. No world clock, Flow route or action dispatch.
    class ScreenTransitionPlayer
    {
    public:
        bool Start(ScreenTransitionPair policy,bool reducedMotion,std::string& error)
        {
            if(!ValidateScreenTransition(policy.enter,true,error)||
                !ValidateScreenTransition(policy.exit,false,error))return false;
            policy_=std::move(policy);time_=0;exitFrom_={};
            phase_=reducedMotion||policy_.enter.empty()?ScreenTransitionPhase::Active:ScreenTransitionPhase::Entering;
            error.clear();return true;
        }
        bool BeginExit(const std::vector<ScreenAnimationTrack>* overrideTracks,
            bool reducedMotion,std::string& error)
        {
            if(phase_==ScreenTransitionPhase::Exiting||phase_==ScreenTransitionPhase::Exited)
            {error="This Screen instance is already exiting.";return false;}
            if(overrideTracks&&!ValidateScreenTransition(*overrideTracks,false,error))return false;
            exitFrom_=Frame();
            if(overrideTracks)policy_.exit=*overrideTracks;
            time_=0;
            phase_=reducedMotion||policy_.exit.empty()?ScreenTransitionPhase::Exited:ScreenTransitionPhase::Exiting;
            error.clear();return true;
        }
        void Tick(double delta,bool reducedMotion) noexcept
        {
            if(phase_!=ScreenTransitionPhase::Entering&&phase_!=ScreenTransitionPhase::Exiting)return;
            if(!std::isfinite(delta)||delta<0)return;
            time_+=delta;
            const bool entering=phase_==ScreenTransitionPhase::Entering;
            const auto& tracks=entering?policy_.enter:policy_.exit;
            if(reducedMotion||time_>=ScreenTransitionDuration(tracks))
                phase_=entering?ScreenTransitionPhase::Active:ScreenTransitionPhase::Exited;
        }
        void CancelExit() noexcept {phase_=ScreenTransitionPhase::Active;time_=0;exitFrom_={};}
        void Reset() noexcept {policy_={};CancelExit();}
        ScreenTransitionPhase Phase() const noexcept{return phase_;}
        bool AcceptsInput() const noexcept{return phase_==ScreenTransitionPhase::Active;}
        bool ExitComplete() const noexcept{return phase_==ScreenTransitionPhase::Exited;}
        ScreenTransitionFrame Frame() const
        {
            if(phase_==ScreenTransitionPhase::Active)return {};
            if(phase_==ScreenTransitionPhase::Entering)return EvaluateScreenTransition(policy_.enter,time_);
            auto frame=EvaluateScreenTransition(policy_.exit,
                phase_==ScreenTransitionPhase::Exited?ScreenTransitionDuration(policy_.exit):time_);
            frame.opacity*=exitFrom_.opacity;frame.scale*=exitFrom_.scale;
            frame.offsetX+=exitFrom_.offsetX;frame.offsetY+=exitFrom_.offsetY;
            return frame;
        }
    private:
        ScreenTransitionPair policy_;
        ScreenTransitionPhase phase_=ScreenTransitionPhase::Active;
        double time_=0;ScreenTransitionFrame exitFrom_;
    };
}
