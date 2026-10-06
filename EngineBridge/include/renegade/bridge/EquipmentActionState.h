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
    bool activeWhileHeld = false;
};
struct EquipmentProjectileBinding {
    EquipmentAction action = EquipmentAction::PrimaryUse;
    std::string projectileAssetId;
    std::string launchSocketName;
};
struct EquipmentDefinition {
    std::string assetId, name, presentationAssetId;
    EquipmentHandUse handUse = EquipmentHandUse::PrimaryOnly;
    std::vector<EquipmentActionDefinition> actions;
    std::vector<EquipmentProjectileBinding> projectiles;
};
inline bool ValidateEquipmentDefinition(const EquipmentDefinition& item) {
    if(item.assetId.empty()||item.name.empty()||item.actions.empty()||item.actions.size()>32||
       unsigned(item.handUse)>unsigned(EquipmentHandUse::PrimaryWithSupport))return false;
    std::array<bool,12> seen{};
    for(const auto& action:item.actions) {
        const auto index=unsigned(action.action);
        if(index>=seen.size()||seen[index]||action.animationAction.empty())return false;
        seen[index]=true;
        if(action.activeWhileHeld && (action.holdUntilRelease || action.activeSeconds!=0))return false;
        for(float seconds:{action.prepareSeconds,action.windupSeconds,action.activeSeconds,action.recoverySeconds})
            if(!std::isfinite(seconds)||seconds<0||seconds>60)return false;
    }
    std::array<bool,12> projectileSeen{};
    for(const auto& binding:item.projectiles) {
        const auto index=unsigned(binding.action);
        if(index>=seen.size()||!seen[index]||projectileSeen[index]||
           binding.projectileAssetId.empty()||binding.launchSocketName.size()>64||
           binding.launchSocketName.find_first_of("\r\n\t")!=std::string::npos||
           (binding.action!=EquipmentAction::PrimaryUse&&binding.action!=EquipmentAction::Release&&
            binding.action!=EquipmentAction::Cast&&binding.action!=EquipmentAction::AlternateUse&&
            binding.action!=EquipmentAction::Use))return false;
        projectileSeen[index]=true;
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
        EquipmentHand ownerHand = EquipmentHand::Primary;
    };
    struct Event {
        std::string itemId, animationAction;
        EquipmentAction action;
        EquipmentActionPhase phase;
        uint8_t hands;
        EquipmentHand ownerHand = EquipmentHand::Primary;
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
            c={item.assetId,*definition,EquipmentActionPhase::Prepare,0,hands,false,hand};
            events_.push_back({c.itemId,c.definition.animationAction,action,c.phase,hands,c.ownerHand});
            return true;
        }
        return false;
    }
    // Legacy item-wide operations remain available. Runtime uses the scoped
    // overloads: definition identity is not equipped-instance identity.
    bool Release(const std::string& itemId) { return ReleaseMatching(itemId,3); }
    bool Release(const std::string& itemId, EquipmentHand hand) {
        return ReleaseMatching(itemId,OwnerMask(hand));
    }
    bool Cancel(const std::string& itemId) { return CancelMatching(itemId,3); }
    bool Cancel(const std::string& itemId, EquipmentHand hand) {
        return CancelMatching(itemId,OwnerMask(hand));
    }
private:
    static uint8_t OwnerMask(EquipmentHand hand) {
        return unsigned(hand)<=unsigned(EquipmentHand::OffHand) ? uint8_t(1u<<unsigned(hand)) : 0;
    }
    static bool OwnedBy(const Channel& c,uint8_t owners) {
        return (OwnerMask(c.ownerHand)&owners)!=0;
    }
    bool ReleaseMatching(const std::string& itemId,uint8_t owners) {
        bool changed=false;
        for(auto& c:channels_)if(c.phase!=EquipmentActionPhase::Ready&&c.itemId==itemId&&OwnedBy(c,owners)&&
            (c.definition.holdUntilRelease||c.definition.activeWhileHeld)&&!c.released){c.released=true;changed=true;}
        return changed;
    }
    bool CancelMatching(const std::string& itemId,uint8_t owners) {
        bool changed=false;
        for(auto& c:channels_)if(c.phase!=EquipmentActionPhase::Ready&&c.itemId==itemId&&OwnedBy(c,owners)&&
            c.definition.cancellableBeforeActive&&unsigned(c.phase)<unsigned(EquipmentActionPhase::Active)) {
            events_.push_back({c.itemId,c.definition.animationAction,c.definition.action,EquipmentActionPhase::Ready,c.hands,c.ownerHand});
            c={};changed=true;
        }
        return changed;
    }
