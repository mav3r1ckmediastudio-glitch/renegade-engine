#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace renegade::bridge {
enum class EquipmentHandUse { PrimaryOnly, OffHandOnly, EitherHand, TwoHanded, PrimaryWithSupport };
enum class EquipmentAction { Equip, Unequip, PrimaryUse, AlternateUse, Reload, Charge, Release, Block, Parry, Cast, Use, Inspect };
enum class EquipmentActionPhase { Ready, Prepare, Windup, Hold, Active, Recovery };
enum class EquipmentHand { Primary, OffHand };
struct EquipmentActionDefinition {
    EquipmentAction action = EquipmentAction::PrimaryUse;
    std::string animationAction;
    float prepareSeconds = 0, windupSeconds = 0, activeSeconds = 0, recoverySeconds = 0;
    bool holdUntilRelease = false;
    bool cancellableBeforeActive = true;
};
struct EquipmentDefinition {
    std::string assetId, name, presentationAssetId;
    EquipmentHandUse handUse = EquipmentHandUse::PrimaryOnly;
    std::vector<EquipmentActionDefinition> actions;
};
inline bool ValidateEquipmentDefinition(const EquipmentDefinition& item) {
    if(item.assetId.empty()||item.name.empty()||item.actions.empty()||item.actions.size()>32||
       unsigned(item.handUse)>unsigned(EquipmentHandUse::PrimaryWithSupport))return false;
    std::array<bool,12> seen{};
    for(const auto& action:item.actions) {
        const auto index=unsigned(action.action);
        if(index>=seen.size()||seen[index]||action.animationAction.empty())return false;
        seen[index]=true;
        for(float seconds:{action.prepareSeconds,action.windupSeconds,action.activeSeconds,action.recoverySeconds})
            if(!std::isfinite(seconds)||seconds<0||seconds>60)return false;
    }
    return true;
}
// Gameplay action ownership only. Native animation continues to evaluate skeletons.
// Each actor owns one state object; different actors never share reservations.
class EquipmentActionState {
public:
    struct Channel {
        std::string itemId;
        EquipmentActionDefinition definition;
        EquipmentActionPhase phase = EquipmentActionPhase::Ready;
        float elapsed = 0;
        uint8_t hands = 0;
        bool released = false;
    };
    struct Event {
        std::string itemId, animationAction;
        EquipmentAction action;
        EquipmentActionPhase phase;
        uint8_t hands;
    };
    bool Begin(const EquipmentDefinition& item, EquipmentAction action, EquipmentHand hand) {
        if(!ValidateEquipmentDefinition(item))return false;
        const uint8_t hands=ReservedHands(item.handUse,hand);
        if(hands==0)return false;
        const EquipmentActionDefinition* definition=nullptr;
        for(const auto& candidate:item.actions)if(candidate.action==action)definition=&candidate;
        if(!definition)return false;
        for(const auto& c:channels_)if(c.phase!=EquipmentActionPhase::Ready&&(c.hands&hands))return false;
        for(auto& c:channels_)if(c.phase==EquipmentActionPhase::Ready) {
            c={item.assetId,*definition,EquipmentActionPhase::Prepare,0,hands,false};
            events_.push_back({c.itemId,c.definition.animationAction,action,c.phase,hands});
            return true;
        }
        return false;
    }
    bool Release(const std::string& itemId) {
        bool changed=false;
        for(auto& c:channels_)if(c.phase!=EquipmentActionPhase::Ready&&c.itemId==itemId&&
            c.definition.holdUntilRelease&&!c.released){c.released=true;changed=true;}
        return changed;
    }
    bool Cancel(const std::string& itemId) {
        bool changed=false;
        for(auto& c:channels_)if(c.phase!=EquipmentActionPhase::Ready&&c.itemId==itemId&&
            c.definition.cancellableBeforeActive&&unsigned(c.phase)<unsigned(EquipmentActionPhase::Active)) {
            events_.push_back({c.itemId,c.definition.animationAction,c.definition.action,EquipmentActionPhase::Ready,c.hands});
            c={};changed=true;
        }
        return changed;
    }
    void Update(float gameplaySeconds, bool nativeOwnsActive = false) {
        // Zero is pause; negative/nonfinite time must not mutate ownership.
        if(!std::isfinite(gameplaySeconds)||gameplaySeconds<=0)return;
        for(auto& c:channels_) {
            float remaining=gameplaySeconds;
            // At most five transitions; no unbounded loop for instant actions.
            for(int step=0;step<6&&c.phase!=EquipmentActionPhase::Ready;++step) {
                if(nativeOwnsActive&&c.phase==EquipmentActionPhase::Active)break;
                if(c.phase==EquipmentActionPhase::Hold&&!c.released)break;
                const float duration=Duration(c);
                const float needed=duration-c.elapsed;
                if(remaining<needed){c.elapsed+=remaining;break;}
                remaining-=needed;c.elapsed=0;
                c.phase=Next(c);
                events_.push_back({c.itemId,c.definition.animationAction,c.definition.action,c.phase,c.hands});
                if(c.phase==EquipmentActionPhase::Ready){c={};break;}
            }
        }
    }
    // Replace an activated charge with its release definition without freeing hands.
    bool RetargetActive(const EquipmentDefinition& item, EquipmentAction action) {
        if(!ValidateEquipmentDefinition(item))return false;
        const EquipmentActionDefinition* definition=nullptr;
        for(const auto& d:item.actions)if(d.action==action)definition=&d;
        if(!definition || definition->prepareSeconds!=0 || definition->windupSeconds!=0 ||
           definition->holdUntilRelease)return false;
        for(auto& c:channels_)if(c.itemId==item.assetId&&c.phase==EquipmentActionPhase::Active) {
            c.definition=*definition;c.elapsed=0;
            events_.push_back({c.itemId,c.definition.animationAction,action,c.phase,c.hands});
            return true;
        }
        return false;
    }
    // Native presentation completion releases Active into authored recovery.
    bool CompleteActive(const std::string& itemId) {
        for(auto& c:channels_)if(c.itemId==itemId&&c.phase==EquipmentActionPhase::Active) {
            c.phase=EquipmentActionPhase::Recovery;c.elapsed=0;
            events_.push_back({c.itemId,c.definition.animationAction,c.definition.action,c.phase,c.hands});
            return true;
        }
        return false;
    }
    uint8_t ReservedHands() const {
        uint8_t result=0;for(const auto& c:channels_)if(c.phase!=EquipmentActionPhase::Ready)result|=c.hands;return result;
    }
    const std::array<Channel,2>& Channels() const {return channels_;}
    std::vector<Event> TakeEvents(){auto events=std::move(events_);events_.clear();return events;}
    void Reset(){channels_={};events_.clear();}
private:
    static uint8_t ReservedHands(EquipmentHandUse use,EquipmentHand hand) {
        if(unsigned(hand)>unsigned(EquipmentHand::OffHand))return 0;
        switch(use) {
        case EquipmentHandUse::PrimaryOnly:return hand==EquipmentHand::Primary?1:0;
        case EquipmentHandUse::OffHandOnly:return hand==EquipmentHand::OffHand?2:0;
        case EquipmentHandUse::EitherHand:return hand==EquipmentHand::Primary?1:2;
        case EquipmentHandUse::TwoHanded:
        case EquipmentHandUse::PrimaryWithSupport:return hand==EquipmentHand::Primary?3:0;
        }
        return 0;
    }
    static float Duration(const Channel& c) {
        switch(c.phase){
        case EquipmentActionPhase::Prepare:return c.definition.prepareSeconds;
        case EquipmentActionPhase::Windup:return c.definition.windupSeconds;
        case EquipmentActionPhase::Active:return c.definition.activeSeconds;
        case EquipmentActionPhase::Recovery:return c.definition.recoverySeconds;
        default:return 0;
        }
    }
    static EquipmentActionPhase Next(const Channel& c) {
        switch(c.phase){
        case EquipmentActionPhase::Prepare:return EquipmentActionPhase::Windup;
        case EquipmentActionPhase::Windup:return c.definition.holdUntilRelease?EquipmentActionPhase::Hold:EquipmentActionPhase::Active;
        case EquipmentActionPhase::Hold:return EquipmentActionPhase::Active;
        case EquipmentActionPhase::Active:return EquipmentActionPhase::Recovery;
        default:return EquipmentActionPhase::Ready;
        }
    }
    std::array<Channel,2> channels_{};
    std::vector<Event> events_;
};
}
