#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/WorldNavigationService.h"
#include "renegade/bridge/ScreenPrefabService.h"
#include "RuntimeScriptRuntime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace renegade::bridge;
using namespace renegade::runtime;
namespace fs=std::filesystem;
int failures=0;
void Check(bool ok,const std::string& why){if(!ok){++failures;std::cerr<<"FAIL: "<<why<<'\n';}}
int main(int argc,char** argv)
{
    const auto temp=fs::temp_directory_path()/("RenegadeNavigation-"+GenerateStableId());fs::create_directories(temp/"Content/UI");
    struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{temp};
    std::string error;auto w=MakeWorldNavigationWidget();
    ScreenDocument d;d.usage=ScreenUsage::Hud;d.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Navigation.renegade-screen","navigation-tests");d.widgets={w};
    const auto file=temp/d.envelope.pathHint;
    if(argc==3&&std::string(argv[1])=="--install-demo")
    {
        ProjectService projects;ProjectMetadata project;
        if(!projects.InspectProject(argv[2],project,error)){std::cerr<<error;return 2;}
        const auto root=fs::u8path(project.rootPath),scenePath=root/"Content/Scenes/Starter.wiscene";
        DocumentEnvelope envelope;if(!ReadDocumentEnvelope(scenePath.generic_u8string()+".rmeta",envelope,error)){std::cerr<<error;return 3;}
        const auto companion=ScriptDocumentPathForScene(scenePath.generic_u8string());
        if(fs::exists(fs::u8path(companion))||fs::exists(root/"Content/Scripts/navigation_example.lua")){std::cerr<<"Refuse to replace existing scripts.";return 4;}
        fs::create_directories(root/"Content/Scripts");
        fs::copy_file(fs::path(__FILE__).parent_path().parent_path()/"Studio/assets/ui/examples/navigation_example.lua",root/"Content/Scripts/navigation_example.lua");
        auto scripts=CreateScriptDocument(project.projectId,envelope.documentId,"Content/Scenes/Starter.wiscene","navigation-review");
        ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/navigation_example.lua";binding.apiVersion=1;
        auto attachment=CreateScriptAttachment(ScriptScope::Level,{},binding);attachment.presentation=ScriptPresentation::GlobalScript;scripts.attachments.push_back(std::move(attachment));
        if(!WriteScriptDocument(companion,scripts,error)){std::cerr<<error;return 7;}
        std::cout<<"NAVIGATION_DEMO_ATTACHED";return 0;
    }
    if(argc>=3&&std::string(argv[1])=="--author-neutral")
    {
        Check(WriteScreenDocument(file.generic_u8string(),d,error),error);ScreenAuthoringSession session;
        Check(session.Open(file.generic_u8string(),d.envelope.projectId,error),error);
        Check(PublishScreenWidgetPrefab(temp.generic_u8string(),session,w.id,argv[2],error),error);return failures?1:0;
    }
    if(argc==4&&std::string(argv[1])=="--author-fantasy")
    {
        const auto art=fs::u8path(argv[2]);const auto output=fs::path("Content/UI/NavigationFantasy");
        fs::create_directories(temp/output);
        const auto copy=[&](const fs::path& relative){const auto target=output/relative.filename();fs::copy_file(art/relative,temp/target);return target.generic_u8string();};
        w.resourcePath=copy("png/hud/compass_backing.png");w.navigation->bearingStrip=copy("png/hud/bearing_ruler_strip.png");w.navigation->pointerImage=copy("png/hud/heading_needle.png");
        for(const auto& item:fs::directory_iterator(art/"png/poi"))if(item.path().extension()==".png")w.navigation->icons[item.path().stem().u8string()]=copy(fs::path("png/poi")/item.path().filename());
        w.name="Fantasy navigation appearance";w.style.borderWidth=0;for(auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})v->background={0,0,0,0};
        d.widgets={w};Check(WriteScreenDocument(file.generic_u8string(),d,error),error);ScreenAuthoringSession session;
        Check(session.Open(file.generic_u8string(),d.envelope.projectId,error),error);
        Check(PublishScreenWidgetPrefab(temp.generic_u8string(),session,w.id,argv[3],error),error);return failures?1:0;
    }
    Check(WrapNavigationHeading(-1)==359&&WrapNavigationHeading(720)==0,"heading normalization");
    Check(NavigationBearingDelta(1,359)==2&&NavigationBearingDelta(359,1)==-2,"north wrap shortest direction");
    NavigationObserver camera{true,{},0},player{true,{},90};
    std::vector<NavigationTarget> targets={{"north","North","quest","",{0,0,20},10,true},{"east","East","town","",{20,0,0},0,true},{"west","West","town","",{-20,0,0},0,true}};
    auto config=*w.navigation;config.fieldOfView=120;
    auto frame=BuildNavigationFrame(config,camera,player,targets,{},640);
    Check(frame.available&&frame.markers.size()==1&&std::abs(frame.markers[0].fraction-.5f)<.001f,"camera north target projection and clipping");
    config.headingSource=NavigationHeadingSource::Player;frame=BuildNavigationFrame(config,camera,player,targets,{},640);
    Check(frame.markers.size()==1&&frame.markers[0].id=="east","independent player heading source");
    config.headingSource=NavigationHeadingSource::Manual;config.manualHeading=270;frame=BuildNavigationFrame(config,{},player,targets,{},640);
    Check(frame.markers.size()==1&&frame.markers[0].id=="west","manual heading without camera");
    config.headingSource=NavigationHeadingSource::Camera;config.categoryFilter="town";frame=BuildNavigationFrame(config,camera,player,targets,{},640);Check(frame.markers.empty(),"category filtering");
    config.categoryFilter="";config.maxDistance=10;frame=BuildNavigationFrame(config,camera,player,targets,{},640);Check(frame.markers.empty(),"range filtering");
    config.maxDistance=0;config.markerGap=64;targets.push_back({"overlap","Overlap","quest","",{0,0,21},0,true});
    frame=BuildNavigationFrame(config,camera,player,targets,{},640);Check(frame.markers.size()==1&&frame.markers[0].id=="north","priority decluttering");
    targets={{"bound","Entity","quest","entity",{1,0,1},0,true}};
    Check(BuildNavigationFrame(config,camera,player,targets,{},640).markers.empty(),"missing entity never uses stale fallback");
    Check(BuildNavigationFrame(config,camera,player,targets,{{"entity",{0,0,30}}},640).markers.size()==1,"resolved entity target");
    config.targets={{"authored","Landmark","town","",{0,0,40},0,true}};w.navigation=config;d.widgets={w};
    Check(WriteScreenDocument(file.generic_u8string(),d,error),"save navigation: "+error);ScreenDocument reopened;
    Check(ReadScreenDocument(file.generic_u8string(),d.envelope.projectId,reopened,error)&&reopened.widgets[0].navigation&&reopened.widgets[0].navigation->targets[0].label=="Landmark","read navigation settings/targets: "+error);
    auto invalid=d;invalid.widgets[0].navigation->fieldOfView=std::numeric_limits<float>::quiet_NaN();Check(!ValidateScreenDocument(invalid,d.envelope.projectId,error),"reject NaN");
    invalid=d;invalid.widgets[0].style.text.fontResource="../unsafe.ttf";Check(!ValidateScreenDocument(invalid,d.envelope.projectId,error),"reject unsafe navigation font");
    invalid=d;invalid.widgets[0].navigation->pointerImage="../unsafe.png";Check(!ValidateScreenDocument(invalid,d.envelope.projectId,error),"reject unsafe art");
    invalid=d;invalid.widgets[0].navigation->targets.push_back(config.targets[0]);Check(!ValidateScreenDocument(invalid,d.envelope.projectId,error),"reject duplicate target identity");
    ScreenAuthoringSession session;Check(session.Open(file.generic_u8string(),d.envelope.projectId,error),"open authoring");
    auto changed=config;changed.fieldOfView=180;Check(session.ConfigureNavigation(w.id,changed,error),"configure component");
    Check(session.Undo(error)&&session.FindWidget(w.id)->navigation->fieldOfView==120&&session.Redo(error)&&session.FindWidget(w.id)->navigation->fieldOfView==180,"atomic settings Undo/Redo");
    auto appearance=MakeWorldNavigationWidget();appearance.style.normal.foreground={1,2,3,255};
    const auto before=*session.FindWidget(w.id);
    Check(session.ApplyNavigationAppearance(w.id,appearance,error)&&session.FindWidget(w.id)->rect.x==before.rect.x&&session.FindWidget(w.id)->navigation->targets[0].id==before.navigation->targets[0].id&&session.FindWidget(w.id)->navigation->fieldOfView==180,"appearance retains geometry/targets/behaviour");
    Check(session.Save(error),"save authoring");
    const auto image=temp/"Content/UI/test.png";fs::copy_file(fs::path(__FILE__).parent_path()/"assets/ui/liquid/alpha-mask.png",image);
    auto withArt=*session.FindWidget(w.id);withArt.resourcePath="Content/UI/test.png";withArt.navigation->pointerImage=withArt.resourcePath;withArt.navigation->icons["town"]=withArt.resourcePath;
    Check(session.ConfigureNavigationComponent(w.id,withArt,error),"set owned appearance");const auto package=temp/"Package";
    Check(PublishScreenWidgetPrefab(temp.generic_u8string(),session,w.id,package.generic_u8string(),error),"portable nav publication: "+error);
    const auto target=temp/"Target";fs::create_directories(target/"Content/UI");ScreenDocument targetDoc;targetDoc.usage=ScreenUsage::Hud;
    targetDoc.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Hud.renegade-screen","nav-adopt");
    Check(WriteScreenDocument((target/targetDoc.envelope.pathHint).generic_u8string(),targetDoc,error),"target document");
    ScreenAuthoringSession receiving;Check(receiving.Open((target/targetDoc.envelope.pathHint).generic_u8string(),targetDoc.envelope.projectId,error),"target session");
    std::vector<StableId> created;Check(InsertScreenWidgetPrefab(target.generic_u8string(),receiving,package.generic_u8string(),50,50,created,error),"portable nav insertion: "+error);
    fs::remove(image);Check(!created.empty()&&receiving.FindWidget(created[0])->navigation->icons.count("town"),"destination owns separate navigation art");
    if(!created.empty()){std::string path;Check(ResolveScreenResourcePath(target.generic_u8string(),receiving.FindWidget(created[0])->navigation->pointerImage,path,error)&&fs::exists(fs::u8path(path)),"pointer survives source removal");}
    WorldNavigationService service;StableId first,second;Check(service.Create(w,first,error)&&service.Create(w,second,error),"C++ independent creation");
    if(!service.View(first)||!service.View(second)){std::cerr<<error;return 1;}
    auto modified=service.View(first)->widget;modified.navigation->manualHeading=45;
    Check(service.Configure(first,modified,error)&&service.View(second)->widget.navigation->manualHeading==w.navigation->manualHeading&&service.View(first)->widget.navigation->manualHeading==45,"C++ per-view configuration");
    Check(service.PutTarget({"poi","POI","quest","",{0,0,20},0,true},error)&&service.Snapshot().targets.size()==1,"C++ marker registration");
    auto grouped=d;auto group=MakeWorldNavigationWidget("Group");group.navigation.reset();grouped.widgets.insert(grouped.widgets.begin(),group);grouped.widgets[1].parentId=group.id;
    Check(service.ObserveDocument(grouped,error),"register grouped navigation");auto nested=service.View(w.id)->widget;nested.navigation->fieldOfView=150;
    Check(service.Configure(w.id,nested,error)&&service.View(w.id)->widget.parentId==group.id,"script configuration preserves authored group hierarchy");
    Check(service.ObserveDocument(d,error)&&service.View(w.id),"register authored view");
    service.RetainAuthoredViews({});Check(!service.View(w.id)&&service.View(first),"retire unloaded authored view without touching scripted views");
    const auto revision=service.Snapshot().revision;service.Reset();Check(service.Snapshot().revision>revision&&!service.View(first)&&service.Snapshot().targets.empty(),"monotonic scene-reset invalidation");
    // Governed Lua creates/configures the same service, including entity-tracked targets.
    const auto ownerId=GenerateStableId();wi::scene::Scene scene;const auto owner=scene.Entity_CreateTransform("Moving landmark");
    Check(AssignPersistentEntityId(scene,owner,ownerId,error),"owner identity");
    fs::create_directories(temp/"Content/Scripts");const auto luaPath=temp/"Content/Scripts/navigation.lua";
    fs::copy_file(fs::path(__FILE__).parent_path()/"assets/ui/liquid/alpha-mask.png",temp/"Content/UI/lua.png");
    std::ofstream(luaPath)<<R"LUA(
