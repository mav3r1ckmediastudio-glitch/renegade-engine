#include "RuntimeApplication.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace fs=std::filesystem;
static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
 std::cout.setf(std::ios::unitbuf);
 HWND window=CreateWindowExW(0,L"STATIC",L"Runtime late-scene graphics regression",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{
  renegade::bridge::SceneService scenes;
  renegade::runtime::RuntimeRenderPath path;path.BindScene(scenes,{});app.ActivatePath(&path);
  path.Update(1.f/60);path.PreRender();path.Render();wi::graphics::GetDevice()->SubmitCommandLists();
  const auto file=fs::temp_directory_path()/("renegade-late-scene-"+std::to_string(GetCurrentProcessId())+".wiscene");
  {
   wi::scene::Scene source;auto& weather=source.weathers.Create(wi::ecs::CreateEntity());
   weather.SetRealisticSky(true);weather.SetVolumetricClouds(true);
   wi::Archive archive(file.generic_u8string(),false,false);source.Serialize(archive);
   Require(archive.SaveFile(file.generic_u8string()),"could not save native cloud fixture");archive=wi::Archive();
  }
  // This is the actual application ordering on a Start click: native Update
  // completed for the menu, then the action adopts the cloud Level.
  Require(scenes.LoadScene(file.generic_u8string()),"could not adopt late cloud scene");
  Require(scenes.GetScene().weather.IsVolumetricClouds(),"clouds must remain enabled");
  Require(!scenes.GetScene().cloudmap.IsValid(),"negative control unexpectedly preallocated the cloud map");
  std::cout<<"LATE_SCENE_BEFORE_RENDER cloudmap=0 clouds=1\n";
  path.PreRender();path.Render();wi::graphics::GetDevice()->SubmitCommandLists();
  Require(!scenes.GetScene().cloudmap.IsValid(),"transition fence advanced the scene outside normal Update");
  std::cout<<"LATE_SCENE_FENCE_PASS\n";
  for(int frame=0;frame<4;++frame){
   path.Update(1.f/60);path.PreRender();path.Render();wi::graphics::GetDevice()->SubmitCommandLists();
   Require(scenes.GetScene().cloudmap.IsValid()&&scenes.GetScene().cloudmap_variance.IsValid(),"normal Update did not create both cloud resources");
   Require(scenes.GetScene().weather.IsVolumetricClouds(),"repair disabled authored clouds");
  }
  wi::graphics::GetDevice()->WaitForGPU();
  while(wi::renderer::IsPipelineCreationActive())Sleep(10);
  std::error_code ignored;fs::remove(file,ignored);
  std::cout<<"PASS: real RuntimeRenderPath waits for its scene update and then renders enabled volumetric clouds\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<std::endl;return 1;}
}
