#include "renegade/bridge/NightSkyService.h"
#include "renegade/bridge/SunService.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
static_assert(sizeof(ShaderWeather)==1040);
static_assert(offsetof(VolumetricCloudLayer,padding0)==12);
static_assert(offsetof(VolumetricCloudLayer,padding1)==28);
static void Check(bool ok,const char* message)
{if(!ok){std::fprintf(stderr,"Cloud formations: %s\n",message);std::_Exit(1);}}
int main(int argc,char** argv)
{
    using namespace renegade::bridge;
    wi::scene::Scene scene;
    if(argc==3 && std::string(argv[1])=="--inspect-scene")
    {
        wi::Archive archive(argv[2],true,false);
        Check(archive.IsOpen(),"read-only input open failed");
        scene.Serialize(archive);
        RestoreNightSky(scene);
        std::printf("READ ONLY scene: %s\nweathers=%zu cameras=%zu time=%g\n",
            argv[2],scene.weathers.GetCount(),scene.cameras.GetCount(),double(scene.time));
        for(size_t i=0;i<scene.weathers.GetCount();++i)
        {
            const auto& w=scene.weathers[i];const auto& p=w.volumetricCloudParameters;
            std::printf("weather[%zu] clouds=%d base=%g depth=%g exposure=%g layers=%g/%g floor=%g/%g type=%g/%g shapeScale=%g/%g weatherScale=%g/%g animation=%g customMaps=%d/%d\n",
                i,w.IsVolumetricClouds(),p.cloudStartHeight,p.cloudThickness,w.skyExposure,
                p.layerFirst.coverageAmount,p.layerSecond.coverageAmount,p.layerFirst.coverageMinimum,
                p.layerSecond.coverageMinimum,p.layerFirst.typeAmount,p.layerSecond.typeAmount,
                p.layerFirst.totalNoiseScale,p.layerSecond.totalNoiseScale,p.layerFirst.weatherScale,
                p.layerSecond.weatherScale,p.animationMultiplier,
                w.volumetricCloudsWeatherMapFirst.IsValid(),w.volumetricCloudsWeatherMapSecond.IsValid());
        }
        scene.Clear();std::fflush(stdout);std::_Exit(0);
    }
    const auto e=wi::ecs::CreateEntity();auto& w=scene.weathers.Create(e);
    scene.weather=w;
    w.volumetricCloudParameters.layerFirst.coverageAmount=.8f;
    w.volumetricCloudParameters.layerSecond.coverageAmount=.23f;
    if(argc==3 && (std::string(argv[1])=="--fixture" || std::string(argv[1])=="--boundary-fixture"))
    {
        w.SetRealisticSky(true);w.SetVolumetricClouds(true);w.SetVolumetricCloudsCastShadow(true);
        w.volumetricCloudParameters.layerSecond.coverageAmount=0;
        w.volumetricCloudParameters.animationMultiplier=0;
        StoreCloudFormation(scene,e,CloudFormation::Cumulus);
        MoonState moon;moon.enabled=true;moon.automatic=true;moon.phase=.5f;moon.brightness=.9f;
        StoreMoon(scene,e,moon);
        NightSkyState night;night.gradient=.7f;StoreNightSky(scene,e,night);
        w.stars=.6f;
        (void)EnsurePrimarySunLight(scene,e);auto sun=CaptureSun(scene,e);
        const bool boundary=std::string(argv[1])=="--boundary-fixture";
        if(boundary) { w.volumetricCloudParameters.cloudStartHeight=2280;
            w.volumetricCloudParameters.cloudThickness=3190; }
        SetSunTime(sun,boundary ? 0 : 12);ApplySun(scene,e,sun);
        const auto ce=wi::ecs::CreateEntity();
        auto& cam=scene.cameras.Create(ce);
        cam.CreatePerspective(1920,1080,.1f,1000,XMConvertToRadians(70));
        auto& transform=scene.transforms.Create(ce);
        transform.MatrixTransform(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
            XMVectorSet(0,2,0,1),XMVectorSet(0,2.573576f,.819152f,1),XMVectorSet(0,1,0,0))));
        transform.UpdateTransform();cam.TransformCamera(transform);cam.UpdateCamera();
        scene.names.Create(ce).name="Cloud comparison camera";
        scene.names.Create(e).name="Environment";
        wi::Archive fixture;scene.Serialize(fixture);
        Check(fixture.SaveFile(argv[2]),"preview fixture save failed");
        scene.Clear();std::puts("Cloud formation preview fixture saved");std::fflush(stdout);std::_Exit(0);
    }
    const auto original=w.volumetricCloudParameters;
    Check(CaptureCloudFormation(scene,e)==CloudFormation::Legacy,"legacy fallback");
    RestoreNightSky(scene);
    Check(w.volumetricCloudParameters.layerFirst.padding0==0,"legacy renderer selection");
    for(int type=1;type<=5;++type)
    {
        SetCloudFormationCommand command(scene,e,static_cast<CloudFormation>(type));
        Check(command.Execute(),"select failed");
        Check(int(CaptureCloudFormation(scene,e))==type,"capture mismatch");
        Check(w.volumetricCloudParameters.layerFirst.padding0==float(type),"GPU field mismatch");
        Check(w.volumetricCloudParameters.layerFirst.coverageAmount==original.layerFirst.coverageAmount &&
              w.volumetricCloudParameters.cloudStartHeight==original.cloudStartHeight &&
              w.volumetricCloudParameters.cloudThickness==original.cloudThickness &&
              w.volumetricCloudParameters.layerSecond.coverageAmount==original.layerSecond.coverageAmount,
              "formation changed independent native controls");
        command.Undo();Check(CaptureCloudFormation(scene,e)==CloudFormation::Legacy,"undo failed");
        Check(command.Execute(),"redo failed");
        const std::string path="CloudFormation-roundtrip.wiscene";
        wi::Archive output;scene.Serialize(output);Check(output.SaveFile(path),"save failed");
        wi::scene::Scene loaded;wi::Archive input(path,true,false);Check(input.IsOpen(),"reopen failed");
        loaded.Serialize(input);RestoreNightSky(loaded);
        const auto le=loaded.weathers.GetEntity(0);
        Check(int(CaptureCloudFormation(loaded,le))==type,"disk formation lost");
        Check(loaded.weathers[0].volumetricCloudParameters.layerFirst.padding0==float(type),"disk hydration failed");
        loaded.Clear();std::filesystem::remove(path);
        ClearCloudFormation(scene,e);
    }
    auto& m=*scene.metadatas.GetComponent(e);
    m.int_values.set("renegade.clouds.schema",99);m.int_values.set("renegade.clouds.formation",3);
    Check(CaptureCloudFormation(scene,e)==CloudFormation::Legacy,"unknown schema accepted");
    SetCloudFormationCommand future(scene,e,CloudFormation::Cumulus);
    Check(future.Execute(),"future select failed");
    m.float_values.set("unrelated.owner.value",42);
    future.Undo();
    Check(m.int_values.get("renegade.clouds.schema")==99,"unknown metadata not restored");
    Check(m.float_values.has("unrelated.owner.value"),"undo clobbered unrelated metadata");
    Check(!StoreCloudFormation(scene,wi::ecs::CreateEntity(),CloudFormation::Cumulus),"non-weather accepted");
    scene.Clear();std::puts("Cloud formations: legacy, independent controls, Undo/Redo, disk persistence and future schema passed");
    std::fflush(stdout);std::_Exit(0);
}
