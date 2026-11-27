#include "renegade/bridge/ScreenNavigation.h"
#include <algorithm>
#include <set>
#include <sstream>
namespace renegade::bridge
{
namespace
{
    bool text(const std::string& s,size_t max,bool empty=true)
    {return (empty||!s.empty())&&s.size()<=max&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;});}
    bool point(NavigationPoint p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<=1e7&&std::abs(p.y)<=1e7&&std::abs(p.z)<=1e7;}
}
    float WrapNavigationHeading(float a) noexcept {if(!std::isfinite(a))return 0;return std::fmod(std::fmod(a,360.f)+360.f,360.f);}
    float NavigationBearingDelta(float a,float h) noexcept {return WrapNavigationHeading(a-h+180)-180;}
    bool ValidateNavigationTarget(const NavigationTarget& t,std::string& error)
    {
        if(!text(t.id,120,false)||!text(t.label,160)||!text(t.category,48,false)||!text(t.entityId,120)||!point(t.position)||t.priority< -1000||t.priority>1000)
        {error="Invalid navigation target identity, label, category, position or priority.";return false;}
        error.clear();return true;
    }
    bool ValidateScreenNavigation(const ScreenNavigation& c,std::string& error)
    {
        const auto bounded=[](float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
        if(int(c.headingSource)<0||int(c.headingSource)>2||!bounded(c.fieldOfView,30,360)||
            !bounded(c.manualHeading,-360000,360000)||!bounded(c.markerSize,4,128)||!bounded(c.markerGap,0,256)||
            !bounded(c.maxDistance,0,1e7)||!text(c.categoryFilter,512)||c.icons.size()>64||c.targets.size()>64)
        {error="Navigation settings exceed supported ranges (span30-360; icon4-128; gap0-256; up to64 target/icon entries).";return false;}
        for(const auto& [key,value]:c.icons)if(!text(key,48,false)||!text(value,512,false)){error="Invalid navigation icon category or path.";return false;}
        std::set<std::string> ids;
        for(const auto& t:c.targets)if(!ValidateNavigationTarget(t,error)||!ids.insert(t.id).second){if(error.empty())error="Navigation target IDs must be unique.";return false;}
        error.clear();return true;
    }
    bool NavigationCategoryVisible(const std::string& filter,const std::string& category)
    {
        if(filter.empty())return true;std::istringstream stream(filter);std::string token;
        while(std::getline(stream,token,',')){const auto first=token.find_first_not_of(" ");if(first==std::string::npos)continue;
            const auto last=token.find_last_not_of(" ");if(token.substr(first,last-first+1)==category)return true;}
        return false;
    }
    NavigationFrame BuildNavigationFrame(const ScreenNavigation& c,const NavigationObserver& camera,
        const NavigationObserver& player,const std::vector<NavigationTarget>& targets,
        const std::map<std::string,NavigationPoint>& entities,float width)
    {
        NavigationFrame frame;auto observer=c.headingSource==NavigationHeadingSource::Player?player:camera;
        if(c.headingSource==NavigationHeadingSource::Manual){observer.available=true;observer.heading=c.manualHeading;}
        if(!observer.available||!point(observer.position)||!std::isfinite(observer.heading)||!std::isfinite(width)||width<=0)return frame;
        frame.available=true;frame.heading=WrapNavigationHeading(observer.heading);
        const auto add=[&](const NavigationTarget& target){
            if(!target.visible||!NavigationCategoryVisible(c.categoryFilter,target.category))return;
            auto p=target.position;if(!target.entityId.empty()){const auto found=entities.find(target.entityId);if(found==entities.end())return;p=found->second;}
            const double dx=double(p.x)-observer.position.x,dz=double(p.z)-observer.position.z;
            const float distance=float(std::hypot(dx,dz));if(distance<.0001f||(c.maxDistance>0&&distance>c.maxDistance))return;
            const float delta=NavigationBearingDelta(float(std::atan2(dx,dz)*180/3.141592653589793),frame.heading);
            if(std::abs(delta)>c.fieldOfView*.5f)return;
            frame.markers.push_back({target.id,target.label,target.category,.5f+delta/c.fieldOfView,distance,target.priority});
        };
        for(const auto& t:c.targets)add(t);for(const auto& t:targets)add(t);
        std::stable_sort(frame.markers.begin(),frame.markers.end(),[](const auto& a,const auto& b){
            if(a.priority!=b.priority)return a.priority>b.priority;if(a.distance!=b.distance)return a.distance<b.distance;return a.id<b.id;});
        std::vector<NavigationMarker> kept;
        for(const auto& m:frame.markers)if(std::all_of(kept.begin(),kept.end(),[&](const auto& other){return std::abs(m.fraction-other.fraction)*width>=c.markerGap;}))kept.push_back(m);
        std::sort(kept.begin(),kept.end(),[](const auto& a,const auto& b){return a.fraction<b.fraction;});frame.markers=std::move(kept);return frame;
    }
}
