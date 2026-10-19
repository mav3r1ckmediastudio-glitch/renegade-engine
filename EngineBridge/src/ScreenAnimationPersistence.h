#pragma once
#include "renegade/bridge/ScreenAnimation.h"
#include "ScreenBindingPersistence.h"
#include <wiConfig.h>
#include <sstream>
#include <iomanip>
#include <limits>
namespace renegade::bridge::detail
{
    template<class T> inline bool ReadAnimationScalar(const wi::config::Section& s,const char* key,T& value)
    {
        if(!s.Has(key))return false;
        std::istringstream in(s.GetText(key));in.imbue(std::locale::classic());
        in>>std::noskipws>>value;
        return !in.fail()&&in.peek()==std::char_traits<char>::eof();
    }
    inline void WriteWidgetAnimations(wi::config::File& file,const std::string& prefix,
        const std::vector<ScreenAnimationTrack>& tracks)
    {
        file.GetSection(prefix.c_str()).Set("animation_count",static_cast<int>(tracks.size()));
        for(std::size_t i=0;i<tracks.size();++i)
        {
            const auto& t=tracks[i];auto& s=file.GetSection((prefix+"_animation_"+std::to_string(i)).c_str());
            s.Set("property",static_cast<int>(t.property));s.Set("easing",static_cast<int>(t.easing));
            s.Set("loop",static_cast<int>(t.loop));
            std::ostringstream delay;delay.imbue(std::locale::classic());delay<<std::setprecision(17)<<t.delay;
            s.Set("delay",delay.str());s.Set("keys_hex",EncodeBindingText(AnimationKeysText(t.keys)));
        }
    }
    inline bool ReadWidgetAnimations(wi::config::File& file,const std::string& prefix,
        unsigned schema,std::vector<ScreenAnimationTrack>& tracks,std::string& error)
    {
        const auto& root=file.GetSection(prefix.c_str());
        if(!root.Has("animation_count"))return true;
        int count=-1;
        if(schema<3||!ReadAnimationScalar(root,"animation_count",count)||count<0||count>5)
        {error="Invalid Screen animation count/schema.";return false;}
        for(int i=0;i<count;++i)
        {
            const auto name=prefix+"_animation_"+std::to_string(i);
            if(!file.HasSection(name.c_str())){error="Missing Screen animation track.";return false;}
            const auto& s=file.GetSection(name.c_str());ScreenAnimationTrack t;int p=-1,e=-1,l=-1;std::string decoded;
            if(!ReadAnimationScalar(s,"property",p)||!ReadAnimationScalar(s,"easing",e)||
                !ReadAnimationScalar(s,"loop",l)||!ReadAnimationScalar(s,"delay",t.delay)||
                !s.Has("keys_hex")||!DecodeBindingText(s.GetText("keys_hex"),16384,decoded)||!ParseAnimationKeys(decoded,t.keys))
            {error="Malformed Screen animation track.";return false;}
            t.property=static_cast<ScreenAnimationProperty>(p);t.easing=static_cast<ScreenAnimationEasing>(e);
            t.loop=static_cast<ScreenAnimationLoop>(l);tracks.push_back(std::move(t));
        }
        return true;
    }
}
