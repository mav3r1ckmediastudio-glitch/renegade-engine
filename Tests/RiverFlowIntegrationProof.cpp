#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/SceneDocumentService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <cstring>
using namespace renegade::bridge;using namespace DirectX;namespace fs=std::filesystem;
static void Check(bool v,const std::string& why){if(!v)throw std::runtime_error(why);}
struct Path:wi::RenderPath3D {
 SceneService* service=nullptr;RiverFlowRenderer flow;CommandService history;
 bool frozen=false;float simulationStep=0;uint64_t edits=0;
 void Update(float dt)override{
  flow.Prepare(*scene,service->Revision(),simulationStep,edits);
  {auto binding=flow.Bind();wi::RenderPath3D::Update(frozen?0:dt);}
 }
 void Render()const override{auto binding=flow.Bind();flow.Dispatch();wi::RenderPath3D::Render();}
};
int main(int argc,char** argv){
 if(argc<3||argc>4)return 2;
 std::cout.setf(std::ios::unitbuf);auto out=fs::absolute(argv[1]);if(fs::exists(out)&&!fs::is_empty(out))return 2;fs::create_directories(out);
 bool reopen=argc==4&&(std::string(argv[3])=="--reopen"||std::string(argv[3])=="--reopen-any");
 HWND window=CreateWindowExW(0,L"STATIC",L"Renegade River V2 G6 integration verification",WS_OVERLAPPEDWINDOW,50,50,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;
 app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();auto* gpu=wi::graphics::GetDevice();
 try{
  wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
  SceneService service;Check(service.LoadScene(fs::absolute(argv[2]).generic_u8string()),service.LastError());
  auto& scene=service.GetScene();auto river=scene.Entity_FindByName("River Water 5");
  Check(scene.splines.Contains(river),"Fixture spline missing");
  if(argc==4&&std::string(argv[3])=="--normalize-review-copy"){
   Check(fs::absolute(argv[2]).parent_path().parent_path().parent_path().filename()=="RenegadeRiverReview_20261025","Normalize only the disposable G9 review copy");
   const auto builtin=fs::absolute("Content/river/waterNormalMap.jpg").generic_u8string();
   const auto resource=wi::resourcemanager::Load(builtin);Check(resource.IsValid()&&resource.GetTexture().IsValid(),"Governed built-in normal");
   for(size_t n=0;n<scene.materials.GetCount();++n){auto& m=scene.materials[n];if(m.shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
    auto& slot=m.textures[wi::scene::MaterialComponent::NORMALMAP];if(slot.name.empty()||fs::u8path(slot.name).filename()!="waterNormalMap.jpg")continue;std::cout<<"NORMALIZE_REVIEW_BUILTIN="<<slot.name<<"\n";slot.name=builtin;slot.resource=resource;m.SetDirty();}

  }
  auto native=*scene.materials.GetComponent(river);
  std::string error;Check(EnsurePersistentEntityIdentities(scene,error),error);
  Path path;path.scene=&scene;path.service=&service;
  wi::scene::CameraComponent camera;camera.CreatePerspective(1264,761,.05f,1000,XMConvertToRadians(60));path.camera=&camera;
  XMFLOAT3 eye(-2,.7f,65.5f),target(.4f,-.15f,68);
  camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMLoadFloat3(&eye),XMLoadFloat3(&target),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
  path.init(1264,761,96);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);path.setOcclusionCullingEnabled(false);path.ResizeBuffers();wi::renderer::SetTemporalAAEnabled(false);
  struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}guard{app};
  auto frames=[&](int n){for(int k=0;k<n;++k){app.Run();auto* m=scene.materials.GetComponent(river);Check(!m->IsCustomShader()&&std::memcmp(&m->userdata,&native.userdata,sizeof(native.userdata))==0,"Transient bindings leaked outside render/update");}};
  auto capture=[&](const std::string& name){
   float step=path.simulationStep;path.simulationStep=0;frames(12);while(gpu->GetFrameCount()%256)frames(1);gpu->WaitForGPU();
   Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/(name+".png")).generic_u8string()),"Capture failed");
   path.simulationStep=step;std::cout<<"G6_CAPTURE="<<name<<"\n";
  };
  auto apply=[&](RiverFlowProfile p){gpu->WaitForGPU();Check(path.history.Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,river,p)),"Profile command refused");++path.edits;frames(3);Check(!p.enabled||path.flow.ActiveCount()==1,path.flow.Status(river));};
  auto stats=[&](const char* label){RiverFlowDiagnostics d;Check(path.flow.Diagnostics(river,d,error,true),error);std::cout<<"G6_STATE="<<label<<" mass="<<d.foamMass<<" max="<<d.maximumDensity<<" rebuilds="<<d.rebuilds<<" build_ms="<<d.lastBuildMilliseconds<<"\n";return d;};
  app.ActivatePath(&path);frames(120);path.frozen=true;wi::renderer::SetGameSpeed(0);
  RiverFlowProfile p;Check(ReadRiverFlowProfile(scene,river,p,error),error);
  if(argc==4&&std::string(argv[3])=="--normalize-review-copy"){
   gpu->WaitForGPU();{wi::Archive archive(fs::absolute(argv[2]).generic_u8string(),false);Check(archive.IsOpen(),"Review copy archive");
   archive.SetCompressionEnabled(true);scene.Serialize(archive);}gpu->WaitForGPU();
   std::cout<<"G9_NORMALIZE_DISPOSABLE_REVIEW=PASS\n";return 0;
  }
  if(argc==4&&std::string(argv[3])=="--head-whitewater"){
   auto commit=[&](const RiverFlowProfile& next,const char* label){
    gpu->WaitForGPU();
    Check(path.history.Execute(std::make_unique<SetRiverFlowProfileCommand>(
        scene,river,next)),"G13E8 profile command refused");
    ++path.edits;
    for(int k=0;k<120 && next.enabled && path.flow.ActiveCount()==0;++k)frames(1);
    frames(6);
    std::cout<<"G13E8_HEAD_STAGE="<<label<<" enabled="<<next.enabled
      <<" active="<<path.flow.ActiveCount()<<" status="<<path.flow.Status(river)<<std::endl;
    // This inherited fixture contains several independently active rivers.
    // Check the selected river's GPU diagnostics below, not a global count=1.
    Check(!next.enabled||path.flow.ActiveCount()>=1,path.flow.Status(river));
   };
   path.frozen=false;path.simulationStep=1.f/30;
   if(p.enabled){p.enabled=false;commit(p,"disable_prior");}
   p.enabled=true;p.foam=true;p.startWhitewater=false;
   commit(p,"start_baseline");frames(180);
   auto off=stats("head-off");
   p.startWhitewater=true;p.startWhitewaterAmount=1.45f;
   p.startWhitewaterNodeIndex=1;commit(p,"enable_selected_node");
   frames(180);
   const auto* spline=scene.splines.GetComponent(river);
   Check(spline&&spline->spline_node_entities.size()>1,"G13E8 head camera needs nodes");
   const bool backwards=p.reversed;
   const size_t source= p.startWhitewaterNodeIndex>=0?
       size_t(p.startWhitewaterNodeIndex):
       (backwards?spline->spline_node_entities.size()-1:0);
   const auto inlet=spline->spline_node_entities[source];
   const auto next=spline->spline_node_entities[
       source+1<spline->spline_node_entities.size()?source+1:source-1];
   XMFLOAT3 a{},b{};
   XMStoreFloat3(&a,scene.ComputeEntityMatrixRecursive(inlet).r[3]);
   XMStoreFloat3(&b,scene.ComputeEntityMatrixRecursive(next).r[3]);
   const float dx=b.x-a.x,dz=b.z-a.z,length=std::max(.01f,std::hypot(dx,dz));
   const float ux=dx/length,uz=dz/length;
   XMFLOAT3 headEye{a.x-ux*9.f+uz*4.f,a.y+9.f,a.z-uz*9.f-ux*4.f};
   XMFLOAT3 headTarget{a.x+ux*3.f,a.y,a.z+uz*3.f};
   camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(
     XMLoadFloat3(&headEye),XMLoadFloat3(&headTarget),XMVectorSet(0,1,0,0))));
   camera.UpdateCamera();
   capture("G13E8-head-on");
   auto on=stats("head-on");
   Check(on.foamMass>off.foamMass+.0001,
     "G13E8 source whitewater did not add transported foam mass");
   p.startWhitewater=false;commit(p,"retire_head");frames(300);
   auto retired=stats("head-retired");
   Check(retired.foamMass<on.foamMass,
     "G13E8 head foam continued emitting after disable");
   std::cout<<"G13E8_HEAD_WHITEWATER_SELECTED_NODE_GPU_PASS nodeIndex=1 off="<<off.foamMass
     <<" on="<<on.foamMass<<" retired="<<retired.foamMass<<std::endl;
   return 0;
  }
  if(argc==4&&std::string(argv[3])=="--cache-regression"){
   p.enabled=true;p.foam=true;apply(p);path.frozen=false;path.simulationStep=1.f/30;
   frames(60);auto baseline=stats("cache-baseline");
   wi::ecs::Entity rock=wi::ecs::INVALID_ENTITY;
   for(size_t n=0;n<scene.objects.GetCount();++n){
    const auto* mesh=scene.meshes.GetComponent(scene.objects[n].meshID);
    if(mesh&&mesh->indices.size()>3000&&mesh->indices.size()<15000){rock=scene.objects.GetEntity(n);break;}
   }
   Check(rock!=wi::ecs::INVALID_ENTITY,"Cache fixture rock missing");
   gpu->WaitForGPU();auto foreground=scene.Entity_Duplicate(rock);
   scene.objects.GetComponent(foreground)->SetForeground(true);
   auto* transform=scene.transforms.GetComponent(foreground);
   for(int frame=0;frame<60;++frame){transform->translation_local.x+=.001f;transform->SetDirty();transform->UpdateTransform();++path.edits;frames(1);}
   Check(stats("moving-foreground").rebuilds==baseline.rebuilds,"Foreground animation rebuilt river");
   gpu->WaitForGPU();scene.Entity_Remove(foreground);++path.edits;frames(3);
   Check(stats("foreground-removed").rebuilds==baseline.rebuilds,"Irrelevant object count rebuilt river");
   auto suspendedRock=scene.Entity_Duplicate(rock);transform=scene.transforms.GetComponent(suspendedRock);
   auto originalTransform=transform->translation_local;transform->translation_local.y+=100;transform->SetDirty();transform->UpdateTransform();++path.edits;frames(3);
   Check(stats("above-water").rebuilds==baseline.rebuilds,"Above-water object rebuilt river");
   for(int frame=0;frame<30;++frame){transform->translation_local.x+=.001f;transform->SetDirty();transform->UpdateTransform();++path.edits;frames(1);}
   Check(stats("moving-above-water").rebuilds==baseline.rebuilds,"Above-water animation rebuilt river");
   transform->translation_local=originalTransform;transform->SetDirty();transform->UpdateTransform();++path.edits;frames(3);
   auto entered=stats("rock-entered-water");Check(entered.rebuilds>baseline.rebuilds,"Real obstacle entering water did not invalidate");
   transform->translation_local.x+=.1f;transform->SetDirty();transform->UpdateTransform();++path.edits;frames(3);
   auto moved=stats("rock-moved");Check(moved.rebuilds>entered.rebuilds,"Real rock edit failed to invalidate");
   gpu->WaitForGPU();scene.Entity_Remove(suspendedRock);++path.edits;frames(3);
   Check(stats("rock-removed").rebuilds>moved.rebuilds,"Real rock removal failed to invalidate");
   capture("cache-regression");std::cout<<"G6_CACHE_REGRESSION=PASS\n";
  }else if(reopen){
   Check(p.enabled&&(p.reversed||std::string(argv[3])=="--reopen-any"),"Reopened profile absent");path.simulationStep=1.f/30;frames(240);Check(stats("fresh").foamMass>0,"Fresh foam missing");capture("fresh-reopened");
   std::cout<<"G6_FRESH_REOPEN=PASS\n";
  }else{
   p.enabled=false;apply(p);capture("native-before");
   p.enabled=true;p.intensity=1.8f;p.turbulence=1.5f;apply(p);path.simulationStep=1.f/30;frames(240);capture("enabled");
   auto baseline=stats("enabled");Check(baseline.foamMass>0,"No contact foam");Check(baseline.maximumDetail>0,"No source-born foam artwork detail");
   frames(60);auto cached=stats("cached");Check(cached.rebuilds==baseline.rebuilds,"Static frame rebuilt CPU geometry");
   ++path.edits;frames(3);Check(stats("unrelated-edit").rebuilds==cached.rebuilds,"Unrelated edit rebuilt source");
   path.flow.SetSuspended(true);frames(6);Check(stats("suspended").rebuilds==cached.rebuilds,"Suspended drag rebuilt source");
   path.flow.SetSuspended(false);frames(3);
   auto node=scene.splines.GetComponent(river)->spline_node_entities[1];
   auto before=CaptureTransform(*scene.transforms.GetComponent(node)),after=before;after.translation.z+=.4f;
   gpu->WaitForGPU();Check(path.history.Execute(std::make_unique<SetWatercourseTransformCommand>(scene,node,before,after)),"Spline edit failed");
   ++path.edits;frames(90);Check(path.flow.ActiveCount()==1,path.flow.Status(river));capture("spline-edited");stats("edited");
   Check(path.history.Undo(),"Spline undo");++path.edits;frames(3);Check(path.history.Redo(),"Spline redo");++path.edits;frames(3);
   Check(path.history.Undo(),"Restore spline");++path.edits;frames(3);
   Check(path.history.Execute(MakeSetRiverWidthCommand(scene,river,4.3f)),"Width edit failed");++path.edits;frames(3);Check(path.flow.ActiveCount()==1,path.flow.Status(river));capture("width-edited");
   Check(path.history.Undo(),"Width undo");++path.edits;frames(3);
   p.reversed=true;apply(p);frames(240);capture("reversed");Check(stats("reverse").foamMass>0,"Reverse foam missing");
   auto saved=out/"river-v2-enabled.wiscene";{wi::Archive a(saved.generic_u8string(),false);scene.Serialize(a);}
   {wi::Archive a(saved.generic_u8string(),true);wi::scene::Scene loaded;loaded.Serialize(a);
    auto e=loaded.Entity_FindByName("River Water 5");RiverFlowProfile restored;Check(ReadRiverFlowProfile(loaded,e,restored,error)&&restored.reversed&&restored.enabled,"Profile archive failed");
    Check(!loaded.materials.GetComponent(e)->IsCustomShader()&&loaded.materials.GetComponent(e)->userdata.y!=0x52464c35,"GPU binding in archive");}
   p.enabled=false;apply(p);capture("native-restored");Check(path.flow.ActiveCount()==0,"Disable retained active GPU simulation");
   Check(wi::renderer::GetShaderErrorCount()==0,"Shader errors");
   std::cout<<"G6_SCOPED_RENDER_EDIT_SAVE=PASS shader_errors=0\n";
   // Build a NEW disposable creator project with a valid authored Story Flow.
   // Public document services own identity, save and dependencies.
   p.enabled=true;p.reversed=false;apply(p);gpu->WaitForGPU();
   TransformState spawn;spawn.translation={-2.05f,.05f,65.5f};
   XMStoreFloat4(&spawn.rotation,XMQuaternionRotationRollPitchYaw(0,.65f,0));
   auto existingPlayer=ResolvePlayerStart(scene);
   if(existingPlayer.resolution==PlayerStartResolution::Missing){CreatePlayerStartCommand player(scene,spawn);Check(player.Execute(),"Player Start fixture");}
   else {Check(existingPlayer.resolution==PlayerStartResolution::Success,existingPlayer.message);
    SetTransformCommand movePlayer(scene,existingPlayer.start.entity,existingPlayer.start.transform,spawn);Check(movePlayer.Execute(),"Move copied Player Start");}
   auto view=scene.Entity_CreateCamera("River Review Camera",1264,761,.05f,1000,XMConvertToRadians(60));
   auto* viewTransform=scene.transforms.GetComponent(view);
   viewTransform->MatrixTransform(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMLoadFloat3(&eye),XMLoadFloat3(&target),XMVectorSet(0,1,0,0))));
   viewTransform->UpdateTransform();
   scene.cameras.GetComponent(view)->TransformCamera(*viewTransform);
   ProjectService projects;projects.Initialize((out/"review-recents.ini").generic_u8string());
   Check(projects.CreateStoryFlowProject(out.generic_u8string(),"RiverV2Review",false),projects.LastError());
   const auto project=projects.CurrentProject();auto scenePath=fs::u8path(project.rootPath)/"Content/Scenes/River.wiscene";fs::create_directories(scenePath.parent_path());
   SelectionService selection;SceneDocumentService documents(service,selection,path.history,projects);
   Check(documents.Save(scenePath.generic_u8string()),service.LastError());
   DocumentEnvelope envelope=CreateDocumentEnvelope(project.projectId,SceneDocumentType,"Content/Scenes/River.wiscene","river-v2-g6-proof");
   Check(WriteDocumentEnvelope(scenePath.generic_u8string()+".rmeta",envelope,error),error);
   FlowDocument flow;auto flowPath=(fs::u8path(project.rootPath)/fs::u8path(project.startupFlow)).generic_u8string();
   Check(ReadFlowDocument(flowPath,project.projectId,flow,error),error);
   FlowNode level;level.id=GenerateStableId();level.kind=FlowNodeKind::Level;level.name="River";
   level.sceneAssetId=envelope.documentId;level.scenePathHint="Content/Scenes/River.wiscene";
   FlowNode complete;complete.id=GenerateStableId();complete.kind=FlowNodeKind::CompleteGame;complete.name="Complete Game";
   FlowRoute start;start.id=GenerateStableId();start.sourceNodeId=flow.startNodeId;start.outcome=GameStartOutcome;start.destinationNodeId=level.id;start.destinationEntry="default";
   FlowRoute finish;finish.id=GenerateStableId();finish.sourceNodeId=level.id;finish.outcome="level.complete";finish.destinationNodeId=complete.id;
   flow.nodes.push_back(level);flow.nodes.push_back(complete);flow.routes.push_back(start);flow.routes.push_back(finish);
   Check(WriteFlowDocument(flowPath,flow,error),error);
   std::cout<<"G6_REVIEW_PROJECT="<<project.descriptorPath<<"\n";

  }
  gpu->WaitForGPU();return 0;
 }catch(const std::exception& x){std::cerr<<"G6_FAIL="<<x.what()<<"\n";return 1;}
}
