#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/RiverSplineService.h"
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <iostream>
using namespace renegade::bridge;using namespace DirectX;namespace fs=std::filesystem;
static void Check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
struct Path:wi::RenderPath3D {
 SceneService* service=nullptr;RiverFlowRenderer flow;uint64_t edit=0;
 bool measuring=false;mutable unsigned measured=0;double prepareMilliseconds=0;unsigned prepares=0;
 wi::graphics::GPUQueryHeap queries;wi::graphics::GPUBuffer results;
 void Update(float dt)override{
  auto start=std::chrono::steady_clock::now();flow.Prepare(*scene,service->Revision(),dt,edit);
  if(measuring){prepareMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();++prepares;}
  {auto bindings=flow.Bind();wi::RenderPath3D::Update(dt);}
 }
 void Render()const override{
  auto bindings=flow.Bind();auto* gpu=wi::graphics::GetDevice();
  if(measuring){auto cmd=gpu->BeginCommandList();gpu->QueryEnd(&queries,measured*2,cmd);}
  flow.Dispatch();
  if(measuring){auto cmd=gpu->BeginCommandList();gpu->QueryEnd(&queries,measured*2+1,cmd);
   gpu->QueryResolve(&queries,measured*2,2,&results,measured*2*sizeof(uint64_t),cmd);++measured;}
  wi::RenderPath3D::Render();
 }
};
int main(int argc,char** argv){
 if(argc!=3)return 2;std::cout.setf(std::ios::unitbuf);auto out=fs::absolute(argv[1]);fs::create_directories(out);
 HWND window=CreateWindowExW(0,L"STATIC",L"Renegade River V2 G6 scalability proof",WS_OVERLAPPEDWINDOW,50,50,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;
 app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();auto* gpu=wi::graphics::GetDevice();
 try{
  wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);SceneService service;
  Check(service.LoadScene(fs::absolute(argv[2]).generic_u8string()),service.LastError());auto& scene=service.GetScene();
  auto original=scene.Entity_FindByName("River Water 5");auto optics=*scene.materials.GetComponent(original);
  RiverFlowProfile p;std::string error;Check(ReadRiverFlowProfile(scene,original,p,error),error);p.enabled=false;Check(SetRiverFlowProfileCommand(scene,original,p).Execute(),"Disable source");
  std::vector<wi::ecs::Entity> rocks;
  for(size_t k=0;k<scene.objects.GetCount();++k){auto e=scene.objects.GetEntity(k);if(e==original)continue;
   auto* mesh=scene.meshes.GetComponent(scene.objects[k].meshID);if(!mesh)continue;
   if(mesh->indices.size()>3000&&mesh->indices.size()<15000)rocks.push_back(e);
  }
  Check(!rocks.empty(),"Static rock fixture missing");std::vector<wi::ecs::Entity> rivers{original};
  for(int n=1;n<16;++n){
   float offset=n*12.f;RiverSplineOptions options;options.widthMeters=4;options.surfaceOffsetMeters=0;
   std::vector<XMFLOAT3> points{{-20,.15f,70.5f+offset},{0,.15f,68+offset},{20,.15f,70.5f+offset}};
   CreateRiverSplineCommand curve(scene,points,options);Check(curve.Execute(),"Create stress river");
   auto river=curve.CreatedEntity();*scene.materials.GetComponent(river)=optics;scene.materials.GetComponent(river)->SetDirty();rivers.push_back(river);
   for(auto rock:rocks){
    auto clone=scene.Entity_Duplicate(rock);XMMATRIX parent=XMMatrixIdentity();
    if(const auto* h=scene.hierarchy.GetComponent(clone))parent=scene.ComputeEntityMatrixRecursive(h->parentID);
    auto world=scene.ComputeEntityMatrixRecursive(rock)*XMMatrixTranslation(0,.3f,offset);
    XMVECTOR scale,rotation,translation;Check(XMMatrixDecompose(&scale,&rotation,&translation,world*XMMatrixInverse(nullptr,parent)),"Stress obstacle transform");
    auto* t=scene.transforms.GetComponent(clone);XMStoreFloat3(&t->scale_local,scale);XMStoreFloat4(&t->rotation_local,rotation);XMStoreFloat3(&t->translation_local,translation);t->SetDirty();t->UpdateTransform();
   }
  }
  Path path;path.scene=&scene;path.service=&service;wi::scene::CameraComponent camera;
  camera.CreatePerspective(1264,761,.05f,1000,XMConvertToRadians(60));path.camera=&camera;
  XMFLOAT3 eye(0,190,158),target(0,0,158);camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMLoadFloat3(&eye),XMLoadFloat3(&target),XMVectorSet(0,0,1,0))));camera.UpdateCamera();
  path.init(1264,761,96);path.ResizeBuffers();path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);
  struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);wi::graphics::GetDevice()->WaitForGPU();}}guard{app};
  wi::graphics::GPUQueryHeapDesc qd;qd.type=wi::graphics::GpuQueryType::TIMESTAMP;qd.query_count=300;
  Check(gpu->CreateQueryHeap(&qd,&path.queries),"GPU timestamp heap");
  wi::graphics::GPUBufferDesc rd;rd.usage=wi::graphics::Usage::READBACK;rd.size=300*sizeof(uint64_t);
  Check(gpu->CreateBuffer(&rd,nullptr,&path.results),"GPU timestamp readback");
  app.ActivatePath(&path);for(int k=0;k<240;++k)app.Run();
  unsigned round=0;
  for(int count:{0,1,4,16,4,1,0}){
   gpu->WaitForGPU();auto start=std::chrono::steady_clock::now();
   for(int n=0;n<16;++n){RiverFlowProfile profile;profile.enabled=n<count;Check(SetRiverFlowProfileCommand(scene,rivers[n],profile).Execute(),"Stress profile");}
   ++path.edit;for(int k=0;k<5;++k)app.Run();
   Check(path.flow.ActiveCount()==size_t(count),"Stress active river count");
   double setup=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   std::vector<uint64_t> rebuilds;
   for(int n=0;n<count;++n){RiverFlowDiagnostics d;Check(path.flow.Diagnostics(rivers[n],d,error),error);rebuilds.push_back(d.rebuilds);}
   for(int k=0;k<120;++k)app.Run();gpu->WaitForGPU();
   path.measured=0;path.prepareMilliseconds=0;path.prepares=0;path.measuring=true;
   start=std::chrono::steady_clock::now();for(int k=0;k<150;++k)app.Run();
   double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
   path.measuring=false;gpu->WaitForGPU();Check(path.measured==150&&path.prepares==150,"Profiling frame count");
   auto* ticks=static_cast<const uint64_t*>(path.results.mapped_data);double milliseconds=0;
   for(unsigned k=0;k<150;++k){Check(ticks[k*2+1]>=ticks[k*2],"GPU timestamp ordering");milliseconds+=double(ticks[k*2+1]-ticks[k*2])*1000.0/gpu->GetTimestampFrequency();}
   std::cout<<"G6_PROFILE round="<<round++<<" active="<<count<<" prepare_cpu_ms="<<path.prepareMilliseconds/150<<" foam_graphics_queue_ms="<<milliseconds/150<<"\n";
   for(int n=0;n<count;++n){RiverFlowDiagnostics d;Check(path.flow.Diagnostics(rivers[n],d,error,true),error);Check(d.rebuilds==rebuilds[n],"Static stress source recaptured");Check(d.foamMass>0,"Stress contact foam missing");}
   gpu->WaitForGPU();Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/("rivers-"+std::to_string(count)+".png")).generic_u8string()),"Stress capture");
   std::cout<<"G6_STRESS active="<<count<<" total=16 copiedRockClusters="<<rocks.size()*16<<" setup_ms="<<setup<<" fps="<<150/seconds<<" static_rebuilds=0\n";
  }
  Check(wi::renderer::GetShaderErrorCount()==0,"Stress shader errors");std::cout<<"G6_STRESS=PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<"G6_STRESS_FAIL="<<e.what()<<"\n";return 1;}
}
