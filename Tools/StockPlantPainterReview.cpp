// Actual Terrain painter review: independent stock plants, native breeze and saved night GI.
#include "renegade/bridge/VegetationService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/RenderSettingsService.h"
#include "renegade/bridge/NightSkyService.h"
#include "renegade/bridge/SunService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <limits>
#include <cfloat>
#include <algorithm>
using namespace renegade::bridge;
using namespace DirectX;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
static std::vector<uint8_t> Positions(const wi::HairParticleSystem& hair){
 wi::vector<uint8_t> bytes;Check(wi::helper::saveBufferToMemory(hair.generalBuffer,bytes),"Plant position readback failed");
 const auto& span=hair.vb_pos[0];Check(span.size&&span.offset+span.size<=bytes.size(),"Invalid plant position span");
 return {bytes.begin()+span.offset,bytes.begin()+span.offset+span.size};
}
int main(int argc,char** argv){try{
 Check(argc==3,"usage: StockPlantPainterReview approved-night.wiscene fresh-output-project");
 const fs::path root=fs::u8path(argv[2]);Check(!fs::exists(root),"Use a fresh review output");fs::create_directories(root/"Content/Scenes");
 HWND window=CreateWindowExW(0,L"STATIC",L"Stock plant painter review",WS_POPUP,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 SceneService service;Check(service.LoadScene(argv[1]),service.LastError());auto& scene=service.GetScene();RestoreNightSky(scene);
 Check(scene.terrains.GetCount()==1&&scene.hairs.GetCount()==6,"Expected approved six-plant reference");
 {
  wi::scene::CameraComponent c;c.CreatePerspective(1280,800,.05f,250);c.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(0,2,-14,1),XMVectorSet(0,.4f,10,1),XMVectorSet(0,1,0,0))));c.UpdateCamera();
  wi::RenderPath3D warm;warm.scene=&scene;warm.camera=&c;warm.init(1280,800,96);ApplyRenderSettingsToPath(warm,CaptureRenderSettings(scene),false);warm.ResizeBuffers();app.ActivatePath(&warm);
  for(unsigned i=0;i<30;++i){app.is_window_active=true;app.Run();}
  wi::graphics::GetDevice()->WaitForGPU();Check(wi::helper::saveTextureToFile(warm.GetRenderResult3D(),(root/"reference-before-paint.png").generic_u8string()),"Prepaint capture failed");app.ActivatePath(nullptr);
 }
 scene.hairs.Clear();auto& terrain=scene.terrains[0];terrain.Generation_Cancel();
 std::string error;Check(InitializeWickedVegetation(scene,terrain,&error),error);Check(IsWickedVegetationInitialized(scene,terrain),"Native terrain identity not initialized");
 std::ofstream report(root/"painter-review-receipt.txt");
 std::vector<GrassAtlas> atlases(StockGrassPresetCount);
 std::vector<wi::ecs::Entity> plants;
 std::vector<XMFLOAT3> surface;
 float minX=FLT_MAX,maxX=-FLT_MAX,minZ=FLT_MAX,maxZ=-FLT_MAX,minY=FLT_MAX,maxY=-FLT_MAX;
 for(const auto& entry:terrain.chunks){const auto* mesh=scene.meshes.GetComponent(entry.second.entity);const auto* transform=scene.transforms.GetComponent(entry.second.entity);if(!mesh||!transform)continue;
  for(const auto& p:mesh->vertex_positions){XMFLOAT3 world;XMStoreFloat3(&world,XMVector3TransformCoord(XMLoadFloat3(&p),transform->GetWorldMatrix()));surface.push_back(world);minX=std::min(minX,world.x);maxX=std::max(maxX,world.x);minZ=std::min(minZ,world.z);maxZ=std::max(maxZ,world.z);minY=std::min(minY,world.y);maxY=std::max(maxY,world.y);}}
 Check(maxX>minX&&maxZ>minZ,"Missing finite terrain bounds");const float floorY=(minY+maxY)*.5f;
 std::cout<<"PAINTER_TERRAIN_BOUNDS x="<<minX<<","<<maxX<<" z="<<minZ<<","<<maxZ<<" y="<<floorY<<std::endl;
 const float centreX=(minX+maxX)*.5f,centreZ=(minZ+maxZ)*.5f;
 const float spanX=std::min(64.f,maxX-minX),spanZ=std::min(40.f,maxZ-minZ);
 minZ=centreZ-spanZ*.5f;
 const float radius=std::min({4.75f,spanX/10,spanZ/5});
 float x[8],z[8],y[8];for(unsigned i=0;i<8;++i){
  x[i]=centreX+spanX*((float(i%4)+.5f)/4-.5f);z[i]=centreZ+spanZ*(i<4?-.25f:.25f);
  float nearest=FLT_MAX;for(const auto& p:surface){const float d=(p.x-x[i])*(p.x-x[i])+(p.z-z[i])*(p.z-z[i]);if(d<nearest){nearest=d;y[i]=p.y;}}
 }
 for(unsigned i=0;i<StockGrassPresetCount;++i){
  Check(BuildStockGrassAtlas(root,StockGrassPreset(i),atlases[i],error),error);
  VegetationStrokeState before;const auto painted=PaintWickedVegetation(scene,terrain,{x[i],y[i],z[i]},radius,VegetationBrushMode::Paint,&before,atlases[i]);
  std::cout<<"PLOT "<<i<<" x="<<x[i]<<" y="<<y[i]<<" z="<<z[i]<<" vertices="<<painted.affectedVertices<<" error="<<painted.error<<std::endl;
  Check(painted.changed,"Empty stock paint plot");
  FinalizeWickedVegetationStroke(scene,terrain,before);
  size_t strands=0;
  for(size_t j=0;j<scene.hairs.GetCount();++j){auto e=scene.hairs.GetEntity(j);const auto* m=scene.materials.GetComponent(e);
   if(m&&fs::u8path(m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name).filename()==fs::u8path(atlases[i].path).filename())
   {strands+=scene.hairs[j].strandCount;plants.push_back(e);}}
  Check(strands>0,"Stock plot has no strands");report<<"preset="<<i<<" strands="<<strands<<"\n";
 }
 Check(scene.weathers.GetCount()==1,"Expected one sky");auto& weather=scene.weathers[0];
 weather.windDirection={.08f,0,0};weather.windSpeed=1;weather.windRandomness=5;
 const auto start=ResolvePlayerStart(scene);Check(start.resolution==PlayerStartResolution::Success,"Missing Player Start");
 auto* pose=scene.transforms.GetComponent(start.start.entity);pose->ClearTransform();pose->Translate(XMFLOAT3{centreX,floorY+.15f,minZ-4});pose->UpdateTransform();
 const auto file=(root/"Content/Scenes/Main.wiscene").generic_u8string();
 {wi::Archive archive(file,false,false);scene.Serialize(archive);Check(archive.SaveFile(file),"Painter review save failed");}
 SceneService reopened;Check(reopened.LoadScene(file),reopened.LastError());auto& cold=reopened.GetScene();RestoreNightSky(cold);
 Check(cold.hairs.GetCount()==scene.hairs.GetCount(),"Painter reopen lost a layer");
 plants.clear();
 for(size_t i=0;i<cold.hairs.GetCount();++i){const auto e=cold.hairs.GetEntity(i);const auto* m=cold.metadatas.GetComponent(e);if(m&&m->float_values.has(PlantDensityKey)&&cold.hairs[i].strandCount>0)plants.push_back(e);}
 Check(plants.size()>=StockGrassPresetCount,"Cold stock layer identities missing");
 const auto settings=CaptureRenderSettings(cold);Check(settings.surfelGiEnabled&&settings.exposure==3,"Approved night GI/exposure changed");
 wi::scene::CameraComponent camera;camera.CreatePerspective(1280,800,.05f,250);
 wi::RenderPath3D render;render.scene=&cold;render.camera=&camera;render.init(1280,800,96);ApplyRenderSettingsToPath(render,settings,false);render.setOcclusionCullingEnabled(false);render.ResizeBuffers();
 app.ActivatePath(&render);struct Detach{wi::Application& app;~Detach(){app.ActivatePath(nullptr);}}detach{app};
 auto frame=[&]{render.PreUpdate();render.Update(1.f/75);render.PreRender();render.Render();wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();};
 auto view=[&](float px,float py,float pz,float tx,float ty,float tz){camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(px,py,pz,1),XMVectorSet(tx,ty,tz,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();};
 auto capture=[&](const std::string& name,unsigned warm){for(unsigned i=0;i<warm;++i)frame();wi::graphics::GetDevice()->WaitForGPU();Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(root/(name+".png")).generic_u8string()),"Painter capture failed");};
 view(centreX,floorY+2,minZ-8,centreX,floorY+.4f,centreZ);capture("night-overview",360);
 for(unsigned i=0;i<StockGrassPresetCount;++i){view(x[i],y[i]+(i==6?.32f:.7f),z[i]-std::min(3.f,radius),x[i],y[i]+.12f,z[i]+radius*.5f);capture("night-plant-"+std::to_string(i),120);}
 view(centreX,floorY+2,minZ-8,centreX,floorY+.4f,centreZ);for(unsigned i=0;i<90;++i)frame();wi::graphics::GetDevice()->WaitForGPU();
 std::vector<std::vector<uint8_t>> old;
 for(auto e:plants){const auto* h=cold.hairs.GetComponent(e);Check(h&&!h->indices.empty(),"Native emitter lost its distribution");old.push_back(Positions(*h));}
 for(unsigned i=0;i<60;++i)frame();wi::graphics::GetDevice()->WaitForGPU();
 for(size_t i=0;i<plants.size();++i)Check(Positions(*cold.hairs.GetComponent(plants[i]))!=old[i],"Native breeze left a stock species static");
 report<<"ALL_EIGHT_STOCK_NATIVE_POSITION_SPANS_MOVED_PASS\n";
 view(centreX,floorY+3,minZ-25,centreX,floorY+.3f,centreZ);capture("night-25m",150);view(centreX,floorY+4,minZ-60,centreX,floorY+.3f,centreZ);capture("night-60m",150);
 auto sun=CaptureSun(cold,cold.weathers.GetEntity(0));SetSunTime(sun,12.f);ApplySun(cold,cold.weathers.GetEntity(0),sun);auto day=CaptureRenderSettings(cold);day.surfelGiEnabled=false;day.exposure=1;ApplyRenderSettingsToPath(render,day,false);
 view(centreX,floorY+2,minZ-8,centreX,floorY+.4f,centreZ);capture("day-overview",150);view(centreX,floorY+3,minZ-25,centreX,floorY+.3f,centreZ);capture("day-25m",90);view(centreX,floorY+4,minZ-60,centreX,floorY+.3f,centreZ);capture("day-60m",90);
 report<<"STOCK_PAINTER_NATIVE_DAY_NIGHT_NEAR_DISTANCE_CAPTURE_PASS\n";report.flush();
 app.ActivatePath(nullptr);cold.Clear();scene.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
 std::cout<<"STOCK_PAINTER_REVIEW_SAVE_REOPEN_WIND_DISTANCE_PASS"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;std::cerr.flush();std::_Exit(1);}}
