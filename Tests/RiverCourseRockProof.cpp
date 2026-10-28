#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/SceneService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <chrono>
#include "renegade/bridge/ReusableAssetService.h"
#include "../EngineBridge/src/RiverCourseField.h"
#include "../EngineBridge/src/RiverGradedTransport.h"
#include "../EngineBridge/src/RiverFlowSourceSelection.h"
using namespace renegade::bridge;using namespace DirectX;namespace fs=std::filesystem;
struct Path:wi::RenderPath3D{RiverFlowRenderer flow;float step=1.f/60;void Update(float dt)override{flow.Prepare(*scene,1,step,0);auto binding=flow.Bind();wi::RenderPath3D::Update(dt);}void Render()const override{auto binding=flow.Bind();flow.Dispatch();wi::RenderPath3D::Render();}};
static void Check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){
 if(argc!=4&&argc!=5)return 2;std::cout.setf(std::ios::unitbuf);
 HWND window=CreateWindowExW(0,L"STATIC",L"River course fresh reopen",WS_OVERLAPPEDWINDOW,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{SceneService service;Check(service.LoadScene(fs::absolute(argv[1]).generic_string()),service.LastError());auto& scene=service.GetScene();auto river=scene.Entity_FindByName("River Water 5");Check(scene.splines.Contains(river),"Saved ordinary river missing");
 auto* spline=scene.splines.GetComponent(river);Check(spline->spline_node_entities.size()==7,"Derived supports leaked into authored spline");
 Check(!scene.materials.GetComponent(river)->IsCustomShader(),"Serialized transient shader");auto count=scene.transforms.GetCount();
 wi::scene::CameraComponent camera;camera.CreatePerspective(1264,761,.05f,500,XMConvertToRadians(60));camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(5,9,-13,1),XMVectorSet(-1,-.6f,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 Path path;path.scene=&scene;path.camera=&camera;path.init(1264,761,96);path.setOcclusionCullingEnabled(false);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);path.ResizeBuffers();path.camera=&camera;
 struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}guard{app};app.ActivatePath(&path);

 // Reproduce native browser directory changes after renderer construction.
 // Built-in shaders/artwork must still come from the executable's Content.
 fs::current_path(fs::absolute(argv[1]).parent_path());
 std::cout<<"SHADER_DIRECTORY_CHANGE_PROOF cwd="<<fs::current_path().generic_string()<<std::endl;
 for(int k=0;k<120;++k)app.Run();Check(path.flow.ActiveCount()==1,path.flow.Status(river));std::string error;

 wi::ecs::Entity wrapper=wi::ecs::INVALID_ENTITY;float rockX=6;
 const bool existing=std::string(argv[3])=="--existing";
 if(existing){
  wrapper=scene.Entity_FindByName("new rock (2)");Check(scene.transforms.Contains(wrapper),"Existing owner rock missing");
  rockX=scene.transforms.GetComponent(wrapper)->translation_local.x;
  RiverFlowDiagnostics baseline;Check(path.flow.Diagnostics(river,baseline,error,false),error);
  std::cout<<"EXISTING_OWNER_SCENE objects="<<scene.objects.GetCount()<<" baselineBuildMS="<<baseline.lastBuildMilliseconds<<" rockX="<<rockX<<std::endl;
 }else{
 ReusableModelAssetDocument document;Check(ReadReusableModelAssetDocument(fs::absolute(argv[3]).generic_string(),document,error),error);
 auto rockScene=std::make_unique<wi::scene::Scene>();wi::Archive rockArchive(document.payload.data(),document.payload.size());Check(rockArchive.IsOpen(),"Rock native payload");rockScene->Serialize(rockArchive);rockScene->Update(0);
 std::cout<<"ACTUAL_ROCK vertices="<<rockScene->meshes[0].vertex_positions.size()<<" triangles="<<rockScene->meshes[0].indices.size()/3<<std::endl;
 wrapper=scene.Entity_CreateTransform("Regression rock");
 std::vector<wi::ecs::Entity> roots;for(size_t k=0;k<rockScene->transforms.GetCount();++k){auto e=rockScene->transforms.GetEntity(k);if(!rockScene->hierarchy.Contains(e))roots.push_back(e);}
 scene.Merge(*rockScene);for(auto e:roots)scene.Component_Attach(e,wrapper);
 auto* pose=scene.transforms.GetComponent(wrapper);rockX=argc==5?std::stof(argv[4]):6;pose->translation_local={rockX,rockX< -4?-.5f:-1.5f,0};pose->scale_local={100,100,100};pose->SetDirty();
 }
 const float baseZ=scene.transforms.GetComponent(wrapper)->translation_local.z;
 auto* pose=scene.transforms.GetComponent(wrapper);
 for(int trial=0;trial<3;++trial){
  pose=scene.transforms.GetComponent(wrapper);pose->translation_local.z=baseZ+float(trial+1)*.2f;pose->SetDirty();scene.Update(0);
  for(size_t m=0;m<scene.objects.GetCount();++m){auto e=scene.objects.GetEntity(m);const auto& o=scene.objects[m];const auto* mesh=scene.meshes.GetComponent(o.meshID);if(mesh&&mesh->vertex_positions.size()==15672){auto box=mesh->aabb.transform(scene.ComputeEntityMatrixRecursive(e));auto a=box.getMin(),b=box.getMax();std::cout<<"ROCK_BOUNDS "<<a.x<<","<<a.y<<","<<a.z<<" .. "<<b.x<<","<<b.y<<","<<b.z<<std::endl;}}
  auto start=std::chrono::steady_clock::now();std::cout<<"ROCK_REBUILD_BEGIN "<<trial<<std::endl;
  RiverFlowDiagnostics prior;Check(path.flow.Diagnostics(river,prior,error,false),error);
  double maximumFrame=0;int frames=0;
  do{
   auto tick=std::chrono::steady_clock::now();app.Run();
   maximumFrame=std::max(maximumFrame,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-tick).count());++frames;
   if(trial==1&&frames==2){auto* moved=scene.transforms.GetComponent(wrapper);moved->translation_local.z+=.11f;moved->SetDirty();scene.Update(0);}

   Check(std::chrono::steady_clock::now()-start<std::chrono::seconds(120),"Obstacle update timeout");
  }while(path.flow.Status(river).find("updating")!=std::string::npos||frames<4);
  std::cout<<"RESPONSIVENESS maximumFrameMS="<<maximumFrame<<" frames="<<frames<<" status="<<path.flow.Status(river)<<std::endl;
  Check(maximumFrame<500,"Obstacle edit blocked the render thread");

  RiverFlowDiagnostics d;Check(path.flow.Diagnostics(river,d,error,false),error);Check(d.rebuilds==prior.rebuilds+1,"Stale obstacle snapshot published or final update missing");
  std::cout<<"ROCK_REBUILD_END "<<trial<<" wallMS="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" buildMS="<<d.lastBuildMilliseconds<<" rebuilds="<<d.rebuilds<<std::endl;
 }
 // A source-only update must preserve frozen bulk density byte-for-byte in sum.
 path.step=0;RiverFlowDiagnostics frozenBefore;Check(path.flow.Diagnostics(river,frozenBefore,error,true),error);
 auto* moved=scene.transforms.GetComponent(wrapper);moved->translation_local.x+=.25f;moved->SetDirty();scene.Update(0);
 auto frozenStart=std::chrono::steady_clock::now();int frozenFrames=0;
 do{app.Run();++frozenFrames;Check(std::chrono::steady_clock::now()-frozenStart<std::chrono::seconds(120),"Frozen update timeout");}
 while(path.flow.Status(river).find("updating")!=std::string::npos||frozenFrames<4);
 RiverFlowDiagnostics frozenAfter;Check(path.flow.Diagnostics(river,frozenAfter,error,true),error);
 Check(frozenAfter.rebuilds==frozenBefore.rebuilds+1&&frozenAfter.foamMass==frozenBefore.foamMass,"Obstacle update reset existing foam");
 std::cout<<"OBSTACLE_FROZEN_FOAM_PRESERVED mass="<<frozenAfter.foamMass<<std::endl;
 // Speed/contact-width edits must return promptly and retain the last valid frame.
 RiverFlowProfile changing;Check(ReadRiverFlowProfile(scene,river,changing,error),error);
 const auto originalSettings=changing;double maximumSettingFrame=0;
 for(int n=0;n<10;++n){
  changing.speed=.7f+.025f*n;changing.sourceWidth=.03f+.002f*n;changing.intensity=.3f+.02f*n;changing.lifetime=2.5f+.05f*n;changing.turbulence=.8f+.02f*n;changing.threshold=.005f*n;changing.rapidIntensity=.7f+.02f*n;changing.rapids=(n%2)==0;
  SetRiverFlowProfileCommand change(scene,river,changing);Check(change.Execute(),"Flow slider commit");
  auto tick=std::chrono::steady_clock::now();app.Run();
  maximumSettingFrame=std::max(maximumSettingFrame,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-tick).count());
 }
 auto settingsStart=std::chrono::steady_clock::now();int settingsFrames=0;
 do{app.Run();++settingsFrames;Check(std::chrono::steady_clock::now()-settingsStart<std::chrono::seconds(120),"Flow settings update timeout");}
 while(path.flow.Status(river).find("updating")!=std::string::npos||settingsFrames<4);
 Check(maximumSettingFrame<500,"Flow setting blocked the render thread");
 Check(path.flow.Status(river).find("River ready")==0,path.flow.Status(river));
 std::cout<<"SPEED_CONTACT_SETTINGS_PASS maximumFrameMS="<<maximumSettingFrame<<std::endl;
 // Enable/disable is a live parameter change, never a structural rebuild.
 RiverFlowDiagnostics toggleBefore;Check(path.flow.Diagnostics(river,toggleBefore,error,true),error);
 RiverFlowProfile profile;Check(ReadRiverFlowProfile(scene,river,profile,error),error);
 auto toggleStart=std::chrono::steady_clock::now();
 for(int n=0;n<4;++n){profile.enabled=!profile.enabled;SetRiverFlowProfileCommand change(scene,river,profile);Check(change.Execute(),"Flow toggle");for(int k=0;k<3;++k)app.Run();}
 RiverFlowDiagnostics toggleAfter;Check(path.flow.Diagnostics(river,toggleAfter,error,false),error);
 Check(toggleAfter.rebuilds==toggleBefore.rebuilds,"Enable toggle rebuilt the obstacle domain");
 std::cout<<"LIVE_TOGGLE_PASS elapsedMS="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-toggleStart).count()<<" rebuilds="<<toggleAfter.rebuilds<<std::endl;
 // A drag freezes preparation/transport but must keep the cached course/foam bound.
 path.flow.SetSuspended(true);path.flow.Prepare(scene,1,0,0);
 {auto binding=path.flow.Bind();Check(scene.materials.GetComponent(river)->IsCustomShader(),"Suspended drag lost course foam shader");}
 path.flow.SetSuspended(false);std::cout<<"SUSPENDED_COURSE_BINDING_PASS"<<std::endl;
 SetRiverFlowProfileCommand restore(scene,river,originalSettings);Check(restore.Execute(),"Restore authored defaults");
 auto restoreStart=std::chrono::steady_clock::now();int restoreFrames=0;
 do{app.Run();++restoreFrames;Check(std::chrono::steady_clock::now()-restoreStart<std::chrono::seconds(120),"Default restore timeout");}
 while(path.flow.Status(river).find("updating")!=std::string::npos||restoreFrames<4);
 path.step=1.f/60;
 // Give contact foam time to become visible, then capture close to the actual rock.
 for(int k=0;k<180;++k)app.Run();
 camera=wi::scene::CameraComponent();camera.CreatePerspective(1264,761,.05f,500,XMConvertToRadians(60));camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(rockX+1,2,-5,1),XMVectorSet(rockX,rockX< -4?-.3f:-1,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 for(int k=0;k<8;++k)app.Run();
 RiverFlowDiagnostics d;Check(path.flow.Diagnostics(river,d,error,true),error);
 Check(scene.splines.GetComponent(river)->spline_node_entities.size()==7&&!scene.materials.GetComponent(river)->IsCustomShader(),"Transient state leaked after reopen");
 wi::graphics::GetDevice()->WaitForGPU();Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),fs::absolute(argv[2]).generic_string()),"Reopen capture");
 std::cout<<"COURSE_ROCK_REGRESSION_PASS mass="<<d.foamMass<<" rebuilds="<<d.rebuilds<<" authoredNodes=7"<<std::endl;return 0;
 }catch(const std::exception& e){std::cerr<<"COURSE_ROCK_REGRESSION_FAIL "<<e.what()<<std::endl;return 1;}
}
