#include "renegade/bridge/SceneService.h"
#include "RiverFoamTransport.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
using namespace DirectX;using namespace renegade::bridge;namespace fs=std::filesystem;
static void Check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
static uint32_t Bits(float v){uint32_t b;std::memcpy(&b,&v,4);return b;}
struct Params {XMFLOAT4 bounds;XMUINT4 control;XMFLOAT4 settings;XMUINT4 foamControl;XMFLOAT4 foamSettings;};
static_assert(sizeof(Params)==80);
struct SimPush {XMUINT4 resolution;XMFLOAT4 bounds,settings;};
static_assert(sizeof(SimPush)==48);
struct FoamPath:wi::RenderPath3D {
    bool frozen=false,active=false,paused=true,enabled=false,emission=true;
    float clock=0,manualStep=0,step=0,lifetime=3.2f,turbulence=1;
    unsigned state=0;
    wi::ecs::Entity river=wi::ecs::INVALID_ENTITY;
    wi::graphics::Texture foam[2],velocity,source,baseVelocity;
    wi::graphics::Shader simulation;
    XMFLOAT4 bounds{};
    void Update(float dt)override{
        step=active?(paused?manualStep:std::min(dt,1.f/30)):0;manualStep=0;clock+=step;
        if(active)state=1-state;
        if(enabled){auto* water=scene->materials.GetComponent(river);water->userdata.z=state;water->userdata.w=Bits(clock);water->SetDirty();}
        wi::RenderPath3D::Update(frozen?0.f:dt);
    }
    void Render()const override{
        if(!active){wi::RenderPath3D::Render();return;}
        using namespace wi::graphics;auto* gpu=GetDevice();auto cmd=gpu->BeginCommandList();
        GPUBarrier first=GPUBarrier::Image(&foam[state],ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);
        gpu->Barrier(&first,1,cmd);wi::renderer::BindCommonResources(cmd);
        gpu->BindComputeShader(&simulation,cmd);
        gpu->BindResource(&foam[1-state],0,cmd);gpu->BindResource(&velocity,1,cmd);gpu->BindResource(&source,2,cmd);gpu->BindResource(&baseVelocity,3,cmd);
        gpu->BindUAV(&foam[state],0,cmd);
        const auto& desc=foam[state].GetDesc();
        SimPush push{{desc.width,desc.height,Bits(turbulence),0},bounds,{step,clock,lifetime,emission?1.f:0.f}};
        gpu->PushConstants(&push,sizeof(push),cmd);gpu->Dispatch((desc.width+7)/8,(desc.height+7)/8,1,cmd);
        GPUBarrier end=GPUBarrier::Image(&foam[state],ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);
        gpu->Barrier(&end,1,cmd);
        wi::RenderPath3D::Render();
    }
};
static std::unordered_set<wi::ecs::Entity> TerrainChunks(const wi::scene::Scene& scene){
    std::unordered_set<wi::ecs::Entity> ids;
    for(size_t i=0;i<scene.terrains.GetCount();++i)for(const auto& entry:scene.terrains[i].chunks)ids.insert(entry.second.entity);
    return ids;
}
static std::vector<river_g4::Surface> Gather(const wi::scene::Scene& scene,wi::ecs::Entity river,
    const RiverFlowField& flow,std::vector<wi::ecs::Entity>* rocks=nullptr){
    auto terrain=TerrainChunks(scene);std::vector<river_g4::Surface> result;
    float minX=INFINITY,minZ=INFINITY,maxX=-INFINITY,maxZ=-INFINITY;
    for(const auto& row:flow.NativeRibbon())for(auto p:{row.leftBank,row.rightBank}){
        minX=std::min(minX,p.x);minZ=std::min(minZ,p.z);maxX=std::max(maxX,p.x);maxZ=std::max(maxZ,p.z);
    }
    size_t inspected=0;
    for(size_t i=0;i<scene.objects.GetCount();++i){
        const auto entity=scene.objects.GetEntity(i);const auto& object=scene.objects[i];
        if(entity==river||!object.IsRenderable())continue;
        const auto* mesh=scene.meshes.GetComponent(object.meshID);if(!mesh||!mesh->IsRenderable())continue;
        river_g4::Surface surface;surface.terrain=terrain.count(entity)||terrain.count(object.meshID);
        const auto world=scene.ComputeEntityMatrixRecursive(entity);
        uint32_t first=0,last=0;mesh->GetLODSubsetRange(0,first,last);
        for(uint32_t subset=first;subset<last;++subset){
            const auto& part=mesh->subsets[subset];
            const auto* material=scene.materials.GetComponent(part.materialID);
            if(material&&material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
            Check(size_t(part.indexOffset)+part.indexCount<=mesh->indices.size(),"Invalid native subset range");
            for(uint32_t j=part.indexOffset;j+2<part.indexOffset+part.indexCount;j+=3){
                Check(++inspected<=2000000,"G4 native triangle scan limit");
                river_g4::Triangle triangle;
                XMFLOAT3* output[]={&triangle.a,&triangle.b,&triangle.c};
                for(int k=0;k<3;++k){
                    auto index=mesh->indices[j+k];Check(index<mesh->vertex_positions.size(),"Invalid native mesh index");
                    XMStoreFloat3(output[k],XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[index]),world));
                    Check(river_g4::Finite(*output[k]),"Nonfinite native geometry");
                }
                const auto& a=triangle.a;const auto& b=triangle.b;const auto& c=triangle.c;
                if(std::max({a.x,b.x,c.x})<minX-.5f||std::min({a.x,b.x,c.x})>maxX+.5f||
                    std::max({a.z,b.z,c.z})<minZ-.5f||std::min({a.z,b.z,c.z})>maxZ+.5f)continue;
                surface.triangles.push_back(triangle);
            }
        }
        if(!surface.triangles.empty()){
            if(rocks){float lo=INFINITY,hi=-INFINITY;for(const auto& t:surface.triangles)for(auto v:{t.a,t.b,t.c}){lo=std::min(lo,v.y);hi=std::max(hi,v.y);}
                std::cout<<"G4_SOURCE_GEOMETRY terrain="<<surface.terrain<<" worldY="<<lo<<","<<hi<<" triangles="<<surface.triangles.size()<<"\n";}
            if(!surface.terrain&&rocks)rocks->push_back(entity);
            result.push_back(std::move(surface));
        }
    }
    return result;
}


