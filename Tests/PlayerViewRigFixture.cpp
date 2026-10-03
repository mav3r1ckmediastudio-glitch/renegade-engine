#include "renegade/bridge/PlayerService.h"
#include <WickedEngine.h>
#include <windows.h>
#include <iostream>
LRESULT CALLBACK ProofWindow(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProcW(h,m,w,l);}
int main(int argc,char** argv) {
 if(argc!=2) return 2;
 WNDCLASSEXW c={}; c.cbSize=sizeof(c); c.lpfnWndProc=ProofWindow;
 c.hInstance=GetModuleHandleW(nullptr); c.lpszClassName=L"P1CleanProof"; RegisterClassExW(&c);
 HWND h=CreateWindowExW(0,c.lpszClassName,L"P1 Clean",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,c.hInstance,nullptr);
 int result=0;
 {
 wi::Application app; app.allow_hdr=false; app.SetWindow(h); wi::initializer::InitializeComponentsImmediate();
 wi::scene::Scene s;
 auto floor=s.Entity_CreateCube("Solid proof floor");
 auto* t=s.transforms.GetComponent(floor); t->Translate(XMFLOAT3(0,-0.5f,0)); t->UpdateTransform();
 auto* fm=s.meshes.GetComponent(s.objects.GetComponent(floor)->meshID); for(auto& v:fm->vertex_positions) {v.x*=50;v.y*=0.5f;v.z*=50;} fm->CreateRenderData();
 auto& body=s.rigidbodies.Create(floor); body.mass=0; body.shape=wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX; body.box.halfextents=XMFLOAT3(50,0.5f,50);
 auto& w=s.weathers.Create(wi::ecs::CreateEntity()); w.ambient=XMFLOAT3(0.4f,0.4f,0.4f); w.horizon=XMFLOAT3(0.25f,0.4f,0.55f); w.zenith=XMFLOAT3(0.08f,0.18f,0.35f);
 auto light=s.Entity_CreateLight("Proof sun"); s.lights.GetComponent(light)->SetType(wi::scene::LightComponent::DIRECTIONAL);
 s.transforms.GetComponent(light)->RotateRollPitchYaw(XMFLOAT3(-0.7f,0.6f,0));
 // Visible landmarks make movement/yaw/pitch changes assessable.
 for(int i=0;i<3;++i) {
 auto e=s.Entity_CreateCube("Proof landmark");
 auto* tr=s.transforms.GetComponent(e); tr->Translate(XMFLOAT3(float(i*4-4),1,8)); tr->UpdateTransform();
 s.materials.GetComponent(e)->baseColor=XMFLOAT4(0.2f+0.25f*i,0.4f,0.65f,1);
 }
 renegade::bridge::TransformState pose; pose.translation=XMFLOAT3(0,2,0);
 renegade::bridge::CreatePlayerStartCommand cmd(s,pose); if(!cmd.Execute()) return 3;


 {wi::Archive output(argv[1],false,false); if(!output.IsOpen()) return 5; s.Serialize(output);}
 {wi::scene::Scene reopened; wi::Archive a(argv[1],true,false); reopened.Serialize(a);
 auto start=renegade::bridge::ResolvePlayerStart(reopened);
 if(start.resolution!=renegade::bridge::PlayerStartResolution::Success || !start.start.settings.firstPersonArmsAssetId.empty()) result=6;}
 while(wi::renderer::IsPipelineCreationActive()>0) Sleep(10); wi::graphics::GetDevice()->WaitForGPU();
 }
 DestroyWindow(h); std::cout<<"CLEAN_PROOF_RESULT="<<result<<"\n"; return result;
}
