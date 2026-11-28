#include "renegade/bridge/ScreenAmmoCounter.h"
#include "renegade/bridge/ScreenPrefabService.h"
#include "renegade/bridge/ScreenSimulation.h"
#include "RuntimeScriptRuntime.h"
#include "renegade/bridge/ProjectService.h"
#include "../Runtime/src/RuntimeUiGameplaySnapshot.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
using namespace renegade::runtime;
namespace fs=std::filesystem;
int main(int argc,char** argv)
{
    if(argc==3&&std::string(argv[1])=="--review"){
        ProjectService projects;ProjectMetadata project;std::string error;
        if(!projects.InspectProject(argv[2],project,error)){std::cerr<<error;return 2;}
        const auto root=fs::u8path(project.rootPath);
        if(root.filename()!="Ammo Counter Review"){std::cerr<<"Use only the disposable ammo fixture.";return 3;}
        const auto path=(root/fs::u8path(project.defaultHud)).generic_u8string();ScreenDocument hud;
        if(!ReadScreenDocument(path,project.projectId,hud,error)){std::cerr<<error;return 4;}
        hud.widgets.clear();hud.actions.clear();hud.focusOrder.clear();
        if(!WriteScreenDocument(path,hud,error)){std::cerr<<error;return 5;}
        ScreenAuthoringSession target;std::vector<StableId> created;
        if(!target.Open(path,project.projectId,error)||!InsertAmmoCounterPrefab(target,hud.designWidth-370,hud.designHeight-155,created,error)||!target.Save(error)){std::cerr<<error;return 6;}
        std::cout<<"AMMO_REVIEW_READY "<<project.descriptorPath<<'\n';return 0;
    }
    int failures=0;std::string error;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<": "<<error<<'\n';}};
    const auto temp=fs::temp_directory_path()/("RenegadeAmmo-"+GenerateStableId());
    fs::create_directories(temp/"Content/UI");
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{temp};
    const auto project=GenerateStableId();auto prefab=MakeAmmoCounterPrefab(project);
    check(prefab.widgets.front().style.normal.background.alpha==0&&prefab.widgets.front().style.borderWidth==0,"Transparent stock counter background");
    check(WriteScreenDocument((temp/prefab.envelope.pathHint).generic_u8string(),prefab,error),"Write counter layers");
    ScreenAuthoringSession source;
    check(source.Open((temp/prefab.envelope.pathHint).generic_u8string(),project,error),"Open counter");
    const auto package=argc==3&&std::string(argv[1])=="--author"?fs::u8path(argv[2]):temp/"Package";
    check(PublishScreenWidgetPrefab(temp.generic_u8string(),source,prefab.widgets.front().id,package.generic_u8string(),error),"Publish ordinary portable counter");
    if(argc==3&&std::string(argv[1])=="--author")return failures?1:0;
    const auto target=temp/"Target";fs::create_directories(target/"Content/UI");
    ScreenDocument hud;hud.usage=ScreenUsage::Hud;
    hud.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Hud.renegade-screen","ammo-test");
    check(WriteScreenDocument((target/hud.envelope.pathHint).generic_u8string(),hud,error),"Write target HUD");
    ScreenAuthoringSession session;
    check(session.Open((target/hud.envelope.pathHint).generic_u8string(),hud.envelope.projectId,error),"Open target HUD");
    std::vector<StableId> created;
    check(InsertAmmoCounterPrefab(session,100,100,created,error)&&created.size()==1&&session.Document().widgets.size()==5,"Insert prefab with editable layers");
    check(session.Undo(error)&&session.Document().widgets.empty()&&session.Redo(error)&&session.Document().widgets.size()==5,"One-step insert history");
    check(session.Save(error)&&session.Reload(error),"Save/reopen counter");
    const auto& doc=session.Document();
    RuntimePlayerState player;wi::scene::Scene scene;
    CreatePlayerStartCommand start(scene,TransformState{});check(start.Execute(),"Create Player Start");
    check(SpawnRuntimePlayer(scene,ResolvePlayerStart(scene).start,player,error),"Spawn actual Player");
    RuntimePlayerViewAnimationState view;view.initialized=true;view.pairedAssembly=true;
    RuntimeUiGameplaySources sources;sources.player=&player;sources.scene=&scene;sources.playerView=&view;sources.sceneGeneration=1;
    RuntimeUiSnapshotPublisher publisher;check(publisher.BeginSession(1,error),"Begin snapshot session");
    ScreenBindingSnapshot snapshot;ScreenBindingContext context;ScreenBindingFrame frame;std::uint64_t revision=0;
    const auto requests=ScreenSimulation::Sources(doc);
    const auto evaluate=[&]{return publisher.Publish(requests,++revision,sources,snapshot,context,error)&&BuildScreenBindingFrame(doc,snapshot,context,frame,error);};
    const auto resolved=[&](const char* name)->const ScreenResolvedWidget*{
        for(const auto& w:doc.widgets)if(w.name.rfind(name,0)==0)for(const auto& value:frame.widgets)if(value.id==w.id)return &value;
        return nullptr;
    };
    for(unsigned slot=1;slot<=9;++slot){
        sources.weaponSlot=slot;sources.weaponName="Weapon "+std::to_string(slot);view.loadedShells=int(slot);view.firearm.capacity=int(slot*5);
        check(evaluate()&&frame.ready,"Evaluate all nine equipped slot states");
        check(resolved("Loaded rounds")&&resolved("Loaded rounds")->text=="AMMO: "+std::to_string(slot),"Loaded count follows active weapon");
        check(resolved("Weapon name")&&resolved("Weapon name")->text==sources.weaponName,"Name follows active weapon");
        check(resolved("Magazine capacity")&&resolved("Magazine capacity")->text=="CAPACITY: "+std::to_string(slot*5),"Capacity follows active weapon");
        check(resolved("Weapon slot")&&resolved("Weapon slot")->text=="SLOT "+std::to_string(slot),"Slot follows active weapon");
    }
    if(!resolved("Loaded rounds"))return 2;
    view.loadedShells=0;check(evaluate()&&resolved("Loaded rounds")->text=="AMMO: 0"&&resolved("Loaded rounds")->visible,"An empty magazine remains visible");
    view.meleeWeapon=true;check(evaluate()&&frame.ready,"Melee does not make HUD unready");
    for(const auto& w:frame.widgets)check(!w.visible,"Melee hides complete counter hierarchy");
    view.meleeWeapon=false;view.equipped=false;check(evaluate()&&!resolved("Loaded rounds")->visible,"Holstered weapon hides counter");
    view.equipped=true;view.spellPresentation=true;check(evaluate()&&!resolved("Loaded rounds")->visible,"Spells hide firearm counter");
    view.spellPresentation=false;view.loadedShells=7;view.oneShotPlaying=true;view.activeAction=PlayerViewAction::Reload;
    auto ammo=CapturePlayerWeaponAmmo(&view,"Rifle",9);check(ammo.available&&ammo.reloading&&ammo.loaded==7&&ammo.slot==9,"C++ read-only reload state");
    fs::create_directories(temp/"Content/Scripts");
    std::ofstream(temp/"Content/Scripts/ammo.lua")<<R"LUA(
