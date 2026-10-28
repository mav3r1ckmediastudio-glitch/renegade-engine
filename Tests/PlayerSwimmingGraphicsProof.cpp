#include "renegade/bridge/PlayerSwimmingService.h"
#include "renegade/bridge/RiverUnderwaterService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace renegade::bridge;
void Require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Path:wi::RenderPath3D{
 RuntimePlayerState player;PlayerInputFrame input;bool moving=false;RiverUnderwaterRenderer water;
 void Update(float) override{
  if(moving)Require(UpdateRuntimePlayer(*scene,player,input),"live player update");
  wi::RenderPath3D::Update(1.f/60);
  if(moving)ApplyRuntimePlayerCamera(*scene,player,*camera);
 }
 void PreRender() override{water.Update(*this);wi::RenderPath3D::PreRender();}
};
int main(int argc,char** argv){
 if(argc!=2)return 2;
 std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
 HWND window=CreateWindowExW(0,L"STATIC",L"River Swimming Native Physics Proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{
 wi::scene::Scene scene;wi::scene::CameraComponent camera;camera.CreatePerspective(960,640,.05f,100,XM_PI/3);
 Path path;path.scene=&scene;path.camera=&camera;path.init(960,640,96);path.ResizeBuffers();
 struct Cleanup{wi::Application& app;~Cleanup(){while(wi::renderer::IsPipelineCreationActive())Sleep(10);app.ActivatePath(nullptr);wi::graphics::GetDevice()->WaitForGPU();}} cleanup{app};
 const auto river=scene.Entity_CreateObject("Native river");
 const auto id=GenerateStableId();scene.metadatas.Create(river).string_values.set(PersistentEntityIdMetadataKey,id);scene.splines.Create(river);
 auto& material=scene.materials.Create(river);material.shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;material.baseColor={.1f,.3f,.4f,.5f};
 auto& mesh=scene.meshes.Create(river);mesh.vertex_positions={{-10,0,-10},{-10,0,10},{10,0,10},{10,0,-10}};mesh.vertex_normals.assign(4,{0,1,0});mesh.indices={0,1,2,0,2,3};
 mesh.subsets.emplace_back();mesh.subsets.back().materialID=river;mesh.subsets.back().indexCount=6;mesh.CreateRenderData();scene.objects.GetComponent(river)->meshID=river;
 auto terrain=scene.Entity_CreateTransform("Depth authority");scene.terrains.Create(terrain);
 scene.metadatas.Create(terrain).string_values.set(RiverTerrainLayersMetadataKey,std::string("{\"chunks\":[],\"layers\":[{\"bank\":2,\"depth\":3,\"river\":\"")+id+"\"}],\"version\":1}");
 const auto floor=scene.Entity_CreateTransform("Solid riverbed");scene.transforms.GetComponent(floor)->Translate(XMFLOAT3(0,-3.5f,0));
 auto& bed=scene.rigidbodies.Create(floor);bed.shape=wi::scene::RigidBodyPhysicsComponent::CollisionShape::BOX;bed.box.halfextents={20,.5f,20};bed.mass=0;
 TransformState pose;pose.translation={0,-1.05f,0};CreatePlayerStartCommand create(scene,pose);Require(create.Execute(),"player start");
 std::string error;Require(SpawnRuntimePlayer(scene,ResolvePlayerStart(scene).start,path.player,error),error.c_str());
 wi::physics::SetEnabled(true);wi::physics::SetSimulationEnabled(true);app.ActivatePath(&path);
 auto frames=[&](int n){for(int i=0;i<n;++i)app.Run();wi::graphics::GetDevice()->WaitForGPU();};
 frames(3);path.moving=true;
 auto position=[&](){return wi::physics::GetPosition(*scene.rigidbodies.GetComponent(path.player.entity));};
 const float initial=position().y;path.player.pitch=wi::math::DegreesToRadians(80);path.input.moveForward=1;frames(40);
 const float dived=position().y;Require(path.player.swimming,"entered river swimming");Require(dived<initial-.5f,"W must overcome native buoyancy");Require(dived+1.65f<0,"camera below actual river surface");
 Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(output/"diving.png").string()),"dive capture");
 path.input={};frames(30);const float idle=position().y;Require(idle-dived<.4f,"idle must not force immediate surface");
 path.player.pitch=wi::math::DegreesToRadians(89);path.input.moveForward=1;frames(120);const float bottom=position().y;Require(bottom>-3.15f,"capsule collided with riverbed");Require(bottom<-2.5f,"reached riverbed");
 path.player.pitch=wi::math::DegreesToRadians(-80);frames(45);const float risen=position().y;Require(risen>bottom+.5f,"look up W ascends");
 // Boundary setup is confined to this disposable fixture; gameplay never teleports.
 wi::physics::SetPosition(*scene.rigidbodies.GetComponent(path.player.entity),{12,0,0});frames(2);
 Require(!path.player.swimming,"dry bank leaves swim state");Require(scene.rigidbodies.GetComponent(path.player.entity)->buoyancy==1.2f,"native buoyancy restored");
 std::cout<<"NATIVE_SWIM=PASS initial="<<initial<<" dive="<<dived<<" idle="<<idle<<" bottom="<<bottom<<" ascend="<<risen<<"\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