local n=renegade.navigation
local a,b
return {
 on_start=function(self)
  assert(n.contract_version==1)
  a=assert(n.create{name="Lua bar",source="manual",heading=359,span=180,x=20,y=30,width=600,height=100})
  b=assert(n.create{name="Other bar"})
  assert(n.get(a).heading==359 and n.get(b).heading==0)
  assert(n.configure(a,{heading=1,marker_size=28,show_names=false,categories="quest,town"}))
  assert(n.get(a).heading==1 and n.get(a).marker_size==28 and not n.get(a).show_names)
  assert(n.configure(a,{text_colour="ffaa00ff",backing_colour="10203080",font_size=22}))
  assert(n.configure(a,{background="Content/UI/lua.png",backing_colour="aabbccdd"}))
  assert(n.get(a).image_tint=="aabbccdd")
  assert(n.configure(a,{background="",backing_colour="10203080"}))
  assert(n.get(a).background_colour=="10203080")
  local settings=n.get(a)
  assert(settings.text_colour=="ffaa00ff" and settings.background_colour=="10203080" and settings.font_size==22)
  settings.span=150
  assert(n.configure(a,settings) and n.get(a).span==150)

  assert(not n.configure(a,{heading=45,span=0}) and n.get(a).heading==1)
  assert(not n.configure(a,{span=0/0}))
  assert(not n.configure(a,{show_ticks=1}))
  assert(not n.configure(a,{typo=10}))
  assert(not n.configure(a,{targets={[2]={id="sparse",position={x=0,y=0,z=4}}}}))
  assert(not n.configure(a,{targets={unexpected=true}}))

  assert(not n.configure(a,{background="../../outside.png"}))
  assert(n.put_target{id="moving",label="Landmark",category="quest",entity=self.entity})
  assert(not n.put_target{id="invalid",position={x=0/0,y=0,z=1}})
  assert(n.remove(b) and n.get(b)==nil)
  assert(#n.views()==1)
 end,
 on_update=function(self,dt) assert(n.get(a)~=nil) end
}
)LUA";
    auto scripts=CreateScriptDocument(d.envelope.projectId,GenerateStableId(),"Content/Scenes/Proof.wiscene","navigation-lua");
    ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/navigation.lua";binding.apiVersion=1;
    scripts.attachments.push_back(CreateScriptAttachment(ScriptScope::Entity,ownerId,binding));
    RuntimeScriptRuntime runtime;Check(runtime.StartScene(scripts,scene,temp.generic_u8string(),error),"governed Lua start: "+error);
    for(const auto& diagnostic:runtime.Diagnostics())std::cerr<<diagnostic.message<<'\n';
    Check(runtime.DisabledInstanceCount()==0&&runtime.Navigation().Snapshot().views.size()==1,"Lua creation/configuration parity");
    scene.transforms.GetComponent(owner)->Translate(XMFLOAT3(0,0,30));scene.transforms.GetComponent(owner)->UpdateTransform();runtime.ResolveNavigationEntities();
    Check(runtime.Navigation().Snapshot().entities.count(ownerId)&&runtime.Navigation().Snapshot().entities.at(ownerId).z==30,"Lua EntityRef follows actual transform");
    runtime.Update(.01f);Check(runtime.DisabledInstanceCount()==0,"Lua update lifecycle");
    scene.Entity_Remove(owner);runtime.ResolveNavigationEntities();Check(!runtime.Navigation().Snapshot().entities.count(ownerId),"deleted entity no stale target");
    runtime.StopScene();Check(runtime.Navigation().Snapshot().views.empty()&&runtime.Navigation().Snapshot().targets.empty(),"scene stop clears views/targets");
    std::cout<<"WorldNavigationTests failures="<<failures<<'\n';return failures?1:0;
}
