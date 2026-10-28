#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/PlayerService.h"
#include <cmath>
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/SpecialistComponentService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace renegade::bridge;
void Require(bool v,const std::string& m){if(!v)throw std::runtime_error(m);}
int main(int argc,char** argv){
 if(argc!=3&&argc!=4)return 2;const auto output=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(output);
 HWND window=CreateWindowExW(0,L"STATIC",L"Native River Endpoint Proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{
 SceneService scenes;Require(scenes.LoadScene(std::filesystem::absolute(argv[1]).generic_string()),"load old saved native terrain");auto& scene=scenes.GetScene();
 wi::scene::CameraComponent camera;camera.CreatePerspective(960,640,.05f,500,XM_PI/3);
 wi::RenderPath3D path;path.scene=&scene;path.camera=&camera;path.init(960,640,96);path.setOcclusionCullingEnabled(false);path.setMotionBlurEnabled(false);path.ResizeBuffers();
 struct Cleanup{wi::Application& app;wi::scene::Scene& scene;~Cleanup(){for(size_t i=0;i<scene.terrains.GetCount();++i)scene.terrains[i].Generation_Cancel();while(wi::renderer::IsPipelineCreationActive())Sleep(10);app.ActivatePath(nullptr);wi::graphics::GetDevice()->WaitForGPU();}} cleanup{app,scene};
 wi::ecs::Entity river=wi::ecs::INVALID_ENTITY;for(size_t i=0;i<scene.splines.GetCount();++i){auto e=scene.splines.GetEntity(i);auto* m=scene.materials.GetComponent(e);if(m&&m->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER){const auto id=PersistentEntityId(scene,e);auto* metadata=scene.metadatas.GetComponent(scene.terrains.GetEntity(0));if(!id.empty()&&metadata&&metadata->string_values.get(RiverTerrainLayersMetadataKey).find(id)!=std::string::npos){river=e;break;}}}
 Require(river!=wi::ecs::INVALID_ENTITY,"river missing");auto points=SampleSplineWorldPoints(scene,river,64);Require(points.size()>1,"river curve");
 const auto end=points.back();const auto previous=points[points.size()-2];float dx=end.x-previous.x,dz=end.z-previous.z;const float len=std::hypot(dx,dz);dx/=len;dz/=len;
 camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(end.x+8,end.y+9,end.z+8,1),XMLoadFloat3(&end),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 app.ActivatePath(&path);wi::renderer::SetTemporalAAEnabled(false);
 auto frames=[&](int n){for(int i=0;i<n;++i){app.Run();Sleep(8);}wi::graphics::GetDevice()->WaitForGPU();};
 auto capture=[&](const char* name){frames(20);Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(output/name).string()),"capture");};
 if(argc==4 && std::string(argv[3])=="deep")
 {
  auto centre=points[points.size()/2];centre.y-=.3f;const auto terrain=scene.terrains.GetEntity(0);std::string error;bool ready=false;
  wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[&](uint64_t){
   auto* spline=scene.splines.GetComponent(river);
   for(const auto node:spline->spline_node_entities){auto* transform=scene.transforms.GetComponent(node);transform->translation_local.y-=.3f;transform->SetDirty();transform->UpdateTransform();}
   spline->width=6;spline->SetDirty();
   auto* water=scene.materials.GetComponent(river);water->texMulAdd.x=water->texMulAdd.y=3;water->SetDirty();
   for(size_t i=0;i<scene.weathers.GetCount();++i)scene.weathers[i].ambient={.08f,.08f,.08f};
   const auto sun=scene.Entity_CreateLight("Deep river test sun",{0,30,68},{1,.95f,.85f},4,200,wi::scene::LightComponent::DIRECTIONAL);
   scene.lights.GetComponent(sun)->SetCastShadow(true);scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3(1.35f,-.78f,0));
   ready=SetRiverTerrainLayer(scene,terrain,river,3.f,.5f,true,error);
   if(!ready)return;
   auto start=ResolvePlayerStart(scene);
   if(start.resolution==PlayerStartResolution::Missing){
    TransformState pose;pose.translation={centre.x,centre.y-.9f,centre.z};
    CreatePlayerStartCommand create(scene,pose);ready=create.Execute();start=ResolvePlayerStart(scene);
   }
   if(start.resolution!=PlayerStartResolution::Success){ready=false;error="Deep fixture needs one player start";return;}
   auto* transform=scene.transforms.GetComponent(start.start.entity);
   transform->translation_local={centre.x,centre.y-.9f,centre.z};transform->rotation_local={0,0,0,1};transform->SetDirty();transform->UpdateTransform();
  });
  frames(4);Require(ready,error);
  const auto bed=wi::scene::Pick(wi::primitive::Ray(XMFLOAT3(centre.x,centre.y+20,centre.z),XMFLOAT3(0,-1,0)),wi::enums::FILTER_TERRAIN,~0u,scene);
  Require(bed.entity!=wi::ecs::INVALID_ENTITY&&centre.y-bed.position.y>2.6f,"Deep test bed must have actual head clearance");
  camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(centre.x+5,centre.y+7,centre.z-5,1),XMLoadFloat3(&centre),XMVectorSet(0,1,0,0))));camera.UpdateCamera();capture("deep-river-overview.png");
  const auto saved=(output/"persistent-river-layers.wiscene").string();wi::Archive archive(saved,false,false);scene.Serialize(archive);Require(archive.SaveFile(saved),"save deep fixture");
  std::cout<<"DEEP_RIVER=PASS measuredDepth="<<centre.y-bed.position.y<<" spawn="<<centre.x<<","<<centre.y-.9f<<","<<centre.z<<" scene="<<saved<<"\n";return 0;
 }
 capture("old-rounded-trench-square-water.png");
 const auto terrain=scene.terrains.GetEntity(0);TerrainSculptState baseline;std::string error;Require(ReadRiverTerrainBaseline(scene,terrain,baseline,error),error);
 auto before=CaptureTerrainSculpt(scene,scene.terrains[0]);bool recomposed=false;
 wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[&](uint64_t){recomposed=RecomposeRiverTerrainLayers(scene,terrain,error);});frames(2);Require(recomposed,error);capture("contained-rounded-ending.png");
 auto after=CaptureTerrainSculpt(scene,scene.terrains[0]);size_t restored=0,exterior=0;
 for(size_t c=0;c<after.chunks.size();++c){auto* mesh=scene.meshes.GetComponent(after.chunks[c].entity);const auto world=scene.ComputeEntityMatrixRecursive(after.chunks[c].entity);
 for(size_t i=0;i<mesh->vertex_positions.size();++i){XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[i]),world));float outward=(p.x-end.x)*dx+(p.z-end.z)*dz;float side=std::abs((p.x-end.x)*dz-(p.z-end.z)*dx);
 if(outward>.01f&&outward<2&&side<1.5f){++exterior;if(after.chunks[c].heights[i]>before.chunks[c].heights[i]+.001f)++restored;Require(std::abs(after.chunks[c].heights[i]-baseline.chunks[c].heights[i])<.001f,"exterior trench not restored");}}}
 Require(exterior>0&&restored>0,"fixture did not exercise former exterior trench");
 // Disk reload then repeat composition must retain exact terrain and metadata.
 const auto saved=(output/"contained-endpoint.wiscene").string();wi::Archive archive(saved,false,false);scene.Serialize(archive);Require(archive.SaveFile(saved),"save");
 wi::scene::Scene reopened;wi::Archive disk(saved,true,false);reopened.Serialize(disk);for(size_t i=0;i<reopened.terrains.GetCount();++i){reopened.terrains[i].scene=&reopened;reopened.terrains[i].terrainEntity=reopened.terrains.GetEntity(i);}auto reopenedID=reopened.terrains.GetEntity(0);
 auto savedHeights=CaptureTerrainSculpt(reopened,reopened.terrains[0]);Require(RecomposeRiverTerrainLayers(reopened,reopenedID,error),error);auto repeated=CaptureTerrainSculpt(reopened,reopened.terrains[0]);Require(RetainChangedTerrainSculpt(savedHeights,repeated)==0,"reload composition changed terrain");
 for(size_t i=0;i<reopened.terrains.GetCount();++i)reopened.terrains[i].Generation_Cancel();
 std::cout<<"NATIVE_ENDPOINT=PASS exteriorVertices="<<exterior<<" restored="<<restored<<" end="<<end.x<<","<<end.y<<","<<end.z<<" diskRepeat=PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
