#include <filesystem>
#include "renegade/bridge/ScreenService.h"
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
    bool IsNavigationIconPath(const std::string& value)
    {
        if(value.empty()||value.size()>512||std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127||c==':';}))return false;
        const auto path=std::filesystem::u8path(value);
        if(path.is_absolute()||path.has_root_name()||path.begin()==path.end()||*path.begin()!="Content")return false;
        for(const auto& part:path)if(part=="."||part==".."||part.empty())return false;
        return true;
    }
    float WrapNavigationHeading(float a) noexcept {if(!std::isfinite(a))return 0;return std::fmod(std::fmod(a,360.f)+360.f,360.f);}
    float NavigationBearingDelta(float a,float h) noexcept {return WrapNavigationHeading(a-h+180)-180;}
    bool ValidateNavigationTarget(const NavigationTarget& t,std::string& error)
    {
        if((!t.icon.empty()&&!IsNavigationIconPath(t.icon))||!text(t.id,120,false)||!text(t.label,160)||!text(t.category,48,false)||!text(t.entityId,120)||!point(t.position)||t.priority< -1000||t.priority>1000)
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
        if(c.minimap){const auto& m=*c.minimap;
            if(!bounded(m.range,1,1000000)||!bounded(m.cameraHeight,5,100000)||
                !bounded(m.updateHz,1,60)||!bounded(m.playerSize,4,128)||m.resolution<64||m.resolution>(m.fullMap&&!m.live?4096u:512u)||
                !bounded(m.west,-1e7,1e7)||!bounded(m.east,-1e7,1e7)||!bounded(m.south,-1e7,1e7)||!bounded(m.north,-1e7,1e7)||
                m.east-m.west<1||m.north-m.south<1||
                (!m.worldMapImage.empty()&&!IsNavigationIconPath(m.worldMapImage))||(!m.image.empty()&&!IsNavigationIconPath(m.image))||(!m.playerIcon.empty()&&!IsNavigationIconPath(m.playerIcon)))
            {error="Minimap needs valid world bounds, range 1-1000000, height 5-100000, 64-512 pixels and 1-60 updates per second.";return false;}
        }
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
            if(target.minimapOnly||!target.visible||!NavigationCategoryVisible(c.categoryFilter,target.category))return;
            auto p=target.position;if(!target.entityId.empty()){const auto found=entities.find(target.entityId);if(found==entities.end())return;p=found->second;}
            const double dx=double(p.x)-observer.position.x,dz=double(p.z)-observer.position.z;
            const float distance=float(std::hypot(dx,dz));if(distance<.0001f||(c.maxDistance>0&&distance>c.maxDistance))return;
            const float delta=NavigationBearingDelta(float(std::atan2(dx,dz)*180/3.141592653589793),frame.heading);
            if(std::abs(delta)>c.fieldOfView*.5f)return;
            frame.markers.push_back({target.id,target.label,target.category,.5f+delta/c.fieldOfView,distance,target.priority,target.icon});
        };
        for(const auto& t:c.targets)add(t);for(const auto& t:targets)add(t);
        std::stable_sort(frame.markers.begin(),frame.markers.end(),[](const auto& a,const auto& b){
            if(a.priority!=b.priority)return a.priority>b.priority;if(a.distance!=b.distance)return a.distance<b.distance;return a.id<b.id;});
        std::vector<NavigationMarker> kept;
        for(const auto& m:frame.markers)if(std::all_of(kept.begin(),kept.end(),[&](const auto& other){return std::abs(m.fraction-other.fraction)*width>=c.markerGap;}))kept.push_back(m);
        std::sort(kept.begin(),kept.end(),[](const auto& a,const auto& b){return a.fraction<b.fraction;});frame.markers=std::move(kept);return frame;
    }
}

namespace renegade::bridge
{
    MinimapFrame BuildMinimapFrame(const ScreenNavigation& c,const NavigationObserver& camera,
        const NavigationObserver& player,const std::vector<NavigationTarget>& targets,
        const std::map<std::string,NavigationPoint>& entities,float width,float height)
    {
        MinimapFrame f;if(!c.minimap)return f;std::string error;
        if(!ValidateScreenNavigation(c,error)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return f;
        auto observer=c.headingSource==NavigationHeadingSource::Camera?camera:player;
        if(!observer.available||!point(observer.position)||!std::isfinite(observer.heading))return f;
        if(c.headingSource==NavigationHeadingSource::Manual)observer.heading=c.manualHeading;
        const auto& m=*c.minimap;f.available=true;f.observer=observer;
        f.heading=m.rotate?WrapNavigationHeading(observer.heading):0;
        f.playerHeading=WrapNavigationHeading((player.available?player.heading:observer.heading)-f.heading);
        const double a=f.heading*3.141592653589793/180,co=std::cos(a),si=std::sin(a);
        const double aspect=m.round?1:double(width)/height;
        const float pad=std::min(.2f,c.markerSize/std::max(1.f,std::min(width,height))*.75f);
        const float limit=.5f-pad;std::set<std::string> seen;
        const auto add=[&](const NavigationTarget& t){
            if(!t.visible||!NavigationCategoryVisible(c.categoryFilter,t.category)||!seen.insert(t.id).second)return;
            if((t.category=="enemy"&&!m.showEnemies)||(t.category=="questgiver"&&!m.showQuestGivers)||(t.category=="vendor"&&!m.showVendors))return;
            auto p=t.position;if(!t.entityId.empty()){const auto i=entities.find(t.entityId);if(i==entities.end())return;p=i->second;}
            if(!point(p))return;
            const double dx=double(p.x)-observer.position.x,dz=double(p.z)-observer.position.z;
            const auto origin=m.fullMap&&player.available?player.position:observer.position;
            const float distance=float(std::hypot(double(p.x)-origin.x,double(p.z)-origin.z));if(c.maxDistance>0&&distance>c.maxDistance)return;
            if(!m.fullMap&&(t.category=="enemy"||t.category=="questgiver"||t.category=="vendor")&&distance>m.range)return;
            float x=float((co*dx-si*dz)/(2*m.range*aspect)),y=float(-(si*dx+co*dz)/(2*m.range));
            const float extent=m.round?std::hypot(x,y):std::max(std::abs(x),std::abs(y));
            bool clamped=extent>limit;if(clamped){if(!m.clampEdges)return;x*=limit/extent;y*=limit/extent;}
            f.markers.push_back({t,.5f+x,.5f+y,distance,clamped});
        };
        for(const auto& t:c.targets)add(t);for(const auto& t:targets)add(t);
        std::stable_sort(f.markers.begin(),f.markers.end(),[](const auto& a,const auto& b){
            if(a.target.priority!=b.target.priority)return a.target.priority>b.target.priority;
            if(a.distance!=b.distance)return a.distance<b.distance;return a.target.id<b.target.id;});
        if(f.markers.size()>128)f.markers.resize(128);
        return f;
    }
}
