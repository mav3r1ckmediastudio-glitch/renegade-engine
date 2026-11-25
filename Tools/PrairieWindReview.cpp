// Prairie motion trial using native animated directional force volumes.
#include "renegade/bridge/PlayerService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <algorithm>
#include <algorithm>
using namespace DirectX;
using namespace wi::scene;
namespace fs=std::filesystem;
static void Check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
static wi::ecs::Entity Force(Scene&s,const char*name,float strength,float radius,float z){
 auto e=wi::ecs::CreateEntity();s.names.Create(e)=name;auto&f=s.forces.Create(e);f.type=ForceFieldComponent::Type::Plane;f.gravity=strength;f.range=radius;
 auto&t=s.transforms.Create(e);XMStoreFloat4(&t.rotation_local,XMQuaternionRotationRollPitchYaw(0,0,XM_PIDIV2));t.translation_local={0,.3f,z};t.SetDirty();t.UpdateTransform();return e;
}
int main(int argc,char**argv){try{
 Check(argc==3,"usage: PrairieWindReview field.wiscene output-directory");const fs::path out=fs::u8path(argv[2]);fs::create_directories(out);
 HWND w=CreateWindowExW(0,L"STATIC",L"Prairie wind native author",WS_POPUP,0,0,1920,1009,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(w);wi::initializer::InitializeComponentsImmediate();app.setTargetFrameRate(60);app.setFrameRateLock(true);
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 Scene s;LoadModel(s,argv[1]);Check(s.hairs.GetCount()==1,"Expected one large meadow");
 for(size_t i=0;i<s.weathers.GetCount();++i){auto&weather=s.weathers[i];weather.windSpeed=0;weather.windRandomness=0;weather.windDirection={0,0,0};}
 s.weather.windSpeed=0;s.weather.windRandomness=0;s.weather.windDirection={0,0,0};
 s.forces.Clear();
 auto ambient=Force(s,"Prairie ambient breeze",.08f,500,32);
 auto ad=wi::ecs::CreateEntity();auto&ak=s.animation_datas.Create(ad);ak.keyframe_times={0,3,6,9,12};
 for(float heading:{-.22f,0.f,.22f,0.f,-.22f}){XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(0,heading,XM_PIDIV2));ak.keyframe_data.push_back(q.x);ak.keyframe_data.push_back(q.y);ak.keyframe_data.push_back(q.z);ak.keyframe_data.push_back(q.w);}
 auto ae=wi::ecs::CreateEntity();s.names.Create(ae)="Prairie breeze direction";auto&aa=s.animations.Create(ae);aa.start=0;aa.end=12;aa.SetLooped(true);aa.Play();auto&as=aa.samplers.emplace_back();as.data=ad;
 auto&ac=aa.channels.emplace_back();ac.target=ambient;ac.samplerIndex=0;ac.path=AnimationComponent::AnimationChannel::Path::ROTATION;
 std::vector<wi::ecs::Entity>gusts;
 for(unsigned i=0;i<4;++i){
  auto e=Force(s,("Prairie rolling gust "+std::to_string(i+1)).c_str(),.4f,20,-4.f+24.f*i);gusts.push_back(e);
  auto data=wi::ecs::CreateEntity();auto&keys=s.animation_datas.Create(data);keys.keyframe_times={0,20,40};keys.keyframe_data={-60,.3f,-4.f+24.f*i,0,.3f,-4.f+24.f*i,60,.3f,-4.f+24.f*i};
  auto ae=wi::ecs::CreateEntity();s.names.Create(ae)="Prairie gust travel";auto&a=s.animations.Create(ae);a.start=0;a.end=40;a.timer=15.5f+float(i%3)*.8f;a.SetLooped(true);a.Play();
  auto&sampler=a.samplers.emplace_back();sampler.data=data;sampler.mode=AnimationComponent::AnimationSampler::LINEAR;
  auto&channel=a.channels.emplace_back();channel.target=e;channel.samplerIndex=0;channel.path=AnimationComponent::AnimationChannel::Path::TRANSLATION;
 }
 auto&hair=s.hairs[0];
 auto*emitter=s.meshes.GetComponent(hair.meshID);Check(emitter,"Missing prairie emitter");
 std::cout<<"PRAIRIE_SOURCE_EMITTER_SUBSETS="<<emitter->subsets.size()<<std::endl;
 if(emitter->subsets.empty()){auto&subset=emitter->subsets.emplace_back();subset.indexOffset=0;subset.indexCount=uint32_t(emitter->indices.size());emitter->CreateRenderData();}
 hair.CreateFromMesh(*emitter);Check(!hair.indices.empty(),"Prairie emitter has no usable triangles");
 hair.stiffness=.5f;hair.drag=.12f;hair.gravityPower=.02f;hair.uniformity=-1;hair.SetDirty();
 s.Update(0);
 const fs::path file=out/"PrairieWind.wiscene";wi::Archive ar(file.generic_string(),false,false);s.Serialize(ar);Check(ar.SaveFile(file.generic_string()),"Prairie save failed");
 Scene cold;LoadModel(cold,file.generic_string());Check(cold.forces.GetCount()==5&&cold.animations.GetCount()>=4&&cold.hairs.GetCount()==1&&cold.hairs[0].strandCount==204800,"Prairie cold reopen failed");cold.Clear();
 CameraComponent camera;camera.CreatePerspective(1920,1009,.05f,300);camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(-8,3,-7,1),XMVectorSet(0,.15f,26,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 wi::RenderPath3D render;render.scene=&s;render.camera=&camera;render.init(1920,1009,96);render.setMotionBlurEnabled(false);render.setDepthOfFieldEnabled(false);render.ResizeBuffers();app.ActivatePath(&render);
 struct Disarm{wi::Application&app;~Disarm(){app.ActivatePath(nullptr);}}disarm{app};
 float first=s.transforms.GetComponent(gusts[0])->GetPosition().x;std::vector<uint8_t>initial;
 for(int f=0;f<601;++f){app.is_window_active=true;app.Run();
  if(f==120){Check(!hair.indices.empty(),"Prairie emitter distribution was lost");wi::graphics::GetDevice()->WaitForGPU();wi::vector<uint8_t>b;Check(wi::helper::saveBufferToMemory(hair.generalBuffer,b),"Initial prairie GPU readback failed");initial.assign(b.begin()+hair.simulation_view.offset,b.begin()+hair.simulation_view.offset+hair.simulation_view.size);}
  if(f%120==0){wi::graphics::GetDevice()->WaitForGPU();Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(out/("prairie-"+std::to_string(f)+".png")).generic_string()),"Prairie capture failed");std::cout<<"PRAIRIE_FRAME "<<f<<" gust_x="<<s.transforms.GetComponent(gusts[0])->GetPosition().x<<std::endl;}
 }
 wi::graphics::GetDevice()->WaitForGPU();wi::vector<uint8_t>b;Check(wi::helper::saveBufferToMemory(hair.generalBuffer,b),"Final prairie GPU readback failed");
 Check(std::abs(s.transforms.GetComponent(gusts[0])->GetPosition().x-first)>10,"Gust did not travel across field");
 Check(!std::equal(initial.begin(),initial.end(),b.begin()+hair.simulation_view.offset),"Native meadow geometry did not move");
 app.ActivatePath(nullptr);s.Clear();while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();
 std::cout<<"PRAIRIE_WIND_SAVE_REOPEN_ANIMATION_GPU_MOTION_PASS"<<std::endl;return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
