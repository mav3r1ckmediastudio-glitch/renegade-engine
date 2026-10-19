#pragma once
#include "renegade/bridge/ScreenService.h"
#include <algorithm>
namespace renegade::bridge
{
    inline const ScreenWidget* FindScreenInteractionWidget(const ScreenDocument& doc,const StableId& id)
    {
        const auto found=std::find_if(doc.widgets.begin(),doc.widgets.end(),[&](const auto& w){return w.id==id;});
        return found==doc.widgets.end()?nullptr:&*found;
    }
    inline bool ValidateScreenInteractions(const ScreenDocument& doc,const ScreenWidget& source,std::string& error)
    {
        if(source.interactions.size()>32||(!source.interactions.empty()&&
            (doc.schemaVersion<3||!IsScreenInteractive(source.kind))))
        {error="UI interactions require an interactive v3 element and at most 32 steps.";return false;}
        for(const auto& action:source.interactions)
        {
            if(int(action.event)<0||int(action.event)>1||int(action.kind)<0||int(action.kind)>3||
                (action.event==ScreenInteractionEvent::ValueChanged&&!IsScreenValueControl(source.kind))||
                (action.event==ScreenInteractionEvent::Activate&&source.kind!=ScreenWidgetKind::Button))
            {error="UI interaction event or local action is incompatible.";return false;}
            if(const auto* number=std::get_if<double>(&action.value);number&&!std::isfinite(*number))
            {error="UI action number must be finite.";return false;}
            if(const auto* text=std::get_if<std::string>(&action.value);text&&text->size()>4096)
            {error="UI action text exceeds its bound.";return false;}
            const auto* target=FindScreenInteractionWidget(doc,action.targetId);
            if(!target){error="UI interaction target is missing: "+action.targetId;return false;}
            if(action.kind==ScreenLocalActionKind::SetVisible||action.kind==ScreenLocalActionKind::SetEnabled)
            {
                if(!std::holds_alternative<bool>(action.value))
                {error="Visibility and enabled actions require a Boolean value.";return false;}
                const auto property=action.kind==ScreenLocalActionKind::SetVisible?ScreenBindingProperty::Visible:ScreenBindingProperty::Enabled;
                if(std::any_of(target->bindings.begin(),target->bindings.end(),[&](const auto& b){return b.property==property;}))
                {error="A local UI action cannot overwrite a gameplay-bound state.";return false;}
            }
            if(action.kind==ScreenLocalActionKind::SetValue)
            {
                if(!IsScreenValueControl(target->kind)){error="Set Value target is not a UI value control.";return false;}
                auto check=target->control;check.value=action.value;
                if(!ValidateScreenControl(ScreenControlKindFor(target->kind),check,error))return false;
            }
            if(action.kind==ScreenLocalActionKind::Focus&&
                (!IsScreenInteractive(target->kind)||std::find(doc.focusOrder.begin(),doc.focusOrder.end(),target->id)==doc.focusOrder.end()))
            {error="Focus target must be an element in the Screen focus order.";return false;}
            if(action.condition)
            {
                const auto& condition=*action.condition;const auto* input=FindScreenInteractionWidget(doc,condition.widgetId);
                if(!input||!IsScreenValueControl(input->kind)||input->control.value.index()!=condition.value.index()||
                    int(condition.comparison)<0||int(condition.comparison)>5||
                    (condition.value.index()!=0&&int(condition.comparison)>1))
                {error="UI condition source, type or comparison is incompatible.";return false;}
                if(const auto* n=std::get_if<double>(&condition.value);n&&!std::isfinite(*n))
                {error="UI condition requires a finite number.";return false;}
                if(const auto* text=std::get_if<std::string>(&condition.value);text&&text->size()>4096)
                {error="UI condition text exceeds its bound.";return false;}
            }
        }
        error.clear();return true;
    }
    struct ScreenInteractionPlan
    {
        ScreenDocument document;
        StableId focusWidgetId;
        unsigned executed=0;
    };
    // Pure bounded, ordered transaction. Conditions observe preceding steps.
    // Local actions never trigger another list or issue gameplay/Flow commands.
    inline bool BuildScreenInteractionPlan(const ScreenDocument& live,const StableId& sourceId,
        ScreenInteractionEvent event,ScreenInteractionPlan& result,std::string& error)
    {
        const auto* source=FindScreenInteractionWidget(live,sourceId);
        if(!source||!source->visible||!source->enabled||!ValidateScreenInteractions(live,*source,error))
        {if(error.empty())error="UI interaction source is unavailable.";return false;}
        ScreenInteractionPlan candidate;candidate.document=live;
        for(const auto& action:source->interactions)
        {
            if(action.event!=event)continue;
            if(action.condition)
            {
                const auto* input=FindScreenInteractionWidget(candidate.document,action.condition->widgetId);
                if(!CompareScreenLocalValue(input->control.value,action.condition->value,action.condition->comparison))continue;
            }
            auto target=std::find_if(candidate.document.widgets.begin(),candidate.document.widgets.end(),
                [&](const auto& w){return w.id==action.targetId;});
            switch(action.kind)
            {
            case ScreenLocalActionKind::SetVisible:target->visible=std::get<bool>(action.value);break;
            case ScreenLocalActionKind::SetEnabled:target->enabled=std::get<bool>(action.value);break;
            case ScreenLocalActionKind::SetValue:target->control.value=action.value;break;
            case ScreenLocalActionKind::Focus:candidate.focusWidgetId=target->id;break;
            }
            ++candidate.executed;
        }
        if(!candidate.focusWidgetId.empty())
        {
            const auto* target=FindScreenInteractionWidget(candidate.document,candidate.focusWidgetId);
            for(std::size_t depth=0;target&&depth<=live.widgets.size();++depth)
            {
                if(!target->visible||(target->id==candidate.focusWidgetId&&!target->enabled)){error="Ordered UI focus target is unavailable after the action list.";return false;}
                target=target->parentId.empty()?nullptr:FindScreenInteractionWidget(candidate.document,target->parentId);
            }
        }
        result=std::move(candidate);error.clear();return true;
    }
}
