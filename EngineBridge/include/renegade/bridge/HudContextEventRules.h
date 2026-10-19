#pragma once
#include "renegade/bridge/GameplayEventService.h"
#include "renegade/bridge/HudSelection.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    // A Level-owned, authorable event pair. The UI selector remains the only
    // presentation authority; no event may issue Flow navigation or gameplay
    // writes. Source HUDs are referenced by stable project identity.
    struct HudContextEventRule
    {
        std::string enterEvent;
        std::string exitEvent;
        HudReference hud;
    };

    inline bool ValidateHudContextEventRules(
        const std::vector<HudContextEventRule>& rules,std::string& error)
    {
        if(rules.size()>16)
        {error="A Level may define at most 16 contextual HUD event rules.";return false;}
        std::vector<std::string> seen;
        for(const auto& rule:rules)
        {
            const auto goodName=[](const std::string& name)
            {
                if(name.empty()||name.size()>GameplayEventService::MaxNameLength)return false;
                for(const auto ch:name)
                    if(static_cast<unsigned char>(ch)<0x21 || static_cast<unsigned char>(ch)>0x7e)
                        return false;
                return true;
            };
            if(!goodName(rule.enterEvent)||!goodName(rule.exitEvent)||
               rule.enterEvent==rule.exitEvent)
            {error="Context HUD events need two distinct, bounded event names.";return false;}
            if(std::find(seen.begin(),seen.end(),rule.enterEvent)!=seen.end()||
               std::find(seen.begin(),seen.end(),rule.exitEvent)!=seen.end())
            {error="Context HUD event names must not be ambiguous across rules.";return false;}
            if(!ValidateHudReference(rule.hud,error))return false;
            seen.push_back(rule.enterEvent);
            seen.push_back(rule.exitEvent);
        }
        error.clear();return true;
    }

    class HudContextEventRouter final
    {
    public:
        bool Configure(std::vector<HudContextEventRule> rules,std::string& error)
        {
            if(!ValidateHudContextEventRules(rules,error))return false;
            rules_=std::move(rules);
            active_.assign(rules_.size(),0);
            levelSession_=0;
            error.clear();return true;
        }
        void BeginLevel(std::uint64_t session) noexcept
        {
            levelSession_=session;
            std::fill(active_.begin(),active_.end(),0);
        }
        void LeaveLevel() noexcept
        {
            levelSession_=0;std::fill(active_.begin(),active_.end(),0);
        }
        // Called on the bounded governed script/gameplay event consumer.
        // Returns true for a successful pass, including unrelated events.
        // matched communicates that this rule actually consumed the event.
        bool Handle(const GameplayEvent& event,std::uint64_t session,
            HudSelectionController& selection,bool& matched,std::string& error)
        {
            matched=false;
            if(!levelSession_||session!=levelSession_||selection.LevelSession()!=session)
            {error="HUD event belongs to no active matching Level session.";return false;}
            for(std::size_t i=0;i<rules_.size();++i)
            {
                const auto& rule=rules_[i];
                const bool enter=event.name==rule.enterEvent;
                const bool exit=event.name==rule.exitEvent;
                if(!enter&&!exit)continue;
                matched=true;
                if(enter)
                {
                    if(active_[i]){error.clear();return true;} // Duplicate event: keep lifetime
                    const auto token=lastToken_+1;
                    if(!selection.PushContext(session,token,rule.enterEvent,rule.hud,error))
                        return false;
                    lastToken_=token;active_[i]=token;error.clear();return true;
                }
                if(!active_[i]){error.clear();return true;} // Idempotent exit
                if(!selection.PopContext(session,active_[i],error))return false;
                active_[i]=0;error.clear();return true;
            }
            error.clear();return true;
        }
        [[nodiscard]] std::size_t RuleCount() const noexcept{return rules_.size();}
        [[nodiscard]] std::uint64_t LevelSession() const noexcept{return levelSession_;}
    private:
        std::vector<HudContextEventRule> rules_;
        std::vector<std::uint64_t> active_;
        std::uint64_t levelSession_=0,lastToken_=0;
    };
}
