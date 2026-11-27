#pragma once
#include "renegade/bridge/ScreenNavigation.h"
#include <cstdlib>
#include <limits>
#include <type_traits>
namespace renegade::bridge::detail
{
    template<class S,class T> bool NavigationScalar(const S& s,const char* key,T& out)
    {
        const auto value=s.GetText(key);char* end=nullptr;const double n=std::strtod(value.c_str(),&end);
        if(value.empty()||end==value.c_str()||*end||!std::isfinite(n)||n<std::numeric_limits<T>::lowest()||n>std::numeric_limits<T>::max())return false;
        if constexpr(std::is_integral_v<T>)if(n!=std::floor(n))return false;
        out=static_cast<T>(n);return true;
    }
    template<class S> bool NavigationBool(const S& s,const char* key,bool& out)
    {const auto v=s.GetText(key);if(v!="true"&&v!="false")return false;out=v=="true";return true;}
    template<class S> void WriteNavigation(S& s,const ScreenNavigation& c)
    {
        s.Set("navigation_version",1);s.Set("nav_heading_source",int(c.headingSource));s.Set("nav_span",c.fieldOfView);
        s.Set("nav_heading",c.manualHeading);s.Set("nav_icon_size",c.markerSize);s.Set("nav_gap",c.markerGap);
        s.Set("nav_range",c.maxDistance);s.Set("nav_names",c.showNames);s.Set("nav_distances",c.showDistance);
        s.Set("nav_ticks",c.showTicks);s.Set("nav_strip",c.bearingStrip);s.Set("nav_pointer",c.pointerImage);s.Set("nav_filter",c.categoryFilter);
        s.Set("nav_icon_count",int(c.icons.size()));int i=0;
        for(const auto& [category,path]:c.icons){const auto k="nav_icon_"+std::to_string(i++);s.Set((k+"_category").c_str(),category);s.Set((k+"_path").c_str(),path);}
        s.Set("nav_target_count",int(c.targets.size()));i=0;
        for(const auto& t:c.targets){const auto k="nav_target_"+std::to_string(i++);
            s.Set((k+"_id").c_str(),t.id);s.Set((k+"_label").c_str(),t.label);s.Set((k+"_category").c_str(),t.category);s.Set((k+"_entity").c_str(),t.entityId);
            s.Set((k+"_x").c_str(),t.position.x);s.Set((k+"_y").c_str(),t.position.y);s.Set((k+"_z").c_str(),t.position.z);s.Set((k+"_priority").c_str(),t.priority);s.Set((k+"_visible").c_str(),t.visible);}
    }
    template<class S> bool ReadNavigation(const S& s,ScreenNavigation& c)
    {
        int version=0,source=0,icons=0,targets=0;
        if(!NavigationScalar(s,"navigation_version",version)||version!=1||
            !NavigationScalar(s,"nav_heading_source",source)||source<0||source>2||
            !NavigationScalar(s,"nav_span",c.fieldOfView)||!NavigationScalar(s,"nav_heading",c.manualHeading)||
            !NavigationScalar(s,"nav_icon_size",c.markerSize)||!NavigationScalar(s,"nav_gap",c.markerGap)||
            !NavigationScalar(s,"nav_range",c.maxDistance)||!NavigationBool(s,"nav_names",c.showNames)||
            !NavigationBool(s,"nav_distances",c.showDistance)||!NavigationBool(s,"nav_ticks",c.showTicks)||
            !NavigationScalar(s,"nav_icon_count",icons)||icons<0||icons>64||
            !NavigationScalar(s,"nav_target_count",targets)||targets<0||targets>64)return false;
        c.headingSource=static_cast<NavigationHeadingSource>(source);c.bearingStrip=s.GetText("nav_strip");c.pointerImage=s.GetText("nav_pointer");c.categoryFilter=s.GetText("nav_filter");
        for(int i=0;i<icons;++i){const auto k="nav_icon_"+std::to_string(i);if(!c.icons.emplace(s.GetText((k+"_category").c_str()),s.GetText((k+"_path").c_str())).second)return false;}
        for(int i=0;i<targets;++i){const auto k="nav_target_"+std::to_string(i);NavigationTarget t;
            t.id=s.GetText((k+"_id").c_str());t.label=s.GetText((k+"_label").c_str());t.category=s.GetText((k+"_category").c_str());t.entityId=s.GetText((k+"_entity").c_str());
            if(!NavigationScalar(s,(k+"_x").c_str(),t.position.x)||!NavigationScalar(s,(k+"_y").c_str(),t.position.y)||!NavigationScalar(s,(k+"_z").c_str(),t.position.z)||
                !NavigationScalar(s,(k+"_priority").c_str(),t.priority)||!NavigationBool(s,(k+"_visible").c_str(),t.visible))return false;
            c.targets.push_back(std::move(t));}
        return true;
    }
}