int main(int argc,char** argv){
 if(argc<4||argc>5)return 2;
 const bool interactive=argc==5&&std::string(argv[4])=="--interactive";
 const bool reopen=argc==5&&std::string(argv[4])=="--reopen";
 const bool benchmark=argc==5&&std::string(argv[4])=="--benchmark";
 std::cout.setf(std::ios::unitbuf);const auto out=fs::absolute(argv[1]),shaderDir=fs::absolute(argv[3]);
 if(fs::exists(out)&&!fs::is_empty(out))return 2;fs::create_directories(out);
 HWND window=CreateWindowExW(0,L"STATIC",L"Renegade River V2 - Contact Foam G5",WS_OVERLAPPEDWINDOW,
  60,60,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;
 app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 auto* gpu=wi::graphics::GetDevice();
 try{
  wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
  SceneService service;Check(service.LoadScene(fs::absolute(argv[2]).generic_u8string()),service.LastError());
  auto& scene=service.GetScene();auto river=scene.Entity_FindByName("River Water 5");
  auto* water=scene.materials.GetComponent(river);Check(water&&!water->IsCustomShader(),"Native source required");
  const auto optics=*water;const auto objectCount=scene.objects.GetCount(),meshCount=scene.meshes.GetCount();
  std::shared_ptr<const RiverFlowField> field;std::string error;bool reverse=false;float speedScale=1,intensity=1,sourceWidth=.04f,threshold=0;
  auto buildField=[&](){RiverFlowSettings settings;settings.downstreamReversed=reverse;
   Check(BuildRiverFlowField(scene,river,settings,field,error),error);};
  buildField();std::vector<wi::ecs::Entity> rocks;Gather(scene,river,*field,&rocks);Check(!rocks.empty(),"Native obstacle missing");
  std::vector<wi::scene::TransformComponent> transforms;std::vector<XMFLOAT4X4> worlds;std::vector<bool> visible;
  for(auto id:rocks){transforms.push_back(*scene.transforms.GetComponent(id));XMFLOAT4X4 m;
   XMStoreFloat4x4(&m,scene.ComputeEntityMatrixRecursive(id));worlds.push_back(m);visible.push_back(scene.objects.GetComponent(id)->IsRenderable());}
  FoamPath render;render.scene=&scene;render.river=river;
  river_g4::Mask mask;river_g5::Transport transport;Params params{};wi::graphics::Texture normalVelocity;
  auto upload=[&](wi::graphics::Texture& texture,const std::vector<XMFLOAT4>& data,bool writable=false){
   wi::graphics::TextureDesc td;td.width=mask.width;td.height=mask.height;
   td.format=wi::graphics::Format::R32G32B32A32_FLOAT;td.layout=wi::graphics::ResourceState::SHADER_RESOURCE;
   td.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;if(writable)td.bind_flags|=wi::graphics::BindFlag::UNORDERED_ACCESS;
   wi::graphics::SubresourceData initial;initial.data_ptr=data.data();initial.row_pitch=td.width*sizeof(XMFLOAT4);initial.slice_pitch=initial.row_pitch*td.height;
   Check(gpu->CreateTexture(&td,&initial,&texture),"G5 texture upload failed");
  };
  auto descriptor=[&](const wi::graphics::Texture& t){int index=gpu->GetDescriptorIndex(&t,wi::graphics::SubresourceType::SRV);Check(index>=0,"Missing SRV");return uint32_t(index);};
  auto rebuild=[&](const char* label){
   gpu->WaitForGPU();buildField();mask=river_g4::Build(*field,Gather(scene,river,*field));
   transport=river_g5::Prepare(*field,mask,speedScale,sourceWidth);render.bounds=mask.bounds;
   upload(render.velocity,transport.velocity);upload(render.source,transport.source);
   std::vector<XMFLOAT4> normals(mask.cells.size());float maxNormalSpeed=0;
   for(size_t k=0;k<normals.size();++k){
    XMFLOAT2 p{mask.bounds.x+(k%mask.width+.5f)/mask.width*mask.bounds.z,mask.bounds.y+(k/mask.width+.5f)/mask.height*mask.bounds.w};
    auto q=field->Evaluate(p);if(q.valid){normals[k]={q.directionXZ.x,q.directionXZ.y,q.speedMetersPerSecond*speedScale,1};maxNormalSpeed=std::max(maxNormalSpeed,normals[k].z);}
   }
   upload(normalVelocity,normals);render.baseVelocity=normalVelocity;
   params.bounds=mask.bounds;params.control={descriptor(normalVelocity),Bits(-1),Bits(1),0};
   const float tile=field->Samples().front().widthMeters/optics.texMulAdd.x;
   params.settings={tile,std::min(4.f,tile/std::max(maxNormalSpeed,.001f)),0,1};
   std::cout<<"G5_TRANSPORT="<<label<<" rockRate="<<transport.rockRate<<" bankRate="<<transport.bankRate<<" maxSpeed="<<transport.maximumSpeed<<" fieldHash="<<field->DeterministicHash()<<"\n";
  };
  rebuild("original");std::vector<XMFLOAT4> zero(mask.cells.size());
  upload(render.foam[0],zero,true);upload(render.foam[1],zero,true);
  params.foamControl={descriptor(render.foam[0]),descriptor(render.foam[1]),descriptor(render.source),descriptor(render.velocity)};
  params.foamSettings={intensity,0,0,0};
  wi::vector<uint8_t> bytes;wi::graphics::Shader pixel;
  Check(wi::helper::FileRead((shaderDir/"RiverContactFoamPS.cso").generic_u8string(),bytes),"Foam PS missing");
  Check(gpu->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),bytes.size(),&pixel),"Foam PS create failed");
  Check(wi::helper::FileRead((shaderDir/"RiverFoamTransportCS.cso").generic_u8string(),bytes),"Transport CS missing");
  Check(gpu->CreateShader(wi::graphics::ShaderStage::CS,bytes.data(),bytes.size(),&render.simulation),"Transport CS create failed");
  wi::renderer::CustomShader custom;custom.name="River V2 G5 transported foam";
  custom.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
  wi::graphics::PipelineStateDesc pso;pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);pso.ps=&pixel;
  pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
  pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
  pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
  Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Foam main PSO failed");
  pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);pso.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);
  pso.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);pso.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
  pso.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
  Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_SHADOW]),"Native shadow PSO failed");
  const int shaderID=wi::renderer::RegisterCustomShader(custom);
  wi::scene::CameraComponent camera;camera.CreatePerspective(1264,761,.05f,1000,XMConvertToRadians(60));
  render.camera=&camera;render.init(1264,761,96);render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);
  render.setOcclusionCullingEnabled(false);render.ResizeBuffers();wi::renderer::SetTemporalAAEnabled(false);
  struct Guard{wi::Application& app;wi::graphics::GraphicsDevice* gpu;~Guard(){app.ActivatePath(nullptr);
   while(wi::renderer::IsPipelineCreationActive())Sleep(10);gpu->WaitForGPU();}}guard{app,gpu};
  wi::gui::Label heading;heading.Create("River contact foam G5");heading.SetPos({12,12});heading.SetSize({1160,105});render.GetGUI().AddWidget(&heading);
  wi::graphics::GPUBuffer buffer;int mode=0,scenario=0;bool live=true;
  auto bind=[&](){
   gpu->WaitForGPU();water=scene.materials.GetComponent(river);render.enabled=mode>0;render.active=mode>0;
   params.foamControl.z=descriptor(render.source);params.foamControl.w=descriptor(render.velocity);params.foamSettings.x=mode==2?intensity:0;params.foamSettings.y=threshold;
   if(mode>0){
    wi::graphics::GPUBufferDesc bd;bd.size=sizeof(params);bd.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
    bd.misc_flags=wi::graphics::ResourceMiscFlag::BUFFER_RAW;Check(gpu->CreateBuffer(&bd,&params,&buffer),"Foam params failed");
    int index=gpu->GetDescriptorIndex(&buffer,wi::graphics::SubresourceType::SRV);
    water->SetCustomShaderID(shaderID);water->userdata={uint32_t(index),0x52464c35,render.state,Bits(render.clock)};
   }else{water->DisableCustomShader();water->userdata=optics.userdata;}
   water->SetDirty();
   const char* names[]={"Original rock","Moved 2m","Rotated / scaled","Above water","Fully submerged","Removed"};
   heading.SetText(std::string("RIVER V2 / G5  |  ")+(mode==0?"NATIVE WICKED":mode==1?"DIRECTED WATER":"DIRECTED WATER + CONTACT FOAM")+
    (reverse?"  REVERSED":"  FORWARD")+(render.paused?"  PAUSED":"  RUNNING")+"  |  "+names[scenario]+(render.emission?"  source ON":"  source OFF")+(speedScale==0?"  FLOW ZERO":"")+
    "\nF native / flow / foam   R reverse   C rock scenario   Space pause   N step   K emission   Z zero flow   X reset   +/- foam   1-5 views\n"+
    "WASD / arrows / Shift. [ ] decay   T turbulence   B source width   G breakup threshold. Foam "+std::to_string(intensity).substr(0,3)+" | decay "+std::to_string(render.lifetime).substr(0,3)+"s | turbulence "+std::to_string(render.turbulence).substr(0,3));
  };
  const XMFLOAT3 eyes[]={{-2,.7f,65.5f},{0,18,56},{-7,1.1f,67},{6,1.1f,67},{0,.5f,66.2f}};
  const XMFLOAT3 targets[]={{.4f,-.15f,68},{1,-.15f,68},{1,-.15f,68},{-1,-.15f,68},{.2f,-.15f,68}};
  auto pose=[&](int i){camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMLoadFloat3(&eyes[i]),XMLoadFloat3(&targets[i]),XMVectorSet(0,1,0,0))));camera.UpdateCamera();};
  auto frames=[&](int n){for(int i=0;i<n;++i)app.Run();};
  auto clear=[&](){
   // Invalidate both histories at an explicit safe point, never every frame.
   gpu->WaitForGPU();render.clock=0;
   upload(render.foam[0],zero,true);upload(render.foam[1],zero,true);
   params.foamControl.x=descriptor(render.foam[0]);params.foamControl.y=descriptor(render.foam[1]);
   bind();frames(1);
  };
  auto advance=[&](int n){for(int i=0;i<n;++i){render.manualStep=1.f/30;frames(1);}};
  auto capture=[&](const std::string& name){
   frames(12);while(gpu->GetFrameCount()%256!=0)frames(1);gpu->WaitForGPU();
   Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/(name+".png")).generic_u8string()),"Image capture failed");
   Check(wi::helper::saveTextureToFile(render.GetRenderResult2D(),(out/(name+"-hud.png")).generic_u8string()),"HUD capture failed");
   std::cout<<"CAPTURE="<<name<<" time="<<render.clock<<"\n";
  };
  struct Stats{double mass=0,x=0,z=0;float maximum=0;};
  auto stats=[&](const char* label){
   wi::vector<uint8_t> raw;Check(wi::helper::saveTextureToMemory(render.foam[render.state],raw),"Foam readback failed");
   Check(raw.size()==mask.cells.size()*sizeof(XMFLOAT4),"Foam readback size");
   Stats s;std::ofstream dump(out/(std::string(label)+"-state.bin"),std::ios::binary);dump.write(reinterpret_cast<const char*>(raw.data()),raw.size());
   for(size_t k=0;k<mask.cells.size();++k){
    XMFLOAT4 v;std::memcpy(&v,raw.data()+k*sizeof(v),sizeof(v));
    Check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.w)&&v.x>=0&&v.x<=3.001f,"Nonfinite/negative foam state");
    if(k==0)Check(v.z==render.turbulence,"GPU turbulence push constant mismatch");
    if(transport.velocity[k].w<.5f)Check(v.x<1e-6f,"Foam outside WATER/inside solid");
    s.mass+=v.x;s.x+=v.x*(mask.bounds.x+(k%mask.width+.5f)/mask.width*mask.bounds.z);
    s.z+=v.x*(mask.bounds.y+(k/mask.width+.5f)/mask.height*mask.bounds.w);s.maximum=std::max(s.maximum,v.x);
   }
   if(s.mass>0){s.x/=s.mass;s.z/=s.mass;}
   std::cout<<"G5_GPU_STATE="<<label<<" mass="<<s.mass<<" centroid="<<s.x<<","<<s.z<<" max="<<s.maximum<<"\n";return s;
  };
  auto variant=[&](int n){
   for(size_t i=0;i<rocks.size();++i){
    auto* t=scene.transforms.GetComponent(rocks[i]);*t=transforms[i];scene.objects.GetComponent(rocks[i])->SetRenderable(visible[i]);
    if(n==1||n==3||n==4){
     XMMATRIX parent=XMMatrixIdentity();if(const auto* h=scene.hierarchy.GetComponent(rocks[i]))parent=scene.ComputeEntityMatrixRecursive(h->parentID);
     auto local=XMLoadFloat4x4(&worlds[i])*XMMatrixTranslation(n==1?2.f:0,n==3?1.f:n==4?-.7f:0,0)*XMMatrixInverse(nullptr,parent);
     XMVECTOR scale,rotation,translation;Check(XMMatrixDecompose(&scale,&rotation,&translation,local),"Scenario world transform refused");
     XMStoreFloat3(&t->scale_local,scale);XMStoreFloat4(&t->rotation_local,rotation);XMStoreFloat3(&t->translation_local,translation);
    }
    if(n==2){t->scale_local.x*=1.4f;t->scale_local.y*=.7f;XMStoreFloat4(&t->rotation_local,
     XMQuaternionMultiply(XMLoadFloat4(&t->rotation_local),XMQuaternionRotationRollPitchYaw(.15f,.7f,0)));}
    if(n==5)scene.objects.GetComponent(rocks[i])->SetRenderable(false);t->SetDirty();t->UpdateTransform();
   }
  };
  pose(0);bind();app.ActivatePath(&render);frames(120);
  render.frozen=true;render.paused=true;render.active=true;wi::renderer::SetGameSpeed(0);clear();
  if(reopen){
   capture("fresh-native");mode=2;bind();advance(240);capture("fresh-foam");stats("fresh");
   std::cout<<"G5_FRESH_PROCESS_REOPEN=PASS\n";
  }else if(benchmark){
   render.paused=false;render.frozen=false;wi::renderer::SetGameSpeed(1);
   for(int m=0;m<3;++m){mode=m;bind();frames(60);auto start=std::chrono::steady_clock::now();frames(180);
    std::cout<<"G5_FPS mode="<<m<<" value="<<180/std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";}
  }else if(!interactive){
   capture("native-before");mode=1;bind();capture("flow-only");
   mode=2;bind();advance(240);capture("foam-close");auto initial=stats("established");
   mode=1;bind();capture("flow-same-time");mode=2;bind();
   pose(1);capture("foam-overhead");pose(2);capture("foam-upstream");pose(3);capture("foam-downstream");pose(4);capture("foam-macro");
   for(int i=0;i<90;++i){advance(1);gpu->WaitForGPU();Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/("animation-"+std::to_string(i)+".png")).generic_u8string()),"Animation capture");}
   render.emission=false;auto before=stats("emission-off-before");advance(60);auto after=stats("emission-off-after");
   Check(after.mass<before.mass*.8,"Foam did not decay after source removal");
   Check(after.x>before.x+.25,"Foam centroid did not travel downstream");capture("decayed");
   advance(300);Check(stats("expired").mass==0,"Foam exceeded finite lifetime");
   render.emission=true;advance(120);auto moveBefore=stats("continuous-move-before");
   variant(1);rebuild("continuous-move");bind();advance(1);auto moveAfter=stats("continuous-move-after");
   Check(moveAfter.mass>moveBefore.mass*.25,"Moved contact erased existing downstream foam");
   capture("continuous-move");variant(0);rebuild("continuous-restore");bind();
   render.emission=true;reverse=true;rebuild("reverse");bind();clear();advance(240);pose(0);capture("reverse-foam");stats("reverse");
   reverse=false;speedScale=0;rebuild("zero-flow");bind();clear();advance(90);Check(stats("zero").mass==0,"Zero flow emitted foam");
   speedScale=1;
   for(int n=1;n<=5;++n){scenario=n;variant(n);rebuild("scenario");bind();clear();advance(180);pose(0);capture("scenario-"+std::to_string(n));
    if(n>=3)Check(transport.rockRate==0,"Absent surface intersection emitted rock foam");stats(("scenario-"+std::to_string(n)).c_str());}
   scenario=0;variant(0);rebuild("restored");mode=0;bind();pose(0);capture("native-restored");
   Check(scene.objects.GetCount()==objectCount&&scene.meshes.GetCount()==meshCount,"Foam added geometry");
   const auto saved=out/"native-contact-foam.wiscene";{wi::Archive archive(saved.generic_u8string(),false);scene.Serialize(archive);}
   {wi::Archive archive(saved.generic_u8string(),true);wi::scene::Scene fresh;fresh.Serialize(archive);
    const auto* m=fresh.materials.GetComponent(fresh.Entity_FindByName("River Water 5"));
    Check(m&&!m->IsCustomShader()&&std::memcmp(&m->userdata,&optics.userdata,sizeof(m->userdata))==0,"Transient foam serialized");}
   Check(wi::renderer::GetShaderErrorCount()==0,"Shader errors");
   std::cout<<"G5_GPU_TRANSPORT_PROOF=PASS shader_errors=0\n";
  }else{
   mode=2;bind();advance(240);render.paused=false;render.frozen=false;wi::renderer::SetGameSpeed(1);bind();
   ShowWindow(window,SW_SHOW);SetForegroundWindow(window);std::cout<<"G5_INTERACTIVE_READY=1\n";
   auto previous=std::chrono::steady_clock::now();MSG msg{};
   while(live&&IsWindow(window)){
    bool change=false,doClear=false;
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
     if(msg.hwnd==window&&msg.message==WM_KEYDOWN&&!(msg.lParam&(1<<30))){
      auto key=msg.wParam;
      if(key=='F'){mode=(mode+1)%3;change=true;}
      if(key=='R'){reverse=!reverse;rebuild("live-reverse");change=true;}
      if(key=='C'){scenario=(scenario+1)%6;variant(scenario);rebuild("live-scenario");change=true;}
      if(key=='Z'){speedScale=speedScale==0?1.f:0.f;rebuild("live-speed");change=true;}
      if(key=='X'){doClear=true;change=true;}
      if(key=='K'){render.emission=!render.emission;change=true;}
      if(key==VK_SPACE){render.paused=!render.paused;render.frozen=render.paused;wi::renderer::SetGameSpeed(render.paused?0:1);change=true;}
      if(key=='N'&&render.paused)render.manualStep=1.f/30;
      if(key==VK_OEM_4){render.lifetime=std::max(.5f,render.lifetime-.5f);change=true;}
      if(key==VK_OEM_6){render.lifetime=std::min(6.f,render.lifetime+.5f);change=true;}
      if(key=='T'){render.turbulence=render.turbulence>1.1f?.5f:render.turbulence+.5f;change=true;}
      if(key=='B'){sourceWidth=sourceWidth>.05f?.02f:sourceWidth+.02f;rebuild("live-width");change=true;}
      if(key=='G'){threshold=threshold>.05f?-.08f:threshold+.08f;change=true;}
      if(key==VK_OEM_PLUS){intensity=std::min(2.f,intensity+.1f);change=true;}
      if(key==VK_OEM_MINUS){intensity=std::max(.1f,intensity-.1f);change=true;}
      if(key>='1'&&key<='5')pose(int(key-'1'));if(key==VK_ESCAPE)live=false;
     }
     TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    if(change)bind();if(doClear){bool pause=render.paused;render.paused=true;clear();render.paused=pause;}
    auto now=std::chrono::steady_clock::now();float dt=std::min(.05f,float(std::chrono::duration<double>(now-previous).count()));previous=now;
    if(GetForegroundWindow()==window){
     auto held=[](int key){return (GetAsyncKeyState(key)&0x8000)!=0;};
     float move=(held(VK_SHIFT)?8.f:2.f)*dt;
     XMVECTOR e=XMLoadFloat3(&camera.Eye),at=XMLoadFloat3(&camera.At),up=XMVectorSet(0,1,0,0);
     auto forward=XMVector3Normalize(at),right=XMVector3Normalize(XMVector3Cross(up,forward));
     if(held('W'))e+=forward*move;if(held('S'))e-=forward*move;if(held('A'))e-=right*move;if(held('D'))e+=right*move;
     float yaw=(held(VK_RIGHT)-held(VK_LEFT))*dt,pitch=(held(VK_DOWN)-held(VK_UP))*dt;
     forward=XMVector3TransformNormal(forward,XMMatrixRotationAxis(right,pitch)*XMMatrixRotationY(yaw));
     camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookToLH(e,forward,up)));camera.UpdateCamera();
    }
    app.Run();Sleep(2);
   }
  }
  render.GetGUI().RemoveWidget(&heading);return 0;
 }catch(const std::exception& e){std::cerr<<"G5_FAIL: "<<e.what()<<"\n";return 1;}
}
