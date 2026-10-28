#include "RiverWaterwayFixture.h"
#include "../EngineBridge/src/RiverWaterwaySurfaceBinding.h"
#include "../EngineBridge/src/RiverWaterwayRenderer.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <optional>
#include <chrono>
using namespace waterway_proof;namespace fs=std::filesystem;
static void Require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
struct ComposedRenderer {
 RiverWaterwayRenderer isolated;RiverFlowRenderer combined;bool composed=false;wi::ecs::Entity root=wi::ecs::INVALID_ENTITY;
 std::optional<RiverFlowRenderer::BindingScope> scope;
 explicit ComposedRenderer(const std::string& directory):isolated(directory),combined(directory){}
 void Prepare(wi::scene::Scene& scene,uint64_t revision,float dt,uint64_t edits){root=scene.Entity_FindByName("Receiving native river");if(composed)combined.Prepare(scene,revision,dt,edits);else isolated.Prepare(scene,revision,dt,edits);}
 bool Ready()const{return composed?combined.ActiveCount()>0:isolated.Ready();}
 const std::string& Status()const{return composed?combined.Status(root):isolated.Status();}
 void Apply(){if(composed)scope.emplace(combined);else isolated.Apply();}
 void Restore(){if(composed)scope.reset();else isolated.Restore();}
 void Dispatch(){if(composed)combined.Dispatch();else isolated.Dispatch();}
 bool Diagnostics(RiverFlowDiagnostics& d,std::string& error,bool readback){return composed?combined.Diagnostics(root,d,error,readback):isolated.Diagnostics(d,error,readback);}
};
struct Path:wi::RenderPath3D {
 RiverWaterwaySurfaceBinding* binding=nullptr;ComposedRenderer* network=nullptr;uint64_t editRevision=0;bool joined=false;unsigned applied=0;float flowDt=-1;double prepareMilliseconds=0;
 void Update(float dt)override{if(joined&&network){auto began=std::chrono::steady_clock::now();network->Prepare(*scene,1,flowDt<0?dt:flowDt,editRevision);prepareMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();Require(network->Ready(),network->Status());network->Apply();try{wi::RenderPath3D::Update(dt);}catch(...){network->Restore();throw;}network->Restore();++applied;}else if(joined){auto scope=binding->Bind();Require(scope.Applied(),binding->Error());++applied;wi::RenderPath3D::Update(dt);}else wi::RenderPath3D::Update(dt);}
 void PreRender()override{if(joined&&network){network->Apply();try{wi::RenderPath3D::PreRender();}catch(...){network->Restore();throw;}network->Restore();}else if(joined){auto scope=binding->Bind();Require(scope.Applied(),binding->Error());wi::RenderPath3D::PreRender();}else wi::RenderPath3D::PreRender();}
 void Render()const override{if(joined&&network){network->Apply();try{network->Dispatch();wi::RenderPath3D::Render();}catch(...){network->Restore();throw;}network->Restore();}else if(joined){auto scope=binding->Bind();Require(scope.Applied(),binding->Error());wi::RenderPath3D::Render();}else wi::RenderPath3D::Render();}
};
struct Original {wi::ecs::Entity root;std::vector<XMFLOAT3> positions;std::vector<uint32_t> indices;uint32_t flags;XMFLOAT4 tiling;int shader;XMUINT4 userdata;};
static std::vector<Original> Capture(wi::scene::Scene& scene,const std::vector<wi::ecs::Entity>& members){
 std::vector<Original> out;for(auto e:members){const auto& mesh=*scene.meshes.GetComponent(e);const auto& material=*scene.materials.GetComponent(e);
  out.push_back({e,{mesh.vertex_positions.begin(),mesh.vertex_positions.end()},{mesh.indices.begin(),mesh.indices.end()},scene.objects.GetComponent(e)->_flags,material.texMulAdd,material.customShaderID,material.userdata});}return out;
}
static void Restored(wi::scene::Scene& scene,const std::vector<Original>& originals){
 for(const auto& o:originals){const auto& mesh=*scene.meshes.GetComponent(o.root);const auto& m=*scene.materials.GetComponent(o.root);
  Require(mesh.vertex_positions.size()==o.positions.size()&&std::memcmp(mesh.vertex_positions.data(),o.positions.data(),o.positions.size()*sizeof(XMFLOAT3))==0,"Authored native positions not restored");
  Require(mesh.indices.size()==o.indices.size()&&std::equal(mesh.indices.begin(),mesh.indices.end(),o.indices.begin()),"Authored native indices not restored");
  Require(scene.objects.GetComponent(o.root)->_flags==o.flags,"Authored object flags not restored");
  Require(m.customShaderID==o.shader&&std::memcmp(&m.userdata,&o.userdata,sizeof(o.userdata))==0&&std::memcmp(&m.texMulAdd,&o.tiling,sizeof(o.tiling))==0,"Native material changed by surface binding");
 }
}
static wi::ecs::Entity Root(wi::scene::Scene& scene){return scene.Entity_FindByName("Receiving native river");}
static std::vector<wi::ecs::Entity> Members(wi::scene::Scene& scene){return {Root(scene),scene.Entity_FindByName("Native tributary")};}
static std::shared_ptr<const RiverJunctionSurface> Surface(wi::scene::Scene& scene){RiverWaterway graph;std::string error;Require(ReadRiverWaterway(scene,graph,error)&&graph.network,error);std::shared_ptr<const RiverJunctionSurface> s;Require(BuildRiverJunctionSurface(*graph.network,s,error),error);return s;}
int main(int argc,char** argv){if(argc!=4&&argc!=5)return 2;std::cout.setf(std::ios::unitbuf);const bool reopen=std::string(argv[1])=="--reopen";const bool rapids=argc==5&&std::string(argv[4])=="--rapids";const fs::path out=fs::absolute(argv[2]),reference=fs::absolute(argv[3]);fs::create_directories(out);
 HWND window=CreateWindowExW(0,L"STATIC",L"River G7D scoped native WATER binding proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();auto* gpu=wi::graphics::GetDevice();
 try{
  wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);SceneService source;Require(source.LoadScene(reference.generic_u8string()),source.LastError());
  Fixture fixture(true,true);auto& scene=fixture.scene;std::string error;
  if(reopen){scene.Clear();wi::Archive archive((out/"authoritative.wiscene").generic_u8string(),true);Require(archive.IsOpen(),"Reopen authoritative scene");scene.Serialize(archive);}
  else {
   const auto* optics=source.GetScene().materials.GetComponent(source.GetScene().Entity_FindByName("River Water 5"));
   if(!optics)for(size_t k=0;k<source.GetScene().materials.GetCount();++k)if(source.GetScene().materials[k].shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER){optics=&source.GetScene().materials[k];break;}
   Require(optics!=nullptr,"Reference native WATER material missing");
   for(auto e:Members(scene)){auto material=*optics;material.DisableCustomShader();material.userdata={};*scene.materials.GetComponent(e)=material;}
   SetRiverConnectionCommand connect(scene,fixture.tributary,fixture.receiver,fixture.node);Require(connect.Execute(),connect.Error());
   // A textured submerged native PBR bed makes real WATER refraction visible.
   auto bed=scene.Entity_CreateObject("Disposable textured shallow bed");auto& bedMesh=scene.meshes.Create(bed);bedMesh.SetDoubleSided(true);
   const int nx=80,nz=40;for(int z=0;z<=nz;++z)for(int x=0;x<=nx;++x){float px=-28+x*56.f/nx,pz=-14+z*28.f/nz;
    bedMesh.vertex_positions.push_back({px,(rapids?-.45f+.34f*std::exp(-std::pow(px/3.f,4.f)):-.3f+.025f*std::sin(px)*std::cos(pz)),pz});bedMesh.vertex_normals.push_back({0,1,0});bedMesh.vertex_tangents.push_back({0,0,1,1});bedMesh.vertex_uvset_0.push_back({px,pz});}
   for(int z=0;z<nz;++z)for(int x=0;x<nx;++x){uint32_t a=z*(nx+1)+x,bb=a+1,c=a+nx+1,d=c+1;for(auto index:{a,c,bb,bb,c,d})bedMesh.indices.push_back(index);}
   auto& part=bedMesh.subsets.emplace_back();part.materialID=bed;part.indexCount=uint32_t(bedMesh.indices.size());bedMesh.CreateRenderData();scene.objects.GetComponent(bed)->meshID=bed;
   auto& ground=scene.materials.Create(bed);bool foundGround=false;
   const auto sourceBed=source.GetScene().Entity_FindByName("Shallow rocky confluence bed");if(auto* object=source.GetScene().objects.GetComponent(sourceBed))if(auto* mesh=source.GetScene().meshes.GetComponent(object->meshID))if(!mesh->subsets.empty())if(auto* m=source.GetScene().materials.GetComponent(mesh->subsets.front().materialID)){ground=*m;foundGround=true;}
   if(!foundGround)for(size_t k=0;k<source.GetScene().materials.GetCount();++k){const auto& m=source.GetScene().materials[k];if(m.shaderType==wi::scene::MaterialComponent::SHADERTYPE_PBR&&m.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.GetTexture().IsValid()){ground=m;foundGround=true;break;}}
   Require(foundGround,"Reference textured riverbed material missing");ground.DisableCustomShader();ground.userdata={};ground.texMulAdd={.5f,.5f,0,0};ground.roughness=.9f;
   auto parent=scene.hierarchy.GetComponent(Root(scene))->parentID;scene.Component_Attach(bed,parent,true);
   // Two actual closed native mesh obstacles intersect the water at independent inlets.
   for(auto centre:{XMFLOAT2{-10,0},XMFLOAT2{-8,-6}}){auto rock=scene.Entity_CreateObject("Disposable static inlet contact");auto& mesh=scene.meshes.Create(rock);mesh.SetDoubleSided(true);
    const float x=centre.x,z=centre.y;const XMFLOAT3 v[]={{x-.15f,-.3f,z-.15f},{x+.15f,-.3f,z-.15f},{x+.15f,-.3f,z+.15f},{x-.15f,-.3f,z+.15f},{x-.15f,.3f,z-.15f},{x+.15f,.3f,z-.15f},{x+.15f,.3f,z+.15f},{x-.15f,.3f,z+.15f}};
    for(auto p:v){mesh.vertex_positions.push_back(p);mesh.vertex_normals.push_back({0,1,0});mesh.vertex_tangents.push_back({0,0,1,1});mesh.vertex_uvset_0.push_back({p.x,p.z});}
    const uint32_t ix[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};mesh.indices.assign(std::begin(ix),std::end(ix));auto& subset=mesh.subsets.emplace_back();subset.materialID=rock;subset.indexCount=36;mesh.CreateRenderData();scene.objects.GetComponent(rock)->meshID=rock;scene.materials.Create(rock)=ground;scene.Component_Attach(rock,parent,true);
   }
   auto sun=scene.Entity_CreateLight("Disposable sunlight");auto* light=scene.lights.GetComponent(sun);light->SetType(wi::scene::LightComponent::DIRECTIONAL);light->intensity=4;
   auto* sunTransform=scene.transforms.GetComponent(sun);XMStoreFloat4(&sunTransform->rotation_local,XMQuaternionRotationRollPitchYaw(.9f,.4f,0));sunTransform->SetDirty();sunTransform->UpdateTransform();
   scene.weather=wi::scene::WeatherComponent{};scene.weather.SetRealisticSky(false);scene.weather.SetVolumetricClouds(false);
   scene.weather.fogStart=1000;scene.weather.fogDensity=0;scene.weather.ambient={.22f,.25f,.3f};scene.weather.horizon={.5f,.6f,.7f};scene.weather.zenith={.08f,.2f,.4f};
   scene.weathers.Create(scene.Entity_CreateTransform("Disposable persistent gradient weather"))=scene.weather;
  }
  if(rapids){RiverFlowProfile p;Require(ReadRiverFlowProfile(scene,Root(scene),p,error),error);
   if(reopen)Require(p.rapids&&p.rapidIntensity==1,"Serialized opt-in rapids profile missing");
   else {p.rapids=true;SetRiverFlowProfileCommand enable(scene,Root(scene),p);Require(enable.Execute(),"Enable measured rapid profile");}
  }
  wi::renderer::SetGameSpeed(1);wi::scene::CameraComponent camera;camera.CreatePerspective(944,601,.05f,500,XMConvertToRadians(60));

  RiverWaterwaySurfaceBinding binding;ComposedRenderer network((out.parent_path()/"network-shaders").generic_u8string());network.composed=argc==5&&(std::string(argv[4])=="--composed"||std::string(argv[4])=="--animated"||rapids);Path path;path.binding=&binding;if(argc==5)path.network=&network;path.scene=&scene;path.camera=&camera;path.init(944,601,96);path.ResizeBuffers();wi::renderer::SetTemporalAAEnabled(false);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);path.setOcclusionCullingEnabled(false);
  camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(45,22,-40,1),XMVectorSet(40,3,-25,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
  struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}guard{app};app.ActivatePath(&path);
  for(int k=0;k<90;++k)app.Run();while(wi::renderer::IsPipelineCreationActive())app.Run();gpu->WaitForGPU();wi::renderer::SetGameSpeed(0);for(int k=0;k<16;++k)app.Run();gpu->WaitForGPU();
  std::cout<<"CAMERA="<<camera.Eye.x<<","<<camera.Eye.y<<","<<camera.Eye.z<<" visibleObjects="<<path.visibility_main.visibleObjects.size()<<"\n";
  std::cout<<"WEATHER fog="<<scene.weather.fogStart<<","<<scene.weather.fogDensity<<" weatherComponents="<<scene.weathers.GetCount()<<"\n";
  for(size_t k=0;k<scene.objects.GetCount();++k){auto e=scene.objects.GetEntity(k);const auto* m=scene.meshes.GetComponent(scene.objects[k].meshID);auto p=scene.ComputeEntityMatrixRecursive(e).r[3];XMFLOAT3 position;XMStoreFloat3(&position,p);std::cout<<"VISIBLE_OBJECT="<<e<<" world="<<position.x<<","<<position.y<<","<<position.z<<" vertices="<<(m?m->vertex_positions.size():0)<<" indices="<<(m?m->indices.size():0)<<"\n";}
  {wi::vector<uint8_t> bytes;Require(wi::helper::saveTextureToMemory(path.rtPrimitiveID,bytes),"Primitive ID readback");std::ofstream f((out/"native-primitive-id.bin").string(),std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());std::cout<<"PRIMITIVE_ID_BYTES="<<bytes.size()<<" format="<<int(path.rtPrimitiveID.GetDesc().format)<<"\n";}
  {std::ofstream triangles((out/"native-water-screen.csv").string());for(auto e:Members(scene)){const auto* mesh=scene.meshes.GetComponent(e);auto world=scene.ComputeEntityMatrixRecursive(e);auto matrix=world*camera.GetViewProjection();for(size_t k=0;k+2<mesh->indices.size();k+=3){for(int j=0;j<3;++j){auto p=mesh->vertex_positions[mesh->indices[k+j]];auto q=XMVector4Transform(XMVectorSet(p.x,p.y,p.z,1),matrix);XMFLOAT4 clip;XMStoreFloat4(&clip,q);triangles<<(clip.x/clip.w*.5f+.5f)*944<<","<<(.5f-clip.y/clip.w*.5f)*601<<(j==2?"\n":",");}}}}
  const auto members=Members(scene);auto originals=Capture(scene,members);const auto objects=scene.objects.GetCount(),meshes=scene.meshes.GetCount(),materials=scene.materials.GetCount();
  auto surface=Surface(scene);Require(binding.Configure(scene,Root(scene),members,*surface,4,error),error);
  while(gpu->GetFrameCount()%256)app.Run();gpu->WaitForGPU();Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/(reopen?"fresh-native.png":"native-before.png")).generic_u8string()),"Native image");
  {
   auto scope=binding.Bind();Require(scope.Applied(),binding.Error());Require(binding.IsBound(),"Scope bound");auto nested=binding.Bind();Require(!nested.Applied()&&binding.IsBound(),"Nested scope must not release outer binding");
   const auto* mesh=scene.meshes.GetComponent(Root(scene));Require(mesh->indices.size()==surface->Indices().size(),"Native mesh slot does not own union during scope");
   auto crossY=[](XMFLOAT3 a,XMFLOAT3 b,XMFLOAT3 c){return (double(b.z)-a.z)*(c.x-a.x)-(double(b.x)-a.x)*(c.z-a.z);};
   const auto& original=originals.front();const auto& nativePositions=original.positions;const auto& nativeIndices=original.indices;
   const auto nativeSign=crossY(nativePositions[nativeIndices[0]],nativePositions[nativeIndices[1]],nativePositions[nativeIndices[2]]);
   const auto joinedSign=crossY(mesh->vertex_positions[mesh->indices[0]],mesh->vertex_positions[mesh->indices[1]],mesh->vertex_positions[mesh->indices[2]]);
   Require(nativeSign*joinedSign>0,"Joined front-face convention differs from native WATER");std::cout<<"NATIVE_WATER_FRONT_FACE=PASS native="<<nativeSign<<" joined="<<joinedSign<<"\n";
   size_t visible=0;for(auto e:members)visible+=scene.objects.GetComponent(e)->IsRenderable();Require(visible==1,"Joined surface must render once");
   const auto world=scene.ComputeEntityMatrixRecursive(Root(scene));float maximum=0;
   for(size_t k=0;k<surface->Positions().size();++k){XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[k]),world));maximum=std::max(maximum,Distance(p,surface->Positions()[k]));}
   Require(maximum<.00002f,"Parent-transformed joined mesh differs from native footprint");std::cout<<"SCOPED_WORLD_VERTICES=PASS maxError="<<maximum<<"\n";
  }
  Restored(scene,originals);
  auto* metadata=scene.metadatas.GetComponent(members[1]);const auto connection=metadata->string_values.get(RiverConnectionMetadataKey);metadata->string_values.erase(RiverConnectionMetadataKey);
  {auto stale=binding.Bind();Require(!stale.Applied(),"Metadata-only disconnect must invalidate joined binding");}metadata->string_values.set(RiverConnectionMetadataKey,connection);Restored(scene,originals);
  auto* incoming=scene.objects.GetComponent(members[1]);const auto flags=incoming->_flags;incoming->SetRenderable(false);
  {auto stale=binding.Bind();Require(!stale.Applied(),"Explicitly hidden source must not become joined visible water");}Require(!incoming->IsRenderable(),"Owner visibility preserved on refusal");incoming->_flags=flags;
  path.joined=true;
  for(int k=0;k<90;++k){app.Run();Restored(scene,originals);Require(scene.objects.GetCount()==objects&&scene.meshes.GetCount()==meshes&&scene.materials.GetCount()==materials,"Transient binding changed native component counts");}
  gpu->WaitForGPU();while(gpu->GetFrameCount()%256)app.Run();gpu->WaitForGPU();Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/(reopen?"fresh-joined.png":"joined.png")).generic_u8string()),"Joined image");
  if(path.network){RiverFlowDiagnostics diagnostics;Require(network.Diagnostics(diagnostics,error,true),error);Require(diagnostics.foamMass>0,"Shared GPU domain must contain actual contact foam");Require(diagnostics.maximumDetail>0,"Shared source-born foam detail missing");Require(diagnostics.rebuilds==1,"Static connected domain rebuilt per frame");std::cout<<"SHARED_GPU_CONTACT=PASS mass="<<diagnostics.foamMass<<" maximum="<<diagnostics.maximumDensity<<" rebuilds="<<diagnostics.rebuilds<<" cells="<<diagnostics.cells<<"\n";}
  if(path.network){
   path.flowDt=0;app.Run();gpu->WaitForGPU();
   RiverFlowDiagnostics before;Require(network.Diagnostics(before,error,true),error);
   RiverFlowProfile original;Require(ReadRiverFlowProfile(scene,Root(scene),original,error),error);
   for(int mode=0;mode<(rapids?6:5);++mode){
    auto next=original;if(mode==0)next.intensity=0;if(mode==1){next.intensity=2;next.threshold=.07f;}if(mode==2)next.foam=false;if(mode==3)next.lifetime=4.1f;if(mode==4)next.sourceWidth=.075f;if(mode==5)next.rapidIntensity=0;
    SetRiverFlowProfileCommand edit(scene,Root(scene),next);Require(edit.Execute(),"Live shared profile command");++path.editRevision;
    app.Run();gpu->WaitForGPU();RiverFlowDiagnostics after;Require(network.Diagnostics(after,error,true),error);
    Require(after.rebuilds==before.rebuilds&&after.foamMass==before.foamMass&&after.maximumDensity==before.maximumDensity&&after.detailSignature==before.detailSignature,"Live foam setting reset or advanced frozen history");
    Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/("live-profile-"+std::to_string(mode)+".png")).generic_u8string()),"Live settings capture");
    edit.Undo();++path.editRevision;app.Run();gpu->WaitForGPU();
    Require(network.Diagnostics(after,error,true)&&after.rebuilds==before.rebuilds&&after.foamMass==before.foamMass&&after.maximumDensity==before.maximumDensity&&after.detailSignature==before.detailSignature,"Profile Undo reset frozen history");
   }
   if(rapids){
    auto nativeContact=original;nativeContact.rapids=false;
    SetRiverFlowProfileCommand toggle(scene,Root(scene),nativeContact);
    RiverFlowDiagnostics initial;Require(network.Diagnostics(initial,error,false),error);
    Require(toggle.Execute(),"Switch rapids off");++path.editRevision;app.Run();gpu->WaitForGPU();
    RiverFlowDiagnostics current;Require(network.Diagnostics(current,error,true)&&current.rebuilds==initial.rebuilds+1,"Legacy shader/domain switch rebuilds once");
    toggle.Undo();++path.editRevision;app.Run();gpu->WaitForGPU();
    Require(network.Diagnostics(current,error,true)&&current.rebuilds==initial.rebuilds+2,"Rapids shader/domain Undo rebuilds once");
    std::cout<<"RAPIDS_LEGACY_SHADER_TOGGLE=PASS\n";
   }
   path.flowDt=-1;std::cout<<"LIVE_SHARED_PROFILE_HISTORY=PASS amount0/2 threshold foam-toggle lifetime Undo no domain rebuild frozen mass/max/detail exact\\n";
  }
  std::cout<<"SCOPED_NATIVE_UPDATE_RENDER=PASS frames="<<path.applied<<" nativeObjects="<<objects<<" nativeMeshes="<<meshes<<" reflectionVisible="<<path.visibility_main.planar_reflection_visible<<" reflectionPlaneY="<<path.visibility_main.reflectionPlane.w<<"\n";
  try{auto scope=binding.Bind();Require(scope.Applied(),binding.Error());throw 1;}catch(int){}Restored(scene,originals);
  path.joined=false;for(int k=0;k<16;++k)app.Run();gpu->WaitForGPU();Restored(scene,originals);
  while(gpu->GetFrameCount()%256)app.Run();gpu->WaitForGPU();Require(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/(reopen?"fresh-restored.png":"native-restored.png")).generic_u8string()),"Restored image");
  if(!reopen){
   auto before=CaptureTransform(*scene.transforms.GetComponent(fixture.node));auto after=before;after.translation.x+=.7f;after.translation.z+=.3f;
   SetWatercourseTransformCommand move(scene,fixture.node,before,after);Require(move.Execute(),"Connected edit for binding invalidation");
   {auto stale=binding.Bind();Require(!stale.Applied(),"Stale native geometry must refuse binding");}
   for(int k=0;k<4;++k)app.Run();surface=Surface(scene);Require(binding.Configure(scene,Root(scene),members,*surface,4,error),error);originals=Capture(scene,members);
   ++path.editRevision;path.joined=true;for(int k=0;k<16;++k){app.Run();Restored(scene,originals);}path.joined=false;gpu->WaitForGPU();
   std::cout<<"SCOPED_EDIT_REBUILD=PASS\n";
   wi::Archive archive((out/"authoritative.wiscene").generic_u8string(),false);Require(archive.IsOpen(),"Write authoritative native archive");scene.Serialize(archive);
  }
  if(path.network){
   path.joined=true;path.flowDt=1.f/60;
   for(auto entity:members){
    RiverFlowProfile original;Require(ReadRiverFlowProfile(scene,entity,original,error),error);
    auto changed=original;changed.speed=1.5f;
    network.Prepare(scene,1,0,path.editRevision);RiverFlowDiagnostics before;Require(network.Diagnostics(before,error,false),error);
    SetRiverFlowProfileCommand speed(scene,entity,changed);Require(speed.Execute(),"GPU inlet speed command");++path.editRevision;
    for(int k=0;k<8;++k)app.Run();gpu->WaitForGPU();RiverFlowDiagnostics after;Require(network.Diagnostics(after,error,true),error);
    Require(after.rebuilds==before.rebuilds+1&&after.foamMass>0,"Inlet speed must rebuild once and resume actual GPU foam");
    speed.Undo();++path.editRevision;for(int k=0;k<8;++k)app.Run();gpu->WaitForGPU();
    Require(network.Diagnostics(after,error,false)&&after.rebuilds==before.rebuilds+2,"Speed Undo rebuilds exactly once");
   }
   std::cout<<"LIVE_INLET_SPEED_GPU=PASS receiver/source commit Undo rebuild once and resume foam\n";
  }
  if(argc==5&&(std::string(argv[4])=="--animated"||rapids)){
   path.joined=true;path.flowDt=1.f/60;
   network.Prepare(scene,1,0,path.editRevision);
   RiverFlowDiagnostics before;Require(network.Diagnostics(before,error,false),error);
   auto* material=scene.materials.GetComponent(Root(scene));const auto initial=material->texMulAdd;
   for(auto e:members){auto* m=scene.materials.GetComponent(e);m->texAnimDirection={.025f,-.035f};m->texAnimFrameRate=60;}
   wi::renderer::SetGameSpeed(1);double total=0,maximum=0;
   for(int frame=0;frame<30;++frame){app.Run();total+=path.prepareMilliseconds;maximum=std::max(maximum,path.prepareMilliseconds);}
   gpu->WaitForGPU();RiverFlowDiagnostics after;Require(network.Diagnostics(after,error,false),error);
   const auto offset=scene.materials.GetComponent(Root(scene))->texMulAdd;
   const auto rebuilds=after.rebuilds-before.rebuilds;
   std::cout<<"ANIMATED_CACHE rebuildDelta="<<rebuilds<<" prepareMeanMs="<<total/30<<" prepareMaxMs="<<maximum<<" offsetDelta="<<offset.z-initial.z<<","<<offset.w-initial.w<<"\\n";
   Require(offset.z!=initial.z||offset.w!=initial.w,"Native WATER UV animation did not advance");
   Require(rebuilds==0,"Native UV animation must not rebuild the shared flow domain");
   Require(total/30<10,"Static shared preparation exceeds 10ms budget");
   std::cout<<"ANIMATED_SHARED_CACHE=PASS\\n";
  }
  Require(wi::renderer::GetShaderErrorCount()==0,"Native shader errors");std::cout<<"WATERWAY_BINDING="<<(reopen?"FRESH_REOPEN_PASS":"PASS")<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"WATERWAY_BINDING=FAIL "<<e.what()<<"\n";gpu->WaitForGPU();return 1;}}
