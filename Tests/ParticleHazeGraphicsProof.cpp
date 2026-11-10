#include "renegade/bridge/ParticleEffectService.h"
#include <Windows.h>
#include <iostream>
#include <algorithm>
using namespace renegade::bridge;
LRESULT CALLBACK HazeProc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
struct HazeApp:wi::Application{HazeApp(){alwaysactive=true;}};
struct HazePath:wi::RenderPath3D{
 void Update(float)override{wi::RenderPath3D::Update(1.0f/60);}
 const wi::graphics::Texture& Distortion(){return rtParticleDistortion;}
};
int main(int argc,char**argv){
 if(argc!=4)return 2;std::cout<<std::unitbuf;
 WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.lpfnWndProc=HazeProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"RenegadeHazeProof";RegisterClassExW(&wc);
 HWND window=CreateWindowExW(0,wc.lpszClassName,L"Heat haze rendering check",WS_OVERLAPPEDWINDOW,60,60,680,540,nullptr,nullptr,wc.hInstance,nullptr);ShowWindow(window,SW_SHOWNOACTIVATE);
 int result=1;
 {
 wi::scene::Scene scene;HazePath path;wi::scene::CameraComponent camera;HazeApp app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 path.scene=&scene;path.camera=&camera;path.init(640,480,96);camera.CreatePerspective(640,480,.01f,100,XM_PI/3);
 camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(0,1,-4,1),XMVectorSet(0,1,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 path.setBloomEnabled(false);path.setAO(wi::RenderPath3D::AO_DISABLED);path.setEyeAdaptionEnabled(false);path.ResizeBuffers();
 auto weather=wi::ecs::CreateEntity();scene.weathers.Create(weather).ambient={1,1,1};
 for(int y=0;y<8;++y)for(int x=0;x<10;++x){
  auto e=scene.Entity_CreateCube("Checker wall");auto* t=scene.transforms.GetComponent(e);t->Scale(XMFLOAT3(.2f,.2f,.05f));t->Translate(XMFLOAT3((x-4.5f)*.4f,(y-3.5f)*.4f+1,1));t->UpdateTransform();
  auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);auto* material=scene.materials.GetComponent(mesh->subsets[0].materialID);material->baseColor=(x+y)%2?XMFLOAT4(.9f,.9f,.9f,1):XMFLOAT4(.04f,.04f,.04f,1);material->emissiveColor={(x+y)%2?.8f:.02f,(x+y)%2?.8f:.02f,(x+y)%2?.8f:.02f,1};material->SetDirty();
 }
 std::string error;auto effects=ListParticleEffects(argv[1],argv[2],error);ParticleEffect haze;for(auto& e:effects)if(e.name=="Heat Haze")haze=e;
 if(haze.layers.empty()){std::cerr<<"Missing haze preset";return 3;}
 auto& l=haze.layers[0];l.transform.translation={0,1,0};l.emitter.color={1,1,1,.25f};l.emitter.emitCount=12;l.emitter.size=.8f;l.emitter.life=3;l.emitter.velocity={0,.2f,0};
 ParticleEffectCommand create(scene,argv[1],argv[2],haze);if(!create.Execute()){std::cerr<<create.Error();return 4;}
 app.ActivatePath(&path);
 auto run=[&](int frames){for(int i=0;i<frames;++i){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}app.Run();}};
 run(100);auto* device=wi::graphics::GetDevice();device->WaitForGPU();
 for(size_t i=0;i<scene.emitters.GetCount();++i){auto e=scene.emitters.GetEntity(i);auto* m=scene.materials.GetComponent(e);std::cout<<"EMITTER shader="<<scene.emitters[i].shaderType<<" count="<<scene.emitters[i].count<<" normal="<<m->textures[wi::scene::MaterialComponent::NORMALMAP].resource.IsValid()<<" alpha="<<m->baseColor.w<<'\n';}
 wi::vector<uint8_t> data;bool read=wi::helper::saveTextureToMemory(path.Distortion(),data);float maximum=0;size_t nonzero=0;
 if(read){auto* h=reinterpret_cast<const uint16_t*>(data.data());for(size_t i=0;i<data.size()/2;++i){float v=std::abs(DirectX::PackedVector::XMConvertHalfToFloat(h[i]));if(v>.00001f)++nonzero;maximum=std::max(maximum,v);}}
 std::cout<<"DISTORTION read="<<read<<" bytes="<<data.size()<<" format="<<int(path.Distortion().desc.format)<<" max="<<maximum<<" nonzero="<<nonzero<<'\n';
 wi::helper::saveTextureToFile(path.GetRenderResult3D(),std::string(argv[3])+"/haze_proof_on.png");
 for(size_t i=0;i<scene.emitters.GetCount();++i){auto& e=scene.emitters[i];e.count=0;e.SetPaused(false);e.Restart();}
 run(200);wi::helper::saveTextureToFile(path.GetRenderResult3D(),std::string(argv[3])+"/haze_proof_off.png");
 result=read&&maximum>.0001f?0:1;device->WaitForGPU();
 }
 DestroyWindow(window);return result;
}