return {
 on_start=function(self)
  local a=renegade.player.weapon_ammo()
  assert(a.available and a.name=="Rifle" and a.slot==9)
  assert(a.loaded==7 and a.capacity==45 and a.reloading and a.reserve==nil)
  a.loaded=999 -- a detached value; must not mutate gameplay
 end,
 on_update=function(self,dt)
  local a=renegade.player.weapon_ammo()
  assert(not a.available and a.loaded==nil and a.capacity==nil and a.reserve==nil)
 end
}
)LUA";
    auto scripts=CreateScriptDocument(project,GenerateStableId(),"Content/Scenes/Proof.wiscene","ammo-query");
    ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/ammo.lua";binding.apiVersion=1;
    auto attachment=CreateScriptAttachment(ScriptScope::Level,{},binding);attachment.presentation=ScriptPresentation::GlobalScript;scripts.attachments.push_back(attachment);
    RuntimeScriptRuntime lua;lua.SetWeaponAmmoProvider([&]{return CapturePlayerWeaponAmmo(&view,"Rifle",9);});
    check(lua.StartScene(scripts,scene,temp.generic_u8string(),error)&&lua.DisabledInstanceCount()==0&&lua.ActiveInstanceCount()==1,"Governed Lua sees native active weapon ammo");
    check(view.loadedShells==7,"Lua readout mutated ammunition");view.meleeWeapon=true;lua.Update(.01f);
    check(lua.DisabledInstanceCount()==0,"Governed Lua sees unavailable firearm state");
    for(const auto& d:lua.Diagnostics())std::cerr<<d.message<<'\n';lua.StopScene();
    ScreenDocument exported;
    check(InspectPortableScreenPackage(package.generic_u8string(),exported,error)&&exported.widgets.size()==5,"Counter can be published as an ordinary layered prefab");
    std::cout<<"ScreenAmmoPrefabTests failures="<<failures<<'\n';return failures?1:0;
}
