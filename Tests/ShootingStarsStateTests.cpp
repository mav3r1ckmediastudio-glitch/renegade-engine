#include "renegade/bridge/NightSkyService.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <cstddef>
static_assert(offsetof(ShaderWeather,sky_exposure)==136);
static_assert(offsetof(ShaderWeather,night_alignment_padding)==76);
static_assert(offsetof(ShaderWeather,night_padding)==116);
static_assert(sizeof(ShaderWeather)==1040);
namespace
{
    void Check(bool v,const char* msg){if(!v){std::fprintf(stderr,"Meteors: %s\n",msg);std::_Exit(1);}}
    bool Near(float a,float b){return std::abs(a-b)<1e-4f;}
}
int main()
{
    using namespace renegade::bridge;
    wi::scene::Scene scene;const auto e=wi::ecs::CreateEntity();
    auto& weather=scene.weathers.Create(e);weather.stars=.72f;scene.weather=weather;
    RestoreNightSky(scene);
    Check(!CaptureShootingStars(scene,e).enabled && Near(weather.shooting_stars.x,0),"legacy scene enabled meteors");
    NightSkyState night;night.aurora=.4f;night.milkyWay=.65f;StoreNightSky(scene,e,night);
    MoonState moon;moon.enabled=true;moon.phase=.75f;StoreMoon(scene,e,moon);
    ShootingStarsState s;s.enabled=true;s.frequency=7;s.brightness=1.4f;s.trailLength=13;
    SetShootingStarsCommand command(scene,e,s);
    Check(command.Execute(),"command failed");
    Check(SameShootingStars(s,CaptureShootingStars(scene,e)),"metadata fields disconnected");
    Check(Near(weather.shooting_stars.x,7)&&Near(weather.shooting_stars.z,13),"renderer fields disconnected");
    PreviewShootingStar(scene,e,2.5f);
    Check(Near(weather.shooting_star_preview_time,2.5f),"preview did not reach renderer");
    Check(SameShootingStars(s,CaptureShootingStars(scene,e)),"preview altered persisted state");
    command.Undo();
    Check(!HasShootingStars(scene,e) && Near(weather.shooting_stars.x,0),"undo did not restore legacy absence");
    Check(Near(weather.shooting_star_preview_time,-1000),"disabled preview remained active");
    Check(command.Execute(),"redo failed");
    wi::Archive output;scene.Serialize(output);const char* path="ShootingStars-roundtrip.wiscene";
    Check(output.SaveFile(path),"disk save failed");
    wi::Archive input(path,true,false);Check(input.IsOpen(),"disk open failed");
    wi::scene::Scene loaded;loaded.Serialize(input);RestoreNightSky(loaded);
    const auto le=loaded.weathers.GetEntity(0);
    Check(SameShootingStars(s,CaptureShootingStars(loaded,le)),"disk reopen lost settings");
    Check(Near(loaded.weathers.GetComponent(le)->shooting_stars.y,1.4f),"runtime hydration failed");
    Check(Near(CaptureMoon(loaded,le).phase,.75f)&&Near(CaptureNightSky(loaded,le).milkyWay,.65f),"meteor settings changed moon or galaxy");
    loaded.Clear();std::filesystem::remove(path);
    s.frequency=std::numeric_limits<float>::quiet_NaN();s.brightness=99;s.trailLength=-5;
    const auto safe=SanitizeShootingStars(s);
    Check(Near(safe.frequency,2)&&Near(safe.brightness,4)&&Near(safe.trailLength,1),"sanitation failed");
    scene.metadatas.GetComponent(e)->int_values.set("renegade.meteors.schema",99);
    RestoreShootingStars(scene);
    Check(Near(weather.shooting_stars.x,0),"unknown schema did not fail closed");
    ClearShootingStars(scene,e);
    Check(Near(weather.stars,.72f)&&Near(weather.aurora_intensity,.4f)&&Near(weather.night_zenith.w,.65f),"meteor off changed other sky contributions");
    Check(!StoreShootingStars(scene,wi::ecs::CreateEntity(),s),"non-weather accepted metadata");
    scene.Clear();std::puts("Meteors: persistence, Undo/Redo, preview, legacy and independent sky checks passed");
    std::fflush(stdout);std::_Exit(0);
}
