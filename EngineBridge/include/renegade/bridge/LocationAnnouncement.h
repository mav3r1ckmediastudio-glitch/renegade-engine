#pragma once
#include "renegade/bridge/ScreenNavigation.h"
#include <string>
#include <optional>
namespace renegade::bridge
{
    enum class LocationTextEffect { None, Fade, Glitch, Wobble };
    struct LocationAnnouncementSettings
    {
        bool enabled=true,repeatOnReturn=false;
        float range=40,duration=4,fadeIn=.35f,fadeOut=.35f,strength=6,speed=2;
        LocationTextEffect effect=LocationTextEffect::Fade;
    };
    struct LocationProfile
    {
        std::string type="location",icon;
        bool showOnCompass=true,onlyAfterDiscovery=false;
        LocationAnnouncementSettings announcement;
    };
    struct LocationAnnouncementFrame
    {
        std::string id,text;
        LocationAnnouncementSettings settings;
        double elapsed=0;
    };
    struct LocationTextSample {bool visible=false;float opacity=0,x=0,y=0;};
    bool ValidateLocationProfile(const LocationProfile&,std::string& error);
    std::string LocationProfileKey(const LocationProfile&);
    LocationTextSample SampleLocationText(const LocationAnnouncementFrame&,bool reducedMotion=false);
}
