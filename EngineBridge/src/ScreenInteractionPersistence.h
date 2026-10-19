#pragma once
#include "renegade/bridge/ScreenInteractionPlan.h"
#include "ScreenBindingPersistence.h"
#include "ScreenAnimationPersistence.h"
namespace renegade::bridge::detail
{
    inline void WriteLocalValue(wi::config::Section& section,const char* typeKey,const char* valueKey,const GameplayAttributeValue& value)
    {
        section.Set(typeKey,static_cast<int>(value.index()));
        if(const auto* n=std::get_if<double>(&value))section.Set(valueKey,ScreenControlNumber(*n));
        else if(const auto* b=std::get_if<bool>(&value))section.Set(valueKey,*b?"true":"false");
        else section.Set(valueKey,EncodeBindingText(std::get<std::string>(value)));
    }
    inline bool ReadLocalValue(const wi::config::Section& section,const char* typeKey,const char* valueKey,
        GameplayAttributeValue& value)
    {
        int type=-1;if(!ReadAnimationScalar(section,typeKey,type)||!section.Has(valueKey))return false;
        if(type==0){double number=0;if(!ReadAnimationScalar(section,valueKey,number)||!std::isfinite(number))return false;value=number;}
        else if(type==1){const auto text=section.GetText(valueKey);if(text!="true"&&text!="false")return false;value=text=="true";}
        else if(type==2){std::string text;if(!DecodeBindingText(section.GetText(valueKey),4096,text))return false;value=std::move(text);}
        else return false;
        return true;
    }
    inline void WriteWidgetInteractions(wi::config::File& file,const std::string& widgetSection,const std::vector<ScreenLocalAction>& actions)
    {
        file.GetSection(widgetSection.c_str()).Set("interaction_count",static_cast<int>(actions.size()));
        for(std::size_t i=0;i<actions.size();++i)
        {
            const auto& action=actions[i];auto& section=file.GetSection((widgetSection+"_interaction_"+std::to_string(i)).c_str());
            section.Set("event",static_cast<int>(action.event));section.Set("kind",static_cast<int>(action.kind));
            section.Set("target",action.targetId);WriteLocalValue(section,"value_type","value",action.value);
            section.Set("conditional",action.condition?"true":"false");
            if(action.condition)
            {
                section.Set("condition_source",action.condition->widgetId);
                section.Set("comparison",static_cast<int>(action.condition->comparison));
                WriteLocalValue(section,"condition_type","condition_value",action.condition->value);
            }
        }
    }
    inline bool ReadWidgetInteractions(wi::config::File& file,const std::string& widgetSection,
        unsigned schema,std::vector<ScreenLocalAction>& actions,std::string& error)
    {
        const auto& widget=file.GetSection(widgetSection.c_str());if(!widget.Has("interaction_count"))return true;
        int count=-1;if(schema<3||!ReadAnimationScalar(widget,"interaction_count",count)||count<0||count>32)
        {error="Malformed UI interaction count.";return false;}
        for(int i=0;i<count;++i)
        {
            const auto name=widgetSection+"_interaction_"+std::to_string(i);
            if(!file.HasSection(name.c_str())){error="Missing UI interaction section.";return false;}
            const auto& section=file.GetSection(name.c_str());ScreenLocalAction action;int event=-1,kind=-1;
            if(!ReadAnimationScalar(section,"event",event)||event<0||event>1||
                !ReadAnimationScalar(section,"kind",kind)||kind<0||kind>3||!section.Has("target")||
                !ReadLocalValue(section,"value_type","value",action.value)||!section.Has("conditional"))
            {error="Malformed UI interaction data.";return false;}
            action.event=static_cast<ScreenInteractionEvent>(event);action.kind=static_cast<ScreenLocalActionKind>(kind);action.targetId=section.GetText("target");
            const auto conditional=section.GetText("conditional");
            if(conditional!="true"&&conditional!="false"){error="Malformed UI interaction condition flag.";return false;}
            if(conditional=="true")
            {
                ScreenLocalCondition condition;int comparison=-1;
                if(!section.Has("condition_source")||!ReadAnimationScalar(section,"comparison",comparison)||
                    comparison<0||comparison>5||!ReadLocalValue(section,"condition_type","condition_value",condition.value))
                {error="Malformed UI interaction condition.";return false;}
                condition.widgetId=section.GetText("condition_source");condition.comparison=static_cast<ScreenLocalComparison>(comparison);action.condition=std::move(condition);
            }
            actions.push_back(std::move(action));
        }
        return true;
    }
}
