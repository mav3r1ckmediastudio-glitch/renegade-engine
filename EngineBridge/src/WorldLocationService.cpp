#include "renegade/bridge/WorldLocationService.h"
#include "renegade/bridge/ScreenThemeStyle.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cctype>
namespace renegade::bridge
{
namespace {
    bool text(const std::string& s,size_t limit,bool empty=false)
    {return (empty||!s.empty())&&s.size()<=limit&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;});}
    bool point(NavigationPoint p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<=1e7&&std::abs(p.y)<=1e7&&std::abs(p.z)<=1e7;}
}
    bool ValidateLocationProfile(const LocationProfile& p,std::string& error)
    {
        const auto& a=p.announcement;
        auto extension=std::filesystem::u8path(a.font).extension().generic_u8string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
        const auto number=[](float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
        if(!text(p.type,48)||(!p.icon.empty()&&!IsNavigationIconPath(p.icon))||
            (!a.font.empty()&&a.font!=BuiltinScreenFont&&(!IsNavigationIconPath(a.font)||
                extension!=".ttf"))||
            !number(a.range,0,1e7)||!number(a.duration,.1f,600)||!number(a.fadeIn,0,60)||!number(a.fadeOut,0,60)||
            !number(a.strength,0,128)||!number(a.speed,.1f,30)||int(a.effect)<0||int(a.effect)>3)
        {error="Invalid location type, icon or announcement settings.";return false;}
        error.clear();return true;
    }
    std::string LocationProfileKey(const LocationProfile& p)
    {
        const auto& a=p.announcement;std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9);
        out<<p.type<<'\n'<<p.icon<<'\n'<<a.font<<'\n'<<p.showOnCompass<<p.onlyAfterDiscovery<<a.enabled<<a.repeatOnReturn
            <<' '<<a.range<<' '<<a.duration<<' '<<a.fadeIn<<' '<<a.fadeOut<<' '<<a.strength<<' '<<a.speed<<' '<<int(a.effect);
        return out.str();
    }
    LocationTextSample SampleLocationText(const LocationAnnouncementFrame& f,bool reducedMotion)
    {
        LocationTextSample s;
        if(!std::isfinite(f.elapsed)||f.elapsed<0||f.elapsed>=f.settings.duration||f.text.empty())return s;
        s.visible=true;s.opacity=1;const auto& a=f.settings;
        if(a.effect==LocationTextEffect::None)return s;
        const float t=float(f.elapsed),remaining=a.duration-t;
        const float fadeIn=std::min(a.fadeIn,a.duration*.5f),fadeOut=std::min(a.fadeOut,a.duration*.5f);
        const float envelope=std::clamp(std::min(fadeIn>0?t/fadeIn:1.f,fadeOut>0?remaining/fadeOut:1.f),0.f,1.f);
        s.opacity=envelope*envelope*(3.f-2.f*envelope);
        if(reducedMotion)return s;
        if(a.effect==LocationTextEffect::Wobble)
            s.y=std::sin(t*a.speed*3.14159265f)*a.strength;
        return s;
    }
    LocationGlitchSample SampleLocationGlitch(const LocationAnnouncementFrame& f,bool reducedMotion)
    {
        LocationGlitchSample s;
        if(reducedMotion||!SampleLocationText(f).visible||f.settings.effect!=LocationTextEffect::Glitch||
            f.settings.strength<=0||f.elapsed<.25)return s;
        const double clock=(f.elapsed-.25)*f.settings.speed;
        const double phase=std::fmod(clock,1.8);
        // Two brief cuts separated by clean, readable text. Never blank the whole name.
        if(!((phase>=.32&&phase<.44)||(phase>=.52&&phase<.58)))return s;
        const auto burst=uint64_t(clock/1.8);
        const float sign=burst%2?1.f:-1.f;
        s.active=true;
        s.separation=std::clamp(f.settings.strength*.3f,1.f,4.f);
        s.bands={{{.22f,.13f,sign*f.settings.strength},
                  {.63f,.10f,-sign*f.settings.strength*.7f}}};
        return s;
    }
    bool WorldLocationService::Put(WorldLocation l,std::string& error)
    {
        if(!text(l.id,120)||!text(l.name,160)||!point(l.position)||!ValidateLocationProfile(l.profile,error))
        {if(error.empty())error="Invalid location identity, name or position.";return false;}
        const auto found=locations_.find(l.id);
        if(found==locations_.end()){
            if(locations_.size()>=MaximumLocations){error="Location limit reached.";return false;}
            const auto id=l.id;locations_.emplace(id,LocationVisit{std::move(l)});
        }else found->second.location=std::move(l);
        error.clear();return true;
    }
    bool WorldLocationService::SyncNative(WorldLocation l,std::string& error)
    {
        const auto key=l.name+'\n'+LocationProfileKey(l.profile),id=l.id;
        const auto previous=nativeBaseline_.find(id);auto found=locations_.find(id);
        if(previous!=nativeBaseline_.end()&&previous->second==key&&found!=locations_.end()){
            if(!point(l.position)){error="Invalid native location position.";return false;}
            found->second.location.position=l.position;error.clear();return true;
        }
        if(!Put(std::move(l),error))return false;nativeBaseline_[id]=key;return true;
    }
    bool WorldLocationService::Remove(const std::string& id,std::string& error)
    {
        if(!locations_.erase(id)){error="Unknown location.";return false;}
        nativeBaseline_.erase(id);pending_.erase(std::remove(pending_.begin(),pending_.end(),id),pending_.end());
        if(current_&&current_->id==id)current_.reset();error.clear();return true;
    }
    bool WorldLocationService::Discover(const std::string& id,std::string& error)
    {auto i=locations_.find(id);if(i==locations_.end()){error="Unknown location.";return false;}i->second.discovered=true;error.clear();return true;}
    const LocationVisit* WorldLocationService::Get(const std::string& id)const
    {const auto i=locations_.find(id);return i==locations_.end()?nullptr:&i->second;}
    bool WorldLocationService::Queue(const std::string& id)
    {
        if(current_&&current_->id==id)return true;
        if(std::find(pending_.begin(),pending_.end(),id)!=pending_.end())return true;
        if(pending_.size()>=MaximumQueued)return false;pending_.push_back(id);return true;
    }
    bool WorldLocationService::Announce(const std::string& id,std::string& error)
    {
        const auto* v=Get(id);if(!v){error="Unknown location.";return false;}
        if(!Queue(id)){error="Location announcement queue is full.";return false;}
        StartNext();error.clear();return true;
    }
    void WorldLocationService::StartNext()
    {
        while(!current_&&!pending_.empty()){
            auto id=std::move(pending_.front());pending_.pop_front();const auto* v=Get(id);if(!v)continue;
            current_=LocationAnnouncementFrame{id,v->location.name,v->location.profile.announcement,0};
        }
    }
    void WorldLocationService::HideAnnouncement(){current_.reset();pending_.clear();}
    void WorldLocationService::Tick(float dt,std::optional<NavigationPoint> player)
    {
        if(!std::isfinite(dt)||dt<0)return;
        if(current_){current_->elapsed+=dt;if(current_->elapsed>=current_->settings.duration)current_.reset();}
        std::vector<std::pair<double,std::string>> entering;
        if(player&&point(*player))for(auto& [id,v]:locations_){
            const auto& a=v.location.profile.announcement;
            const double d=std::hypot(double(v.location.position.x)-player->x,double(v.location.position.z)-player->z);
            if(v.withinRange&&d>a.range+std::max(2.f,a.range*.05f)){v.withinRange=false;if(a.repeatOnReturn)v.fired=false;}
            if(!v.withinRange&&d<=a.range){v.withinRange=true;if(a.enabled&&!v.fired)entering.emplace_back(d,id);}
        }
        std::sort(entering.begin(),entering.end());
        for(const auto& [distance,id]:entering)if(Queue(id))locations_.at(id).fired=true;
        StartNext();
    }
    void WorldLocationService::Reset(){locations_.clear();nativeBaseline_.clear();pending_.clear();current_.reset();}
    ScreenWidget MakeLocationAnnouncementWidget()
    {
        ScreenWidget w;w.id=GenerateStableId();w.name="Location announcement";w.kind=ScreenWidgetKind::Text;
        w.locationAnnouncement=true;w.text="Location name";w.rect={160,150,960,100};w.enabled=false;
        w.style=MakeScreenWidgetStyleTemplate(w.kind,720);w.style.text.fontSize=34;w.style.text.bolden=.1f;
        w.style.text.horizontalAlignment=ScreenHorizontalAlignment::Center;w.style.text.verticalAlignment=ScreenVerticalAlignment::Center;
        w.style.text.shadowColor={0,0,0,220};w.style.text.shadowOffsetY=2;w.style.borderWidth=0;
        for(auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled}){v->background={0,0,0,0};v->foreground={255,255,255,255};}
        return w;
    }
}