public:
    void Update(float gameplaySeconds, bool nativeOwnsActive = false, uint8_t pausedOwners = 0) {
        // Zero is pause; negative/nonfinite time must not mutate ownership.
        if(!std::isfinite(gameplaySeconds)||gameplaySeconds<=0)return;
        for(auto& c:channels_) {
            if(OwnedBy(c,pausedOwners))continue;
            float remaining=gameplaySeconds;
            // At most five transitions; no unbounded loop for instant actions.
            for(int step=0;step<6&&c.phase!=EquipmentActionPhase::Ready;++step) {
                if(c.phase==EquipmentActionPhase::Active) {
                    if(c.definition.activeWhileHeld && !c.released)break;
                    if(nativeOwnsActive && !c.definition.activeWhileHeld)break;
                }
                if(c.phase==EquipmentActionPhase::Hold&&!c.released)break;
                const float duration=Duration(c);
                const float needed=duration-c.elapsed;
                if(remaining<needed){c.elapsed+=remaining;break;}
                remaining-=needed;c.elapsed=0;
                c.phase=Next(c);
                events_.push_back({c.itemId,c.definition.animationAction,c.definition.action,c.phase,c.hands,c.ownerHand});
                if(c.phase==EquipmentActionPhase::Ready){c={};break;}
            }
        }
    }
    // Replace an activated charge with its release definition without freeing hands.
    bool RetargetActive(const EquipmentDefinition& item, EquipmentAction action) {
        return RetargetActiveMatching(item,action,3);
    }
    bool RetargetActive(const EquipmentDefinition& item, EquipmentAction action,EquipmentHand hand) {
        return RetargetActiveMatching(item,action,OwnerMask(hand));
    }
    bool CompleteActive(const std::string& itemId) { return CompleteActiveMatching(itemId,3); }
    bool CompleteActive(const std::string& itemId,EquipmentHand hand) {
        return CompleteActiveMatching(itemId,OwnerMask(hand));
    }
private:
    bool RetargetActiveMatching(const EquipmentDefinition& item, EquipmentAction action,uint8_t owners) {
        if(!ValidateEquipmentDefinition(item))return false;
        const EquipmentActionDefinition* definition=nullptr;
        for(const auto& d:item.actions)if(d.action==action)definition=&d;
        if(!definition || definition->prepareSeconds!=0 || definition->windupSeconds!=0 ||
           definition->holdUntilRelease||definition->activeWhileHeld)return false;
        for(auto& c:channels_)if(c.itemId==item.assetId&&OwnedBy(c,owners)&&c.phase==EquipmentActionPhase::Active) {
            c.definition=*definition;c.elapsed=0;
            events_.push_back({c.itemId,c.definition.animationAction,action,c.phase,c.hands,c.ownerHand});
            return true;
        }
        return false;
    }
    // Native presentation completion releases Active into authored recovery.
    bool CompleteActiveMatching(const std::string& itemId,uint8_t owners) {
        for(auto& c:channels_)if(c.itemId==itemId&&OwnedBy(c,owners)&&
            c.phase==EquipmentActionPhase::Active&&!c.definition.activeWhileHeld) {
            c.phase=EquipmentActionPhase::Recovery;c.elapsed=0;
            events_.push_back({c.itemId,c.definition.animationAction,c.definition.action,c.phase,c.hands,c.ownerHand});
            return true;
        }
        return false;
    }
public:
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
