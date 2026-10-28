#include <WickedEngine.h>
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include "renegade/bridge/PlaceableWaterfallRenderer.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "../EngineBridge/src/PlaceableWaterfallAuthoring.h"
#include "../Studio/src/WindowsGameBuildController.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
void Require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
 const bool combined=argc==4&&std::string(argv[3])=="--combined";
 if(argc!=3 && !combined)return 2;
 try{
  auto out=fs::absolute(argv[2]);fs::create_directories(out);
  HWND window=CreateWindowExW(0,L"STATIC",L"Renegade G13E3 lifecycle verification",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  struct App:wi::Application{} app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
  wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
  renegade::bridge::StudioSession session;session.Projects().Initialize((out/"proof-project.ini").generic_string());
  Require(session.Projects().OpenProject(fs::absolute(argv[1]).generic_string()),session.Projects().LastError());
  auto startup=session.Projects().StartupScenePath();Require(session.LoadScene(startup),session.Scenes().LastError());
  auto project=session.Projects().CurrentProject();auto& scene=session.Scenes().GetScene();
  namespace wf=renegade::bridge::placeable_waterfall;std::vector<wf::AuthoredWaterfall> before;std::string error;
  Require(wf::EnumerateWaterfalls(scene,before,error)&&before.size()==1,"Expected one authored waterfall");
  auto checkCombined=[&](const char* stage){
    if(!combined)return;
    Require(scene.terrains.GetCount()==1&&scene.splines.GetCount()==1,
      std::string(stage)+": expected one native terrain and one G11 river");
    const auto river=scene.splines.GetEntity(0);
    const auto* spline=scene.splines.GetComponent(river);
    Require(spline&&spline->spline_node_entities.size()==6,
      std::string(stage)+": six native river nodes not retained");
    const auto selected=spline->spline_node_entities[1];
    const auto* node=scene.transforms.GetComponent(selected);
    Require(node&&std::abs(node->scale_local.x-1.5f)<.001f,
      std::string(stage)+": saved local receiving width not 1.5x");
    renegade::bridge::RiverFlowProfile flow;
    std::string why;
    Require(renegade::bridge::ReadRiverFlowProfile(scene,river,flow,why)&&
      flow.enabled&&flow.startWhitewater &&
      renegade::bridge::ResolveRiverWhitewaterSourceNode(scene,river,flow)==selected,
      std::string(stage)+": whitewater lost stable receiving-node anchor "+why);
    renegade::bridge::RiverTerrainLayerSettings bed;
    Require(renegade::bridge::ReadRiverTerrainLayerSettings(
      scene,scene.terrains.GetEntity(0),river,bed,why)&&bed.linked,
      std::string(stage)+": linked carved terrain not retained "+why);
    Require(scene.Entity_FindByName("Optional receiving pool (fixture only)")==
      wi::ecs::INVALID_ENTITY,
      std::string(stage)+": fixture-only pool reappeared");
    std::cout<<"G13E11_COMBINED_STATE_PASS stage="<<stage
      <<" river=1 native_terrain=1 nodes=6 width_scale=1.5 stable_foam=1 linked_bed=1 no_pool=1"<<std::endl;
  };
  checkCombined("initial_open");
  renegade::bridge::PlaceableWaterfallRenderer preview;preview.Prepare(scene,session.Scenes().Revision(),1.f/60,false);scene.Update(0);
  Require(preview.ActiveCount()==1,preview.Status());
  auto undo=session.Commands().UndoCount();auto redo=session.Commands().RedoCount();auto path=session.Scenes().CurrentPath();
  renegade::bridge::TestLevelSnapshotService snapshots(session.Scenes(),session.Commands());renegade::bridge::TestLevelSnapshot snapshot;
  Require(snapshots.Create(project,snapshot,error),error);
  for(size_t i=0;i<scene.metadatas.GetCount();++i)Require(!scene.metadatas[i].string_values.has("renegade.waterfall.generated.v1"),"Snapshot retained generated visuals");
  Require(session.Commands().UndoCount()==undo&&session.Commands().RedoCount()==redo&&session.Scenes().CurrentPath()==path,"Snapshot changed editor history/path");
  checkCombined("after_snapshot");
  {std::ofstream f(out/"snapshot-descriptor.txt");f<<snapshot.descriptorPath;}
  preview.Prepare(scene,session.Scenes().Revision(),1.f/60,false);scene.Update(0);Require(preview.ActiveCount()==1,preview.Status());
  size_t emitters=scene.emitters.GetCount();preview.Prepare(scene,session.Scenes().Revision(),1.f/60,false);Require(scene.emitters.GetCount()==emitters,"Duplicate emitters after snapshot return");
  // The diagnostic scene was relocated from a disposable review path; its
  // original cliff and WATER normal resources currently live OUTSIDE this
  // disposable project. Real Build Game intentionally forbids packaging
  // outside_project dependencies. Import only these two known fixture maps
  // and rebase the corresponding material references before saving.
  const auto projectRoot=fs::u8path(project.rootPath);
  const auto localWaterfall=projectRoot/"Content/waterfall";
  fs::create_directories(localWaterfall);
  const auto sceneFolder=fs::u8path(path).parent_path();
  int relocated=0;
  for(size_t materialIndex=0;materialIndex<scene.materials.GetCount();++materialIndex){
      auto& mat=scene.materials[materialIndex];
      for(auto& texture:mat.textures){
          if(texture.name.empty())continue;
          const fs::path declared=fs::u8path(texture.name);
          const auto filename=declared.filename().generic_string();
          if(filename!="rock_original.png"&&filename!="waterNormalMap.jpg")
              continue;
          auto original=(declared.is_absolute()?declared:sceneFolder/declared).lexically_normal();
          if(combined&&!fs::is_regular_file(original)){
              // Explicitly fixture-only G13E3 inputs, NEVER production resolution
              // or a relaxation of Windows Build's outside-project rejection.
              const auto knownFixture=fs::u8path(__FILE__).parent_path().parent_path()/
                  "BUILD/waterfall-placeable/evidence/G13E3/Project/Content/waterfall"/filename;
              if(fs::is_regular_file(knownFixture))original=knownFixture;
          }
          Require(fs::is_regular_file(original),
              std::string("Fixture source texture missing: ")+original.generic_string());
          const auto destination=localWaterfall/filename;
          if(original!=destination){
              fs::copy_file(original,destination,fs::copy_options::overwrite_existing);
          }
          texture.name=fs::relative(destination,sceneFolder).generic_string();
          ++relocated;
      }
  }
  Require(relocated>=2,"Expected cliff and receiving-water fixture textures");
  std::cout<<"G13E3_FIXTURE_RESOURCES_IMPORTED count="<<relocated
      <<" project_local=1"<<std::endl;
  Require(session.SaveScene(path),session.Scenes().LastError());
  checkCombined("after_native_save");
  std::cout<<"NATIVE_SNAPSHOT_RETURN_PASS authored=1 generated_rebuilt=1 no_duplicates=1 history_preserved=1"<<std::endl;
  auto built=renegade::studio::BuildActiveWindowsGame();std::cout<<built.message<<std::endl;
  Require(built.succeeded,built.message);{std::ofstream f(out/"package-path.txt");f<<built.finalOutputPath;}
  std::cout<<"ACTUAL_BUILD_WINDOWS_GAME_PASS "<<built.finalOutputPath<<std::endl;
  DestroyWindow(window);return 0;
 }catch(const std::exception& e){std::cerr<<"G13E3_LIFECYCLE_FAIL "<<e.what()<<std::endl;return 1;}
}
