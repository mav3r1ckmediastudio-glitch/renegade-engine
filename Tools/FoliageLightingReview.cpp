// Controlled native foliage/terrain lighting study; production shaders unchanged.
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/SunService.h"
#include "renegade/bridge/NightSkyService.h"
#include "renegade/bridge/RenderSettingsService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
using namespace DirectX;
using namespace wi::scene;
using namespace renegade::bridge;
namespace fs=std::filesystem;
using Entity=wi::ecs::Entity;
static void Check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Case {const char* name;float hour;bool moon;float phase;bool flatAmbient,lunarLamp,lantern;int gi;bool black;float exposure=1;};
static const Case cases[]={
 {"day-off",12,false,.5f,false,false,false,0,false},
 {"day-surfel",12,false,.5f,false,false,false,1,false},
 {"dusk-off",18.5f,false,.5f,false,false,false,0,false},
 {"night-inherited-ambient",0,true,.5f,true,false,false,0,false},
 {"night-no-moon",0,false,.5f,false,false,false,0,false},
 {"night-sky-full-moon",0,true,.5f,false,false,false,0,false},
 {"night-sky-new-moon",0,true,0,false,false,false,0,false},
 {"night-cold-moonlight",0,true,.5f,false,true,false,0,false},
 {"night-cold-moonlight-surfel",0,true,.5f,false,true,false,1,false},
 {"night-lantern-off",0,true,.5f,false,true,true,0,false},
 {"night-lantern-surfel",0,true,.5f,false,true,true,1,false},
 {"night-lantern-ssgi",0,true,.5f,false,true,true,2,false},
 {"zero-light",0,false,.5f,false,false,false,0,true},
 {"night-readable-inherited-ambient",0,true,.5f,true,false,false,0,false,3},
 {"night-readable-inherited-ambient-surfel",0,true,.5f,true,false,false,1,false,3},
 {"night-no-moon-surfel",0,false,.5f,false,false,false,1,false,3},
 {"night-sky-full-moon-surfel",0,true,.5f,false,false,false,1,false,3},
 {"night-readable-cold-moonlight",0,true,.5f,false,true,false,0,false,3},
 {"night-readable-cold-moonlight-surfel",0,true,.5f,false,true,false,1,false,3},
 {"night-readable-lantern-off",0,true,.5f,false,true,true,0,false,3},
 {"night-readable-lantern-surfel",0,true,.5f,false,true,true,1,false,3}
};
static Entity Box(Scene& s,const char* name,XMFLOAT3 center,XMFLOAT3 halfSize,XMFLOAT3 color){
 Entity e=s.Entity_CreateCube(name);auto* t=s.transforms.GetComponent(e);t->Scale(halfSize);t->Translate(center);t->UpdateTransform();
 auto* m=s.materials.GetComponent(e);Check(m,"Cube material missing");m->baseColor={color.x,color.y,color.z,1};m->SetRoughness(.95f);m->SetReflectance(.02f);m->SetDirty();m->CreateRenderData();return e;
}
static void SetView(CameraComponent& c,unsigned view){
 XMVECTOR eye,target;
 if(view==0){eye=XMVectorSet(0,2.2f,-5,1);target=XMVectorSet(0,.2f,19,1);}
 else{eye=XMVectorSet(-5,1.4f,1,1);target=XMVectorSet(-10,.4f,9,1);}
 c.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(eye,target,XMVectorSet(0,1,0,0))));c.UpdateCamera();
}
int main(int argc,char** argv){try{
 Check(argc==3||argc==4,"usage: FoliageLightingReview approved-scene.wiscene fresh-output-directory [case-name]");
 const fs::path out=fs::u8path(argv[2]);fs::create_directories(out);
 HWND window=CreateWindowExW(0,L"STATIC",L"Foliage and terrain lighting study",WS_POPUP,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 Check(window!=nullptr,"Window failed");wi::Application app;app.allow_hdr=false;app.SetWindow(window);
 wi::initializer::InitializeComponentsImmediate();app.setTargetFrameRate(60);app.setFrameRateLock(true);
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 std::ofstream log(out/"lighting-receipt.txt");
 log<<"Explicit exposure per case (1 proof, 3 readable night); eye adaptation/bloom/DOF/motion blur off. Wind frozen for matched lighting images.\n";
 for(const auto& test:cases){
  if(argc==4 && (std::string(argv[3])=="followup" ? test.exposure==1 : std::string(argv[3])!=test.name))continue;
  const auto folder=out/test.name;Check(!fs::exists(folder),"Case output already exists");fs::create_directories(folder);
  Scene s;LoadModel(s,argv[1]);Check(s.hairs.GetCount()==6&&s.terrains.GetCount()>0,"Expected six approved plant plots and terrain");
  const Entity weather=s.weathers.GetCount()?s.weathers.GetEntity(0):wi::ecs::INVALID_ENTITY;Check(weather!=wi::ecs::INVALID_ENTITY,"Missing weather");
  const XMFLOAT3 sourceAmbient=s.weathers[0].ambient;
  log<<test.name<<" exposure="<<test.exposure<<" source_realistic_sky="<<s.weathers[0].IsRealisticSky()<<" source_ambient="<<sourceAmbient.x<<","<<sourceAmbient.y<<","<<sourceAmbient.z<<"\n";
  // Remove self-lit labels so they cannot contaminate a darkness/GI comparison.
  std::vector<Entity> labels;
  for(size_t i=0;i<s.names.GetCount();++i)if(s.names[i].name.size()>=6&&s.names[i].name.substr(s.names[i].name.size()-6)==" label")labels.push_back(s.names.GetEntity(i));
  for(auto e:labels)s.Entity_Remove(e);
  for(size_t i=0;i<s.terrains.GetCount();++i){s.terrains[i].Generation_Cancel();s.terrains[i].SetGrassEnabled(false);}
  const Entity sun=EnsurePrimarySunLight(s,weather);Check(sun!=wi::ecs::INVALID_ENTITY,"Missing primary Sun");
  std::vector<Entity> otherLights;for(size_t i=0;i<s.lights.GetCount();++i)if(s.lights.GetEntity(i)!=sun)otherLights.push_back(s.lights.GetEntity(i));
  for(auto e:otherLights)s.Entity_Remove(e);
  auto state=CaptureSun(s,weather);SetSunTime(state,test.hour);Check(ApplySun(s,weather,state),"Sun state failed");
  s.lights.GetComponent(sun)->intensity=test.black?0.f:5.5f;s.lights.GetComponent(sun)->color={1,.95f,.86f};s.lights.GetComponent(sun)->SetCastShadow(true);
  for(size_t i=0;i<s.weathers.GetCount();++i){
   auto& w=s.weathers[i];w.SetRealisticSky(!test.black);w.SetVolumetricClouds(false);w.SetRealisticSkyAerialPerspective(false);w.skyMapName.clear();w.skyMap={};
   w.ambient=test.flatAmbient?sourceAmbient:XMFLOAT3(0,0,0);w.horizon={0,0,0};w.zenith={0,0,0};w.fogDensity=0;w.skyExposure=1;w.stars=test.black?0.f:.45f;
   w.windDirection={0,0,0};w.windSpeed=0;w.windRandomness=0;
  }
  NightSkyState night;night.gradient=test.black?0.f:1.f;night.aurora=0;night.milkyWay=0;Check(StoreNightSky(s,weather,night),"Night sky state failed");
  MoonState moon;moon.enabled=test.moon;moon.automatic=false;moon.phase=test.phase;moon.azimuth=-35;moon.elevation=35;moon.diameter=1;moon.brightness=.7f;
  Check(StoreMoon(s,weather,moon),"Moon state failed");
  if(test.lunarLamp){
   const Entity e=s.Entity_CreateLight("Authored Cold Moonlight preset",{0,8,8},{.56f,.69f,1},.5f,1000,LightComponent::DIRECTIONAL);
   auto* l=s.lights.GetComponent(e);l->SetCastShadow(true);l->cascade_distances={14,55,180};
   auto* t=s.transforms.GetComponent(e);XMStoreFloat4(&t->rotation_local,XMQuaternionRotationRollPitchYaw(XM_PIDIV2-XMConvertToRadians(moon.elevation),XMConvertToRadians(moon.azimuth),0));t->SetDirty();t->UpdateTransform();
  }
  if(test.lantern){
   const Entity e=s.Entity_CreateLight("Warm test lantern",{-10,2.0f,9},{1,.714f,.361f},12,10,LightComponent::POINT);
   s.lights.GetComponent(e)->SetCastShadow(true);
  }
  Box(s,"Lighting shelter roof",{-10,3.0f,10},{3.2f,.18f,4.0f},{.22f,.22f,.22f});
  Box(s,"Red GI bounce wall",{-13.1f,1.5f,10},{.15f,1.5f,4.0f},{.65f,.035f,.02f});
  Box(s,"Neutral shadow post",{-6.5f,1.1f,6},{.25f,1.1f,.25f},{.35f,.35f,.35f});
  auto renderState=CaptureRenderSettings(s);renderState.exposure=test.exposure;renderState.eyeAdaptationEnabled=false;renderState.bloomEnabled=false;
  renderState.motionBlurEnabled=false;renderState.depthOfFieldEnabled=false;renderState.surfelGiEnabled=test.gi==1;renderState.ssgiEnabled=test.gi==2;
  renderState.raytracedDiffuseEnabled=false;renderState.giBoost=1;renderState.antiAliasing=AntiAliasingMode::Off;renderState.sharpenEnabled=false;
  Check(WriteRenderSettings(s,renderState),"Render settings save failed");
  const auto file=folder/"Main.wiscene";
  {wi::Archive archive(file.generic_string(),false,false);s.Serialize(archive);Check(archive.SaveFile(file.generic_string()),"Scene save failed");}
  Scene cold;LoadModel(cold,file.generic_string());RestoreNightSky(cold);
  Check(cold.hairs.GetCount()==6&&cold.terrains.GetCount()>0,"Reopen vegetation/terrain changed");
  const auto reopened=CaptureRenderSettings(cold);
  Check(reopened.surfelGiEnabled==(test.gi==1)&&reopened.ssgiEnabled==(test.gi==2)&&!reopened.eyeAdaptationEnabled&&reopened.exposure==test.exposure,"Reopen render settings changed");
  Check(CaptureMoon(cold,cold.weathers.GetEntity(0)).enabled==test.moon,"Reopen moon changed");
  Check(cold.lights.GetCount()==size_t(1+test.lunarLamp+test.lantern),"Reopen light count changed");
  const uint32_t counts[]={3600,1800,1800,1080,1440,288};
  for(size_t i=0;i<6;++i)Check(cold.hairs[i].strandCount==counts[i],"Plant count changed");
  CameraComponent camera;camera.CreatePerspective(1280,800,.05f,300);
  {
   wi::RenderPath3D render;render.scene=&cold;render.camera=&camera;render.init(1280,800,96);ApplyRenderSettingsToPath(render,reopened,false);render.ResizeBuffers();app.ActivatePath(&render);
   struct Detach{wi::Application& app;~Detach(){app.ActivatePath(nullptr);}}detach{app};
   for(unsigned view=0;view<2;++view){
    SetView(camera,view);
    for(unsigned f=0;f<(test.gi?360u:150u);++f){app.is_window_active=true;app.Run();}
    wi::graphics::GetDevice()->WaitForGPU();
    Check(RenderSettingsMatchPath(render,reopened),"Native render settings not applied");
    if(test.gi==1)Check(render.surfelGIResources.result.IsValid()&&cold.surfelgi.surfelBuffer.IsValid(),"Surfel GI resources unavailable");
    if(test.gi==2)Check(render.rtSSGI.IsValid(),"SSGI resources unavailable");
    const auto image=folder/(view==0?"overview.png":"shelter.png");
    Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),image.generic_string()),"Native capture failed");
    if(view==1&&test.gi){
     for(unsigned f=0;f<120;++f){app.is_window_active=true;app.Run();}
     wi::graphics::GetDevice()->WaitForGPU();Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(folder/"shelter-late.png").generic_string()),"Late GI capture failed");
    }
    log<<test.name<<" view="<<view<<" sun_y="<<cold.weather.sunDirection.y<<" ambient="<<cold.weather.ambient.x<<","<<cold.weather.ambient.y<<","<<cold.weather.ambient.z
       <<" lights="<<cold.lights.GetCount()<<" moon_descriptor="<<cold.weather.moon_parameters.w<<" surfel="<<wi::renderer::GetSurfelGIEnabled()<<" ssgi="<<render.getSSGIEnabled()<<"\n";log.flush();
   }
   app.ActivatePath(nullptr);
  }
  cold.Clear();s.Clear();wi::graphics::GetDevice()->WaitForGPU();
  std::cout<<"FOLIAGE_LIGHTING_CASE_PASS "<<test.name<<std::endl;
 }
 while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
 log<<"FOLIAGE_LIGHTING_SAVE_REOPEN_NATIVE_CAPTURE_PASS\n";log.close();std::cout<<"FOLIAGE_LIGHTING_SAVE_REOPEN_NATIVE_CAPTURE_PASS"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
