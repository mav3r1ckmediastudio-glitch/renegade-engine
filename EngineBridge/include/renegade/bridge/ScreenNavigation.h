#pragma once
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <functional>
#include <optional>
namespace renegade::bridge
{
    struct NavigationPoint {float x=0,y=0,z=0;};
    struct NavigationTarget
    {
        std::string id,label,category="waypoint",entityId;
        NavigationPoint position;
        int priority=0;
        bool visible=true;
    };
    enum class NavigationHeadingSource {Camera,Player,Manual};
    struct ScreenNavigation
    {
        NavigationHeadingSource headingSource=NavigationHeadingSource::Camera;
        float fieldOfView=120,manualHeading=0,markerSize=24,markerGap=32,maxDistance=0;
        bool showNames=true,showDistance=true,showTicks=true;
        std::string bearingStrip,pointerImage,categoryFilter;
        std::map<std::string,std::string> icons;
        std::vector<NavigationTarget> targets;
    };
    struct NavigationObserver {bool available=false;NavigationPoint position;float heading=0;};
    struct NavigationMarker {std::string id,label,category;float fraction=0,distance=0;int priority=0;};
    struct NavigationFrame
    {
        bool available=false;
        float heading=0;
        std::vector<NavigationMarker> markers;
    };
    inline std::vector<std::string*> NavigationResources(ScreenNavigation& c)
    {std::vector<std::string*> paths={&c.bearingStrip,&c.pointerImage};for(auto& i:c.icons)paths.push_back(&i.second);return paths;}
    bool ValidateScreenNavigation(const ScreenNavigation&,std::string& error);
    bool ValidateNavigationTarget(const NavigationTarget&,std::string& error);
    bool NavigationCategoryVisible(const std::string& filter,const std::string& category);
    float WrapNavigationHeading(float degrees) noexcept;
    float NavigationBearingDelta(float bearing,float heading) noexcept;
    NavigationFrame BuildNavigationFrame(const ScreenNavigation&,const NavigationObserver& camera,
        const NavigationObserver& player,const std::vector<NavigationTarget>&,
        const std::map<std::string,NavigationPoint>& entities,float width);
}
