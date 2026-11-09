#include "renegade/bridge/ParticleEffectService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <cmath>
using namespace renegade::bridge;
LRESULT CALLBACK FieldProc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
struct FieldApp:wi::Application {FieldApp(){alwaysactive=true;}};
struct FieldPath:wi::RenderPath3D {
 void Update(float)override {HydrateParticleMovementFields(*scene);wi::RenderPath3D::Update(1.0f/60);}
};
int main(){ std::cout<<std::unitbuf;std::cerr<<std::unitbuf;std::cout<<"GPU_PROOF_START"<<std::endl;
 WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.lpfnWndProc=FieldProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"ParticleFieldGraphicsProof";RegisterClassExW(&wc);
 HWND window=CreateWindowExW(0,wc.lpszClassName,L"Particle movement GPU proof",WS_OVERLAPPEDWINDOW,50,50,360,280,nullptr,nullptr,wc.hInstance,nullptr);if(!window)return 2;ShowWindow(window,SW_SHOWNOACTIVATE);
 int failures=0;
 {
  wi::scene::Scene scene;FieldPath path;wi::scene::CameraComponent camera;FieldApp app;app.allow_hdr=false;app.SetWindow(window);std::cout<<"GPU_DEVICE_READY"<<std::endl;wi::initializer::InitializeComponentsImmediate();std::cout<<"GPU_COMPONENTS_READY"<<std::endl;
  path.scene=&scene;path.camera=&camera;path.init(320,240,96);camera.CreatePerspective(320,240,.01f,100,XM_PI/3);
  camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(0,3,-6,1),XMVectorSet(0,0,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
  path.setBloomEnabled(false);path.setAO(wi::RenderPath3D::AO_DISABLED);path.setEyeAdaptionEnabled(false);path.ResizeBuffers();
  ParticleEffect effect;effect.name="GPU field proof";ParticleMovementField field;field.id=GenerateStableId();field.name="Swirl";field.transform.scale={3,3,3};field.strength=1;field.falloff=0;effect.fields={field};
  for(int n=0;n<8;++n){ParticleEffectLayer layer;layer.id=GenerateStableId();layer.name=std::to_string(n);auto& e=layer.emitter;e=MakeNewParticleEmitterState();e.maxParticles=1;e.emitCount=0;e.burstOnCreate=1;e.size=.01f;e.life=10;e.randomLife=0;e.randomFactor=0;e.normalFactor=0;e.velocity={0,0,0};e.gravity={0,0,0};e.drag=1;e.sorted=n<4&&(n&1);e.depthCollision=n<4&&(n&2);layer.transform.translation={1,0,0};layer.fieldInfluence[0]=n>=4?0:1;if(n>=5){layer.sizeVariation=.5f;layer.speedVariation=.5f;e.maxParticles=100;e.burstOnCreate=80;e.velocity={0,1,0};e.volume=n==6;}if(n==3)layer.speedVariation=.4f;effect.layers.push_back(layer);}
  auto cube=scene.Entity_CreateCube("GPU mesh source");scene.transforms.GetComponent(cube)->Translate(XMFLOAT3(0,-100,0));effect.layers[7].emitter.meshId=scene.objects.GetComponent(cube)->meshID;std::string error;ParticleEffectCommand create(scene,std::filesystem::temp_directory_path().string(),GenerateStableId(),effect);if(!create.Execute()){std::cerr<<create.Error();return 3;}
  std::cout<<"GPU_EFFECT_CREATED"<<std::endl;app.ActivatePath(&path);std::cout<<"GPU_PATH_ACTIVE"<<std::endl;
  for(int frame=0;frame<140;++frame){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}app.Run();if(frame%30==0)std::cout<<"GPU_FRAME "<<frame<<std::endl;}
  std::cout<<"GPU_FRAMES_DONE"<<std::endl;auto* device=wi::graphics::GetDevice();device->WaitForGPU();
  for(size_t n=0;n<scene.emitters.GetCount();++n){
   auto& emitter=scene.emitters[n];if(!emitter.particleBuffer.IsValid()){std::cerr<<"NO_GPU_BUFFER "<<n<<'\n';++failures;continue;}
   wi::graphics::GPUBufferDesc desc;desc.size=emitter.particleBuffer.desc.size;desc.usage=wi::graphics::Usage::READBACK;wi::graphics::GPUBuffer readback;
   if(!device->CreateBuffer(&desc,nullptr,&readback)){++failures;continue;}
   auto cmd=device->BeginCommandList();auto barrier=wi::graphics::GPUBarrier::Buffer(&emitter.particleBuffer,wi::graphics::ResourceState::SHADER_RESOURCE,wi::graphics::ResourceState::COPY_SRC);device->Barrier(&barrier,1,cmd);
   device->CopyResource(&readback,&emitter.particleBuffer,cmd);barrier=wi::graphics::GPUBarrier::Buffer(&emitter.particleBuffer,wi::graphics::ResourceState::COPY_SRC,wi::graphics::ResourceState::SHADER_RESOURCE);device->Barrier(&barrier,1,cmd);device->SubmitCommandLists();device->WaitForGPU();
   if(!readback.mapped_data){std::cerr<<"NO_READBACK_MAPPING"<<std::endl;++failures;continue;}const auto* particles=static_cast<const Particle*>(readback.mapped_data);const Particle* alive=nullptr;size_t aliveCount=0;float sizeMin=100,sizeMax=0,speedMin=100,speedMax=0;
   for(size_t slot=0;slot<readback.desc.size/sizeof(Particle);++slot)if(particles[slot].maxLife==10&&particles[slot].life>0&&particles[slot].life<=10&&std::isfinite(particles[slot].position.x)){
    alive=&particles[slot];++aliveCount;sizeMin=std::min(sizeMin,alive->sizeBeginEnd.x);sizeMax=std::max(sizeMax,alive->sizeBeginEnd.x);speedMin=std::min(speedMin,alive->velocity.y);speedMax=std::max(speedMax,alive->velocity.y);
   }
   if(!alive){std::cerr<<"NO_LIVE_PARTICLE "<<n<<std::endl;++failures;continue;}const auto& particle=*alive;
   if(n>=5){bool ok=aliveCount==80&&sizeMin<.008f&&sizeMax>.012f&&sizeMin>=.0049f&&sizeMax<=.0151f&&speedMin<.8f&&speedMax>1.2f;failures+=!ok;std::cout<<"GPU_VARIATION "<<n<<" alive="<<aliveCount<<" size="<<sizeMin<<','<<sizeMax<<" speed="<<speedMin<<','<<speedMax<<" result="<<(ok?"PASS":"FAIL")<<std::endl;continue;}
   bool valid=std::isfinite(particle.position.x)&&particle.life>0&&particle.maxLife==10;
   bool moved=particle.position.x<.95f&&particle.position.z>.05f&&std::abs(particle.velocity.x)>.05f;
   bool unchanged=std::abs(particle.position.x-1)<.001f&&std::abs(particle.position.z)<.001f;
   bool ok=valid&&(n<4?moved:unchanged);failures+=!ok;
   std::cout<<"GPU_FIELD "<<n<<" pos="<<particle.position.x<<','<<particle.position.y<<','<<particle.position.z<<" velocity="<<particle.velocity.x<<','<<particle.velocity.y<<','<<particle.velocity.z<<" life="<<particle.life<<" result="<<(ok?"PASS":"FAIL")<<'\n';
  }
  device->WaitForGPU();
 }
 DestroyWindow(window);std::cout<<"PARTICLE_FIELD_GPU_FAILURES "<<failures<<'\n';return failures?1:0;
}
