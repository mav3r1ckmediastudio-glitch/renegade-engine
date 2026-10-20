#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class ScreenCompositionLayer { Hud, Screen, Overlay, Modal };
    enum class ScreenUiInputPolicy { None, Controls, Modal };
    enum class ScreenCursorPolicy { Inherit, Show, Hide };
    struct ScreenCompositionPolicy
    {
        ScreenUiInputPolicy uiInput = ScreenUiInputPolicy::Modal;
        ScreenCursorPolicy cursor = ScreenCursorPolicy::Show;
        bool blockGameplayInput = true;
        bool pauseGameplay = true;
        bool takeKeyboardFocus = true;
        int order = 0;
        bool showHudUnderlay = true;
    };

    inline ScreenCompositionPolicy DefaultScreenCompositionPolicy(ScreenCompositionLayer layer)
    {
        ScreenCompositionPolicy policy;
        if(layer==ScreenCompositionLayer::Screen)policy.showHudUnderlay=false;
        if(layer==ScreenCompositionLayer::Hud)
        {
            policy.uiInput=ScreenUiInputPolicy::None;
            policy.cursor=ScreenCursorPolicy::Inherit;
            policy.blockGameplayInput=false;policy.pauseGameplay=false;policy.takeKeyboardFocus=false;
        }
        else if(layer==ScreenCompositionLayer::Overlay)
        {
            policy.uiInput=ScreenUiInputPolicy::Controls;
            policy.cursor=ScreenCursorPolicy::Inherit;
            policy.blockGameplayInput=false;policy.pauseGameplay=false;
        }
        return policy;
    }
    inline bool ValidateScreenCompositionPolicy(const ScreenCompositionPolicy& policy,std::string& error)
    {
        if((policy.uiInput!=ScreenUiInputPolicy::None && policy.uiInput!=ScreenUiInputPolicy::Controls &&
            policy.uiInput!=ScreenUiInputPolicy::Modal) ||
            (policy.cursor!=ScreenCursorPolicy::Inherit && policy.cursor!=ScreenCursorPolicy::Show &&
             policy.cursor!=ScreenCursorPolicy::Hide) || policy.order < -1024 || policy.order > 1024)
        {error="Screen composition policy contains an unknown value or invalid order.";return false;}
        if(policy.uiInput==ScreenUiInputPolicy::None && policy.takeKeyboardFocus)
        {error="A screen without UI input cannot take keyboard focus.";return false;}
        if((policy.uiInput==ScreenUiInputPolicy::Modal || policy.pauseGameplay) && !policy.blockGameplayInput)
        {error="Modal or paused screens must block gameplay input.";return false;}
        error.clear();return true;
    }

    struct ScreenCompositionInstance
    {
        std::uint64_t id=0;
        ScreenCompositionLayer layer=ScreenCompositionLayer::Hud;
        ScreenCompositionPolicy policy;
        bool hasFocusableControls=false;
        bool exiting=false;
    };
    struct ScreenCompositionState
    {
        std::vector<std::uint64_t> drawOrder;
        std::vector<std::uint64_t> uiInputOwners;
        std::uint64_t keyboardFocusOwner=0;
        bool gameplayInputBlocked=false;
        bool gameplayPauseRequested=false;
        bool hudUnderlayAllowed=true;
        ScreenCursorPolicy cursor=ScreenCursorPolicy::Inherit;
    };

    // Coordinates instance lifetime and policy only. Flow remains the sole route
    // authority; controllers retain each instance's widget focus and live values.
    class ScreenCompositionCoordinator
    {
    public:
        bool Activate(ScreenCompositionInstance instance,std::string& error)
        {
            error.clear();
            if(instance.id==0 || instance.id<=largestInstanceId_ || instances_.size()>=32 ||
                static_cast<int>(instance.layer)<0 || static_cast<int>(instance.layer)>3 ||
                !ValidateScreenCompositionPolicy(instance.policy,error))
            {if(error.empty())error="UI instance identity, layer or count is invalid.";return false;}
            largestInstanceId_=instance.id;
            instances_.push_back(std::move(instance));
            ++revision_;error.clear();return true;
        }
        bool BeginExit(std::uint64_t id,std::string& error)
        {
            auto* instance=Find(id);
            if(!instance){error="UI exit references an absent instance.";return false;}
            if(!instance->exiting){instance->exiting=true;++revision_;}
            error.clear();return true;
        }
        bool CancelExit(std::uint64_t id,std::string& error)
        {
            auto* instance=Find(id);
            if(!instance){error="UI cancellation references an absent instance.";return false;}
            if(instance->exiting){instance->exiting=false;++revision_;}
            error.clear();return true;
        }
        bool Remove(std::uint64_t id,std::string& error)
        {
            const auto found=std::find_if(instances_.begin(),instances_.end(),
                [id](const auto& instance){return instance.id==id;});
            if(found==instances_.end()){error="UI removal references an absent instance.";return false;}
            instances_.erase(found);++revision_;error.clear();return true;
        }
        bool Update(std::uint64_t id,const ScreenCompositionPolicy& policy,bool focusable,std::string& error)
        {
            if(!ValidateScreenCompositionPolicy(policy,error))return false;
            auto* instance=Find(id);
            if(!instance){error="UI policy update references an absent instance.";return false;}
            instance->policy=policy;instance->hasFocusableControls=focusable;
            ++revision_;error.clear();return true;
        }
        bool CanActivateControl(std::uint64_t id) const
        {
            const auto state=Resolve();
            return std::find(state.uiInputOwners.begin(),state.uiInputOwners.end(),id)!=state.uiInputOwners.end();
        }
        ScreenCompositionState Resolve() const
        {
            std::vector<const ScreenCompositionInstance*> sorted;
            for(const auto& instance:instances_)sorted.push_back(&instance);
            std::sort(sorted.begin(),sorted.end(),[](const auto* a,const auto* b)
            {
                if(a->layer!=b->layer)return a->layer<b->layer;
                if(a->policy.order!=b->policy.order)return a->policy.order<b->policy.order;
                return a->id<b->id;
            });
            ScreenCompositionState state;
            for(const auto* instance:sorted)
            {
                state.drawOrder.push_back(instance->id);
                if(instance->layer!=ScreenCompositionLayer::Hud)
                    state.hudUnderlayAllowed=state.hudUnderlayAllowed && instance->policy.showHudUnderlay;
                state.gameplayPauseRequested=state.gameplayPauseRequested || instance->policy.pauseGameplay;
                state.gameplayInputBlocked=state.gameplayInputBlocked || instance->policy.blockGameplayInput;
            }
            bool belowModal=false,cursorResolved=false;
            for(auto it=sorted.rbegin();it!=sorted.rend();++it)
            {
                const auto& instance=**it;
                if(!cursorResolved && instance.policy.cursor!=ScreenCursorPolicy::Inherit)
                {state.cursor=instance.policy.cursor;cursorResolved=true;}
                if(!belowModal && !instance.exiting && instance.policy.uiInput!=ScreenUiInputPolicy::None)
                {
                    state.uiInputOwners.push_back(instance.id);
                    if(state.keyboardFocusOwner==0 && instance.policy.takeKeyboardFocus &&
                        instance.hasFocusableControls)state.keyboardFocusOwner=instance.id;
                }
                // An exiting modal shields lower instances until it is actually
                // removed. Its own controls stop accepting input immediately.
                if(instance.policy.uiInput==ScreenUiInputPolicy::Modal)belowModal=true;
            }
            return state;
        }
        void Clear() noexcept {instances_.clear();++revision_;}
        std::uint64_t Revision() const noexcept{return revision_;}
        const std::vector<ScreenCompositionInstance>& Instances() const noexcept{return instances_;}
    private:
        ScreenCompositionInstance* Find(std::uint64_t id)
        {
            const auto found=std::find_if(instances_.begin(),instances_.end(),
                [id](const auto& instance){return instance.id==id;});
            return found==instances_.end()?nullptr:&*found;
        }
        std::vector<ScreenCompositionInstance> instances_;
        std::uint64_t largestInstanceId_=0,revision_=0;
    };
}
