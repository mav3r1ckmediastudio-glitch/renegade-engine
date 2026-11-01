#pragma once
// Explicit disposable-run diagnostic; never active in ordinary Studio/Test Game.
#include "renegade/bridge/PrecipitationService.h"
#include "renegade/bridge/LightningAuthoringService.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
namespace renegade::runtime {
inline void ConfigureWeatherDiagnostic(wi::scene::Scene& scene){
    const char* mode=std::getenv("RENEGADE_G4_PROFILE_CASE");
    if(!mode || !scene.weathers.GetCount())return;
    static const wi::scene::Scene* configured=nullptr;
    if(configured==&scene)return;configured=&scene;
    if(std::getenv("RENEGADE_G4_PROFILE_UNCAPPED"))
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_SET_VSYNC,0);
    const std::string which=mode;
    auto sky=bridge::MakeWeatherPreset(bridge::CaptureWeather(scene.weather),bridge::WeatherPreset::Storm);
    auto rain=bridge::MakePrecipitationProfile(bridge::CapturePrecipitation(scene.weather),bridge::PrecipitationMode::Rain);
    rain.intensity=(which=="rain" || which=="storm")?0.95f:0.0f;
    rain.mode=rain.intensity>0?bridge::PrecipitationMode::Rain:bridge::PrecipitationMode::None;
    bridge::LightningAuthoringState light;
    light.mode=which=="sheet"?bridge::LightningMode::Sheet:
               which=="fork"?bridge::LightningMode::Fork:
               (which=="both" || which=="storm")?bridge::LightningMode::Both:bridge::LightningMode::Off;
    light.sheetPerMinute=20;light.forkPerMinute=20;light.distanceDelay=false;
    for(auto* w:{&scene.weather,&scene.weathers[0]}){bridge::ApplyWeather(*w,sky);bridge::ApplyPrecipitation(*w,rain);}
    bridge::StoreLightning(scene,scene.weathers.GetEntity(0),light);
}
inline void RecordWeatherDiagnostic(const wi::scene::Scene& scene,float dt){
    struct DeferredFrameCSV : std::ostringstream {
        std::string path;
        explicit operator bool() const{return !path.empty();}
        ~DeferredFrameCSV(){if(!path.empty()){std::ofstream f(path);f<<str();}}
    };
    static DeferredFrameCSV out;
    static bool checked=false;
    if(!checked){checked=true;if(const char* p=std::getenv("RENEGADE_G4_FRAME_TRACE")){
        out.path=p;out<<"frame,dt,energy,recovery,capture_frame\n";}}
    if(out){
        const auto& p=scene.weather.sheet_light_power;
        out<<wi::graphics::GetDevice()->GetFrameCount()<<','<<dt<<','<<std::max({p.x,p.y,p.z})<<','<<p.w<<','<<scene.cloudmap_frame<<'\n';
    }
}
}
