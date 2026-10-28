#include <WickedEngine.h>
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/RiverWaterwayService.h"
#include "renegade/bridge/RiverCourseService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/TerrainService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/PlaceableWaterfallRenderer.h"
#include "../EngineBridge/src/PlaceableWaterfallAuthoring.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
namespace fs=std::filesystem;
namespace wf=renegade::bridge::placeable_waterfall;
using renegade::bridge::PlaceableWaterfallRenderer;
static void Need(bool yes,const std::string& reason){if(!yes)throw std::runtime_error(reason);}
static std::vector<wf::AuthoredWaterfall> Records(const wi::scene::Scene& s){
 std::vector<wf::AuthoredWaterfall> list;std::string err;
 Need(wf::EnumerateWaterfalls(s,list,err),err);return list;
}
static size_t Generated(const wi::scene::Scene& scene){
 size_t n=0;for(size_t i=0;i<scene.metadatas.GetCount();++i)
 if(scene.metadatas[i].string_values.has("renegade.waterfall.generated.v1"))++n;
 return n;
}
static std::pair<double,std::unordered_set<wi::ecs::Entity>> OwnedEmitters(
    const wi::scene::Scene& scene,const wf::WaterfallId& id){
 double total=0;
 std::unordered_set<wi::ecs::Entity> entities;
 for(size_t i=0;i<scene.emitters.GetCount();++i){
  const auto e=scene.emitters.GetEntity(i);
  const auto* m=scene.metadatas.GetComponent(e);
  if(m&&m->string_values.has("renegade.waterfall.generated.v1") &&
      m->string_values.get("renegade.waterfall.generated.v1")==id){
       entities.insert(e);total+=scene.emitters[i].count;
  }
 }
 return {total,entities};
}
int main(int argc,char** argv){
 if(argc<3||argc>4)return 2;
 try{
  fs::path out=fs::absolute(fs::u8path(argv[2]));
  fs::create_directories(out);
  std::ofstream stages(out/"g13e7-export-stage.txt",std::ios::app);
  auto checkpoint=[&](const char* msg){
    stages<<msg<<std::endl;
    std::cerr<<msg<<std::endl;
  };
  checkpoint("start");
  const bool surveyJunction=argc==4&&std::string(argv[3])=="--junction-survey";
  const bool verify=argc==4&&std::string(argv[3])=="--verify";
  const bool verifyJunction=argc==4&&std::string(argv[3])=="--verify-junction";
  const bool coexist=argc==4&&std::string(argv[3])=="--river";
  const bool soak=argc==4&&std::string(argv[3])=="--soak";
  const bool meshSurvey=argc==4&&std::string(argv[3])=="--mesh-survey";
  const bool morph=argc==4&&std::string(argv[3])=="--morph";
  const bool terrainGrid=argc==4&&std::string(argv[3])=="--terrain-grid";
  const bool stressExport=argc==4&&std::string(argv[3])=="--native-terrain-junction-81-player-export";
  const bool junctionExport=argc==4&&(std::string(argv[3])=="--native-terrain-junction-export"||stressExport);
  const bool junctionScene=argc==4&&(std::string(argv[3])=="--native-terrain-junction"||junctionExport);
  const bool visualExport=argc==4&&(std::string(argv[3])=="--native-terrain-visual-export"||junctionExport);
  const bool visualScene=argc==4&&(std::string(argv[3])=="--native-terrain-visual"||visualExport||junctionScene);
  const bool nativeTerrainNear=argc==4&&(std::string(argv[3])=="--native-terrain-near"||visualScene);
  const bool nativeTerrain=argc==4&&(std::string(argv[3])=="--native-terrain"||nativeTerrainNear);
  HWND hwnd=CreateWindowExW(0,L"STATIC",L"Renegade G13E4 Multi Waterfalls",
      WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  struct App:wi::Application{} app;
  app.allow_hdr=false;app.SetWindow(hwnd);
  checkpoint("pre_initialize");
  wi::initializer::InitializeComponentsImmediate();
  checkpoint("initialized");
  wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
  renegade::bridge::StudioSession session;
  session.Projects().Initialize((out/"proof.ini").generic_string());
  checkpoint("pre_open_project");
  const bool opened=session.Projects().OpenProject(
      fs::absolute(fs::u8path(argv[1])).generic_string());
  Need(opened,"OpenProject: "+session.Projects().LastError());
  checkpoint("opened_project");
  checkpoint("pre_load_scene");
  Need(session.LoadScene(session.Projects().StartupScenePath()),session.Scenes().LastError());
  checkpoint("loaded_scene");
  auto& scene=session.Scenes().GetScene();
  auto records=Records(scene);
  checkpoint("enumerated_waterfalls");
  Need(records.size()==((verify||soak||surveyJunction)?2:1),"Wrong authored instance count on scene open");
  if(verifyJunction)
    Need(scene.splines.GetCount()==1&&scene.terrains.GetCount()==1,
      "G13E9 reopened junction missing native river or terrain");
  if(junctionScene){
    // The rectangular pool belongs ONLY to the historical G13E3 test prop.
    // It is not an authored river/waterfall junction. Remove it in a
    // disposable fixture before simulating a receiving native G11 river.
    const auto pool=scene.Entity_FindByName("Optional receiving pool (fixture only)");
    Need(pool!=wi::ecs::INVALID_ENTITY,"Expected original test-only pool quad");
    scene.Entity_Remove(pool);
    // The old 34x26m grey rock-floor test quad also hides the real riverbank.
    // Native Wicked terrain (not a fabricated water plane) is the receiving bed.
    const auto floor=scene.Entity_FindByName("Rock floor");
    Need(floor!=wi::ecs::INVALID_ENTITY,"Expected G13E3 test rock-floor prop");
    scene.Entity_Remove(floor);
    Need(scene.Entity_FindByName("Rock floor")==wi::ecs::INVALID_ENTITY,
         "Old test rock floor survived deletion");
    Need(scene.Entity_FindByName("Optional receiving pool (fixture only)")==
         wi::ecs::INVALID_ENTITY,"Disposable rectangular pool survived deletion");
    checkpoint("G13E8_removed_faux_pool");
  }
  if(surveyJunction){
    std::cout<<std::fixed<<std::setprecision(4);
    std::cout<<"G13E8_SURVEY_BEGIN objects="<<scene.objects.GetCount()
      <<" splines="<<scene.splines.GetCount()
      <<" waterfalls="<<records.size()<<std::endl;
    for(size_t i=0;i<scene.objects.GetCount();++i){
      const auto e=scene.objects.GetEntity(i);
      const auto* n=scene.names.GetComponent(e);
      if(!n ||(n->name.find("pool")==std::string::npos &&
          n->name.find("floor")==std::string::npos &&
          n->name.find("water")==std::string::npos &&
          n->name.find("Water")==std::string::npos))continue;
      const auto* mesh=scene.meshes.GetComponent(scene.objects[i].meshID);
      const auto* mat=scene.materials.GetComponent(e);
      const auto world=scene.ComputeEntityMatrixRecursive(e);
      const auto* obj=scene.objects.GetComponent(e);
      std::cout<<"G13E8_OBJECT entity="<<e<<" name="<<n->name
        <<" meshID="<<obj->meshID
        <<" materialID="<<(mesh&& !mesh->subsets.empty()?mesh->subsets.front().materialID:0)
        <<" vertices="<<(mesh?mesh->vertex_positions.size():0);
      if(mesh){
        XMFLOAT3 lower={1e9f,1e9f,1e9f},upper={-1e9f,-1e9f,-1e9f};
        for(const auto& v:mesh->vertex_positions){
          XMFLOAT3 w;
          DirectX::XMStoreFloat3(&w,DirectX::XMVector3TransformCoord(
              DirectX::XMLoadFloat3(&v),world));
          lower.x=std::min(lower.x,w.x);lower.y=std::min(lower.y,w.y);lower.z=std::min(lower.z,w.z);
          upper.x=std::max(upper.x,w.x);upper.y=std::max(upper.y,w.y);upper.z=std::max(upper.z,w.z);
        }
        std::cout<<" bounds=["<<lower.x<<","<<lower.y<<","<<lower.z
          <<"]-["<<upper.x<<","<<upper.y<<","<<upper.z<<"]";
        auto* realMat=scene.materials.GetComponent(mesh->subsets.empty()?e:mesh->subsets.front().materialID);
        if(realMat) std::cout<<" waterShader="<<int(realMat->shaderType)
          <<" baseRGBA="<<realMat->baseColor.x<<","<<realMat->baseColor.y<<","
          <<realMat->baseColor.z<<","<<realMat->baseColor.w
          <<" roughness="<<realMat->roughness
          <<" refraction="<<realMat->refraction
          <<" normalName="<<realMat->textures[wi::scene::MaterialComponent::NORMALMAP].name;
      }
      std::cout<<std::endl;
    }
    for(size_t i=0;i<scene.splines.GetCount();++i){
      const auto e=scene.splines.GetEntity(i);
      const auto* n=scene.names.GetComponent(e);
      const auto* mat=scene.materials.GetComponent(e);
      const auto& sp=scene.splines[i];
      std::cout<<"G13E8_SPLINE entity="<<e<<" name="<<(n?n->name:"(unnamed)")
        <<" halfWidth="<<sp.width<<" nodes="<<sp.spline_node_entities.size()
        <<" shader="<<(mat?int(mat->shaderType):-1);
      if(mat)std::cout<<" base="<<mat->baseColor.x<<","<<mat->baseColor.y<<","
        <<mat->baseColor.z<<","<<mat->baseColor.w
        <<" normal="<<mat->textures[wi::scene::MaterialComponent::NORMALMAP].name;
      std::cout<<std::endl;
      for(auto point:sp.spline_node_entities){
        auto w=scene.ComputeEntityMatrixRecursive(point);
        XMFLOAT3 center;
        DirectX::XMStoreFloat3(&center,w.r[3]);
        std::cout<<"G13E8_RIVER_NODE "<<center.x<<","<<center.y<<","<<center.z<<std::endl;
      }
    }
    for(const auto& fall:records)
      std::cout<<"G13E8_WATERFALL crest="<<fall.settings.crest.x<<","<<fall.settings.crest.y
        <<","<<fall.settings.crest.z<<" landing="<<fall.settings.landing.x<<","
        <<fall.settings.landing.y<<","<<fall.settings.landing.z<<std::endl;
    std::cout<<"G13E8_SURVEY_END"<<std::endl;
    wi::graphics::GetDevice()->WaitForGPU();
    wi::jobsystem::ShutDown();
    DestroyWindow(hwnd);
    return 0;
  }
  PlaceableWaterfallRenderer renderer;
  const auto tick=[&](float dt=1.f/60.f){
    auto start=std::chrono::steady_clock::now();
    renderer.Prepare(scene,session.Scenes().Revision(),dt,false);
    scene.Update(0);
    auto ms=std::chrono::duration<double,std::milli>(
       std::chrono::steady_clock::now()-start).count();
    Need(renderer.Status().find("WATERFALL ERROR")==std::string::npos,
       renderer.Status());
    return ms;
  };
  // Unlike the long-running packaged Runtime, this short-lived harness must
  // drain native GPU work and Wicked worker threads before its app/window dies.
  const auto cleanShutdown=[&](){
    renderer.Clear(scene);
    wi::graphics::GetDevice()->WaitForGPU();
    wi::jobsystem::ShutDown();
    DestroyWindow(hwnd);
  };
  if(verifyJunction){
    const auto river=scene.splines.GetEntity(0);
    const auto* spline=scene.splines.GetComponent(river);
    Need(spline && spline->spline_node_entities.size()==6,
      "G13E9 saved G11 river spline lost authored nodes");
    const auto receiver=spline->spline_node_entities[1];
    const auto* transform=scene.transforms.GetComponent(receiver);
    Need(transform && std::abs(transform->scale_local.x-1.5f)<.001f,
      "G13E9 saved receiving node lost creator width scale");
    renegade::bridge::RiverFlowProfile saved;
    std::string sourceError;
    Need(renegade::bridge::ReadRiverFlowProfile(scene,river,saved,sourceError),
      "G13E9 reopened flow profile failed: "+sourceError);
    Need(saved.enabled && saved.startWhitewater &&
         saved.startWhitewaterAmount>1.f &&
         renegade::bridge::ResolveRiverWhitewaterSourceNode(scene,river,saved)==receiver,
      "G13E9 stable-node whitewater lost source identity after fresh process");
    Need(scene.Entity_FindByName("Optional receiving pool (fixture only)")==
         wi::ecs::INVALID_ENTITY &&
         scene.Entity_FindByName("Rock floor")==wi::ecs::INVALID_ENTITY,
      "G13E9 saved native junction restored unwanted test pool or floor");
    Need(tick()>=0 && renderer.ActiveCount()==1 &&
         !OwnedEmitters(scene,records.front().id).second.empty(),
      "G13E9 reopened waterfall native visual failed");
    std::cout<<"G13E9_SEPARATE_PROCESS_JUNCTION_REOPEN_PASS waterfalls=1"
             <<" native_terrain=1 river=1 spline_nodes=6 width_scale=1.5"
             <<" selected_source=stable-uuid no_faux_pool=1"<<std::endl;
    cleanShutdown();
    return 0;
  }
  if(nativeTerrain){
    Need(records.size()==1,"Native 19x19 proof requires one authored waterfall");
    if(scene.weathers.GetCount()==0)
      scene.weathers.Create(scene.Entity_CreateTransform("G13E6 sky"));
    renegade::bridge::TerrainState state;
    // Linked G11 native riverbed authoring is intentionally capped at
    // 81 fully resident chunks. Use a true 5x5 (25-chunk) native terrain for
    // the combined creator scene; retain separate G13 361-chunk stress tests.
    state.visibleChunkRadius=stressExport?4:((junctionScene||visualExport)?2:9);
    const size_t expectedChunks=size_t((state.visibleChunkRadius*2+1)*(state.visibleChunkRadius*2+1));
    state.propChunkRadius=0;
    // Collision should be local to the active player, not all 81 chunks.
    // The full 9x9 terrain still renders and the entire linked G11 baseline
    // remains in the same one-scene stress proof.
    state.physicsChunkRadius=stressExport?1:0;
    state.physics=stressExport;
    state.chunkScale=1.f;
    state.minimumHeight=-20.f;
    state.maximumHeight=30.f;
    auto start=std::chrono::steady_clock::now();
    checkpoint("pre_create_terrain");
    const auto terrainID=renegade::bridge::CreateTerrain(scene,state,"G13E6 real native terrain");
    checkpoint("created_terrain");
    Need(terrainID!=wi::ecs::INVALID_ENTITY,"Could not create native 19x19 terrain");
    auto* terrain=scene.terrains.GetComponent(terrainID);
    Need(terrain!=nullptr,"Missing native terrain after creation");
    auto* root=scene.transforms.GetComponent(terrainID);
    Need(root!=nullptr,"Missing native terrain transform");
    // Keep the real generator's terrain far below the existing cliff/water.
    // This tests native 361-chunk allocation and renderer scanning without
    // modifying or obscuring the accepted visual/collision fixture.
    const float terrainShift=visualScene?.55f:(nativeTerrainNear?2.f:100.f);
    root->translation_local.y-=terrainShift;
    root->SetDirty();root->UpdateTransform();
    wi::scene::CameraComponent camera;
    const int viewportW=visualScene?1280:800,viewportH=visualScene?720:600;
    camera.CreatePerspective(viewportW,viewportH,.1f,1000.f,DirectX::XMConvertToRadians(60.f));
    camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
        DirectX::XMMatrixLookAtLH(
          visualExport?DirectX::XMVectorSet(12.f,28.f,-15.f,1.f):
            DirectX::XMVectorSet(85.f,65.f-terrainShift,-110.f,1.f),
          DirectX::XMVectorSet(0.f,-terrainShift,0.f,1.f),
          DirectX::XMVectorSet(0.f,1.f,0.f,0.f))));
    camera.UpdateCamera();
    struct VisualPath:wi::RenderPath3D{
      renegade::bridge::RiverFlowRenderer flow;
      void Update(float dt)override{
        flow.Prepare(*scene,1,dt);
        auto bind=flow.Bind();
        wi::RenderPath3D::Update(dt);
      }
      void Render()const override{
        auto bind=flow.Bind();
        flow.Dispatch();
        wi::RenderPath3D::Render();
      }
    } render;
    render.scene=&scene;render.camera=&camera;
    render.init(viewportW,viewportH,96);
    render.setOcclusionCullingEnabled(false);
    render.setMotionBlurEnabled(false);
    render.setDepthOfFieldEnabled(false);
    render.ResizeBuffers();
    app.ActivatePath(&render);
    const auto original=OwnedEmitters(scene,records.front().id).second;
    int frames=0;
    while(frames<480 && std::chrono::steady_clock::now()-start<std::chrono::seconds(95)){
      app.Run();
      ++frames;
      if(frames%40==0){
        std::cout<<"G13E6_NATIVE_GENERATION frame="<<frames
          <<" resident="<<terrain->chunks.size()
          <<" busy="<<terrain->IsGenerationBusy()<<std::endl;
        Sleep(12);
      }
      if(terrain->chunks.size()>=expectedChunks && !terrain->IsGenerationBusy())break;
    }
    if(junctionScene){
      // IsGenerationBusy()/chunk map completion precede native mesh publication:
      // all 25 heights existed but NO 67x67 scene meshes were available at the
      // initial checkpoint. Drive real Scene::Update/App frames until the
      // linked-bed service's declared topology is present; bounded timeout.
      for(int publish=0;publish<180;++publish){
        size_t available=0;
        for(const auto& [key,chunk]:terrain->chunks){
          const auto* mesh=scene.meshes.GetComponent(chunk.entity);
          if(!chunk.invalidated&&mesh &&
             mesh->vertex_positions.size()==wi::terrain::vertexCount)
            ++available;
        }
        if(available==expectedChunks){
          std::cout<<"G13E9_NATIVE_MESH_PUBLISHED frame="<<publish
            <<" ready="<<available<<std::endl;
          break;
        }
        app.Run();
        if(publish%24==0)Sleep(10);
      }
    }
    terrain->Generation_Cancel();
    wi::graphics::GetDevice()->WaitForGPU();
    const size_t chunks=terrain->chunks.size();
    scene.Update(0);
    // Adding 361 genuinely new objects legitimately changes scene content.
    // Establish a baseline AFTER the native terrain generator has finished.
    tick(.4f);
    const auto afterGeneration=OwnedEmitters(scene,records.front().id).second;
    size_t terrainHeightBand=0,terrainOutsideHeightBand=0;
    for(size_t i=0;i<scene.objects.GetCount();++i){
      const auto e=scene.objects.GetEntity(i);
      const auto* name=scene.names.GetComponent(e);
      if(!name||name->name.find("chunk")==std::string::npos)continue;
      if(i>=scene.aabb_objects.size()){++terrainHeightBand;continue;}
      auto bounds=scene.aabb_objects[i];
      const auto low=bounds.getMin(),high=bounds.getMax();
      if(high.y>-6.f && low.y<16.f)++terrainHeightBand;else ++terrainOutsideHeightBand;
      if(terrainHeightBand+terrainOutsideHeightBand<=2){
         std::cout<<"G13E6_NATIVE_CHUNK_BOUNDS y="<<low.y<<".."<<high.y
           <<" x="<<low.x<<".."<<high.x<<std::endl;
      }
    }
    std::vector<double> measured;
    for(int i=0;i<45;++i){
      auto begin=std::chrono::steady_clock::now();
      renderer.Prepare(scene,session.Scenes().Revision(),1.f/60.f,false);
      measured.push_back(std::chrono::duration<double,std::milli>(
          std::chrono::steady_clock::now()-begin).count());
      scene.Update(0);
    }
    Need(renderer.ActiveCount()==1,"Native terrain changed waterfall instance count");
    Need(OwnedEmitters(scene,records.front().id).second==afterGeneration,
      "Finished native terrain caused repeated waterfall rebuilds");
    // Actual Renegade TerrainService sculpt + CommandService Undo/Redo, not
    // merely editing synthetic vertex buffers. Terrain remains far below the
    // fall so its refresh should never invalidate the independent waterfall.
    if(!visualExport && !junctionScene){
    const auto beforeSculpt=renegade::bridge::CaptureTerrainSculpt(scene,*terrain);
    wi::graphics::GetDevice()->WaitForGPU();
    const bool sculpted=renegade::bridge::SculptTerrain(
        scene,*terrain,{0,-terrainShift,0},12.f,.5f,.5f,
        renegade::bridge::TerrainSculptMode::Raise,0.f);
    Need(sculpted,"Native 361-chunk TerrainService sculpt refused");
    const auto afterSculpt=renegade::bridge::CaptureTerrainSculpt(scene,*terrain);
    auto reducedBefore=beforeSculpt,reducedAfter=afterSculpt;
    const auto changedChunks=renegade::bridge::RetainChangedTerrainSculpt(
        reducedBefore,reducedAfter);
    Need(changedChunks>0,"Real terrain sculpt changed no native chunks");
    renegade::bridge::CommandService sculptHistory;
    Need(sculptHistory.RecordExecuted(
        std::make_unique<renegade::bridge::SculptTerrainCommand>(
            scene,terrainID,std::move(reducedBefore),std::move(reducedAfter))),
        "Could not register native sculpt Undo/Redo");
    auto matchesSculpt=[&](const renegade::bridge::TerrainSculptState& expected){
      auto a=expected,b=renegade::bridge::CaptureTerrainSculpt(scene,*terrain);
      return renegade::bridge::RetainChangedTerrainSculpt(a,b)==0;
    };
    Need(sculptHistory.Undo(),"Native terrain sculpt Undo refused");
    Need(matchesSculpt(beforeSculpt),"Native sculpt Undo lost original chunk heights");
    Need(sculptHistory.Redo(),"Native terrain sculpt Redo refused");
    Need(matchesSculpt(afterSculpt),"Native sculpt Redo failed to restore edited heights");
    terrain->Generation_Cancel();
    scene.Update(0);
    tick(.4f);
    const auto afterSculptVisual=OwnedEmitters(scene,records.front().id).second;
    Need(nativeTerrainNear?(afterSculptVisual!=afterGeneration):
         (afterSculptVisual==afterGeneration),
         "Native terrain sculpt rebuild decision mismatched waterfall proximity");
    std::cout<<"G13E6_NATIVE_TERRAIN_SCULPT_HISTORY_PASS changed_chunks="
        <<changedChunks<<" undo_redo=1"
        <<" near_collision_retraced="<<(afterSculptVisual!=afterGeneration)
        <<" terrain_shift="<<terrainShift<<std::endl;
    }else{
      std::cout<<"G13E7_EXPORT_TERRAIN_AUTHORING sculpt_by_studio=1 generator_chunks="
        <<terrain->chunks.size()<<std::endl;
    }
    if(visualScene){
      // One stronger local authoring stroke builds a visible bank
      // where the falling water reaches the receiving pool.
      if(!visualExport && !junctionScene){
        const auto bankSculpted=renegade::bridge::SculptTerrain(
          scene,*terrain,{1.5f,-terrainShift,-1.0f},7.f,1.5f,.48f,
          renegade::bridge::TerrainSculptMode::Raise,0.f);
        Need(bankSculpted,"G13E7 raised terrain bank was not sculpted");
        terrain->Generation_Cancel();
        scene.Update(0);
        renderer.Prepare(scene,session.Scenes().Revision(),.5f,false);
        std::cout<<"G13E7_VISIBLE_TERRAIN_BANK_SCULPT_PASS"<<std::endl;
      }
      // Build a genuine G11 native river flow beside the receiving pool, not
      // a faked rectangle of water. Keep this entirely in the disposable scene.
      renegade::bridge::RiverSplineOptions opt;
      opt.widthMeters=4.2f;
      // The placed fall's receiving water surface is at its authored
      // landing Y=0. No hidden 'pool' plane or +0.20m floating river lip.
      opt.surfaceOffsetMeters=junctionScene?.015f:.20f;
      // 81 chunks * five 20-subdivision spans exceeds the G11 linked-bed
      // bounded 32-million-interaction authoring budget. A 12-subdivision
      // sampling density keeps the identical spline path/width and solver
      // but is valid at the maximum supported 81 resident chunks.
      opt.subdivisionsPerSegment=stressExport?12:20;
      // The source node lies concealed upstream of the falling curtain.
      // The selected SECOND node is located beneath the actual G13 landing,
      // preventing native spline end-cap taper from exposing dry ground
      // under the waterfall. This is ordinary authored G11 river geometry,
      // NOT a second pool plane, waterfall simulation or new shader.
      const auto fallLanding=records.front().settings.landing;
      std::cout<<"G13E9_LANDING_WORLD x="<<fallLanding.x
        <<" y="<<fallLanding.y<<" z="<<fallLanding.z<<std::endl;
      std::vector<XMFLOAT3> nodes=junctionScene
        ?std::vector<XMFLOAT3>{
          {fallLanding.x-4.0f,0.f,fallLanding.z-.5f},
          {fallLanding.x,0.f,fallLanding.z},
          {fallLanding.x+3.2f,0.f,fallLanding.z+1.6f},
          {fallLanding.x+6.1f,0.f,fallLanding.z+3.0f},
          {fallLanding.x+9.8f,0.f,fallLanding.z+4.5f},
          {fallLanding.x+14.f,0.f,fallLanding.z+5.f}}
        :std::vector<XMFLOAT3>{{1.6f,.10f,1.0f},{3.8f,.14f,3.6f},
             {7.5f,.14f,4.9f},{11.5f,.12f,6.2f},{16.f,.08f,6.5f}};
      auto river=std::make_unique<renegade::bridge::CreateRiverSplineCommand>(scene,nodes,opt);
      auto* rawRiver=river.get();
      Need(session.Commands().Execute(std::move(river)),
           "G13E7 could not create native connected river");
      const auto riverID=rawRiver->CreatedEntity();
      if(junctionScene){
        // The stock native terrain has minimum -20m; attempt the bounded
        // actual riverbed carve directly. An unrelated floor rewrite is not
        // required and is deliberately not forced.
        std::cout<<"G13E9_TERRAIN_BOUNDS min="<<terrain->bottomLevel
          <<" max="<<terrain->topLevel<<" chunks="<<terrain->chunks.size()
          <<" generation="<<terrain->generation<<std::endl;
        // Only explicit fixture authoring: link actual native G11 channel to
        // native Wicked terrain rather than drawing a fake receiving pool.
        std::size_t invalidChunks=0,badTopology=0,badHeights=0;
        for(const auto& pair:terrain->chunks){
          const auto& chunk=pair.second;
          const auto* mesh=scene.meshes.GetComponent(chunk.entity);
          invalidChunks+=chunk.invalidated?1:0;
          badTopology+=(!mesh || mesh->vertex_positions.size()!=wi::terrain::vertexCount)?1:0;
          badHeights+=chunk.heightmap_data.size()!=wi::terrain::vertexCount?1:0;
        }
        std::cout<<"G13E9_NATIVE_CHUNK_READINESS count="<<terrain->chunks.size()
          <<" invalid="<<invalidChunks<<" bad_mesh="<<badTopology
          <<" bad_heightmap="<<badHeights
          <<" busy="<<terrain->IsGenerationBusy()<<std::endl;
        // Actual native generator places raw vertices at bottomLevel. Widen
        // encoding headroom via existing authored floor migration *after*
        // all 25 mesh resources have been published, before cutting the bed.
        const float previousFloor=terrain->bottomLevel;
        renegade::bridge::SetTerrainHeightFloorCommand floor(
          scene,terrainID,previousFloor-12.f);
        Need(floor.Execute(),"G13E9 authored terrain floor migration refused");
        std::cout<<"G13E9_AUTHORED_FLOOR_PASS before="<<previousFloor
          <<" after="<<terrain->bottomLevel<<std::endl;
        // Real creator command-history route, not direct metadata mutation.
        auto linkedBed=std::make_unique<renegade::bridge::SetRiverTerrainLayerCommand>(
          scene,terrainID,riverID,1.25f,2.0f,true);
        Need(session.Commands().Execute(std::move(linkedBed)),
          "G13E9 native riverbed creator command refused after terrain publication and floor migration");
        scene.Update(0);
        std::cout<<"G13E8_NATIVE_LINKED_RIVERBED_PASS depth=1.25 bank=2.0"
          <<" ribbon_width="<<opt.widthMeters<<std::endl;
        const auto& authoredNodes=scene.splines.GetComponent(riverID)->spline_node_entities;
        Need(authoredNodes.size()>=3,"Junction river lacks authored receiving nodes");
        const auto nearFall=authoredNodes[1]; // actual landing / interior Point 2
        auto original=renegade::bridge::CaptureTransform(
          *scene.transforms.GetComponent(nearFall));
        auto widened=original;
        widened.scale.x=1.5f;
        Need(session.Commands().Execute(
          std::make_unique<renegade::bridge::SetWatercourseTransformCommand>(
            scene,nearFall,original,widened)),
          "G13E9 receiving river bank refused selected node widening");
        Need(std::abs(scene.transforms.GetComponent(nearFall)->scale_local.x-1.5f)<.001f,
          "G13E9 receiving spline width not applied");
        std::cout<<"G13E9_RECEIVING_RIVER_WIDTH_PASS selected=landing-node-2 width_scale=1.5"
          <<" widened_metres="<<(opt.widthMeters*1.5f)<<std::endl;
      }
      renegade::bridge::RiverFlowProfile profile;
      profile.enabled=true;profile.speed=1.1f;profile.intensity=junctionScene?1.0f:.4f;
      profile.foam=true;
      if(junctionScene){
        // Creator workflow: use the widened first node, not a fabricated
        // separate pool. Native v5 profile resolves a persistent node ID.
        profile.startWhitewater=true;
        profile.startWhitewaterAmount=1.25f; // fixture-only stronger receiving foam
        profile.startWhitewaterNodeIndex=1;
      }
      {
        std::string diagnostic;
        renegade::bridge::RiverFlowProfile current;
        renegade::bridge::RiverWaterwayControls controls;
        const bool nativeSource=scene.splines.Contains(riverID) &&
          scene.materials.Contains(riverID);
        const bool valid=renegade::bridge::ValidateRiverFlowProfile(profile,diagnostic);
        const auto validationMessage=diagnostic;
        const bool read=renegade::bridge::ReadRiverFlowProfile(scene,riverID,current,diagnostic);
        const auto readMessage=diagnostic;
        const bool connected=renegade::bridge::ReadRiverWaterwayControls(
          scene,riverID,controls,diagnostic);
        std::cout<<"G13E9_PRE_FLOW_SOURCE native="<<nativeSource
          <<" valid="<<valid<<" why="<<validationMessage
          <<" read="<<read<<" readError="<<readMessage
          <<" controls="<<connected<<" controlsError="<<diagnostic
          <<" attached="<<controls.connected
          <<" elevated="<<renegade::bridge::RiverCourseHasElevation(scene,riverID)
          <<" riverUUID="<<renegade::bridge::PersistentEntityId(scene,riverID)
          <<" originalNodes="<<scene.splines.GetComponent(riverID)->spline_node_entities.size()
          <<std::endl;
      }
      Need(session.Commands().Execute(
        std::make_unique<renegade::bridge::SetRiverFlowProfileCommand>(
          scene,riverID,profile)),
        "G13E9 creator Undo-history profile command refused after diagnostics");
      if(junctionScene){
        // Verify Undo/Redo of the actual editor whitewater profile. The
        // original point's persistent UUID must remain authoritative.
        Need(session.Commands().Undo(),"G13E9 river source Undo refused");
        renegade::bridge::RiverFlowProfile undone;
        std::string undoError;
        Need(renegade::bridge::ReadRiverFlowProfile(scene,riverID,undone,undoError) &&
             !undone.startWhitewater,"G13E9 source Undo retained foam state");
        Need(session.Commands().Redo(),"G13E9 river source Redo refused");
        std::cout<<"G13E9_CREATOR_WHITEWATER_UNDO_REDO_PASS"<<std::endl;
        renegade::bridge::RiverFlowProfile savedProfile;
        std::string selectedError;
        Need(renegade::bridge::ReadRiverFlowProfile(scene,riverID,savedProfile,selectedError),
          selectedError);
        Need(savedProfile.startWhitewater &&
          renegade::bridge::IsValidStableId(savedProfile.startWhitewaterNodeId),
          "G13E9 river source did not persist stable receiving point ID");
        Need(renegade::bridge::ResolveRiverWhitewaterSourceNode(scene,riverID,savedProfile)==
          scene.splines.GetComponent(riverID)->spline_node_entities[1],
          "G13E9 whitewater source not attached to widened landing point");
        std::cout<<"G13E9_WATERFALL_RIVER_NODE_SOURCE_PASS id="
          <<savedProfile.startWhitewaterNodeId<<" amount="
          <<savedProfile.startWhitewaterAmount<<" no_pool=1"<<std::endl;
      }
      camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
        DirectX::XMMatrixLookAtLH(
          DirectX::XMVectorSet(18.f,12.f,-20.f,1.f),
          DirectX::XMVectorSet(2.f,5.f,0.f,1.f),DirectX::XMVectorSet(0.f,1.f,0.f,0.f))));
      camera.UpdateCamera();
      render.camera=&camera;
      auto captureVisual=[&](const char* filename,int frames){
        auto t0=std::chrono::steady_clock::now();
        for(int i=0;i<frames;++i){
          renderer.Prepare(scene,session.Scenes().Revision(),1.f/60.f,false);
          app.Run();
        }
        wi::graphics::GetDevice()->WaitForGPU();
        const auto save=out/filename;
        Need(wi::helper::saveTextureToFile(render.GetRenderResult3D(),save.generic_string()),
          std::string("G13E7 DX12 image save failed: ")+filename);
        const double seconds=std::chrono::duration<double>(
          std::chrono::steady_clock::now()-t0).count();
        std::cout<<"G13E7_DX12_VISUAL_CAPTURE file="<<filename
          <<" frames="<<frames<<" render_fps="<<double(frames)/seconds
          <<" waterfall_active="<<renderer.ActiveCount()
          <<" river_active="<<render.flow.ActiveCount()
          <<" status="<<render.flow.Status(riverID)<<std::endl;
        Need(render.flow.ActiveCount()==1,"G13E7 native G11 river flow not active");
        Need(render.flow.Status(riverID).find("failed")==std::string::npos,
          "G13E7 G11 obstacle classification failed despite stale active flow: "+
          render.flow.Status(riverID));
      };
      // Existing crest over real rocks with native sculpted terrain rising
      // into the receiving pool. Baseline paired-composition capture.
      captureVisual(junctionScene?"G13E9-waterfall-widened-river-whitewater.png":
                   "G13E7-one-waterfall-river-terrain.png",150);
      camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
        DirectX::XMMatrixLookAtLH(
          DirectX::XMVectorSet(12.f,6.f,-9.f,1.f),
          DirectX::XMVectorSet(7.f,.2f,5.f,1.f),DirectX::XMVectorSet(0.f,1.f,0.f,0.f))));
      camera.UpdateCamera();
      captureVisual(junctionScene?"G13E9-waterfall-impact-close.png":
                   "G13E7-river-receiving-pool-close.png",110);
      camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
        DirectX::XMMatrixLookAtLH(
          DirectX::XMVectorSet(14.f,28.f,-7.f,1.f),
          DirectX::XMVectorSet(8.f,.1f,5.f,1.f),DirectX::XMVectorSet(0.f,1.f,0.f,0.f))));
      camera.UpdateCamera();
      captureVisual(junctionScene?"G13E8-river-course-overhead.png":
                   "G13E7-river-course-overhead.png",100);
      if(junctionScene){
        Need(scene.Entity_FindByName("Optional receiving pool (fixture only)")==
             wi::ecs::INVALID_ENTITY,"Old fake pool returned");
        std::cout<<"G13E8_NO_ARTIFICIAL_POOL_PASS river="<<riverID
          <<" waterfalls="<<renderer.ActiveCount()<<std::endl;
        if(junctionExport){
          if(stressExport)
          {
            // This is the one real creator/gameplay stress fixture. A proper
            // player-start capsule + native 9x9 terrain + linked G11 river
            // permit ordinary Runtime Jolt/player movement, not a spectator.
            renegade::bridge::TransformState start{};
            start.translation={7.f,2.4f,-9.f};
            auto command=std::make_unique<
                renegade::bridge::CreatePlayerStartCommand>(scene,start);
            Need(session.Commands().Execute(std::move(command)),
                "G13E14 real Player Start creator command failed");
            const auto spawn=renegade::bridge::ResolvePlayerStart(scene);
            Need(spawn.resolution==renegade::bridge::PlayerStartResolution::Success,
                "G13E14 player start not found after authoring");
            Need(terrain->chunks.size()==81,
                "G13E14 one combined stress level requires 81 resident chunks");
            std::cout<<"G13E14_COMBINED_81_PLAYER_PASS chunks=81 player_start=1"
              <<" linked_river=1 waterfall=1 foam=1"<<std::endl;
          }
          terrain->Generation_Cancel();
          wi::graphics::GetDevice()->WaitForGPU();
          const auto save=session.Scenes().CurrentPath();
          Need(session.SaveScene(save),
            "G13E8 native junction save failed: "+session.Scenes().LastError());
          Need(fs::is_regular_file(fs::u8path(save)),"Missing saved G13E8 WISCENE");
          if(stressExport){
            std::string error;
            renegade::bridge::TestLevelSnapshotService snapshots(
                session.Scenes(),session.Commands());
            renegade::bridge::TestLevelSnapshot result;
            Need(snapshots.Create(session.Projects().CurrentProject(),result,error),
                "G13E14 actual Test Game snapshot creation failed: "+error);
            Need(result.IsRuntimeReady(),
                "G13E14 Test Game snapshot has no launch descriptor");
            std::ofstream descriptor(out/"g13e14-testgame-descriptor.txt");
            descriptor<<result.descriptorPath;
            std::cout<<"G13E14_STUDIO_TEST_GAME_SNAPSHOT_PASS "
              <<result.descriptorPath<<std::endl;
          }
          auto saved=Records(scene);
          Need(saved.size()==1,"G13E8 should author exactly one waterfall");
          Need(scene.Entity_FindByName("Optional receiving pool (fixture only)")==
               wi::ecs::INVALID_ENTITY,"Pool reappeared after save");
          std::cout<<"G13E8_NATIVE_JUNCTION_SAVED file="<<save
            <<" bytes="<<fs::file_size(fs::u8path(save))
            <<" native_chunks="<<terrain->chunks.size()
            <<" waterfalls=1 river=1 no_faux_pool=1"<<std::endl;
        }
        render.flow.SetSuspended(true);
        app.ActivatePath(nullptr);
        wi::graphics::GetDevice()->WaitForGPU();
        cleanShutdown();
        return 0;
      }
      // The second waterfall needs rocks to trace like the approved first
      // waterfall, not a free-falling rectangular sheet.
      const size_t priorObjects=scene.objects.GetCount();
      int duplicatedRocks=0;
      for(size_t i=0;i<priorObjects;++i){
        const auto existing=scene.objects.GetEntity(i);
        const auto* name=scene.names.GetComponent(existing);
        if(!name||(name->name!="Collision test supplied rock" &&
            name->name!="Backdrop Cliff Face" &&
            name->name!="Rock shelf" &&
            name->name!="Rock floor" &&
            name->name!="Optional receiving pool (fixture only)"))continue;
        const auto* srcTransform=scene.transforms.GetComponent(existing);
        if(!srcTransform)continue;
        const auto sourceObject=scene.objects[i];
        // Imported GLB rocks are sometimes children of importer model roots.
        // Copy their FULL WORLD placement rather than the parent-relative local
        // transform (which would bury the clone inside the cliff).
        const auto sourceWorld=scene.ComputeEntityMatrixRecursive(existing);
        const auto clone=scene.Entity_CreateObject(
            std::string("G13E7 separate fixture ") + name->name);
        *scene.objects.GetComponent(clone)=sourceObject;
        auto* destTransform=scene.transforms.GetComponent(clone);
        destTransform->MatrixTransform(
            sourceWorld * DirectX::XMMatrixTranslation(20.f,0.f,0.f));
        destTransform->SetDirty();destTransform->UpdateTransform();
        if(name->name=="Collision test supplied rock")++duplicatedRocks;
      }
      Need(duplicatedRocks>=3,"G13E7 missing second waterfall collision shelves");
      scene.Update(0);
      captureVisual("G13E7-one-waterfall-with-second-rocks.png",70);
      auto second=records[0].settings;
      second.crest.x+=20.f;second.landing.x+=20.f;
      second.width=5.2f;second.impactSpray=.75f;
      Need(session.Commands().Execute(wf::MakeCreateWaterfallCommand(scene,second)),
          "G13E7 second waterfall authoring failed");
      camera.TransformCamera(DirectX::XMMatrixInverse(nullptr,
        DirectX::XMMatrixLookAtLH(
          DirectX::XMVectorSet(36.f,15.f,-31.f,1.f),
          DirectX::XMVectorSet(10.f,5.f,0.f,1.f),DirectX::XMVectorSet(0.f,1.f,0.f,0.f))));
      camera.UpdateCamera();
      captureVisual("G13E7-two-waterfalls-river-terrain.png",150);
      std::vector<wf::AuthoredWaterfall> actual;
      std::string waterfallError;
      Need(wf::EnumerateWaterfalls(scene,actual,waterfallError)&&actual.size()==2,
           "G13E7 authored record count mismatch");
      for(const auto& record:actual){
        size_t generatedObjects=0;
        for(size_t k=0;k<scene.objects.GetCount();++k){
          auto ent=scene.objects.GetEntity(k);
          const auto* md=scene.metadatas.GetComponent(ent);
          if(!md||!md->string_values.has("renegade.waterfall.generated.v1")||
             md->string_values.get("renegade.waterfall.generated.v1")!=record.id)continue;
          const auto* mesh=scene.meshes.GetComponent(scene.objects[k].meshID);
          const auto* material=scene.materials.GetComponent(ent);
          ++generatedObjects;
          std::cout<<"G13E7_BODY owner="<<record.id
            <<" object="<<ent
            <<" renderable="<<scene.objects[k].IsRenderable()
            <<" verts="<<(mesh?mesh->vertex_positions.size():0)
            <<" shader="<<(material?material->customShaderID:-999)<<std::endl;
        }
        Need(generatedObjects>0,"G13E7 waterfall has no generated body object");
      }
      if(visualExport){
        checkpoint("pre_export_save_branch");
        // Studio's production SceneDocumentService::Save() already removes
        // ephemeral waterfall GPU visuals before native WISCENE serialization.
        // Do NOT call renderer.Clear() or deactivate RenderPath manually before
        // saving: the terrain/scene still owns graphics and job workers.
        // This verifier must only persist the authored scene and allow its
        // ordinary cleanShutdown() after the RenderPath is deactivated.
        std::cerr<<"G13E7_EXPORT_STAGE pre_cancel"<<std::endl;
        terrain->Generation_Cancel();
        wi::graphics::GetDevice()->WaitForGPU();
        const size_t terrainCount=terrain->chunks.size();
        const auto savedPath=session.Scenes().CurrentPath();
        std::cerr<<"G13E7_EXPORT_STAGE saving "<<savedPath<<std::endl;
        Need(session.SaveScene(savedPath),
          "G13E7 Studio SaveScene failed: "+session.Scenes().LastError());
        std::cerr<<"G13E7_EXPORT_STAGE saved"<<std::endl;
        std::vector<wf::AuthoredWaterfall> saved;
        std::string saveError;
        Need(wf::EnumerateWaterfalls(scene,saved,saveError)&&saved.size()==2,
          "G13E7 saved scene lacks two authored waterfalls: "+saveError);
        Need(fs::is_regular_file(fs::u8path(savedPath)),
          "G13E7 native WISCENE file missing after successful save");
        std::cout<<"G13E7_DISPOSABLE_STUDIO_SCENE_SAVED path="<<savedPath
          <<" native_chunks="<<terrainCount
          <<" authored_waterfalls="<<saved.size()
          <<" river="<<riverID<<" bytes="<<fs::file_size(fs::u8path(savedPath))
          <<" generated_after_save="<<Generated(scene)<<std::endl;
        std::cerr<<"G13E7_EXPORT_STAGE deactivate_path"<<std::endl;
        render.flow.SetSuspended(true);
        app.ActivatePath(nullptr);
        wi::graphics::GetDevice()->WaitForGPU();
        std::cerr<<"G13E7_EXPORT_STAGE cleanup"<<std::endl;
        cleanShutdown();
        std::cerr<<"G13E7_EXPORT_STAGE pass"<<std::endl;
        return 0;
      }
      std::cout<<"G13E7_SECOND_COLLISION_SHELVES copies="<<duplicatedRocks
        <<" waterfall_active="<<renderer.ActiveCount()<<std::endl;
      Need(renderer.ActiveCount()==2,"G13E7 second waterfall visual missing");
      std::cout<<"G13E7_NATIVE_PIXEL_TEST_PASS real_terrain="<<terrain->chunks.size()
        <<" waterfalls=2 river=1 captures=2"<<std::endl;
    }
    std::sort(measured.begin(),measured.end());
    const auto avg=std::accumulate(measured.begin(),measured.end(),0.0)/double(measured.size());
    const auto p95=measured[size_t(measured.size()*.95)];
    std::cout<<"G13E6_NATIVE_TERRAIN_GENERATOR chunks="<<chunks
      <<" expected_square="<<expectedChunks<<" frames="<<frames
      <<" waterfall_prepare_avg_ms="<<avg
      <<" waterfall_prepare_p95_ms="<<p95
      <<" stable_after_generation=1"
      <<" changed_from_pre_generation="<<(original!=afterGeneration)
      <<" chunks_height_band="<<terrainHeightBand<<" chunks_below_band="<<terrainOutsideHeightBand
      <<" terrain_proximity="<<(nativeTerrainNear?"near":"distant")
      <<" complete="<<(chunks>=expectedChunks)<<std::endl;
    app.ActivatePath(nullptr);
    wi::graphics::GetDevice()->WaitForGPU();
    cleanShutdown();
    return chunks>=expectedChunks?0:8;
  }
  if(terrainGrid){
    Need(records.size()==1,"G13E6 19x19 grid requires one authored fall");
    constexpr int grid=19, side=67; // Renegade default radius 9, Wicked chunk width 67
    const auto material=scene.Entity_CreateMaterial("G13E6 disposable grid opaque");
    auto* mat=scene.materials.GetComponent(material);
    mat->shaderType=wi::scene::MaterialComponent::SHADERTYPE_PBR;
    mat->baseColor={.4f,.4f,.4f,1.f};
    auto tstart=std::chrono::steady_clock::now();
    wi::ecs::Entity distant=wi::ecs::INVALID_ENTITY;
    std::uint64_t vertexTotal=0,triangleTotal=0;
    for(int z=-9;z<=9;++z)for(int x=-9;x<=9;++x){
      auto entity=scene.Entity_CreateObject("G13E6 terrain-sized opaque chunk");
      auto* object=scene.objects.GetComponent(entity);object->meshID=entity;
      auto& mesh=scene.meshes.Create(entity);
      mesh.vertex_positions.reserve(side*side);
      mesh.indices.reserve((side-1)*(side-1)*6);
      const float worldX=float(x*66),worldZ=float(z*66);
      // Disjoint beneath the waterfall's actual collision envelope so these
      // 361 meshes exercise offscreen terrain scan, not change the water art.
      const float terrainY=-39.f;
      for(int zz=0;zz<side;++zz)for(int xx=0;xx<side;++xx)
        mesh.vertex_positions.push_back({worldX+float(xx),terrainY+.01f*std::sin(float(xx+zz)),worldZ+float(zz)});
      for(int zz=0;zz<side-1;++zz)for(int xx=0;xx<side-1;++xx){
        const uint32_t k=uint32_t(zz*side+xx);
        for(uint32_t v:{k,k+1,k+side,k+side,k+1,k+side+1})mesh.indices.push_back(v);
      }
      auto& sub=mesh.subsets.emplace_back();sub.materialID=material;
      sub.indexCount=uint32_t(mesh.indices.size());
      mesh.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
      mesh.CreateRenderData();
      vertexTotal+=mesh.vertex_positions.size();
      triangleTotal+=mesh.indices.size()/3;
      if(x==9&&z==9)distant=entity;
    }
    const auto buildMs=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-tstart).count();
    scene.Update(0);
    const double firstWithGrid=tick();
    const auto id=records[0].id;
    const auto emitterBefore=OwnedEmitters(scene,id);
    const size_t derivedBefore=Generated(scene);
    std::vector<double> elapsed,prepareOnly;
    for(int frame=0;frame<150;++frame){
      auto start=std::chrono::steady_clock::now();
      renderer.Prepare(scene,session.Scenes().Revision(),1.f/60.f,false);
      auto ms=std::chrono::duration<double,std::milli>(
          std::chrono::steady_clock::now()-start).count();
      prepareOnly.push_back(ms);
      scene.Update(0);
      elapsed.push_back(std::chrono::duration<double,std::milli>(
          std::chrono::steady_clock::now()-start).count());
      Need(renderer.ActiveCount()==1&&Generated(scene)==derivedBefore,
        "G13E6 unchanged distant terrain grid rebuilt waterfall");
    }
    auto summarise=[](std::vector<double> samples){
      const auto mean=std::accumulate(samples.begin(),samples.end(),0.0)/double(samples.size());
      std::sort(samples.begin(),samples.end());
      return std::pair<double,double>{mean,samples[size_t(samples.size()*.95)]};
    };
    const auto [prepAverage,prep95]=summarise(prepareOnly);
    const auto [fullAverage,full95]=summarise(elapsed);
    auto* farMesh=scene.meshes.GetComponent(distant);
    Need(farMesh&&farMesh->vertex_positions.size()==side*side,"G13E6 distant tile absent");
    const auto farFirst=farMesh->vertex_positions[0].y;
    // Regression for the original overbroad obstacle scan: far chunk meshes
    // must not require BVH creation just because a waterfall exists elsewhere.
    Need(!farMesh->bvh.IsValid(),"G13E6 far terrain BVH was eagerly built");
    for(auto& v:farMesh->vertex_positions)v.y+=.48f;
    farMesh->BuildBVH();farMesh->CreateRenderData();
    scene.Update(0);tick(.4f);
    Need(farMesh->vertex_positions[0].y>farFirst+.4f,"G13E6 far sculpt refused");
    Need(OwnedEmitters(scene,id).second==emitterBefore.second,
      "G13E6 far sculpt unnecessarily rebuilt original waterfall");
    auto* distantTransform=scene.transforms.GetComponent(distant);
    Need(distantTransform!=nullptr,"Distant chunk transform missing");
    distantTransform->translation_local.x+=12.f;
    distantTransform->SetDirty();distantTransform->UpdateTransform();
    scene.Update(0);tick(.4f);
    Need(OwnedEmitters(scene,id).second==emitterBefore.second,
      "G13E6 far transform unnecessarily rebuilt original waterfall");
    std::cout<<std::fixed<<std::setprecision(3)
      <<"G13E6_TERRAIN_GRID_PASS chunks="<<grid*grid<<" vertices="<<vertexTotal
      <<" triangles="<<triangleTotal
      <<" fixture_build_ms="<<buildMs
      <<" first_full_ms="<<firstWithGrid
      <<" prepare_avg_ms="<<prepAverage<<" prepare_p95_ms="<<prep95
      <<" prepare_max_ms="<<*std::max_element(prepareOnly.begin(),prepareOnly.end())
      <<" scene_full_avg_ms="<<fullAverage<<" scene_full_p95_ms="<<full95
      <<" distant_sculpt_no_rebuild=1 distant_transform_no_rebuild=1"
      <<" far_bvh_lazy=1 stable_generated="<<derivedBefore<<std::endl;
    cleanShutdown();return 0;
  }
  const double baseline=tick();
  Need(renderer.ActiveCount()==records.size(),"Initial renderer count invalid");
  if(morph){
    Need(records.size()==1,"G13E5 morph proof requires one original waterfall");
    const auto id=records.front().id;
    const auto start=OwnedEmitters(scene,id);
    Need(!start.second.empty(),"G13E5 no initial emitted spray");
    wi::scene::MeshComponent* rock=nullptr;
    for(size_t i=0;i<scene.objects.GetCount();++i){
      auto e=scene.objects.GetEntity(i);
      auto* name=scene.names.GetComponent(e);
      if(name&&name->name=="Collision test supplied rock"){
        rock=scene.meshes.GetComponent(scene.objects[i].meshID);break;
      }
    }
    Need(rock&&rock->vertex_positions.size()>100,
         "G13E5 expected supplied 15k-vertex test rock");
    const size_t verticesBefore=rock->vertex_positions.size();
    const size_t trianglesBefore=rock->indices.size();
    const auto previous=rock->vertex_positions[0];
    for(auto& point:rock->vertex_positions)point.y+=.48f;
    rock->BuildBVH();
    rock->CreateRenderData();
    scene.Update(0);
    Need(rock->vertex_positions.size()==verticesBefore&&
         rock->indices.size()==trianglesBefore&&
         rock->vertex_positions[0].y>previous.y+.4f,
         "G13E5 failed in-place rock sculpt invariants");
    tick(.6f);
    const auto after=OwnedEmitters(scene,id);
    const bool refreshed=after.second!=start.second;
    std::cout<<"G13E5_SAME_COUNT_ROCK_MORPH "
       <<"vertices="<<verticesBefore<<" triangles="<<trianglesBefore/3
       <<" delta_y=0.48 new_emitter_entities="<<refreshed
       <<" renderer="<<renderer.Status()<<std::endl;
    cleanShutdown();
    if(!refreshed){
      std::cerr<<"G13E5_STALE_COLLISION_REPRODUCED same_count_rock_geometry_did_not_invalidate"<<std::endl;
      return 4;
    }
    std::cout<<"G13E5_ROCK_MORPH_REBUILD_PASS"<<std::endl;return 0;
  }
  if(meshSurvey){
    const auto& d=records.front().settings;
    std::cout<<"G13E5_WATERFALL_SPAN crest="<<d.crest.x<<","<<d.crest.y<<","<<d.crest.z
        <<" landing="<<d.landing.x<<","<<d.landing.y<<","<<d.landing.z
        <<" width="<<d.width<<std::endl;
    for(size_t i=0;i<scene.objects.GetCount();++i){
      auto e=scene.objects.GetEntity(i);
      const auto* md=scene.metadatas.GetComponent(e);
      if(md&&md->string_values.has("renegade.waterfall.generated.v1"))continue;
      const auto* mesh=scene.meshes.GetComponent(scene.objects[i].meshID);
      if(!mesh||mesh->vertex_positions.empty())continue;
      auto* name=scene.names.GetComponent(e);
      auto matrix=scene.ComputeEntityMatrixRecursive(e);
      auto world=DirectX::XMVector3Transform(
          DirectX::XMLoadFloat3(&mesh->vertex_positions[0]),matrix);
      XMFLOAT3 point;DirectX::XMStoreFloat3(&point,world);
      std::cout<<"G13E5_OBJECT entity="<<e<<" mesh="<<scene.objects[i].meshID
          <<" name="<<(name?name->name:"[unnamed]")
          <<" vertices="<<mesh->vertex_positions.size()
          <<" first_world="<<point.x<<","<<point.y<<","<<point.z
          <<" bvh="<<mesh->bvh.IsValid()<<std::endl;
    }
    cleanShutdown();return 0;
  }
  if(soak){
    Need(records.size()==2,"Soak requires exactly two persisted waterfalls");
    const auto entities=Generated(scene);
    const auto emitters=scene.emitters.GetCount();
    auto a=OwnedEmitters(scene,records[0].id);
    auto b=OwnedEmitters(scene,records[1].id);
    std::vector<double> elapsed;
    for(int frame=0;frame<180;++frame){
      auto ms=tick();
      elapsed.push_back(ms);
      Need(renderer.ActiveCount()==2&&Generated(scene)==entities&&
           scene.emitters.GetCount()==emitters,
           "Long-run G13E4 generated duplicate/leaked waterfall entities");
      if(frame==89||frame==179)wi::graphics::GetDevice()->WaitForGPU();
    }
    Need(OwnedEmitters(scene,records[0].id).second==a.second&&
         OwnedEmitters(scene,records[1].id).second==b.second,
         "Long-run renderer replaced stable emitter entities unexpectedly");
    const auto mean=std::accumulate(elapsed.begin(),elapsed.end(),0.0)/
        double(elapsed.size());
    std::sort(elapsed.begin(),elapsed.end());
    const auto p95=elapsed[size_t(double(elapsed.size()-.001)*.95)];
    std::cout<<"G13E4_MULTI_SOAK_PASS frames=180 authored=2 active=2 "
        <<"stable_emitters="<<emitters<<" stable_generated="<<entities
        <<" cpu_frame_avg_ms="<<mean<<" cpu_frame_p95_ms="<<p95<<std::endl;
    cleanShutdown();return 0;
  }
  if(coexist){
    Need(records.size()==1,"G11 + G13 fixture must start with one waterfall");
    auto waterfallId=records.front().id;
    auto before=OwnedEmitters(scene,waterfallId);
    renegade::bridge::RiverSplineOptions opt;
    opt.widthMeters=4.f;opt.surfaceOffsetMeters=-.08f;opt.subdivisionsPerSegment=12;
    std::vector<XMFLOAT3> control={{-13.f,.20f,9.f},{-6.f,.20f,9.f},
       {2.f,.20f,9.f},{10.f,.20f,9.f}};
    auto river=std::make_unique<renegade::bridge::CreateRiverSplineCommand>(scene,control,opt);
    auto* raw=river.get();
    Need(session.Commands().Execute(std::move(river)),"Native G11 CreateRiverSpline command refused");
    const auto riverEntity=raw->CreatedEntity();
    Need(scene.splines.Contains(riverEntity),"G11 spline not created");
    renegade::bridge::RiverFlowProfile profile;
    profile.enabled=true;profile.speed=1.f;profile.intensity=.8f;
    Need(session.Commands().Execute(
       std::make_unique<renegade::bridge::SetRiverFlowProfileCommand>(scene,riverEntity,profile)),
       "G11 flow profile refused");
    scene.Update(0);
    const auto dir=fs::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/river";
    {
      // G11 loads shaders relative to the package root, as native Studio does.
      Need(fs::is_regular_file(dir/"RiverRefinedFoamPS.cso"),
           "G11 shader not staged beside verifier executable");
      fs::current_path(dir.parent_path().parent_path());
      renegade::bridge::RiverFlowRenderer g11(dir.generic_string());
      g11.Prepare(scene,session.Scenes().Revision(),1.f/60.f);
      const double startMs=tick();
      std::cout<<"G13E4_G11_DIAGNOSTIC active="<<g11.ActiveCount()
        <<" status="<<g11.Status(riverEntity)
        <<" identity="<<renegade::bridge::PersistentEntityId(scene,riverEntity)
        <<std::endl;
      Need(g11.ActiveCount()>=1,
           "G11 river did not start in mixed scene // "+g11.Status(riverEntity));
      Need(renderer.ActiveCount()==1&&OwnedEmitters(scene,waterfallId).second==before.second,
         "G11 river unexpectedly rebuilt G13 waterfall");
      for(int frame=0;frame<8;++frame){
        g11.Prepare(scene,session.Scenes().Revision(),1.f/60.f);
        tick();
        Need(renderer.ActiveCount()==1 && g11.ActiveCount()>=1,
             "G11/G13 lost active instance while coexisting");
      }
      auto updated=records.front().settings;updated.impactSpray=2;
      Need(session.Commands().Execute(wf::MakeEditWaterfallCommand(scene,waterfallId,updated)),
         "G13 impact control command failed in active G11 scene");
      tick();g11.Prepare(scene,session.Scenes().Revision(),1.f/60.f);
      Need(g11.ActiveCount()>=1&&renderer.ActiveCount()==1,
           "G13 impact update disrupted G11 river");
      Need(OwnedEmitters(scene,waterfallId).first>before.first,
           "Waterfall impact spray did not change beside G11 river");
      Need(session.Commands().Undo(),"G13 impact Undo failed in G11 scene");
      tick();g11.Prepare(scene,session.Scenes().Revision(),1.f/60.f);
      Need(std::abs(OwnedEmitters(scene,waterfallId).first-before.first)<.001,
           "G11 river prevented G13 impact undo");
      std::cout<<"G13E4_G11_G13_COEXISTENCE_PASS g11="<<g11.ActiveCount()
        <<" waterfall="<<renderer.ActiveCount()
        <<" no_unintended_rebuild=1 independent_impact=1"
        <<" mixed_prepare_ms="<<startMs<<std::endl;
      g11.SetSuspended(true);
      wi::graphics::GetDevice()->WaitForGPU();
    }
    cleanShutdown();return 0;
  }
  if(verify){
    Need(OwnedEmitters(scene,records[0].id).second.size()>0,"Waterfall 1 emitter missing after disk reopen");
    Need(OwnedEmitters(scene,records[1].id).second.size()>0,"Waterfall 2 emitter missing after disk reopen");
    std::cout<<"G13E4_SEPARATE_PROCESS_MULTI_REOPEN_PASS authored=2 active=2 generated="
       <<Generated(scene)<<" emitters="<<scene.emitters.GetCount()<<" first_ms="<<baseline
       <<std::endl;cleanShutdown();return 0;
  }
  std::cout<<"G13E4_BASELINE original="<<records.front().id
       <<" active="<<renderer.ActiveCount()<<" emitters="<<scene.emitters.GetCount()
       <<" generated="<<Generated(scene)<<" prepare_ms="<<baseline<<std::endl;
  const auto original=records[0];
  auto second=original.settings;
  second.crest.x+=2.5f;second.landing.x+=2.5f;
  second.crest.z+=.4f;second.landing.z+=.4f;
  second.width=original.settings.width*.83f;
  second.impactSpray=1.f;
  auto create=wf::MakeCreateWaterfallCommand(scene,second);
  const auto secondId=create->Id();
  Need(secondId!=original.id&&session.Commands().Execute(std::move(create)),
       "Create independent waterfall failed");
  auto ms2=tick();
  Need(renderer.ActiveCount()==2,"Renderer did not create second waterfall");
  auto baseRate1=OwnedEmitters(scene,original.id);
  auto baseRate2=OwnedEmitters(scene,secondId);
  Need(baseRate1.first>0&&baseRate2.first>0,"One waterfall lost particle emitters");
  const size_t generatedTwo=Generated(scene),emitterTwo=scene.emitters.GetCount();
  for(int frame=0;frame<12;++frame){
   tick();
   Need(Generated(scene)==generatedTwo&&scene.emitters.GetCount()==emitterTwo,
       "Frames generated duplicate waterfall visual entities");
  }
  auto edit=second;edit.impactSpray=2.5f;
  Need(session.Commands().Execute(wf::MakeEditWaterfallCommand(scene,secondId,edit)),
       "Second waterfall impact spray edit failed");
  tick();
  auto edited1=OwnedEmitters(scene,original.id);
  auto edited2=OwnedEmitters(scene,secondId);
  Need(std::abs(edited1.first-baseRate1.first)<.0001,"Editing second affects first emitter rates");
  Need(edited2.first>baseRate2.first,"Impact slider did not increase second emitter rates");
  Need(edited2.second==baseRate2.second,
       "Impact-only slider caused a geometry/emitter rebuild");
  Need(session.Commands().Undo(),"Undo second impact setting failed");tick();
  Need(std::abs(OwnedEmitters(scene,secondId).first-baseRate2.first)<.001,
       "Undo did not restore independent particle rate");
  Need(session.Commands().Redo(),"Redo second impact failed");tick();
  Need(std::abs(OwnedEmitters(scene,secondId).first-edited2.first)<.001,
       "Redo did not recover particle rate");
  Need(session.Commands().Execute(wf::MakeRemoveWaterfallCommand(scene,secondId)),
       "Delete second failed");tick();
  Need(renderer.ActiveCount()==1&&OwnedEmitters(scene,secondId).second.empty(),
       "Delete left second waterfall VFX behind");
  Need(session.Commands().Undo(),"Undo deletion failed");tick();
  Need(renderer.ActiveCount()==2&&OwnedEmitters(scene,secondId).first>0,
       "Undo delete did not rebuild waterfall VFX");
  Need(session.Commands().Redo(),"Redo deletion failed");tick();
  Need(renderer.ActiveCount()==1,"Redo delete did not clear second VFX");
  Need(session.Commands().Undo(),"Final undo deletion failed");tick();
  Need(renderer.ActiveCount()==2,"Final restore failed");
  const auto path=session.Scenes().CurrentPath();
  Need(session.SaveScene(path),session.Scenes().LastError());
  tick();tick();
  Need(renderer.ActiveCount()==2,"Native save did not recreate generated VFX");
  Need(Records(scene).size()==2,"Saved scene lost authored instance");
  std::cout<<"G13E4_MULTI_IN_PROCESS_PASS authored=2 active=2 "
   "independent_impact=1 undo_redo=1 delete_cleanup=1 "
   "no_duplicate_emission=1 save_return=1 "
   <<"emitters="<<scene.emitters.GetCount()<<" generated="<<Generated(scene)
   <<" two_prepare_ms="<<ms2<<std::endl;
  cleanShutdown();return 0;
 }catch(const std::exception& e){
   if(argc>=3){
     std::ofstream fail(fs::absolute(fs::u8path(argv[2]))/"g13e7-export-error.txt");
     fail<<"G13E4_MULTI_FAIL "<<e.what()<<std::endl;
   }
   std::cerr<<"G13E4_MULTI_FAIL "<<e.what()<<std::endl;return 1;
 }
}
