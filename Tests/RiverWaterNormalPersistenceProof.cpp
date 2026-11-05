#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/RiverSplineService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <map>
using namespace renegade::bridge;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
static std::size_t CheckWater(wi::scene::Scene& scene){
 std::size_t count=0;
 for(std::size_t i=0;i<scene.splines.GetCount();++i){
  const auto id=scene.splines.GetEntity(i);auto* m=scene.materials.GetComponent(id);
  if(!m||m->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
  const auto& n=m->textures[wi::scene::MaterialComponent::NORMALMAP];
  Check(n.resource.IsValid()&&n.resource.GetTexture().IsValid(),"Water normal failed to hydrate");
  Check(m->normalMapStrength==1.f&&m->texAnimDirection.x==0.f&&m->texAnimDirection.y==-.001f&&m->texAnimFrameRate==30.f,"Authored water settings changed");++count;
 }
 Check(count==5,"Expected five owner-fixture water splines");return count;
}
int main(int argc,char** argv){
 if(argc!=4){std::cerr<<"Usage: proof disposable-project output-directory warm|cold\n";return 2;}
 try{
  const fs::path root=fs::u8path(argv[1]),out=fs::u8path(argv[2]);fs::create_directories(out);
  Check(fs::is_regular_file(root/".river-normal-proof"),"Only disposable projects accepted");
  HWND window=CreateWindowExW(0,L"STATIC",L"River normal persistence proof",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  wi::Application app;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
  struct Drain{~Drain(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}drain;
  const auto saved=root/"Content/Scenes/NormalRecovered.wiscene";
  if(std::string(argv[3])=="missing"){
   SceneService runtime;const auto sentinel=runtime.GetScene().Entity_CreateCube("Keep active scene");
   Check(!runtime.LoadScene(saved.generic_string()),"Missing bundled map silently accepted");
   Check(runtime.GetScene().objects.Contains(sentinel),"Failed load replaced active document");
   std::cout<<"MISSING_BUNDLE_REFUSAL_AND_ACTIVE_DOCUMENT_PRESERVATION PASS\n";return 0;
  }
  if(std::string(argv[3])=="cold"){
   SceneService runtime;Check(runtime.LoadScene(saved.generic_string()),runtime.LastError());
   std::cout<<"COLD_RUNTIME_NORMALS="<<CheckWater(runtime.GetScene())<<" PASS\n";return 0;
  }
  StudioSession session;session.Projects().Initialize((out/"settings.ini").generic_string());
  Check(session.Projects().OpenProject((root/"New Renegade Project.renegade").generic_string()),"Project open failed");
  Check(session.LoadScene((root/"Content/Scenes/Starter.wiscene").generic_string()),session.Scenes().LastError());
  auto& scene=session.Scenes().GetScene();CheckWater(scene);
  // Missing custom/empty bindings must stay untouched. Valid stock maps stay bound.
  const auto id=scene.splines.GetEntity(0);auto& m=*scene.materials.GetComponent(id);
  const auto original=m.textures[wi::scene::MaterialComponent::NORMALMAP];
  const auto color=m.baseColor,uv=m.texMulAdd;const float roughness=m.roughness,refraction=m.refraction;
  std::string error;
  auto& normal=m.textures[wi::scene::MaterialComponent::NORMALMAP];
  normal.name="creator/custom-ripple.png";normal.resource={};
  Check(RestoreBundledRiverWaterNormals(scene,error),error);Check(normal.name=="creator/custom-ripple.png"&&!normal.resource.IsValid(),"Custom missing binding replaced");
  normal.name.clear();Check(RestoreBundledRiverWaterNormals(scene,error),error);Check(normal.name.empty()&&!normal.resource.IsValid(),"Intentional empty binding replaced");
  normal=original;
  Check(RestoreBundledRiverWaterNormals(scene,error),error);
  Check(normal.resource.GetTexture().internal_state==original.resource.GetTexture().internal_state,"Valid stock binding changed");
  normal.name="old-machine/waterNormalMap.jpg";normal.resource={};
  Check(RestoreBundledRiverWaterNormals(scene,error),error);CheckWater(scene);
  Check(m.baseColor.x==color.x&&m.baseColor.y==color.y&&m.baseColor.z==color.z&&m.baseColor.w==color.w&&m.texMulAdd.x==uv.x&&m.texMulAdd.y==uv.y&&m.texMulAdd.z==uv.z&&m.texMulAdd.w==uv.w&&m.roughness==roughness&&m.refraction==refraction,"Water optics or UVs changed");
  Check(session.SaveScene(saved.generic_string()),session.Scenes().LastError());
  Check(session.LoadScene(saved.generic_string()),session.Scenes().LastError());CheckWater(session.Scenes().GetScene());
  std::cout<<"STUDIO_SAVE_REOPEN_NORMALS=5 CUSTOM_EMPTY_VALID_BINDINGS_PRESERVED PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
