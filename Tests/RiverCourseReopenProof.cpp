#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/SceneService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
using namespace renegade::bridge;using namespace DirectX;namespace fs=std::filesystem;
struct Path:wi::RenderPath3D{RiverFlowRenderer flow;void Update(float dt)override{flow.Prepare(*scene,1,1.f/60,0);auto binding=flow.Bind();wi::RenderPath3D::Update(dt);}void Render()const override{auto binding=flow.Bind();flow.Dispatch();wi::RenderPath3D::Render();}};
static void Check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){
 if(argc!=3)return 2;std::cout.setf(std::ios::unitbuf);
 HWND window=CreateWindowExW(0,L"STATIC",L"River course fresh reopen",WS_OVERLAPPEDWINDOW,0,0,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{SceneService service;Check(service.LoadScene(fs::absolute(argv[1]).generic_string()),service.LastError());auto& scene=service.GetScene();auto river=scene.Entity_FindByName("River Water 5");Check(scene.splines.Contains(river),"Saved ordinary river missing");
 auto* spline=scene.splines.GetComponent(river);Check(spline->spline_node_entities.size()==7,"Derived supports leaked into authored spline");
 Check(!scene.materials.GetComponent(river)->IsCustomShader(),"Serialized transient shader");auto count=scene.transforms.GetCount();
 wi::scene::CameraComponent camera;camera.CreatePerspective(1264,761,.05f,500,XMConvertToRadians(60));camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(5,9,-13,1),XMVectorSet(-1,-.6f,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 Path path;path.scene=&scene;path.camera=&camera;path.init(1264,761,96);path.setOcclusionCullingEnabled(false);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);path.ResizeBuffers();
 struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}guard{app};app.ActivatePath(&path);
 for(int k=0;k<240;++k)app.Run();Check(path.flow.ActiveCount()==1,path.flow.Status(river));RiverFlowDiagnostics d;std::string error;bool ok=path.flow.Diagnostics(river,d,error,true);Check(ok,error);Check(d.foamMass>0,"Reopened foam absent");
 Check(scene.splines.GetComponent(river)->spline_node_entities.size()==7&&!scene.materials.GetComponent(river)->IsCustomShader(),"Transient state leaked after reopen");
 wi::graphics::GetDevice()->WaitForGPU();Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),fs::absolute(argv[2]).generic_string()),"Reopen capture");
 std::cout<<"COURSE_FRESH_REOPEN_PASS mass="<<d.foamMass<<" rebuilds="<<d.rebuilds<<" authoredNodes=7"<<std::endl;return 0;
 }catch(const std::exception& e){std::cerr<<"COURSE_FRESH_REOPEN_FAIL "<<e.what()<<std::endl;return 1;}
}
