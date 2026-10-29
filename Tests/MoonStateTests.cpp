#include "renegade/bridge/MoonService.h"
#include "renegade/bridge/NightSkyService.h"
#include "renegade/bridge/SunService.h"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <filesystem>
#include <limits>

static_assert(offsetof(ShaderWeather, sky_exposure)==136);
static_assert(offsetof(ShaderWeather, moon_parameters)==1008);
static_assert(offsetof(ShaderWeather, moon_orbit)==1024);
static_assert(sizeof(ShaderWeather)==1040);
namespace
{
    void Check(bool v,const char* msg)
    {if(!v){std::fprintf(stderr,"Moon: %s\n",msg);std::_Exit(1);}}
    bool Near(float a,float b){return std::abs(a-b)<1e-4f;}
}
int main(int argc, char** argv)
{
    using namespace renegade::bridge;
    wi::scene::Scene scene;
    const auto e=wi::ecs::CreateEntity();
    auto& w=scene.weathers.Create(e);
    w.stars=.83f;w.skyExposure=1.7f;scene.weather=w;
    Check(!CaptureMoon(scene,e).enabled,"legacy scene unexpectedly enables moon");
    RestoreMoon(scene);
    Check(Near(w.moon_parameters.z,0),"legacy runtime moon is enabled");
    NightSkyState night;night.aurora=.6f;night.milkyWay=.8f;
    StoreNightSky(scene,e,night);
    MoonState s;s.enabled=true;s.automatic=false;s.phase=.625f;s.diameter=2.4f;
    s.brightness=1.2f;s.azimuth=-72;s.elevation=38;
    SetMoonCommand command(scene,e,s);
    Check(command.Execute(),"moon command failed");
    Check(SameMoon(CaptureMoon(scene,e),s),"authored fields changed");
    Check(Near(w.moon_parameters.x,.625f)&&Near(w.moon_orbit.x,-72),"renderer fields disconnected");
    Check(Near(CaptureNightSky(scene,e).aurora,.6f)&&Near(w.night_zenith.w,.8f),"moon changed accepted night effects");
    if(argc==3 && std::string(argv[1])=="--fixture")
    {
        w.SetRealisticSky(true);
        s.phase=.5f;s.diameter=3;s.brightness=.7f;s.azimuth=0;s.elevation=30;
        StoreMoon(scene,e,s);
        scene.names.Create(e).name="Environment";
        (void)EnsurePrimarySunLight(scene,e);
        auto sun=CaptureSun(scene,e);SetSunTime(sun,0);ApplySun(scene,e,sun);
        wi::Archive fixture;scene.Serialize(fixture);
        Check(fixture.SaveFile(argv[2]),"fixture disk save failed");
        std::puts("Moon preview fixture created");std::fflush(stdout);std::_Exit(0);
    }
    command.Undo();
    Check(!HasMoon(scene,e)&&Near(w.moon_parameters.z,0),"undo failed to restore legacy absence");
    Check(command.Execute(),"redo failed");
    const std::string path="MoonState-roundtrip.wiscene";
    wi::Archive output;scene.Serialize(output);
    Check(output.SaveFile(path),"disk save failed");
    wi::Archive input(path,true,false);
    Check(input.IsOpen(),"disk open failed");
    wi::scene::Scene loaded;loaded.Serialize(input);
    const auto le=loaded.weathers.GetEntity(0);
    RestoreNightSky(loaded); // same hydration path as Studio/Runtime load
    Check(SameMoon(CaptureMoon(loaded,le),s),"disk reopen lost settings");
    Check(Near(loaded.weathers.GetComponent(le)->moon_parameters.z,1.2f),"disk reopen did not hydrate moon");
    Check(Near(loaded.weathers.GetComponent(le)->stars,.83f),"moon changed native stars");
    loaded.Clear();std::filesystem::remove(path);
    // Independent phase and automatic motion: same selected phase at different solar positions.
    s.automatic=true;s.azimuth=0;
    auto sun=CaptureSun(scene,e);
    SetSunTime(sun,0);const auto midnight=MoonDirection(s,sun.direction);
    SetSunTime(sun,12);const auto midday=MoonDirection(s,sun.direction);
    Check(midnight.y>.9f && midday.y<-.9f,"moon failed to rise/set with sun time");
    Check(Near(s.phase,.625f),"orbit changed selected phase");
    s.automatic=false;s.azimuth=90;s.elevation=30;
    const auto fixedA=MoonDirection(s,XMFLOAT3(0,1,0));
    const auto fixedB=MoonDirection(s,XMFLOAT3(0,-1,0));
    Check(Near(fixedA.x,fixedB.x)&&Near(fixedA.y,.5f)&&Near(fixedA.z,0),"fixed position drifted");
    s.phase=std::numeric_limits<float>::quiet_NaN();s.diameter=-5;s.brightness=99;
    s=SanitizeMoon(s);
    Check(Near(s.phase,.5f)&&Near(s.diameter,.1f)&&Near(s.brightness,4),"invalid values not sanitized");
    scene.metadatas.GetComponent(e)->int_values.set("renegade.moon.schema",99);
    RestoreMoon(scene);Check(Near(w.moon_parameters.z,0),"future schema should fail closed");
    ClearMoon(scene,e);
    Check(Near(w.stars,.83f)&&Near(w.skyExposure,1.7f),"moon changed daylight/native weather");
    Check(!StoreMoon(scene,wi::ecs::CreateEntity(),s),"non-weather accepted moon metadata");
    scene.Clear();
    std::puts("Moon: metadata, Undo/Redo, disk persistence, legacy, sanitation and orbit checks passed");
    std::fflush(stdout);std::_Exit(0);
}
