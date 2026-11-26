// Private weather comparison over the unchanged approved foliage/night reference.
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/CloudFormationService.h"
#include "renegade/bridge/NightSkyService.h"
#include "renegade/bridge/RenderSettingsService.h"
#include "renegade/bridge/SunService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace renegade::bridge;
using namespace DirectX;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
 Check(argc==3,"usage: FoliageCloudReview approved.wiscene fresh-output");
 fs::path out=fs::u8path(argv[2]);Check(!fs::exists(out),"Use a fresh output");fs::create_directories(out);
 HWND window=CreateWindowExW(0,L"STATIC",L"Foliage cloud weather review",WS_POPUP,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 std::ofstream receipt(out/"receipt.txt");
 const char* names[]={"night-clouds","day-cloud-shadows","day-clouds-no-shadow","night-clouds-no-shadow"};
 for(unsigned mode=0;mode<4;++mode){
  const bool day=mode==1||mode==2,shadows=mode<2;
  const auto dir=out/names[mode];fs::create_directories(dir/"Content/Scenes");
  SceneService service;Check(service.LoadScene(argv[1]),service.LastError());auto& scene=service.GetScene();RestoreNightSky(scene);
  Check(scene.weathers.GetCount()==1&&scene.hairs.GetCount()==6,"Expected approved reference scene");
  const auto weatherEntity=scene.weathers.GetEntity(0);auto& w=scene.weathers[0];
  auto weather=CaptureWeather(w);weather.skyMode=WeatherState::SkyMode::RealisticWithClouds;
  weather.cloudCoverage=.52f;weather.cloudStartHeight=1200;weather.cloudThickness=3500;weather.cloudMovementSpeed=.65f;weather.cloudsCastShadow=shadows;
  ApplyWeather(w,weather);StoreCloudFormation(scene,weatherEntity,CloudFormation::Cumulus);
  auto settings=CaptureRenderSettings(scene);
  if(day){auto sun=CaptureSun(scene,weatherEntity);SetSunTime(sun,14.f);Check(ApplySun(scene,weatherEntity,sun),"Sun change failed");settings.exposure=1;(void)WriteRenderSettings(scene,settings);}
  const auto file=(dir/"Content/Scenes/Main.wiscene").generic_u8string();
  {wi::Archive archive(file,false,false);scene.Serialize(archive);Check(archive.SaveFile(file),"Cloud scene save failed");}
  SceneService reloaded;Check(reloaded.LoadScene(file),reloaded.LastError());auto& cold=reloaded.GetScene();RestoreNightSky(cold);RestoreCloudFormations(cold);
  auto saved=CaptureWeather(cold.weathers[0]);Check(saved.skyMode==WeatherState::SkyMode::RealisticWithClouds&&saved.cloudsCastShadow==shadows&&std::abs(saved.cloudCoverage-.52f)<.0001f,"Cloud settings did not persist");
  Check(cold.hairs.GetCount()==6&&cold.lights.GetCount()==scene.lights.GetCount(),"Scene contents changed");
  // Freeze cloud phase only in the paired native captures; the saved playable scenes move.
  cold.weathers[0].volumetricCloudParameters.animationMultiplier=0;
  wi::scene::CameraComponent camera;camera.CreatePerspective(1280,800,.05f,250);camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(0,2,-10,1),XMVectorSet(0,3,16,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
  wi::RenderPath3D render;render.scene=&cold;render.camera=&camera;render.init(1280,800,96);ApplyRenderSettingsToPath(render,settings,false);render.setOcclusionCullingEnabled(false);render.ResizeBuffers();app.ActivatePath(&render);
  struct Detach{wi::Application& app;~Detach(){app.ActivatePath(nullptr);}}detach{app};
  for(unsigned i=0;i<360;++i){app.is_window_active=true;app.Run();}
  wi::graphics::GetDevice()->WaitForGPU();Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(dir/"overview.png").generic_u8string()),"Cloud capture failed");
  receipt<<names[mode]<<" coverage="<<saved.cloudCoverage<<" cast_shadow="<<saved.cloudsCastShadow<<" exposure="<<settings.exposure<<" foliage_layers="<<cold.hairs.GetCount()<<"\n";receipt.flush();
  app.ActivatePath(nullptr);cold.Clear();scene.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
 }
 receipt<<"FOLIAGE_CLOUD_SAVE_REOPEN_NATIVE_VIEWS_PASS\n";std::cout<<"FOLIAGE_CLOUD_SAVE_REOPEN_NATIVE_VIEWS_PASS"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;std::cerr.flush();std::_Exit(1);}}
