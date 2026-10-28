#include "RiverWaterwayFixture.h"
#include "../EngineBridge/src/RiverRapidsTransport.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "../EngineBridge/src/RiverWaterwayTransport.h"
#include "renegade/bridge/SceneService.h"
#include "../EngineBridge/src/RiverFlowSourceSelection.h"
#include <unordered_set>
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
using namespace waterway_proof;namespace fs=std::filesystem;
static void Require(bool ok,const std::string& error){if(!ok)throw std::runtime_error(error);}
#include "../EngineBridge/src/RiverFlowGather.inc"
static void Verify(wi::scene::Scene& scene,const fs::path& file,bool reopen){
 std::string error;RiverWaterway graph;Require(ReadRiverWaterway(scene,graph,error),error);
 Require(graph.network&&graph.reaches.size()==3,"Saved interior attachment must resolve three native spans");
 std::shared_ptr<const RiverJunctionSurface> joined;Require(BuildRiverJunctionSurface(*graph.network,joined,error),error);
 struct Tri{XMFLOAT3 a,b,c;};std::vector<Tri> native;float minX=1e9f,maxX=-1e9f,minZ=1e9f,maxZ=-1e9f;double capError=0;
 for(auto river:{scene.Entity_FindByName("Receiving native river"),scene.Entity_FindByName("Native tributary")}){
  const auto* mesh=scene.meshes.GetComponent(river);Require(mesh&&mesh->vertex_positions.size()>=4,"Actual native Scene::Update geometry missing");auto world=scene.ComputeEntityMatrixRecursive(river);
  std::vector<XMFLOAT3> vertices;for(auto p:mesh->vertex_positions){XMFLOAT3 v;XMStoreFloat3(&v,XMVector3TransformCoord(XMLoadFloat3(&p),world));vertices.push_back(v);minX=std::min(minX,v.x);maxX=std::max(maxX,v.x);minZ=std::min(minZ,v.z);maxZ=std::max(maxZ,v.z);}
  for(size_t k=0;k+2<mesh->indices.size();k+=3)native.push_back({vertices[mesh->indices[k]],vertices[mesh->indices[k+1]],vertices[mesh->indices[k+2]]});
  for(const auto& reach:graph.reaches)if(reach.river==river){const auto* f=graph.network->FindReach(reach.id);Require(f!=nullptr,"Native logical span missing");
   std::cout<<"NATIVE_ROW_COUNTS river="<<river<<" actual="<<vertices.size()/2<<" derived="<<f->field->NativeRibbon().size()<<" start="<<reach.firstNativeParameter<<" end="<<reach.lastNativeParameter<<"\n";
   for(const auto& row:f->field->NativeRibbon()){
    const float t=row.nativeParameter*float(vertices.size()/2-1);size_t k=std::min(size_t(t),vertices.size()/2-2);float blend=t-float(k);
    for(bool left:{false,true}){const auto a=vertices[k*2+(left?1:0)],b=vertices[(k+1)*2+(left?1:0)];XMFLOAT3 expected{a.x+(b.x-a.x)*blend,a.y+(b.y-a.y)*blend,a.z+(b.z-a.z)*blend};
     const double measured=Distance(expected,left?row.leftBank:row.rightBank);
     if(measured>capError){auto derived=left?row.leftBank:row.rightBank;std::cout<<"NATIVE_ERROR t="<<row.nativeParameter<<" k="<<k<<" blend="<<blend<<" expected="<<expected.x<<","<<expected.y<<","<<expected.z<<" derived="<<derived.x<<","<<derived.y<<","<<derived.z<<" error="<<measured<<"\n";capError=measured;}}
   }
  }
 }
 Require(capError<1e-4,"Logical span differs from real native generated bank edges");size_t covered=0;
 for(int x=0;x<320;++x)for(int z=0;z<140;++z){XMFLOAT2 p{minX+(x+.317f)/320*(maxX-minX),minZ+(z+.193f)/140*(maxZ-minZ)};bool oracle=false;
  for(auto t:native)if(junction_proof::Inside(p,t.a,t.b,t.c)){oracle=true;break;}
  const bool actual=joined->Evaluate(p).valid;Require(actual==oracle,"Joined logical spans changed actual native triangle footprint");covered+=oracle;}
 std::cout<<"ACTUAL_NATIVE_SPAN_PARITY=PASS rowsMaxError="<<capError<<" footprintSamples=44800 covered="<<covered<<" hash="<<graph.network->DeterministicHash()<<"\n";
 if(reopen){std::ifstream in(file.string()+".hash");uint64_t hash=0;in>>hash;Require(hash==graph.network->DeterministicHash(),"Fresh-process UUID topology/native span hash mismatch");}
 else{wi::Archive archive(file.generic_u8string(),false);Require(archive.IsOpen(),"Native archive write");scene.Serialize(archive);std::ofstream(file.string()+".hash")<<graph.network->DeterministicHash();}
}
int main(int argc,char** argv){const bool prepare=argc==4&&std::string(argv[1])=="--rapids-prepare";if(argc!=3&&!prepare)return 2;std::cout.setf(std::ios::unitbuf);const bool draw=std::string(argv[1])=="--draw";const bool reopen=std::string(argv[1])=="--reopen";auto file=fs::absolute(argv[2]);const bool inspect=std::string(argv[1])=="--inspect"||prepare;if(!reopen&&!inspect&&fs::exists(file))return 2;
 HWND window=CreateWindowExW(0,L"STATIC",L"River G7D native span and UUID oracle",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();auto* gpu=wi::graphics::GetDevice();
 try{
  if(prepare)wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
  Fixture fixture(true,true);auto& scene=fixture.scene;
  if(inspect){SceneService loaded;Require(loaded.LoadScene(file.generic_u8string()),loaded.LastError());scene.Clear();scene.Merge(loaded.GetScene());}
  if(reopen){scene.Clear();wi::Archive archive(file.generic_u8string(),true);Require(archive.IsOpen(),"Native archive read");scene.Serialize(archive);}
  wi::scene::CameraComponent camera;camera.CreatePerspective(320,240,.1f,500,XMConvertToRadians(60));
  camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(40,30,-40,1),XMVectorSet(40,3,-25,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
  wi::RenderPath3D path;path.scene=&scene;path.camera=&camera;path.init(320,240,96);path.ResizeBuffers();
  struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}guard{app};app.ActivatePath(&path);
  for(int k=0;k<60;++k)app.Run();gpu->WaitForGPU();
  if(prepare){
   const auto output=fs::absolute(argv[3]);Require(!fs::exists(output)&&output!=file,"Only a fresh disposable output is permitted");
   Require(scene.terrains.GetCount()==1,"Rapids review requires one native terrain");
   auto terrain=scene.terrains.GetEntity(0);std::string error;
   for(const char* name:{"River Water 5","River Water 6"}){
    auto river=scene.Entity_FindByName(name);RiverTerrainLayerSettings bed;
    Require(ReadRiverTerrainLayerSettings(scene,terrain,river,bed,error)&&bed.linked,error);
    SetRiverTerrainLayerCommand shallow(scene,terrain,river,.45f,bed.bankWidthMeters);
    Require(shallow.Execute(),shallow.Error());
    for(int k=0;k<4;++k)app.Run();gpu->WaitForGPU();
    RiverFlowProfile p;Require(ReadRiverFlowProfile(scene,river,p,error),error);
    p.speed=.35f;p.rapids=std::string(name)=="River Water 5";p.rapidIntensity=1;
    SetRiverFlowProfileCommand profile(scene,river,p);Require(profile.Execute(),"Review-only profile edit");
   }
   for(int k=0;k<16;++k)app.Run();gpu->WaitForGPU();
   RiverWaterway graph;Require(ReadRiverWaterway(scene,graph,error)&&graph.network,error);
   std::shared_ptr<const RiverJunctionSurface> surface;Require(BuildRiverJunctionSurface(*graph.network,surface,error),error);
   river_flow_detail::SourceBounds bounds;
   for(const auto& reach:graph.network->Reaches()){auto q=river_flow_detail::CaptureSourceBounds(*reach.field);
    bounds.minimum={std::min(bounds.minimum.x,q.minimum.x),std::min(bounds.minimum.y,q.minimum.y),std::min(bounds.minimum.z,q.minimum.z)};
    bounds.maximum={std::max(bounds.maximum.x,q.maximum.x),std::max(bounds.maximum.y,q.maximum.y),std::max(bounds.maximum.z,q.maximum.z)};}

   auto surfaces=GatherBounds(scene,scene.Entity_FindByName("River Water 5"),bounds,nullptr,true);
   auto domain=river_rapids_transport::Build(*graph.network,*surface,surfaces);
   std::cout<<"ACTUAL_EDITOR_RAPIDS=PASS cells="<<domain.active.size()<<" discharge="<<domain.inflow<<" maxFr="<<domain.maximumFroude<<" rapidSource="<<domain.rapidEmission<<" divergence="<<domain.maximumDivergence<<"\n";
   wi::Archive archive(output.generic_u8string(),false);Require(archive.IsOpen(),"Disposable rapid scene write");archive.SetCompressionEnabled(true);scene.Serialize(archive);
   return 0;
  }
  if(inspect){
   RiverWaterway graph;std::string error;Require(ReadRiverWaterway(scene,graph,error)&&graph.network,error);
   std::shared_ptr<const RiverJunctionSurface> surface;Require(BuildRiverJunctionSurface(*graph.network,surface,error),error);
   for(const auto& reach:graph.network->Reaches()){
    for(auto port:{reach.upstream,reach.downstream})std::cout<<"PORT "<<reach.id<<" centre="<<port.centre.x<<","<<port.centre.y<<","<<port.centre.z<<" dir="<<port.directionXZ.x<<","<<port.directionXZ.y<<"\n";
   }
   const auto domain=river_waterway_transport::Build(*graph.network,*surface,{});
   river_flow_detail::SourceBounds bounds;
   for(const auto& reach:graph.network->Reaches()){auto b=river_flow_detail::CaptureSourceBounds(*reach.field);bounds.minimum={std::min(bounds.minimum.x,b.minimum.x),std::min(bounds.minimum.y,b.minimum.y),std::min(bounds.minimum.z,b.minimum.z)};bounds.maximum={std::max(bounds.maximum.x,b.maximum.x),std::max(bounds.maximum.y,b.maximum.y),std::max(bounds.maximum.z,b.maximum.z)};}
   std::unique_ptr<ICommand> bed; TerrainSculptState bedBefore;
   if(scene.terrains.GetCount()){
    auto terrain=scene.terrains.GetEntity(0);bedBefore=CaptureTerrainSculpt(scene,scene.terrains[0]);auto river=scene.Entity_FindByName("River Water 5");
    RiverTerrainLayerSettings settings;Require(ReadRiverTerrainLayerSettings(scene,terrain,river,settings,error),error);
    bed=std::make_unique<SetRiverTerrainLayerCommand>(scene,terrain,river,settings.depthMeters,settings.bankWidthMeters);
    Require(bed->Execute(),"Recompose actual connected terrain");
    for(int k=0;k<4;++k)app.Run();gpu->WaitForGPU();
   }
   auto obstacles=GatherBounds(scene,scene.Entity_FindByName("River Water 5"),bounds);
   for(size_t k=0;k<obstacles.size();++k){std::cout<<"OBSTACLE "<<k<<" terrain="<<obstacles[k].terrain<<" triangles="<<obstacles[k].triangles.size()<<"\n";try{auto isolated=river_waterway_transport::Build(*graph.network,*surface,{obstacles[k]});std::cout<<"ISOLATED_PASS fluid="<<isolated.active.size()<<"\n";}catch(const std::exception& ex){std::cout<<"ISOLATED_FAIL "<<ex.what()<<"\n";}}
   auto full=river_waterway_transport::Build(*graph.network,*surface,obstacles);
   std::cout<<"ACTUAL_SCENE_TRANSPORT_WITH_OBSTACLES=PASS fluid="<<full.active.size()<<"\n";
   if(bed){
    bed->Undo();auto restored=CaptureTerrainSculpt(scene,scene.terrains[0]);
    Require(restored.riverLayerDocument==bedBefore.riverLayerDocument&&restored.chunks.size()==bedBefore.chunks.size(),"Terrain Undo document");
    for(size_t k=0;k<restored.chunks.size();++k)Require(restored.chunks[k].heights==bedBefore.chunks[k].heights,"Terrain Undo exact heights");
    Require(bed->Execute(),"Terrain Redo");for(int k=0;k<4;++k)app.Run();gpu->WaitForGPU();
    auto again=river_waterway_transport::Build(*graph.network,*surface,GatherBounds(scene,scene.Entity_FindByName("River Water 5"),bounds));
    Require(again.active==full.active&&again.inflow==full.inflow&&again.outflow==full.outflow,"Terrain Redo exact transport topology");
    std::cout<<"ACTUAL_SCENE_BED_UNDO_REDO=PASS exact heights/document + transport topology\\n";
   }

   std::cout<<"ACTUAL_SCENE_TRANSPORT_NO_OBSTACLES=PASS fluid="<<domain.active.size()<<" inflow="<<domain.inflow<<"\n";return 0;
  }
  if(!reopen){const auto receiver=scene.Entity_FindByName("Receiving native river");const auto* mesh=scene.meshes.GetComponent(receiver);const auto positions=mesh->vertex_positions;const auto indices=mesh->indices;
   auto nodes=scene.splines.GetComponent(receiver)->spline_node_entities;auto node=nodes[2];auto source=scene.Entity_FindByName("Native tributary");
   std::unique_ptr<CreateRiverTributaryCommand> drawing;
   if(draw){
    XMFLOAT3 anchor;std::string error;Require(RiverConnectionAnchor(scene,receiver,node,anchor,error),error);
    std::vector<XMFLOAT3> points;for(auto n:scene.splines.GetComponent(source)->spline_node_entities){XMFLOAT3 p;XMStoreFloat3(&p,scene.ComputeEntityMatrixRecursive(n).r[3]);points.push_back(p);}
    std::reverse(points.begin(),points.end());points.back()=anchor;
    scene.names.GetComponent(source)->name="Unused original fixture branch";scene.objects.GetComponent(source)->SetRenderable(false);
    drawing=std::make_unique<CreateRiverTributaryCommand>(scene,points,receiver,node,3);
    Require(drawing->Execute(),drawing->Error());source=drawing->CreatedEntity();scene.names.GetComponent(source)->name="Native tributary";
   }else{
    SetRiverConnectionCommand connect(scene,source,receiver,node);Require(connect.Execute(),connect.Error());
   }for(int k=0;k<4;++k)app.Run();gpu->WaitForGPU();mesh=scene.meshes.GetComponent(receiver);
   Require(mesh->vertex_positions.size()==positions.size()&&std::memcmp(mesh->vertex_positions.data(),positions.data(),positions.size()*sizeof(XMFLOAT3))==0&&mesh->indices==indices,"Connection modified original native receiving geometry");
   Require(scene.splines.GetComponent(receiver)->spline_node_entities==nodes,"Connection inserted or removed authored controls");
   if(drawing){
    RiverWaterway graph;std::string error;Require(ReadRiverWaterway(scene,graph,error),error);const auto hash=graph.network->DeterministicHash();
    drawing->Undo();Require(drawing->Error().empty(),drawing->Error());for(int k=0;k<4;++k)app.Run();
    Require(ReadRiverWaterway(scene,graph,error)&&!graph.network,"Native drawn tributary Undo");
    Require(drawing->Execute(),drawing->Error());scene.names.GetComponent(source)->name="Native tributary";for(int k=0;k<4;++k)app.Run();
    Require(ReadRiverWaterway(scene,graph,error)&&graph.network->DeterministicHash()==hash,"Native drawn tributary Redo");
    std::cout<<"ACTUAL_NATIVE_DRAW_UNDO_REDO=PASS\\n";
   }
   auto before=CaptureTransform(*scene.transforms.GetComponent(node));auto after=before;after.translation.x+=.7f;after.translation.z+=.3f;
   SetWatercourseTransformCommand move(scene,node,before,after);Require(move.Execute(),"Native connected receiver edit");for(int k=0;k<4;++k)app.Run();gpu->WaitForGPU();
   RiverWaterway first;std::string error;Require(ReadRiverWaterway(scene,first,error),error);const auto movedHash=first.network->DeterministicHash();
   move.Undo();for(int k=0;k<4;++k)app.Run();Require(ReadRiverWaterway(scene,first,error)&&first.network->DeterministicHash()!=movedHash,"Actual connected native Undo");
   Require(move.Execute(),"Actual connected native Redo");for(int k=0;k<4;++k)app.Run();Require(ReadRiverWaterway(scene,first,error)&&first.network->DeterministicHash()==movedHash,"Native Redo hash");
   std::cout<<"ACTUAL_CONNECTED_EDIT_UNDO_REDO=PASS\n";
  }
  Verify(scene,file,reopen);Require(wi::renderer::GetShaderErrorCount()==0,"Native shader errors");std::cout<<"RIVER_WATERWAY_NATIVE="<<(reopen?"FRESH_REOPEN_PASS":"PASS")<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";gpu->WaitForGPU();return 1;}
}
