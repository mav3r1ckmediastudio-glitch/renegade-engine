#pragma once
#include "renegade/bridge/GameplayAttributeProfile.h"
namespace renegade::bridge
{
    enum class ScreenInteractionEvent { Activate, ValueChanged };
    enum class ScreenLocalActionKind { SetVisible, SetEnabled, SetValue, Focus };
    enum class ScreenLocalComparison { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };
    struct ScreenLocalCondition
    {
        StableId widgetId;
        ScreenLocalComparison comparison=ScreenLocalComparison::Equal;
        GameplayAttributeValue value=false;
    };
    struct ScreenLocalAction
    {
        ScreenInteractionEvent event=ScreenInteractionEvent::Activate;
        ScreenLocalActionKind kind=ScreenLocalActionKind::SetVisible;
        StableId targetId;
        GameplayAttributeValue value=true;
        std::optional<ScreenLocalCondition> condition;
    };
    inline bool CompareScreenLocalValue(const GameplayAttributeValue& actual,
        const GameplayAttributeValue& expected,ScreenLocalComparison comparison)
    {
        if(actual.index()!=expected.index())return false;
        if(comparison==ScreenLocalComparison::Equal)return actual==expected;
        if(comparison==ScreenLocalComparison::NotEqual)return actual!=expected;
        const auto* a=std::get_if<double>(&actual);const auto* b=std::get_if<double>(&expected);
        if(!a||!b)return false;
        switch(comparison)
        {
        case ScreenLocalComparison::Less:return *a<*b;
        case ScreenLocalComparison::LessEqual:return *a<=*b;
        case ScreenLocalComparison::Greater:return *a>*b;
        case ScreenLocalComparison::GreaterEqual:return *a>=*b;
        default:return false;
        }
    }
}
