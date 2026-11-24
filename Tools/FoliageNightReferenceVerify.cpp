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
 Check(argc==2||argc==3,"usage: FoliageNightReferenceVerify approved-Main.wiscene [painted-stock-scene.wiscene]");
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
 if(argc==3){
  wi::scene::Scene painted;wi::scene::LoadModel(painted,argv[2]);
  const char* prefixes[]={"meadow-very-full-v2-","short-grass-v1-","dry-grass-v1-","seedhead-grass-v1-","low-clover-v1-","small-wildflowers-v1-"};
  bool appearanceMatches=true;
  for(size_t i=0;i<6;++i){
   const auto& approved=s.hairs[i];const auto* original=s.materials.GetComponent(s.hairs.GetEntity(i));
   const wi::HairParticleSystem* plant=nullptr;const wi::scene::MaterialComponent* material=nullptr;
   for(size_t j=0;j<painted.hairs.GetCount();++j){
    const auto* m=painted.materials.GetComponent(painted.hairs.GetEntity(j));if(!m)continue;
    const auto file=std::filesystem::u8path(m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name).filename().u8string();
    if(file.find(prefixes[i])==0){plant=&painted.hairs[j];material=m;break;}
   }
   Check(plant&&material,"Painted approved species missing");
   auto compare=[&](float a,float b,const char* field){if(!Near(a,b)){std::cerr<<"PLANT_REFERENCE_DIFF species="<<i<<" field="<<field<<" approved="<<a<<" painted="<<b<<std::endl;appearanceMatches=false;}};
   compare(approved.length,plant->length,"height");compare(approved.width,plant->width,"width");
   compare(approved.stiffness,plant->stiffness,"stiffness");compare(approved.drag,plant->drag,"drag");
   compare(approved.gravityPower,plant->gravityPower,"gravity");compare(approved.randomness,plant->randomness,"randomness");
   Check(approved.segmentCount==plant->segmentCount&&approved.billboardCount==plant->billboardCount&&approved.randomSeed==plant->randomSeed,"Painted topology/seed differs");
   compare(original->baseColor.x,material->baseColor.x,"red");compare(original->baseColor.y,material->baseColor.y,"green");compare(original->baseColor.z,material->baseColor.z,"blue");
   compare(original->alphaRef,material->alphaRef,"alpha clip");compare(original->roughness,material->roughness,"roughness");compare(original->reflectance,material->reflectance,"reflectance");compare(original->GetEmissiveStrength(),material->GetEmissiveStrength(),"emissive");
   Check(plant->atlas_rects.size()==approved.atlas_rects.size(),"Painted sprite count differs");
   for(size_t j=0;j<approved.atlas_rects.size();++j){
    const auto a=approved.atlas_rects[j].texMulAdd,b=plant->atlas_rects[j].texMulAdd;
    compare(a.x,b.x,"crop width");compare(a.y,b.y,"crop height");compare(a.z,b.z,"crop x");compare(a.w,b.w,"crop y");
   }
  }
  Check(appearanceMatches,"Painted plant differs from approved appearance");
  painted.Clear();std::cout<<"PAINTED_SIX_SPECIES_APPROVED_APPEARANCE_CONTRACT_PASS"<<std::endl;
 }
 s.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
 std::cout<<"APPROVED_FOLIAGE_NIGHT_REFERENCE_PASS embedded_foliage_atlases="<<embeddedBytes<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;std::cerr.flush();std::_Exit(1);}}
