#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/RiverFlowRenderer.h"
#include "renegade/bridge/TestLevelSnapshotService.h"
#include "TestLevelRuntimeProcess.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <thread>
using namespace renegade::bridge;
namespace fs=std::filesystem;
static void Check(bool ok,const std::string& message){static int stage=0;++stage;if(stage<15||!ok)std::cout<<"COURSE_LIFECYCLE_CHECK "<<stage<<" ok="<<ok<<" message="<<message<<std::endl;if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
 if(argc!=3&&argc!=4)return 2;const bool rapids=argc==4&&std::string(argv[3])=="--rapids";std::cout.setf(std::ios::unitbuf);
 HWND window=CreateWindowExW(0,L"STATIC",L"G6 lifecycle verification",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);wi::initializer::InitializeComponentsImmediate();
 try{
  wi::resourcemanager::SetMode(wi::resourcemanager::Mode::EMBED_FILE_DATA);
  StudioSession session;session.Projects().Initialize("lifecycle-proof-recents.ini");
  bool projectOpened=session.Projects().OpenProject(fs::absolute(argv[1]).generic_u8string());Check(projectOpened,session.Projects().LastError());
  Check(session.Projects().CommitPendingProject(),session.Projects().LastError());
  auto scenePath=fs::absolute(argv[1]).parent_path()/"Content/Scenes/River.wiscene";
  Check(session.Documents().Open(scenePath.generic_u8string()),session.Scenes().LastError());
  auto& scene=session.Scenes().GetScene();auto river=scene.Entity_FindByName("River Water 5");
  RiverFlowProfile profile;std::string error;Check(ReadRiverFlowProfile(scene,river,profile,error)&&profile.enabled,error);
  auto id=PersistentEntityId(scene,river);auto oldPath=session.Scenes().CurrentPath();
  if(rapids){Check(profile.rapids,"Expected opt-in measured rapids");profile.rapidIntensity=.5f;}else profile.reversed=!profile.reversed;
  Check(session.Commands().Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,river,profile)),"Unsaved reverse command");
  auto revision=session.Commands().Revision();
  TestLevelSnapshotService snapshots(session.Scenes(),session.Commands());
  TestLevelSnapshot snapshot;Check(snapshots.Create(session.Projects().CurrentProject(),snapshot,error),error);
  {
   wi::Archive archive(snapshot.scenePath,true);wi::scene::Scene copy;copy.Serialize(archive);
   auto e=copy.Entity_FindByName("River Water 5");RiverFlowProfile copied;
   Check(ReadRiverFlowProfile(copy,e,copied,error)&&copied.enabled&&copied.reversed==profile.reversed&&(!rapids||(copied.rapids&&copied.rapidIntensity==.5f)),"Unsaved profile absent from snapshot");
   Check(PersistentEntityId(copy,e)==id,"Snapshot river UUID changed");
   Check(!copy.materials.GetComponent(e)->IsCustomShader(),"Transient GPU shader in snapshot");
  }
  Check(session.Commands().Revision()==revision&&session.Scenes().CurrentPath()==oldPath,"Snapshot mutated editor state");
  renegade::studio::TestLevelRuntimeProcess child;renegade::studio::TestLevelLaunchOptions options;
  options.executablePath=fs::absolute(argv[2]).generic_u8string();
  options.workingDirectory=fs::absolute(argv[2]).parent_path().generic_u8string();
  options.arguments={"dx12","alwaysactive","--project",snapshot.descriptorPath};
  options.startupTimeout=std::chrono::seconds(45);
  Check(child.Launch(options,snapshot,error),error);
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(48);bool ready=false;
  while(std::chrono::steady_clock::now()<deadline){
   auto result=child.Poll();if(result.finished)throw std::runtime_error(result.message);
   if(result.ready){ready=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  Check(ready,"Runtime readiness deadline");std::cout<<"G6_TEST_GAME_READY pid="<<child.ProcessId()<<"\n";
  const int holdMilliseconds=argc==4&&!rapids?std::stoi(argv[3]):(rapids?30000:3000);
  Check(holdMilliseconds>=1000&&holdMilliseconds<=30000,"Runtime hold duration outside proof limits");
  for(int n=0;n<holdMilliseconds/50;++n){Check(!child.Poll().finished,"Runtime exited after ready");std::this_thread::sleep_for(std::chrono::milliseconds(50));}
  auto stopped=child.Stop();
  Check(stopped.cleanupSucceeded&&!fs::exists(fs::u8path(snapshot.sessionDirectory)),"Snapshot cleanup failed");
  Check(session.Commands().Revision()==revision,"Runtime changed editor history");
  profile.speed=.75f;Check(session.Commands().Execute(std::make_unique<SetRiverFlowProfileCommand>(scene,river,profile)),"Post-return edit failed");
  Check(session.Commands().Undo(),"Post-return undo failed");
  Check(session.Commands().Undo(),"Initial unsaved undo failed");
  std::cout<<"G6_TEST_GAME_RETURN_EDIT_CLEANUP=PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<"G6_LIFECYCLE_FAIL="<<e.what()<<"\n";return 1;}
}
