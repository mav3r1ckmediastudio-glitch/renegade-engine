// Verifies the saved owner-approved visual reference, including embedded artwork.
#include "renegade/bridge/RenderSettingsService.h"
#include "renegade/bridge/NightSkyService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include "../EngineBridge/src/WisceneEmbeddedResources.h"
#include <unordered_set>
using namespace renegade::bridge;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool Near(float a,float b){return std::abs(a-b)<.00001f;}
int main(int argc,char** argv){try{
 Check(argc==2,"usage: FoliageNightReferenceVerify approved-Main.wiscene");
 HWND window=CreateWindowExW(0,L"STATIC",L"Approved foliage night verification",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 std::unordered_set<std::string> names,embeddedNames;
 {wi::Archive archive(argv[1],true);Check(ReadWisceneEmbeddedNames(archive.GetData(),archive.GetSize(),archive.GetPos(),archive.GetVersion(),archive.GetSourceDirectory(),names),"Invalid embedded resource table");}
 for(const auto& name:names)embeddedNames.insert(std::filesystem::u8path(name).lexically_normal().generic_string());
 wi::scene::Scene s;wi::scene::LoadModel(s,argv[1]);RestoreNightSky(s);
 const auto state=CaptureRenderSettings(s);
 Check(state.surfelGiEnabled&&!state.ssgiEnabled&&Near(state.exposure,3)&&Near(state.giBoost,1),"Approved GI/exposure changed");
 Check(!state.eyeAdaptationEnabled&&!state.bloomEnabled&&!state.motionBlurEnabled&&!state.depthOfFieldEnabled&&state.antiAliasing==AntiAliasingMode::Off,"Approved image processing changed");
 Check(s.hairs.GetCount()==6&&s.terrains.GetCount()>0&&s.weathers.GetCount()==1&&s.lights.GetCount()==3,"Approved scene contents changed");
 const auto& w=s.weathers[0];
 Check(Near(w.ambient.x,0)&&Near(w.ambient.y,0)&&Near(w.ambient.z,0)&&w.IsRealisticSky(),"Approved sky/ambient changed");
 auto moon=CaptureMoon(s,s.weathers.GetEntity(0));
 Check(moon.enabled&&!moon.automatic&&Near(moon.phase,.5f)&&Near(moon.brightness,.7f)&&Near(moon.azimuth,-35)&&Near(moon.elevation,35),"Approved sky moon changed");
 const float heights[]={.5f,.25f,.45f,.65f,.12f,.30f};const unsigned counts[]={3600,1800,1800,1080,1440,288};
 size_t embeddedBytes=0;
 for(size_t i=0;i<6;++i){
  const auto& h=s.hairs[i];const auto* m=s.materials.GetComponent(s.hairs.GetEntity(i));
  Check(h.strandCount==counts[i]&&Near(h.length,heights[i])&&Near(h.width,.9f)&&h.segmentCount==2&&h.billboardCount==2&&h.atlas_rects.size()==6,"Approved foliage geometry changed");
  Check(m&&Near(m->alphaRef,.5f)&&Near(m->roughness,.9f)&&Near(m->reflectance,.02f)&&Near(m->GetEmissiveStrength(),0),"Approved foliage material changed");
  const auto& resource=m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource;
  Check(resource.GetTexture().IsValid()&&embeddedNames.count(std::filesystem::u8path(m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name).lexically_normal().generic_string()),"Approved foliage artwork is not embedded/loaded");
  ++embeddedBytes;
  Check(Near(m->baseColor.x,i?1.f:.670896f)&&Near(m->baseColor.y,i?1.f:.660796f)&&Near(m->baseColor.z,i?1.f:.778305f),"Approved foliage colour calibration changed");
 }
 bool sun=false,lunar=false,lantern=false;
 for(size_t i=0;i<s.lights.GetCount();++i){
  auto* name=s.names.GetComponent(s.lights.GetEntity(i));Check(name,"Unnamed reference light");const auto& l=s.lights[i];
  if(name->name=="Sun"){Check(l.GetType()==wi::scene::LightComponent::DIRECTIONAL&&Near(l.intensity,5.5f),"Approved Sun changed");sun=true;}
  else if(name->name=="Authored Cold Moonlight preset"){Check(l.GetType()==wi::scene::LightComponent::DIRECTIONAL&&Near(l.intensity,.5f)&&Near(l.color.x,.56f)&&Near(l.color.y,.69f)&&Near(l.color.z,1),"Approved world moonlight changed");lunar=true;}
  else if(name->name=="Warm test lantern"){Check(l.GetType()==wi::scene::LightComponent::POINT&&Near(l.intensity,12)&&Near(l.color.x,1)&&Near(l.color.y,.714f)&&Near(l.color.z,.361f),"Approved lantern changed");lantern=true;}
 }
 Check(sun&&lunar&&lantern,"Approved reference lights missing");
 s.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
 std::cout<<"APPROVED_FOLIAGE_NIGHT_REFERENCE_PASS embedded_foliage_atlases="<<embeddedBytes<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;std::cerr.flush();std::_Exit(1);}}
