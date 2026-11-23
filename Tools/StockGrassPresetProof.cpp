// Native painter/history/persistence proof for shipped meadow choices.
#include "renegade/bridge/VegetationService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
using namespace DirectX;
using namespace renegade::bridge;
namespace fs=std::filesystem;
static void Check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
static wi::ecs::Entity Find(const wi::scene::Scene&s,const GrassAtlas&a){
 for(size_t i=0;i<s.hairs.GetCount();++i){auto e=s.hairs.GetEntity(i);auto*m=s.materials.GetComponent(e);if(m&&fs::u8path(m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name).filename()==fs::u8path(a.path).filename())return e;}
 return wi::ecs::INVALID_ENTITY;
}
static void Verify(const wi::scene::Scene&s,wi::ecs::Entity e,unsigned preset){
 auto*h=s.hairs.GetComponent(e);auto*m=s.materials.GetComponent(e);
 Check(h&&m&&h->strandCount>0&&h->atlas_rects.size()==6,"Missing six-frame painted meadow");
 Check(h->segmentCount==2&&h->billboardCount==2&&h->length==.5f&&h->width==.9f&&h->uniformity<0,"Lost meadow geometry");
 Check(std::abs(h->atlas_rects[0].texMulAdd.z-(preset?24.f:39.f)/1024)<1e-6,"Lost cropped sprite proportions");
 Check(std::abs(m->baseColor.x-(preset?.670896f:1.f))<1e-6,"Lost meadow colour calibration");
 Check(m->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid(),"Missing project-owned meadow texture");
}
static void VerifyGpu(const wi::HairParticleSystem&h){
 wi::vector<uint8_t>b;Check(wi::helper::saveBufferToMemory(h.generalBuffer,b),"GPU meadow readback failed");
 using UV=wi::scene::MeshComponent::Vertex_UVS;const size_t stride=(2+2*h.segmentCount)*h.billboardCount;
 Check(h.vb_uvs.offset+size_t(h.strandCount)*stride*sizeof(UV)<=b.size(),"Incomplete meadow geometry");
 std::vector<unsigned>counts(6);
 for(unsigned i=0;i<h.strandCount;++i){UV uv;std::memcpy(&uv,b.data()+h.vb_uvs.offset+size_t(i)*stride*sizeof(UV),sizeof(uv));int frame=-1;
  for(unsigned j=0;j<6;++j){auto&r=h.atlas_rects[j];if(std::abs(uv.uv0.x/65535.f-r.texMulAdd.z)<.0001f&&std::abs(uv.uv0.y/65535.f-(r.texMulAdd.w+r.texMulAdd.y))<.0001f){frame=int(j);break;}}
  Check(frame>=0,"GPU sampled outside the six cropped sprites");++counts[frame];
 }
 auto mm=std::minmax_element(counts.begin(),counts.end());Check(*mm.first>0&&*mm.second-*mm.first<=1,"Meadow variations are not evenly distributed");
 std::cout<<"STOCK_MEADOW_GPU_MIX min="<<*mm.first<<" max="<<*mm.second<<" strands="<<h.strandCount<<std::endl;
}
int main(int argc,char**argv){try{
 Check(argc==3,"usage: StockGrassPresetProof source.wiscene output-project");
 HWND w=CreateWindowExW(0,L"STATIC",L"Stock meadow proof",WS_POPUP,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(w);wi::initializer::InitializeComponentsImmediate();wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 const fs::path root=fs::u8path(argv[2]);fs::create_directories(root/"Content/Scenes");
 for(unsigned preset=0;preset<3;++preset){
  wi::scene::Scene scene;wi::scene::LoadModel(scene,argv[1]);Check(scene.terrains.GetCount()>0,"Missing terrain");
  auto&terrain=scene.terrains[0];terrain.Generation_Cancel();std::string error;
  Check(InitializeWickedVegetation(scene,terrain,&error),error);
  wi::scene::CameraComponent camera;camera.CreatePerspective(1280,800,.05f,200);camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(-6,1.8f,-3,1),XMVectorSet(-6,.2f,5,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
  wi::RenderPath3D render;render.scene=&scene;render.camera=&camera;render.init(1280,800,96);render.ResizeBuffers();app.ActivatePath(&render);
  struct Disarm{wi::Application&app;~Disarm(){app.ActivatePath(nullptr);}}disarm{app};
  for(int i=0;i<20;++i){app.is_window_active=true;app.Run();}
  auto kind=static_cast<StockGrassPreset>(preset);GrassAtlas atlas,again;
  Check(BuildStockGrassAtlas(root,kind,atlas,error),error);Check(BuildStockGrassAtlas(root,kind,again,error)&&atlas.path==again.path,"Stock adoption is not deterministic");
  Check(GrassPaletteEntries(root).size()==30,"Meadow presets changed existing palette membership");
  auto before=CaptureWickedVegetationSettings(terrain),settings=StockGrassPresetSettings(kind,before);
  ApplyWickedVegetationSettings(scene,terrain,settings);Check(std::abs(terrain.grass_density-settings.density)<.0001f,"Meadow density was clamped");
  SetWickedVegetationSettingsCommand setting(scene,terrain.terrainEntity,before,settings);setting.Undo();Check(std::abs(terrain.grass_density-before.density)<.0001f,"Settings Undo failed");Check(setting.Execute(),"Settings Redo failed");
  VegetationStrokeState old;auto result=PaintWickedVegetation(scene,terrain,{-6,0,4},5,VegetationBrushMode::Paint,&old,atlas);Check(result.changed,"Meadow paint failed: "+result.error);
  FinalizeWickedVegetationStroke(scene,terrain,old);auto after=CaptureWickedVegetationAfter(scene,terrain,old);
  WickedVegetationStrokeCommand stroke(scene,terrain.terrainEntity,std::move(old),std::move(after));
  Verify(scene,Find(scene,atlas),preset);stroke.Undo();Check(Find(scene,atlas)==wi::ecs::INVALID_ENTITY,"Paint Undo left a meadow layer");Check(stroke.Execute(),"Paint Redo failed");Verify(scene,Find(scene,atlas),preset);
  for(int i=0;i<60;++i){app.is_window_active=true;app.Run();}wi::graphics::GetDevice()->WaitForGPU();VerifyGpu(*scene.hairs.GetComponent(Find(scene,atlas)));
  Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(root/("preset-"+std::to_string(preset)+".png")).generic_string()),"Meadow native capture failed");
  const auto path=root/"Content/Scenes"/("Meadow-"+std::to_string(preset)+".wiscene");wi::Archive ar(path.generic_string(),false,false);scene.Serialize(ar);Check(ar.SaveFile(path.generic_string()),"Meadow save failed");
  wi::scene::Scene reopened;wi::scene::LoadModel(reopened,path.generic_string());Verify(reopened,Find(reopened,atlas),preset);
  app.ActivatePath(nullptr);reopened.Clear();scene.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
  std::cout<<"STOCK_MEADOW_PRESET_PAINT_HISTORY_REOPEN_PASS preset="<<preset<<std::endl;
 }
 std::cout<<"ALL_STOCK_MEADOW_PRESETS_PASS"<<std::endl;return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
