#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace renegade::bridge
{
    struct ScreenHurtFeedback
    {
        bool enabled=true,hitFlash=true,edgeOverlay=true,directional=true,lowHealth=true;
        bool painEnabled=true,heartbeatEnabled=false,cameraShake=false;
        float flashOpacity=.10f,edgeOpacity=.48f,hold=.04f,fade=.65f;
        float directionDuration=1.2f,directionSize=32,directionRadius=.32f;
        float lowThreshold=.25f,lowOpacity=.32f,pulseHz=1;
        float painVolume=.7f,painCooldown=.35f,heartbeatVolume=.5f,heartbeatPeriod=1;
        float shakeStrength=.025f,shakeDuration=.25f,intensity=1;
        std::uint32_t colour=0xC81D25FF,lowColour=0x8F1018FF;
        std::string hitImage,edgeImage,directionImage,lowImage,painSound,heartbeatSound;
    };
    struct HurtNumberField {const char* key;const char* label;float ScreenHurtFeedback::*member;float minimum,maximum;};
    inline const std::vector<HurtNumberField>& HurtNumbers()
    {
        static const std::vector<HurtNumberField> fields={
            {"flash_opacity","Hit flash opacity",&ScreenHurtFeedback::flashOpacity,0,1},
            {"edge_opacity","Edge opacity",&ScreenHurtFeedback::edgeOpacity,0,1},
            {"hold","Hit hold (seconds)",&ScreenHurtFeedback::hold,0,1},
            {"fade","Hit fade (seconds)",&ScreenHurtFeedback::fade,.05f,4},
            {"direction_duration","Direction duration (seconds)",&ScreenHurtFeedback::directionDuration,.1f,5},
            {"direction_size","Direction icon size",&ScreenHurtFeedback::directionSize,8,128},
            {"direction_radius","Direction radius (fraction)",&ScreenHurtFeedback::directionRadius,.05f,.45f},
            {"low_threshold","Low health threshold (fraction)",&ScreenHurtFeedback::lowThreshold,.01f,.99f},
            {"low_opacity","Low health opacity",&ScreenHurtFeedback::lowOpacity,0,1},
            {"pulse_hz","Low health pulses / second",&ScreenHurtFeedback::pulseHz,0,3},
            {"pain_volume","Pain volume",&ScreenHurtFeedback::painVolume,0,1},
            {"pain_cooldown","Pain cooldown (seconds)",&ScreenHurtFeedback::painCooldown,.05f,10},
            {"heartbeat_volume","Heartbeat volume",&ScreenHurtFeedback::heartbeatVolume,0,1},
            {"heartbeat_period","Heartbeat period (seconds)",&ScreenHurtFeedback::heartbeatPeriod,.4f,5},
            {"shake_strength","Camera shake (metres)",&ScreenHurtFeedback::shakeStrength,0,.15f},
            {"shake_duration","Camera shake duration",&ScreenHurtFeedback::shakeDuration,.05f,2},
            {"intensity","Overall intensity",&ScreenHurtFeedback::intensity,0,1}};
        return fields;
    }
    struct HurtBoolField {const char* key;const char* label;bool ScreenHurtFeedback::*member;};
    inline const std::vector<HurtBoolField>& HurtFlags()
    {
        static const std::vector<HurtBoolField> fields={
            {"enabled","Feedback enabled",&ScreenHurtFeedback::enabled},
            {"hit_flash","Hit flash",&ScreenHurtFeedback::hitFlash},
            {"edge_overlay","Hit edge overlay",&ScreenHurtFeedback::edgeOverlay},
            {"directional","Direction indicators",&ScreenHurtFeedback::directional},
            {"low_health","Low health overlay",&ScreenHurtFeedback::lowHealth},
            {"pain_enabled","Pain sound",&ScreenHurtFeedback::painEnabled},
            {"heartbeat_enabled","Low health heartbeat",&ScreenHurtFeedback::heartbeatEnabled},
            {"camera_shake","Camera shake (optional)",&ScreenHurtFeedback::cameraShake}};
        return fields;
    }
    struct HurtResourceField {const char* key;const char* label;std::string ScreenHurtFeedback::*member;bool audio;};
    inline const std::vector<HurtResourceField>& HurtResources()
    {
        static const std::vector<HurtResourceField> fields={
            {"hit_image","Hit flash artwork",&ScreenHurtFeedback::hitImage,false},
            {"edge_image","Hit edge artwork",&ScreenHurtFeedback::edgeImage,false},
            {"direction_image","Direction icon",&ScreenHurtFeedback::directionImage,false},
            {"low_image","Low health artwork",&ScreenHurtFeedback::lowImage,false},
            {"pain_sound","Pain sound",&ScreenHurtFeedback::painSound,true},
            {"heartbeat_sound","Heartbeat sound",&ScreenHurtFeedback::heartbeatSound,true}};
        return fields;
    }
    inline bool HurtPath(const std::string& path)
    {
        if(path.empty())return true;
        if(path.size()>512||path.rfind("Content/",0)!=0||path.find('\\')!=path.npos||path.find(':')!=path.npos)return false;
        size_t at=0;while(at<path.size()){const auto end=path.find('/',at);const auto part=path.substr(at,end==path.npos?end:end-at);
            if(part.empty()||part=="."||part=="..")return false;for(unsigned char c:part)if(c<32||c==127)return false;
            if(end==path.npos)break;at=end+1;}
        return path.back()!='/';
    }
    inline bool ValidateHurtFeedback(const ScreenHurtFeedback& c,std::string& error)
    {
        for(const auto& f:HurtNumbers()){const auto n=c.*(f.member);if(!std::isfinite(n)||n<f.minimum||n>f.maximum){error=std::string(f.label)+" is outside its supported range.";return false;}}
        for(const auto& f:HurtResources())if(!HurtPath(c.*(f.member))){error=std::string(f.label)+" must use project Content.";return false;}
        error.clear();return true;
    }
    struct HurtHit {std::uint64_t sequence=0;float age=0,strength=0;std::optional<float> worldBearing;};
    struct HurtFeedbackFrame
    {
        bool available=false,alive=false;
        float healthFraction=1,heading=0,seconds=0;
        std::uint64_t sequence=0;
        std::vector<HurtHit> hits;
    };
    class PlayerHurtFeedbackSession
    {
        HurtFeedbackFrame frame_;
    public:
        void Reset(){frame_={};}
        void Damage(float amount,float maxHealth,std::optional<float> bearing={})
        {
            if(!std::isfinite(amount)||amount<=0||!std::isfinite(maxHealth)||maxHealth<=0)return;
            if(bearing&&!std::isfinite(*bearing))bearing.reset();
            if(frame_.hits.size()>=8)frame_.hits.erase(frame_.hits.begin());
            frame_.hits.push_back({++frame_.sequence,0,std::clamp(amount/maxHealth*4,.25f,1.f),bearing});
        }
        void Tick(float dt,bool available,bool alive,float health,float maximum,float heading)
        {
            frame_.available=available;frame_.alive=alive;
            frame_.healthFraction=std::isfinite(health)&&std::isfinite(maximum)&&maximum>0?std::clamp(health/maximum,0.f,1.f):1;
            frame_.heading=std::isfinite(heading)?heading:0;
            dt=std::isfinite(dt)?std::clamp(dt,0.f,.25f):0;
            frame_.seconds+=dt;for(auto& hit:frame_.hits)hit.age+=dt;
            frame_.hits.erase(std::remove_if(frame_.hits.begin(),frame_.hits.end(),[](const auto& hit){return hit.age>6;}),frame_.hits.end());
            if(!available||!alive)frame_.hits.clear();
        }
        const HurtFeedbackFrame& Frame()const{return frame_;}
    };
    struct HurtFeedbackSample
    {
        float flash=0,edge=0,low=0,shake=0;
        std::vector<std::pair<float,float>> directions; // angle from view-forward, opacity
    };
    inline float HurtFade(float age,float hold,float fade)
    {return age<=hold?1:std::max(0.f,1-(age-hold)/std::max(.001f,fade));}
    inline HurtFeedbackSample SampleHurtFeedback(const ScreenHurtFeedback& c,const HurtFeedbackFrame& f,bool reducedMotion=false)
    {
        HurtFeedbackSample s;if(!c.enabled||!f.available||!f.alive)return s;
        for(const auto& hit:f.hits){
            const float v=HurtFade(hit.age,c.hold,c.fade)*hit.strength*c.intensity;
            if(c.hitFlash&&!reducedMotion)s.flash=std::max(s.flash,v*c.flashOpacity);
            if(c.edgeOverlay)s.edge=std::max(s.edge,v*c.edgeOpacity);
            if(c.cameraShake&&!reducedMotion)s.shake=std::max(s.shake,HurtFade(hit.age,0,c.shakeDuration)*hit.strength*c.shakeStrength*c.intensity);
            if(c.directional&&hit.worldBearing&&hit.age<c.directionDuration)
                s.directions.push_back({(*hit.worldBearing-f.heading)*3.14159265359f/180,HurtFade(hit.age,0,c.directionDuration)*c.intensity});
        }
        if(c.lowHealth&&f.healthFraction<c.lowThreshold){
            const float severity=1-f.healthFraction/c.lowThreshold;
            const float pulse=reducedMotion||c.pulseHz==0?1:.65f+.35f*std::sin(f.seconds*c.pulseHz*6.28318530718f);
            s.low=c.lowOpacity*(.35f+.65f*severity)*pulse*c.intensity;
        }
        return s;
    }
}
