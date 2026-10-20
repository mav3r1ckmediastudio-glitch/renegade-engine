#pragma once
#include "renegade/bridge/ScreenTransition.h"
#include "ScreenAnimationPersistence.h"
namespace renegade::bridge::detail
{
    inline void WriteScreenTransitionPair(wi::config::File& file,const std::string& prefix,
        const ScreenTransitionPair& pair)
    {
        if(pair.enter.empty()&&pair.exit.empty())return;
        WriteWidgetAnimations(file,prefix+"_enter",pair.enter);
        WriteWidgetAnimations(file,prefix+"_exit",pair.exit);
    }
    inline bool ReadScreenTransitionPair(wi::config::File& file,const std::string& prefix,
        unsigned schema,ScreenTransitionPair& pair,std::string& error)
    {
        const auto enter=prefix+"_enter",exit=prefix+"_exit";
        const bool hasEnter=file.HasSection(enter.c_str()),hasExit=file.HasSection(exit.c_str());
        if(!hasEnter&&!hasExit)return true;
        if(schema<3||hasEnter!=hasExit||
            !file.GetSection(enter.c_str()).Has("animation_count")||
            !file.GetSection(exit.c_str()).Has("animation_count"))
        {error="Incomplete Screen transition pair or unsupported schema.";return false;}
        ScreenTransitionPair candidate;
        if(!ReadWidgetAnimations(file,enter,schema,candidate.enter,error)||
            !ReadWidgetAnimations(file,exit,schema,candidate.exit,error)||
            !ValidateScreenTransition(candidate.enter,true,error)||
            !ValidateScreenTransition(candidate.exit,false,error))return false;
        pair=std::move(candidate);return true;
    }
}
