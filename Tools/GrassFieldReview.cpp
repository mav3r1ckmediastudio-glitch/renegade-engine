// Large-area native hair coverage, distance and uncapped rendering probe.
// Usage: GrassFieldReview <CoverageReview/Main.wiscene> <output-directory>
#include "renegade/bridge/PlayerService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
using namespace DirectX;
using namespace wi::scene;
namespace fs=std::filesystem;
static void Check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
struct ProbeApp:wi::Application{
 wi::graphics::GPUQueryHeap queries;wi::graphics::GPUBuffer readback;bool measuring=false;
 void Render()override{
  auto*d=wi::graphics::GetDevice();
  if(measuring){auto c=d->BeginCommandList();d->QueryEnd(&queries,0,c);}
  wi::Application::Render();
  if(measuring){auto c=d->BeginCommandList();d->QueryEnd(&queries,1,c);d->QueryResolve(&queries,0,2,&readback,0,c);}
 }
 double GpuMs()const{auto*q=static_cast<const uint64_t*>(readback.mapped_data);Check(q&&q[1]>q[0],"Invalid GPU timestamps");return double(q[1]-q[0])*1000./wi::graphics::GetDevice()->GetTimestampFrequency();}
};
static double Quantile(std::vector<double>v,double q){std::sort(v.begin(),v.end());return v[size_t(q*(v.size()-1))];}
int main(int argc,char**argv){try{
 Check(argc==3,"usage: GrassFieldReview source.wiscene output-directory");
 const fs::path output=fs::u8path(argv[2]);fs::create_directories(output);
 HWND window=CreateWindowExW(0,L"STATIC",L"Native large grass probe",WS_POPUP,0,0,1920,1009,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 ProbeApp app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 auto*d=wi::graphics::GetDevice();auto sd=app.swapChain.GetDesc();sd.vsync=false;Check(d->CreateSwapChain(&sd,window,&app.swapChain),"Unable to disable VSync");app.setFrameRateLock(false);
 wi::graphics::GPUQueryHeapDesc qd;qd.query_count=2;Check(d->CreateQueryHeap(&qd,&app.queries),"Query creation failed");
 wi::graphics::GPUBufferDesc bd;bd.size=16;bd.usage=wi::graphics::Usage::READBACK;Check(d->CreateBuffer(&bd,nullptr,&app.readback),"Query readback failed");
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 Scene scene;LoadModel(scene,argv[1]);Check(scene.hairs.GetCount()==4,"Expected four reviewed hair prototypes");
 std::vector<wi::HairParticleSystem> prototypes;std::vector<MaterialComponent> materials;
 for(size_t i=0;i<4;++i){prototypes.push_back(scene.hairs[i]);materials.push_back(*scene.materials.GetComponent(scene.hairs.GetEntity(i)));}
 for(size_t i=0;i<scene.terrains.GetCount();++i){scene.terrains[i].Generation_Cancel();scene.terrains[i].SetGrassEnabled(false);}
 scene.hairs.Clear();
 // Remove review signs; keep the same terrain, sky and sun.
 std::vector<wi::ecs::Entity> signs;for(size_t i=0;i<scene.names.GetCount();++i)if(scene.names[i].name.rfind("plot-",0)==0)signs.push_back(scene.names.GetEntity(i));for(auto e:signs)scene.Entity_Remove(e);
 auto emitter=wi::ecs::CreateEntity();scene.transforms.Create(emitter);scene.names.Create(emitter)="64m grass field emitter";
 auto&m=scene.meshes.Create(emitter);m.vertex_positions={{-32,.025f,0},{-32,.025f,64},{32,.025f,0},{32,.025f,64}};m.vertex_normals.assign(4,{0,1,0});m.vertex_uvset_0={{0,0},{0,1},{1,0},{1,1}};m.indices={0,1,2,2,1,3};m.CreateRenderData();
 auto field=wi::ecs::CreateEntity();scene.transforms.Create(field);scene.names.Create(field)="Large meadow";
 CameraComponent camera;camera.CreatePerspective(1920,1009,.05f,300.f);
 wi::RenderPath3D render;render.scene=&scene;render.camera=&camera;render.init(1920,1009,96);render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);render.setOcclusionCullingEnabled(false);render.ResizeBuffers();app.ActivatePath(&render);
 struct Disarm{wi::Application&app;~Disarm(){app.ActivatePath(nullptr);}}disarm{app};
 std::ofstream csv(output/"field-timing.csv");csv<<"variant,view,area_m2,strands,gpu_render_median_ms,gpu_render_p95_ms,serial_frame_median_ms,serial_frame_p95_ms\n";
 const char*names[]={"bare","wicked25","open50","full25","full50"};
 for(unsigned variant=0;variant<5;++variant){
  d->WaitForGPU();scene.hairs.Clear();
  if(variant){
   auto&h=scene.hairs.Create(field);h=prototypes[variant-1];h.DeleteRenderData();h.meshID=emitter;h.indices.clear();h.vertex_lengths.assign(4,1.f);
   h.strandCount=uint32_t(4096*(variant==1||variant==3?25:50));h.viewDistance=100;h.uniformity=-1.f;h.randomSeed=17;h.SetDirty();
   auto&mat=scene.materials.Contains(field)?*scene.materials.GetComponent(field):scene.materials.Create(field);mat=materials[variant-1];mat.SetDirty();mat.CreateRenderData();
  }
  for(unsigned view=0;view<3;++view){
   const float distance[]={-3,-25,-60};const float targetZ[]={16,24,26};
   camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(0,1.65f,distance[view],1),XMVectorSet(0,view==0?.3f:.2f,targetZ[view],1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
   app.measuring=false;for(int f=0;f<75;++f){app.is_window_active=true;app.Run();d->WaitForGPU();}
   app.measuring=true;std::vector<double>gpu,wall;
   for(int f=0;f<90;++f){auto start=std::chrono::steady_clock::now();app.is_window_active=true;app.Run();d->WaitForGPU();gpu.push_back(app.GpuMs());wall.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
   app.measuring=false;
   const char*views[]={"near","25m","60m"};
   csv<<names[variant]<<","<<views[view]<<",4096,"<<(variant?scene.hairs[0].strandCount:0)<<","<<Quantile(gpu,.5)<<","<<Quantile(gpu,.95)<<","<<Quantile(wall,.5)<<","<<Quantile(wall,.95)<<"\n";csv.flush();
   Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(output/(std::string(names[variant])+"-"+views[view]+".png")).generic_string()),"Field capture failed");
   std::cout<<"FIELD_VIEW_PASS "<<names[variant]<<" "<<views[view]<<" gpu_ms="<<Quantile(gpu,.5)<<" p95="<<Quantile(gpu,.95)<<std::endl;
  }
  if(variant==4){
   auto start=renegade::bridge::ResolvePlayerStart(scene);Check(start.resolution==renegade::bridge::PlayerStartResolution::Success,"Missing player start");
   auto*t=scene.transforms.GetComponent(start.start.entity);t->ClearTransform();t->Translate(XMFLOAT3{0,.15f,-3});t->UpdateTransform();
   wi::Archive ar((output/"LargeMeadow.wiscene").generic_string(),false,false);scene.Serialize(ar);Check(ar.SaveFile((output/"LargeMeadow.wiscene").generic_string()),"Field save failed");
   Scene reopened;LoadModel(reopened,(output/"LargeMeadow.wiscene").generic_string());Check(reopened.hairs.GetCount()==1&&reopened.hairs[0].strandCount==204800&&reopened.hairs[0].atlas_rects.size()==6,"Field cold reopen failed");reopened.Clear();
  }
 }
 csv.close();app.ActivatePath(nullptr);scene.Clear();prototypes.clear();materials.clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);d->WaitForGPU();
 std::cout<<"LARGE_GRASS_FIELD_NATIVE_PASS resolution=1920x1009 vsync=off area=4096m2"<<std::endl;return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
