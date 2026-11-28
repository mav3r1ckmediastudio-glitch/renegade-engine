#pragma once
#include "ScreenNavigationSerialization.h"
#include "renegade/bridge/ScreenHurtFeedback.h"
namespace renegade::bridge::detail
{
    template<class S> void WriteHurt(S& s,const ScreenHurtFeedback& c)
    {
        s.Set("hurt_version",1);
        for(const auto& f:HurtNumbers())s.Set((std::string("hurt_")+f.key).c_str(),c.*(f.member));
        for(const auto& f:HurtFlags())s.Set((std::string("hurt_")+f.key).c_str(),c.*(f.member));
        for(const auto& f:HurtResources())s.Set((std::string("hurt_")+f.key).c_str(),c.*(f.member));
        s.Set("hurt_colour",std::to_string(c.colour));s.Set("hurt_low_colour",std::to_string(c.lowColour));
    }
    template<class S> bool ReadHurt(const S& s,ScreenHurtFeedback& c)
    {
        int version=0;if(!NavigationScalar(s,"hurt_version",version)||version!=1)return false;
        for(const auto& f:HurtNumbers())if(!NavigationScalar(s,(std::string("hurt_")+f.key).c_str(),c.*(f.member)))return false;
        for(const auto& f:HurtFlags())if(!NavigationBool(s,(std::string("hurt_")+f.key).c_str(),c.*(f.member)))return false;
        for(const auto& f:HurtResources())c.*(f.member)=s.GetText((std::string("hurt_")+f.key).c_str());
        if(!NavigationScalar(s,"hurt_colour",c.colour)||!NavigationScalar(s,"hurt_low_colour",c.lowColour))return false;
        std::string error;return ValidateHurtFeedback(c,error);
    }
}
