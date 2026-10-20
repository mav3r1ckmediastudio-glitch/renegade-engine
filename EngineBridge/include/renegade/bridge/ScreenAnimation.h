#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <limits>
#include <charconv>
namespace renegade::bridge
{
    enum class ScreenAnimationProperty { Opacity, OffsetX, OffsetY, Scale, Fill };
    enum class ScreenAnimationEasing { Linear, Smooth, EaseIn, EaseOut, Step };
    enum class ScreenAnimationLoop { Once, Repeat, PingPong };
    struct ScreenAnimationKey { double time=0; float value=0; };
    struct ScreenAnimationTrack
    {
        ScreenAnimationProperty property=ScreenAnimationProperty::Opacity;
        ScreenAnimationEasing easing=ScreenAnimationEasing::Smooth;
        ScreenAnimationLoop loop=ScreenAnimationLoop::Once;
        double delay=0;
        std::vector<ScreenAnimationKey> keys;
    };
    inline const char* ScreenAnimationPropertyName(ScreenAnimationProperty p) noexcept
    {
        switch(p){case ScreenAnimationProperty::Opacity:return "OPACITY";
        case ScreenAnimationProperty::OffsetX:return "OFFSET X";
        case ScreenAnimationProperty::OffsetY:return "OFFSET Y";
        case ScreenAnimationProperty::Scale:return "SCALE";
        case ScreenAnimationProperty::Fill:return "FILL";}
        return "UNKNOWN";
    }
    inline bool ValidateScreenAnimations(const std::vector<ScreenAnimationTrack>& tracks,
        bool progress,std::string& error)
    {
        if(tracks.size()>5){error="At most five animation property tracks are supported.";return false;}
        unsigned mask=0;
        for(const auto& t:tracks)
        {
            const int p=static_cast<int>(t.property),e=static_cast<int>(t.easing),l=static_cast<int>(t.loop);
            if(p<0||p>4||e<0||e>4||l<0||l>2||!std::isfinite(t.delay)||t.delay<0||t.delay>600||
                t.keys.size()<2||t.keys.size()>64)
            {error="Invalid animation property/easing/loop/delay or key count.";return false;}
            if((mask&(1u<<p))!=0){error="Only one animation track may own a property.";return false;}
            mask|=1u<<p;
            if(t.property==ScreenAnimationProperty::Fill&&!progress)
            {error="Fill animation requires a Progress widget.";return false;}
            double last=-1;
            for(const auto& k:t.keys)
            {
                if(!std::isfinite(k.time)||k.time<0||k.time>600||k.time<=last||!std::isfinite(k.value))
                {error="Animation key times must strictly increase within 0..600 seconds.";return false;}
                if((p==0||p==4)&&(k.value<0||k.value>1))
                {error="Opacity/fill keys must be in 0..1.";return false;}
                if(p==3&&(k.value<0.01f||k.value>8))
                {error="Scale keys must be in 0.01..8.";return false;}
                if((p==1||p==2)&&std::abs(k.value)>16384)
                {error="Animation translation exceeds the bounded design range.";return false;}
                last=k.time;
            }
            if(t.keys.front().time!=0){error="Animation tracks must start at time zero.";return false;}
        }
        error.clear();return true;
    }
    // Pure instance-relative UI clock, independent of paused gameplay time.
    inline float EvaluateScreenAnimation(const ScreenAnimationTrack& track,double seconds,
        bool reducedMotion=false) noexcept
    {
        if(track.keys.empty())return 0;
        if(reducedMotion)return track.loop==ScreenAnimationLoop::Once?track.keys.back().value:track.keys.front().value;
        const double end=track.keys.back().time;
        double t=std::isfinite(seconds)?std::max(0.0,seconds-track.delay):0;
        if(end>0&&t>0)
        {
            if(track.loop==ScreenAnimationLoop::Repeat)t=std::fmod(t,end);
            else if(track.loop==ScreenAnimationLoop::PingPong)
            {t=std::fmod(t,end*2);if(t>end)t=end*2-t;}
        }
        if(t>=end)return track.keys.back().value;
        for(std::size_t i=1;i<track.keys.size();++i)
        {
            const auto& a=track.keys[i-1];const auto& b=track.keys[i];
            if(t>b.time)continue;
            double f=(t-a.time)/(b.time-a.time);
            switch(track.easing)
            {
            case ScreenAnimationEasing::Smooth:f=f*f*(3-2*f);break;
            case ScreenAnimationEasing::EaseIn:f=f*f;break;
            case ScreenAnimationEasing::EaseOut:f=1-(1-f)*(1-f);break;
            case ScreenAnimationEasing::Step:f=f>=1?1:0;break;
            default:break;
            }
            return static_cast<float>(a.value+(b.value-a.value)*f);
        }
        return track.keys.back().value;
    }
    inline std::string AnimationKeysText(const std::vector<ScreenAnimationKey>& keys)
    {
        std::ostringstream out;out.imbue(std::locale::classic());
        const auto number=[&](auto value)
        {
            char buffer[64];const auto converted=std::to_chars(buffer,buffer+sizeof(buffer),value);
            out.write(buffer,converted.ptr-buffer);
        };
        for(std::size_t i=0;i<keys.size();++i)
        {if(i)out<<';';number(keys[i].time);out<<':';number(keys[i].value);}
        return out.str();
    }
    inline bool ParseAnimationKeys(const std::string& text,std::vector<ScreenAnimationKey>& keys)
    {
        if(text.size()>16384)return false;
        std::istringstream in(text);in.imbue(std::locale::classic());
        std::vector<ScreenAnimationKey> result;
        while(in.peek()!=std::char_traits<char>::eof())
        {
            ScreenAnimationKey k;char colon=0,separator=0;
            in>>std::noskipws>>k.time>>colon>>k.value;
            if(in.fail()||colon!=':'||result.size()>=64)return false;
            result.push_back(k);
            if(in.peek()==std::char_traits<char>::eof())break;
            in>>separator;if(separator!=';'||in.peek()==std::char_traits<char>::eof())return false;
        }
        keys=std::move(result);return keys.size()>=2;
    }
    struct ScreenAnimationPreview { bool active=false,playing=false,reducedMotion=false; double seconds=0; };
}
