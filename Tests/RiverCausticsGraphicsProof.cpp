#include "renegade/bridge/RiverSplineService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace renegade::bridge;
void Require(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
int main(int argc,char** argv){
 if(argc!=2)return 2;std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
 HWND window=CreateWindowExW(0,L"STATIC",L"Native Water Caustics Proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{
 wi::scene::Scene scene;wi::scene::CameraComponent camera;camera.CreatePerspective(960,640,.05f,100,XM_PI/3);
 wi::RenderPath3D path;path.scene=&scene;path.camera=&camera;path.init(960,640,96);path.setOcclusionCullingEnabled(false);path.setMotionBlurEnabled(false);path.ResizeBuffers();
 struct Cleanup{wi::Application& app;~Cleanup(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);app.ActivatePath(nullptr);wi::graphics::GetDevice()->WaitForGPU();}} cleanup{app};
 const auto floor=scene.Entity_CreateCube("Pale stone bed");auto* pose=scene.transforms.GetComponent(floor);pose->scale_local={15,.25f,15};pose->translation_local={0,-1,0};pose->SetDirty();scene.materials.GetComponent(floor)->SetBaseColor({.55f,.52f,.42f,1});scene.materials.GetComponent(floor)->SetRoughness(.9f);
 auto& weather=scene.weathers.Create(scene.Entity_CreateTransform("Weather"));weather.ambient={.04f,.04f,.04f};weather.horizon={.5f,.65f,.8f};weather.zenith={.2f,.4f,.7f};
 const auto sun=scene.Entity_CreateLight("Sun",{0,20,0},{1,1,1},4,100,wi::scene::LightComponent::DIRECTIONAL);scene.lights.GetComponent(sun)->SetCastShadow(true);scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3(1.4f,0,0));
 RiverSplineOptions options;options.widthMeters=12;options.surfaceOffsetMeters=0;CreateRiverSplineCommand river(scene,{{0,0,-12},{0,0,12}},options);Require(river.Execute(),"create native water");
 auto* water=scene.materials.GetComponent(river.CreatedEntity());water->SetBaseColor({.1f,.3f,.38f,.25f});
 app.ActivatePath(&path);wi::renderer::SetTemporalAAEnabled(false);
 auto frames=[&](int n){for(int i=0;i<n;++i){app.Run();Sleep(8);}wi::graphics::GetDevice()->WaitForGPU();};
 auto capture=[&](const char* name){frames(24);Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(output/name).string()),"capture");};
 // Below the surface, the opaque riverbed receives the WATER shadow shader.
 camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(0,-.25f,-3,1),XMVectorSet(0,-.75f,2,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 frames(24);while(wi::renderer::IsPipelineCreationActive())Sleep(10);
 water->SetCastShadow(false);capture("caustics-off.png");
 water->SetCastShadow(true);capture("caustics-on.png");capture("caustics-animated.png");
 camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(0,3,-3,1),XMVectorSet(0,-.75f,2,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 water->SetCastShadow(false);capture("above-caustics-off.png");
 water->SetCastShadow(true);capture("above-caustics-on.png");
 std::cout<<"NATIVE_CAUSTICS_CAPTURE=PASS waterShadow="<<water->IsCastingShadow()<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
