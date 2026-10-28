#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/TerrainService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/RiverCourseService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <chrono>
using namespace renegade::bridge;using namespace DirectX;namespace fs=std::filesystem;
static void Check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
struct Path:wi::RenderPath3D{
 RiverFlowRenderer flow;uint64_t edits=0;float step=0;
 void Update(float dt)override{flow.Prepare(*scene,1,step,edits);auto binding=flow.Bind();wi::RenderPath3D::Update(dt);}
 void Render()const override{auto binding=flow.Bind();flow.Dispatch();wi::RenderPath3D::Render();}
};
int main(int argc,char** argv){
 if(argc!=2)return 2;std::cout.setf(std::ios::unitbuf);fs::path out=fs::absolute(argv[1]);if(fs::exists(out))return 2;fs::create_directories(out);
 HWND window=CreateWindowExW(0,L"STATIC",L"River creator integration",WS_OVERLAPPEDWINDOW,30,30,1280,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 struct App:wi::Application{void Initialize()override{wi::Application::Initialize();alwaysactive=true;}}app;
 app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();auto* gpu=wi::graphics::GetDevice();
 try{
 wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
 wi::scene::Scene scene;auto& weather=scene.weathers.Create(scene.Entity_CreateTransform("Weather"));weather.SetRealisticSky(false);weather.ambient={.75f,.75f,.75f};weather.horizon={.58f,.66f,.74f};weather.zenith={.29f,.47f,.69f};
 scene.Entity_CreateLight("Daylight",{15,60,-20},{1,1,.96f},3,220,wi::scene::LightComponent::DIRECTIONAL);
 TerrainState terrainState;terrainState.visibleChunkRadius=1;terrainState.propChunkRadius=0;terrainState.physicsChunkRadius=2;terrainState.physics=true;terrainState.minimumHeight=-20;terrainState.maximumHeight=20;terrainState.chunkScale=.5f;
 auto terrainID=CreateTerrain(scene,terrainState,"Creator terrain");Check(terrainID!=wi::ecs::INVALID_ENTITY,"Create terrain");auto* terrain=scene.terrains.GetComponent(terrainID);
 wi::scene::CameraComponent camera;camera.CreatePerspective(1264,761,.05f,500,XMConvertToRadians(60));camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(3,5,-9,1),XMVectorSet(0,-.6f,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();
 Path path;path.scene=&scene;path.camera=&camera;path.init(1264,761,96);path.setOcclusionCullingEnabled(false);path.setMotionBlurEnabled(false);path.setDepthOfFieldEnabled(false);path.ResizeBuffers();path.camera=&camera;wi::renderer::SetTemporalAAEnabled(false);
 struct Guard{wi::Application& app;~Guard(){app.ActivatePath(nullptr);while(wi::renderer::IsPipelineCreationActive())Sleep(10);wi::graphics::GetDevice()->WaitForGPU();}}guard{app};app.ActivatePath(&path);
 auto frames=[&](int n){for(int k=0;k<n;++k)app.Run();};
 frames(160);terrain->Generation_Cancel();gpu->WaitForGPU();Check(terrain->chunks.size()>=9,"Resident real terrain chunks");
 SetTerrainHeightFloorCommand floor(scene,terrainID,terrain->bottomLevel-10);Check(floor.Execute(),"Terrain floor");
 // Disposable authored terrain has a lower receiving shelf before the river edit.
 auto shelf=CaptureTerrainSculpt(scene,*terrain);
 for(auto& chunk:shelf.chunks){const auto* mesh=scene.meshes.GetComponent(chunk.entity);if(!mesh)continue;auto world=scene.ComputeEntityMatrixRecursive(chunk.entity);
  Check(chunk.heights.size()==mesh->vertex_positions.size(),"Shelf sculpt correspondence");
  for(size_t k=0;k<chunk.heights.size();++k){XMFLOAT3 q;XMStoreFloat3(&q,XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[k]),world));float t=std::clamp((q.x+4.6f)/1.4f,0.f,1.f);t=t*t*(3-2*t);chunk.heights[k]-=1.1f*t;}}
 Check(ApplyTerrainSculpt(scene,*terrain,shelf),"Authored receiving shelf");
 RiverSplineOptions options;options.widthMeters=4;options.surfaceOffsetMeters=-.3f;options.subdivisionsPerSegment=24;
 CreateRiverSplineCommand create(scene,{{-15,0,0},{-9,0,0},{-4,0,0},{-3.85f,0,0},{1,0,0},{7,0,-.2f},{15,0,-.6f}},options);Check(create.Execute(),"Ordinary drawn river");
 auto river=create.CreatedEntity();scene.names.GetComponent(river)->name="River Water 5";std::string error;Check(EnsurePersistentEntityIdentities(scene,error),error);frames(4);gpu->WaitForGPU();
 SetRiverTerrainLayerCommand link(scene,terrainID,river,1.35f,2,true);bool linked=link.Execute();Check(linked,link.Error());
 auto nodes=scene.splines.GetComponent(river)->spline_node_entities;auto before=CaptureTransform(*scene.transforms.GetComponent(nodes[3]));auto after=before;after.translation.y-=1.1f;
 CommandService history;Check(history.Execute(std::make_unique<SetWatercourseTransformCommand>(scene,nodes[3],before,after)),"Ordinary lower-node transaction");++path.edits;
 Check(history.Undo(),"Lower-node Undo");Check(std::abs(scene.transforms.GetComponent(nodes[3])->translation_local.y-before.translation.y)<1e-5,"Undo node height");Check(history.Redo(),"Lower-node Redo");++path.edits;
 RiverFlowProfile p;p.enabled=true;p.foam=true;p.intensity=.3f;p.rapids=true;
 Check(history.Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,river,p)),"Flow profile");++path.edits;frames(8);
 Check(path.flow.ActiveCount()==1,path.flow.Status(river));std::cout<<"COURSE_READY "<<path.flow.Status(river)<<std::endl;
 auto stats=[&](const char* label){RiverFlowDiagnostics d;bool ok=path.flow.Diagnostics(river,d,error,true);Check(ok,error);std::cout<<label<<" rebuilds="<<d.rebuilds<<" buildMS="<<d.lastBuildMilliseconds<<" cells="<<d.cells<<" mass="<<d.foamMass<<" maximum="<<d.maximumDensity<<std::endl;return d;};
 RiverCourseSurface contained;std::string bankError;bool containedBuilt=BuildRiverCourseSurface(scene,river,1,contained,bankError);Check(containedBuilt,bankError);
 auto groundAt=[&](XMFLOAT3 p){auto hit=wi::scene::Pick(wi::primitive::Ray(XMFLOAT3(p.x,50,p.z),XMFLOAT3(0,-1,0),0,100),wi::enums::FILTER_TERRAIN,~0u,scene);Check(hit.entity!=wi::ecs::INVALID_ENTITY,"Actual terrain cap/bank pick missed");return hit.position.y;};
 const auto& ribbon=contained.flow->NativeRibbon();
 for(const auto& row:{ribbon.front(),ribbon.back()})Check(groundAt(row.centre)>row.centre.y+.05f,"Straight water endpoint exposed above rounded terrain cap");
 // Every terminal extension vertex must be concealed in the actual uncut
 // terrain; the interior bank checks alone cannot detect an exposed end patch.
 for(size_t row:{size_t(0),contained.positions.size()/17-1})for(size_t j=0;j<=16;++j){
  auto p=contained.positions[row*17+j];Check(groundAt(p)>p.y+.05f,"Continuous terminal extension exposed above terrain");
 }
 std::cout<<"CONTINUOUS_BURIED_EXTENSIONS_PASS vertices=34"<<std::endl;
 size_t containedBanks=0;for(size_t k=0;k<ribbon.size();k+=13)if(ribbon[k].centre.y>=-.32f){
  for(auto edge:{ribbon[k].leftBank,ribbon[k].rightBank}){Check(groundAt(edge)>edge.y+.05f,"Upper water edge not buried in actual terrain");++containedBanks;}}
 Check(containedBanks>8,"Insufficient upper bank checks");std::cout<<"CONCEALED_NATIVE_CAPS_AND_UPPER_BANKS_PASS bankChecks="<<containedBanks<<std::endl;
 auto raisedBefore=CaptureTransform(*scene.transforms.GetComponent(nodes[3]));auto raisedAfter=raisedBefore;raisedAfter.translation.y+=.2f;
 Check(history.Execute(std::make_unique<SetWatercourseTransformCommand>(scene,nodes[3],raisedBefore,raisedAfter)),"Edit existing cascade node");++path.edits;frames(3);Check(path.flow.Status(river).find("River ready")==0,path.flow.Status(river));
 Check(history.Undo(),"Undo existing cascade edit");++path.edits;frames(3);Check(history.Redo(),"Redo existing cascade edit");++path.edits;frames(3);Check(history.Undo(),"Restore initial cascade");++path.edits;frames(3);
 // A dirty native spline can arrive between Prepare and the native update.
 gpu->WaitForGPU();path.flow.Prepare(scene,1,0,path.edits);scene.transforms.GetComponent(nodes[3])->SetDirty();{auto binding=path.flow.Bind();scene.Update(0);}frames(4);
 Check(path.flow.Status(river).find("River ready")==0,path.flow.Status(river));
 path.step=1.f/60;frames(180);auto baseline=stats("SIMULATED");Check(baseline.foamMass>0,"Actual transported cascade foam missing");
 auto perfStart=std::chrono::steady_clock::now();frames(60);std::cout<<"COURSE_UNPACED_FPS "<<60/std::chrono::duration<double>(std::chrono::steady_clock::now()-perfStart).count()<<std::endl;auto steady=stats("STEADY");Check(steady.rebuilds==baseline.rebuilds,"Steady rendering rebuilt course");
 path.step=0;
 // Repeat native regeneration after mature foam exists. It must repair the
 // temporary binding without rebuilding/resetting the obstacle transport.
 for(int n=0;n<3;++n){
  gpu->WaitForGPU();path.flow.Prepare(scene,1,0,path.edits);
  scene.transforms.GetComponent(nodes[3])->SetDirty();
  {auto binding=path.flow.Bind();scene.Update(0);}frames(3);
  auto recovered=stats("NATIVE_REGEN_RECOVERY");
  Check(recovered.rebuilds==steady.rebuilds&&recovered.foamMass==steady.foamMass,"Native regeneration reset established foam");
 }
 std::cout<<"NATIVE_REGEN_FOAM_CONTINUITY_PASS"<<std::endl;
 path.step=0;gpu->WaitForGPU();Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/"course.png").generic_string()),"Capture");
 camera.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(3,12,-5,1),XMVectorSet(-1,-.6f,0,1),XMVectorSet(0,1,0,0))));camera.UpdateCamera();frames(8);gpu->WaitForGPU();Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/"course-overview.png").generic_string()),"Overview capture");
 auto vertices=scene.meshes.GetComponent(river)->vertex_positions;auto authoredCount=scene.transforms.GetCount();auto originalShader=scene.materials.GetComponent(river)->customShaderID;
 Check(originalShader<0,"Shader leaked outside bind");
 p.intensity=.6f;Check(history.Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,river,p)),"Foam amount");++path.edits;frames(3);auto amount=stats("AMOUNT");Check(amount.rebuilds==baseline.rebuilds&&amount.foamMass==steady.foamMass,"Amount edit resets simulation");
 p.rapidIntensity=.5f;Check(history.Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,river,p)),"Rapid amount");++path.edits;frames(3);auto rapidAmount=stats("RAPID_AMOUNT");Check(rapidAmount.rebuilds==amount.rebuilds&&rapidAmount.foamMass==amount.foamMass,"Rapid amount resets simulation");Check(history.Undo(),"Undo rapid amount");++path.edits;frames(3);
 Check(scene.transforms.GetCount()==authoredCount&&scene.meshes.GetComponent(river)->vertex_positions.size()==vertices.size(),"Derived data leaked to author scene");
 p.speed=0;Check(history.Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,river,p)),"Speed zero");++path.edits;frames(3);Check(stats("ZERO").foamMass==0,"Zero speed source");
 Check(history.Undo(),"Undo speed zero");++path.edits;frames(3);Check(path.flow.ActiveCount()==1,path.flow.Status(river));
 Check(history.Undo(),"Undo foam amount");++path.edits;frames(3);
 TransformState spawn;spawn.translation={-3,2,-6};CreatePlayerStartCommand player(scene,spawn);Check(player.Execute(),"Creator player start");authoredCount=scene.transforms.GetCount();
 gpu->WaitForGPU();{wi::Archive archive((out/"creator-course.wiscene").generic_string(),false);Check(archive.IsOpen(),"Native archive");archive.SetCompressionEnabled(true);scene.Serialize(archive);}
 Check(scene.transforms.GetCount()==authoredCount&&scene.materials.GetComponent(river)->customShaderID==originalShader,"Save changed authoring");
 std::cout<<"COURSE_SHARED_RENDERER_PASS"<<std::endl;return 0;
 }catch(const std::exception& ex){std::cerr<<"COURSE_SHARED_RENDERER_FAIL "<<ex.what()<<std::endl;return 1;}
}
