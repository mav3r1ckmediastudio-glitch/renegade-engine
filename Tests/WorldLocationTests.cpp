#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/WorldLocationService.h"
#include "renegade/bridge/WorldNavigationService.h"
#include "renegade/bridge/ZoneService.h"
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
void Check(bool ok,const char* why){if(!ok){++failures;std::cerr<<"FAIL: "<<why<<'\n';}}
int main()
{
    const auto temp=fs::temp_directory_path()/("RenegadeLocations-"+GenerateStableId());fs::create_directories(temp/"Content/UI");
    struct Cleanup {fs::path root;~Cleanup(){std::error_code error;fs::remove_all(root,error);}} cleanup{temp};
    std::string error;
    LocationProfile profile;
    Check(ValidateLocationProfile(profile,error),"default profile");
    auto bad=profile;bad.announcement.range=std::numeric_limits<float>::quiet_NaN();
    Check(!ValidateLocationProfile(bad,error),"reject NaN range");
    bad=profile;bad.icon="../external.png";Check(!ValidateLocationProfile(bad,error),"reject escaping icon");
    bad=profile;bad.announcement.duration=0;Check(!ValidateLocationProfile(bad,error),"reject zero lifetime");
    Check(int(zones::Behaviour::Location)==13&&int(zones::Behaviour::Environment)==12&&int(zones::Behaviour::Win)==1,"legacy zone codes unchanged");
    wi::scene::Scene scene;
    zones::Create create(scene,{0,0,0},zones::Behaviour::Location);Check(create.Execute(),"native Location Zone creation");
    const auto identity=create.PersistentId();zones::Definition definition;
    Check(zones::Read(scene,create.Id(),definition)&&definition.behaviour==zones::Behaviour::Location,"native Location preset");
    definition.location.type="custom.cave";definition.location.icon="Content/UI/icon.png";
    definition.location.announcement.range=75;definition.location.announcement.duration=6;
    definition.location.announcement.effect=LocationTextEffect::Glitch;
    zones::Edit edit(scene,create.Id(),definition);Check(edit.Execute(),"atomic location settings");
    edit.Undo();zones::Definition restored;
    Check(zones::Read(scene,create.Id(),restored)&&restored.location.announcement.range==40,"settings Undo");
    Check(edit.Execute(),"settings Redo");
    wi::Archive archive;scene.Serialize(archive);archive.SetReadModeAndResetPos(true);
    wi::scene::Scene reopened;reopened.Serialize(archive);
    Check(zones::Read(reopened,reopened.metadatas.GetEntity(0),restored)&&
        restored.location.type=="custom.cave"&&restored.location.icon=="Content/UI/icon.png"&&
        restored.location.announcement.range==75&&restored.location.announcement.duration==6&&
        restored.location.announcement.effect==LocationTextEffect::Glitch&&
        PersistentEntityId(reopened,reopened.metadatas.GetEntity(0))==identity,"WISCENE roundtrip settings and identity");
    WisceneDependencyDocument dependencies;InspectWisceneDependencies(reopened,dependencies);
    Check(std::any_of(dependencies.references.begin(),dependencies.references.end(),[](const auto& ref){return ref.declaredPath=="Content/UI/icon.png"&&ref.dependencyClass==DependencyClass::Texture;}),"location icon is a typed scene dependency");
    WorldLocationService locations;
    WorldLocation a{"town","Ravenwood",{},profile};a.profile.announcement.range=50;
    Check(locations.Put(a,error),"register location");
    locations.Tick(0,NavigationPoint{100,0,0});Check(!locations.Announcement(),"outside range");
    locations.Tick(0,NavigationPoint{49,0,0});Check(locations.Announcement()&&locations.Announcement()->text=="Ravenwood","entry starts announcement");
    auto frame=*locations.Announcement();
    Check(SampleLocationText(frame).visible&&SampleLocationText(frame).opacity==0,"fade starts transparent");
    locations.Tick(.5f,NavigationPoint{49,0,0});frame=*locations.Announcement();
    Check(SampleLocationText(frame).opacity==1,"fade reaches full opacity");
    locations.Tick(3.5f,NavigationPoint{49,0,0});Check(!locations.Announcement(),"expires at total duration");
    locations.Tick(1,NavigationPoint{51,0,0});locations.Tick(1,NavigationPoint{49,0,0});
    Check(!locations.Announcement(),"boundary jitter does not retrigger");
    locations.Tick(1,NavigationPoint{100,0,0});locations.Tick(1,NavigationPoint{0,0,0});
    Check(!locations.Announcement(),"once per Level default");
    a.profile.announcement.repeatOnReturn=true;Check(locations.Put(a,error),"repeat setting");
    locations.Tick(0,NavigationPoint{100,0,0});locations.Tick(0,NavigationPoint{0,0,0});
    Check(locations.Announcement().has_value(),"re-entry triggers when enabled");
    Check(locations.Discover("town",error)&&locations.Get("town")->discovered,"explicit zone-entry discovery");
    frame=*locations.Announcement();frame.elapsed=1.125;frame.settings.strength=8;frame.settings.speed=1;
    frame.settings.effect=LocationTextEffect::Wobble;auto sample=SampleLocationText(frame);
    Check(sample.visible&&std::abs(sample.y)>5&&sample.x==0,"wobble moves text");
    sample=SampleLocationText(frame,true);Check(sample.y==0&&sample.x==0&&sample.opacity==1,"reduced motion");
    frame.settings.effect=LocationTextEffect::Glitch;frame.elapsed=0.01;
    sample=SampleLocationText(frame);Check(sample.x!=0&&sample.opacity<1,"glitch flicker and displacement");
    frame.settings.effect=LocationTextEffect::None;sample=SampleLocationText(frame);
    Check(sample.opacity==1&&sample.x==0&&sample.y==0,"none removes all effects");
    frame.settings.effect=LocationTextEffect::Fade;frame.elapsed=3.9;frame.settings.duration=4;
    Check(SampleLocationText(frame).opacity>0&&SampleLocationText(frame).opacity<1,"fade out within total time");
    frame.elapsed=4;Check(!SampleLocationText(frame).visible,"effect expires");
    locations.Reset();Check(!locations.Announcement()&&locations.All().empty(),"scene reset");
    WorldLocation b{"cave","Old Mine",{5,0,0},profile};
    Check(locations.Put(a,error)&&locations.Put(b,error),"overlap locations");
    locations.Tick(0,NavigationPoint{0,0,0});Check(locations.Announcement()->id=="town","nearest announcement first");
    locations.Tick(4,NavigationPoint{0,0,0});Check(locations.Announcement()->id=="cave","queue serializes overlap");
    Check(locations.Remove("cave",error)&&!locations.Announcement(),"deletion retires active text");
    locations.Reset();
    Check(locations.SyncNative(a,error),"native sync");
    auto configured=a;configured.profile.announcement.duration=8;Check(locations.Put(configured,error),"script override native duration");
    a.position.x=10;Check(locations.SyncNative(a,error)&&locations.Get("town")->location.profile.announcement.duration==8&&locations.Get("town")->location.position.x==10,"transform update preserves script override");
    a.profile.announcement.range=60;Check(locations.SyncNative(a,error)&&locations.Get("town")->location.profile.announcement.range==60,"authored content change updates native settings");

    auto widget=MakeLocationAnnouncementWidget();ScreenDocument doc;doc.usage=ScreenUsage::Hud;
    doc.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/HUD.renegade-screen","location-tests");doc.widgets={widget};
    const auto file=temp/doc.envelope.pathHint;Check(WriteScreenDocument(file.generic_u8string(),doc,error),"write editable Text role");
    ScreenDocument loaded;Check(ReadScreenDocument(file.generic_u8string(),doc.envelope.projectId,loaded,error)&&loaded.widgets[0].locationAnnouncement,"Text role roundtrip");
    auto invalid=doc;invalid.widgets[0].kind=ScreenWidgetKind::Image;Check(!ValidateScreenDocument(invalid,doc.envelope.projectId,error),"reject announcement role on Image");
    ScreenAuthoringSession session;Check(session.Open(file.generic_u8string(),doc.envelope.projectId,error),"open announcement template");
    Check(PublishScreenWidgetPrefab(temp.generic_u8string(),session,widget.id,(temp/"Package").generic_u8string(),error),"portable announcement role");

    WorldNavigationService navigation;NavigationTarget target{"cave","Cave","dungeon","",{0,0,20},0,true};target.icon="Content/UI/one.png";
    Check(navigation.PutTarget(target,error),"location icon registration");
    const auto revision=navigation.Snapshot().revision;target.position.z=30;
    Check(navigation.PutTarget(target,error)&&navigation.Snapshot().revision==revision,"position updates reuse resource revision");
    target.icon="Content/UI/two.png";Check(navigation.PutTarget(target,error)&&navigation.Snapshot().revision>revision,"icon content invalidates artwork");
    NavigationTarget second{"other","Other","dungeon","",{5,0,20},0,true};second.icon="Content/UI/one.png";Check(navigation.PutTarget(second,error),"same-type independent icon");
    auto config=ScreenNavigation{};auto projected=BuildNavigationFrame(config,{true,{},0},{},{target,second},{},1000);
    Check(projected.markers.size()==2&&projected.markers[0].icon!=projected.markers[1].icon,"per-location icons independent of type");
    Check(navigation.RemoveTarget("cave",error)&&navigation.Target("other")&&navigation.Target("other")->icon=="Content/UI/one.png","indexed target removal preserves others");

    const auto owner=scene.Entity_CreateTransform("Script owner");const auto ownerId=GenerateStableId();
    Check(AssignPersistentEntityId(scene,owner,ownerId,error),"script owner identity");
    fs::create_directories(temp/"Content/Scripts");
    std::ofstream(temp/"Content/Scripts/locations.lua")<<"local native = \"" << identity << "\"\n" <<R"LUA(
local l=renegade.locations
local id
return {
 on_start=function(self)
  assert(l.contract_version==1)
  assert(l.get(native).name=="New location")
  assert(l.configure(native,{duration=9}))
  id=assert(l.register{name="Old Mine",type="custom.dungeon",position={x=1,y=0,z=3},range=80,duration=5,animation="wobble"})
  assert(l.get(id).name=="Old Mine" and l.get(id).animation=="wobble")
  assert(l.configure(id,{duration=6,fade_in=.2,fade_out=.4,strength=3,speed=2,repeat_on_return=true}))
  assert(l.get(id).duration==6)
  local settings=l.get(id)
  assert(l.configure(id,settings))
  assert(not l.configure(id,{duration=0}) and l.get(id).duration==6)
  assert(not l.configure(id,{range=0/0}))
  assert(not l.configure(id,{icon="../../unsafe.png"}))
  assert(not l.configure(id,{unknown=true}))
  assert(l.discover(id) and l.state(id).discovered)
  assert(l.announce(id))
 end,
 on_update=function(self,dt) assert(l.get(id)~=nil) end
}
)LUA";
    auto scripts=CreateScriptDocument(doc.envelope.projectId,GenerateStableId(),"Content/Scenes/Proof.wiscene","locations");
    ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/locations.lua";binding.apiVersion=1;
    scripts.attachments.push_back(CreateScriptAttachment(ScriptScope::Entity,ownerId,binding));
    RuntimeScriptRuntime lua;Check(lua.StartScene(scripts,scene,temp.generic_u8string(),error),"start governed locations script");
    for(const auto& diagnostic:lua.Diagnostics())std::cerr<<diagnostic.message<<'\n';
    Check(lua.DisabledInstanceCount()==0&&lua.Locations().All().size()==2&&lua.Locations().Get(identity)->location.profile.announcement.duration==9&&lua.Locations().Announcement().has_value(),"Lua locations parity");
    lua.Update(.01f);Check(lua.DisabledInstanceCount()==0,"governed lifecycle");lua.StopScene();
    Check(lua.Locations().All().empty()&&!lua.Locations().Announcement(),"script scene reset clears queued location state");
    std::cout<<"WorldLocationTests failures="<<failures<<'\n';return failures?1:0;
}
