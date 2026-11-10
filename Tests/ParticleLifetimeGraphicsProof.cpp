#include "renegade/bridge/ParticleEffectService.h"
#include "renegade/bridge/ParticleEffectRuntimeService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
using namespace renegade::bridge;
LRESULT CALLBACK CurveProc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
struct CurveApp:wi::Application{CurveApp(){alwaysactive=true;}};
struct CurvePath:wi::RenderPath3D{
 void Update(float)override{UpdateParticleEffectSprites(*scene,1.0f/60);wi::RenderPath3D::Update(1.0f/60);}
};
int main(int argc,char**argv){
 if(argc<2||argc>3)return 2;bool isolation=argc==3;wi::arguments::Parse(L"alwaysactive");std::cout<<std::unitbuf;std::filesystem::create_directories(argv[1]);
 WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.lpfnWndProc=CurveProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"RenegadeLifetimeProof";RegisterClassExW(&wc);
 HWND window=CreateWindowExW(0,wc.lpszClassName,L"Particle lifetime GPU check",WS_OVERLAPPEDWINDOW,60,60,550,570,nullptr,nullptr,wc.hInstance,nullptr);ShowWindow(window,SW_SHOWNOACTIVATE);
 int result=0;
 {
 wi::scene::Scene scene;CurvePath path;wi::scene::CameraComponent camera;CurveApp app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 path.scene=&scene;path.camera=&camera;path.init(480,480,96);camera.CreatePerspective(480,480,.01f,100,XM_PI/3);
 camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(0,0,-4,1),XMVectorSet(0,0,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 path.setBloomEnabled(false);path.setAO(wi::RenderPath3D::AO_DISABLED);path.setEyeAdaptionEnabled(false);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);path.ResizeBuffers();
 auto weather=wi::ecs::CreateEntity();auto& environment=scene.weathers.Create(weather);environment.ambient={1,1,1};environment.horizon=environment.zenith={0,0,0};scene.weather=environment;
 app.ActivatePath(&path);auto* device=wi::graphics::GetDevice();
 auto run=[&](int frames){for(int i=0;i<frames;++i){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}app.Run();}};
 run(5);
 for(int variant=0;variant<(isolation?0:5);++variant){
  ParticleEffect effect;effect.name="GPU lifetime";ParticleEffectLayer l;l.id=GenerateStableId();l.name="Probe";l.emitter=MakeNewParticleEmitterState();
  l.emitter.emitCount=0;l.emitter.burstOnCreate=1;l.emitter.maxParticles=16;l.emitter.life=4;l.emitter.randomLife=0;l.emitter.randomFactor=0;l.emitter.size=.5f;l.emitter.velocity={0,0,0};l.emitter.gravity={0,0,0};l.emitter.normalFactor=0;l.emitter.drag=1;l.emitter.scaleX=l.emitter.scaleY=1;l.emitter.color={1,1,1,1};l.emitter.emissiveStrength=0;l.emitter.shaderType=wi::EmittedParticleSystem::SOFT;
  l.emitter.sorted=(variant&1)!=0;l.emitter.depthCollision=(variant&2)!=0;l.material.blendMode=wi::enums::BLENDMODE_ALPHA;l.material.baseColor={1,1,1,1};
  l.lifetime.enabled=true;l.lifetime.keys={{0,.2f,{1,0,0,1}},{.5f,1,{0,1,0,1}},{1,.2f,{0,0,1,1}}};
  if(variant==4)l.lifetime.keys={{0,1,{1,1,1,1}},{1,1,{1,1,1,0}}};
  effect.layers.push_back(l);std::string error;ParticleEffectCommand create(scene,argv[1],GenerateStableId(),effect);
  if(!create.Execute()){std::cerr<<error<<create.Error();result=3;break;}
  auto root=create.Entity();auto emitter=scene.emitters.GetEntity(0);auto* native=scene.emitters.GetComponent(emitter);
  if(native->GetOpacityCurveTex()->GetDesc().array_size!=2){std::cerr<<"Missing lifetime lookup";result=4;break;}
  for(int frame=1;frame<=222;++frame){
   run(1);
   if(frame==10||frame==120||frame==220){
    auto name=std::string(argv[1])+"/v"+std::to_string(variant)+"_f"+std::to_string(frame)+".png";
    if(!wi::helper::saveTextureToFile(path.GetRenderResult3D(),name)){std::cerr<<"Capture failed";result=5;break;}
    std::cout<<"CAPTURE variant="<<variant<<" frame="<<frame<<" lookup_array="<<native->GetOpacityCurveTex()->GetDesc().array_size<<'\n';
   }
  }
  device->WaitForGPU();scene.Entity_Remove(root);run(2);
 }
 for(int type=0;type<(isolation?0:7);++type){
  ParticleEffect effect;effect.name="GPU shapes and timing";ParticleEffectLayer l;l.id=GenerateStableId();l.name="Shape probe";l.emitter=MakeNewParticleEmitterState();
  l.emitter.emitCount=0;l.emitter.burstOnCreate=0;l.emitter.maxParticles=2048;l.emitter.life=.7f;l.emitter.randomLife=l.emitter.randomFactor=0;l.emitter.size=.025f;l.emitter.velocity=l.emitter.gravity={0,0,0};l.emitter.normalFactor=type==3?.8f:0;l.emitter.drag=1;l.emitter.scaleX=l.emitter.scaleY=1;l.emitter.color={1,1,1,1};l.emitter.shaderType=wi::EmittedParticleSystem::SOFT;
  l.material.blendMode=wi::enums::BLENDMODE_ALPHA;l.material.baseColor={1,1,1,1};l.shape.type=ParticleShapeType(type);l.emitter.volume=type==1;l.shape.radius=1;l.shape.height=2;l.shape.angle=30;l.shape.thickness=.15f;l.transform.scale={1,1,1};
  effect.timing.enabled=true;effect.timing.loop=true;effect.timing.duration=1;effect.timing.delay=.25f;effect.timing.burst=600;effect.layers.push_back(l);
  ParticleEffectCommand create(scene,argv[1],GenerateStableId(),effect);if(!create.Execute()){std::cerr<<create.Error();result=6;break;}
  auto root=create.Entity();
  for(int frame=1;frame<=110;++frame){run(1);if(frame==8||frame==25||frame==65||frame==100){
   auto name=std::string(argv[1])+"/shape"+std::to_string(type)+"_f"+std::to_string(frame)+".png";
   if(!wi::helper::saveTextureToFile(path.GetRenderResult3D(),name))result=7;
   std::cout<<"SHAPE_CAPTURE type="<<type<<" frame="<<frame<<'\n';
  }}
  device->WaitForGPU();scene.Entity_Remove(root);run(2);
 }
 if(isolation){
  ParticleEffect effect;effect.name="Independent layer resize";
  for(int n=0;n<2;++n){ParticleEffectLayer l;l.id=GenerateStableId();l.name=n?"Green retained":"Red resized";l.emitter=MakeNewParticleEmitterState();
   l.emitter.emitCount=0;l.emitter.burstOnCreate=1;l.emitter.maxParticles=16;l.emitter.life=6;l.emitter.randomLife=l.emitter.randomFactor=0;l.emitter.size=n?.4f:.3f;l.emitter.velocity=l.emitter.gravity={0,0,0};l.emitter.normalFactor=0;l.emitter.drag=1;l.emitter.scaleX=l.emitter.scaleY=1;l.emitter.color=n?XMFLOAT4(0,1,0,1):XMFLOAT4(1,0,0,1);l.emitter.shaderType=wi::EmittedParticleSystem::SOFT;l.emitter.opacityPeakStart=0;l.emitter.opacityPeakEnd=1;
   l.material.blendMode=wi::enums::BLENDMODE_ALPHA;l.material.baseColor=l.emitter.color;l.transform.translation={n?.8f:-.8f,0,0};
   if(n){l.lifetime.enabled=true;l.lifetime.keys={{0,.2f,{0,1,0,1}},{.5f,1,{0,1,0,1}},{1,.2f,{0,1,0,1}}};}
   effect.layers.push_back(l);
  }
  ParticleEffectCommand create(scene,argv[1],GenerateStableId(),effect);if(!create.Execute())return 10;
  auto root=create.Entity();auto red=scene.emitters.GetEntity(0),green=scene.emitters.GetEntity(1);run(120);device->WaitForGPU();
  if(!wi::helper::saveTextureToFile(path.GetRenderResult3D(),std::string(argv[1])+"/resize_before.png"))return 11;
  auto retained=scene.emitters.GetComponent(green);auto layer=effect.layers[0];layer.emitter.size=.0001225f;
  ApplyParticleEffectLayerSettings(scene,root,red,layer,effect.fields,true);scene.emitters.GetComponent(red)->Restart();
  if(!scene.transforms.Contains(root)||scene.emitters.GetComponent(green)!=retained||retained->size!=.4f)return 12;
  run(5);device->WaitForGPU();
  if(!wi::helper::saveTextureToFile(path.GetRenderResult3D(),std::string(argv[1])+"/resize_after.png"))return 13;
  auto captured=CaptureParticleEffect(scene,root);if(captured.layers.size()!=2||captured.layers[0].emitter.size!=.0001225f||captured.layers[1].emitter.size!=.4f)return 14;
  std::cout<<"RESIZE_NATIVE_PASS root_and_other_emitter_preserved=1 red_size=.0001225 green_size=.4\\n";
  scene.Entity_Remove(root);
 }
 device->WaitForGPU();
 }
 DestroyWindow(window);std::cout<<"LIFETIME_GPU_EXIT "<<result<<'\n';return result;
}
