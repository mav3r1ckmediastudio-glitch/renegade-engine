#pragma once
#include "renegade/bridge/ScreenComposition.h"
#include "renegade/bridge/ScreenService.h"
#include <optional>

namespace renegade::runtime
{
    inline bridge::ScreenCompositionLayer RuntimeUiLayer(bridge::ScreenUsage usage) noexcept
    {return bridge::ScreenLayerForUsage(usage);}

    // Coordinates independent destination and HUD lifetimes. Flow remains
    // route authority and user-requested pause remains owned by Runtime.
    class RuntimeUiComposition
    {
    public:
        bool ReplacePrimary(const bridge::ScreenDocument& document,bool focusable,std::string& error)
        {
            auto candidate=coordinator_;
            if(primary_!=0 && !candidate.Remove(primary_,error))return false;
            const auto id=nextId_;
            if(document.enabledInGame && !candidate.Activate(
                {id,RuntimeUiLayer(document.usage),bridge::EffectiveScreenCompositionPolicy(document),focusable},error))return false;
            coordinator_=std::move(candidate);primary_=document.enabledInGame?id:0;
            if(document.enabledInGame)++nextId_;
            error.clear();return true;
        }
        bool ReplaceHud(const bridge::ScreenDocument& document,bool focusable,std::string& error)
        {
            if(document.usage!=bridge::ScreenUsage::Hud || !document.enabledInGame)
            {error="HUD composition requires an enabled HUD Screen.";return false;}
            const auto policy=bridge::EffectiveScreenCompositionPolicy(document);
            if(!bridge::ValidateScreenCompositionPolicy(policy,error))return false;
            auto candidate=coordinator_;
            if(hudIdentity_ && !candidate.Remove(hudIdentity_,error))return false;
            const auto identity=hudAllowed_?nextId_:0;
            if(identity && !candidate.Activate({identity,bridge::ScreenCompositionLayer::Hud,policy,focusable},error))return false;
            coordinator_=std::move(candidate);hud_=HudRecord{policy,focusable};hudIdentity_=identity;
            if(identity)++nextId_;
            error.clear();return true;
        }
        bool SetHudUnderlayAllowed(bool allowed,std::string& error)
        {
            if(allowed==hudAllowed_){error.clear();return true;}
            auto candidate=coordinator_;
            auto identity=hudIdentity_;
            if(!allowed && identity)
            {if(!candidate.Remove(identity,error))return false;identity=0;}
            else if(allowed && hud_ && !identity)
            {
                identity=nextId_;
                if(!candidate.Activate({identity,bridge::ScreenCompositionLayer::Hud,hud_->policy,hud_->focusable},error))return false;
            }
            coordinator_=std::move(candidate);hudAllowed_=allowed;
            if(identity && !hudIdentity_)++nextId_;
            hudIdentity_=identity;error.clear();return true;
        }
        bool RefreshFocusAvailability(bool primary,bool hud,std::string& error)
        {
            if(hud_)hud_->focusable=hud;
            for(const auto& instance:coordinator_.Instances())
            {
                const bool available=instance.id==primary_?primary:hud;
                if(instance.hasFocusableControls!=available &&
                    !coordinator_.Update(instance.id,instance.policy,available,error))return false;
            }
            error.clear();return true;
        }
        bool BeginHudExit(std::string& error){return coordinator_.BeginExit(hudIdentity_,error);}
        bool CancelHudExit(std::string& error){return coordinator_.CancelExit(hudIdentity_,error);}
        bool BeginPrimaryExit(std::string& error){return coordinator_.BeginExit(primary_,error);}
        bool CancelPrimaryExit(std::string& error){return coordinator_.CancelExit(primary_,error);}
        void ClearPrimary() noexcept
        {if(primary_){std::string ignored;(void)coordinator_.Remove(primary_,ignored);primary_=0;}}
        void ClearHud() noexcept
        {
            if(hudIdentity_){std::string ignored;(void)coordinator_.Remove(hudIdentity_,ignored);hudIdentity_=0;}
            hud_.reset();
        }
        bridge::ScreenCompositionState Resolve() const{return coordinator_.Resolve();}
        bool PrimaryAcceptsInput() const{return primary_ && coordinator_.CanActivateControl(primary_);}
        bool HudAcceptsInput() const{return hudIdentity_ && coordinator_.CanActivateControl(hudIdentity_);}
        bool IsHudVisible() const noexcept{return hudIdentity_!=0;}
        std::uint64_t PrimaryIdentity() const noexcept{return primary_;}
        std::uint64_t HudIdentity() const noexcept{return hudIdentity_;}
    private:
        struct HudRecord{bridge::ScreenCompositionPolicy policy;bool focusable;};
        bridge::ScreenCompositionCoordinator coordinator_;
        std::optional<HudRecord> hud_;
        std::uint64_t primary_=0,hudIdentity_=0,nextId_=1;
        bool hudAllowed_=true;
    };
}
