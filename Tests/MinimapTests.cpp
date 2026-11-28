#include "renegade/bridge/WorldNavigationService.h"
#include "renegade/bridge/MinimapCameraService.h"
#include "renegade/bridge/ScreenPrefabService.h"
#include "renegade/bridge/GameplayInputService.h"
#include "RuntimeScriptRuntime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace renegade::bridge;
using namespace renegade::runtime;
namespace fs=std::filesystem;
#include "MinimapReviewFixture.h"
int main(int argc,char** argv)
{
    if(argc==4&&std::string(argv[1])=="--create-review")return MakeMinimapReview(argv[2],argv[3]);
    int failures=0;std::string error;
    const auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL "<<text<<": "<<error<<'\n';}};
    const auto root=fs::temp_directory_path()/("RenegadeMinimap-"+GenerateStableId());fs::create_directories(root/"Content/UI");
    struct Cleanup{fs::path root;~Cleanup(){std::error_code e;fs::remove_all(root,e);}} cleanup{root};
    const auto source=fs::path(__FILE__).parent_path().parent_path();
    for(const char* name:{"map.png","world.png","player.png","enemy.png"})fs::copy_file(source/"Tests/assets/ui/liquid/alpha-mask.png",root/"Content/UI"/name);
    auto w=MakeWorldMinimapWidget();auto& c=*w.navigation;auto& m=*c.minimap;
    check(m.live&&m.mapKeyEnabled&&m.resolution==256,"simple live default and M enabled");
    m.range=100;c.markerSize=12;NavigationObserver player{true,{},0};
    const std::vector<NavigationTarget> points={{"north","North","town","",{0,0,50},0,true},{"east","East","town","",{50,0,0},0,true},
        {"far","Far location","town","",{0,0,500},0,true},{"enemy","Nearby enemy","enemy","",{20,0,0},5,true},{"far-enemy","Distant enemy","enemy","",{0,0,500},5,true},
        {"giver","Quest giver","questgiver","",{-20,0,20},10,true},{"vendor","Vendor","vendor","",{20,0,-20},0,true}};
    auto f=BuildMinimapFrame(c,{},player,points,{},280,280);
    const auto find=[&](const MinimapFrame& frame,const std::string& id)->const MinimapMarker*{for(const auto& marker:frame.markers)if(marker.target.id==id)return &marker;return nullptr;};
    check(f.available&&find(f,"north")&&std::abs(find(f,"north")->y-.25f)<.001f,"north maps up");
    check(find(f,"east")&&std::abs(find(f,"east")->x-.75f)<.001f,"east maps right");
    check(find(f,"far")&&find(f,"far")->clamped&&!find(f,"far-enemy"),"location edges and nearby-only enemies");
    m.rotate=true;player.heading=90;f=BuildMinimapFrame(c,{},player,points,{},280,280);
    check(find(f,"east")&&std::abs(find(f,"east")->y-.25f)<.001f&&std::abs(find(f,"east")->x-.5f)<.001f,"rotating east-facing map");
    m.rotate=false;m.round=false;f=BuildMinimapFrame(c,{},player,points,{},560,280);
    check(find(f,"east")&&std::abs(find(f,"east")->x-.625f)<.001f,"rectangular aspect preserves world scale");
    m.showEnemies=false;m.showQuestGivers=false;m.showVendors=false;
    f=BuildMinimapFrame(c,{},player,points,{},280,280);check(!find(f,"enemy")&&!find(f,"giver")&&!find(f,"vendor"),"NPC visibility settings");
    m.showEnemies=m.showQuestGivers=m.showVendors=true;m.clampEdges=false;
    f=BuildMinimapFrame(c,{},player,points,{},280,280);check(!find(f,"far"),"unclamped out-of-range targets hide");
    auto moving=points.front();moving.entityId=GenerateStableId();
    f=BuildMinimapFrame(c,{},player,{moving},{},280,280);check(f.markers.empty(),"unresolved followed entity does not become a false location");
    f=BuildMinimapFrame(c,{},player,{moving},{{moving.entityId,{0,0,-50}}},280,280);check(find(f,"north")&&find(f,"north")->y>.5f,"live entity position wins");
    auto mapConfig=c;mapConfig.minimap->fullMap=true;mapConfig.minimap->range=1000;mapConfig.headingSource=NavigationHeadingSource::Camera;
    auto pan=player;pan.position.x=100;f=BuildMinimapFrame(mapConfig,pan,player,{points[0]},{},280,280);
    check(find(f,"north")&&std::abs(find(f,"north")->distance-50)<.001f,"panned full map measures distance from player");
    auto bad=c;bad.minimap->range=std::numeric_limits<float>::quiet_NaN();check(!ValidateScreenNavigation(bad,error),"reject invalid range");
    bad=c;bad.minimap->worldMapImage="../outside.png";check(!ValidateScreenNavigation(bad,error),"reject outside full-map artwork");
    bad=c;bad.minimap->east=bad.minimap->west;check(!ValidateScreenNavigation(bad,error),"reject zero-width artwork bounds");
    m.image="Content/UI/map.png";m.worldMapImage="Content/UI/world.png";m.playerIcon="Content/UI/player.png";c.icons["enemy"]="Content/UI/enemy.png";
    ScreenDocument doc;doc.usage=ScreenUsage::Hud;doc.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Hud.renegade-screen","minimap-test");doc.widgets={w};
    const auto path=(root/doc.envelope.pathHint).generic_u8string();check(WriteScreenDocument(path,doc,error),"serialize minimap");
    ScreenDocument read;check(ReadScreenDocument(path,doc.envelope.projectId,read,error)&&read.widgets.front().navigation->minimap->worldMapImage==m.worldMapImage,"artwork and settings roundtrip");
    ScreenAuthoringSession session;check(session.Open(path,doc.envelope.projectId,error),"open authored minimap");
    auto draft=*session.FindWidget(w.id);draft.navigation->minimap->live=false;draft.navigation->minimap->rotate=true;
    check(session.ConfigureNavigationComponent(w.id,draft,error)&&session.Undo(error)&&session.FindWidget(w.id)->navigation->minimap->live&&session.Redo(error)&&!session.FindWidget(w.id)->navigation->minimap->live,"minimap settings atomic history");
    check(session.Save(error)&&session.Reload(error)&&session.FindWidget(w.id)->navigation->icons.at("enemy")=="Content/UI/enemy.png","save/reopen custom category icons");
    const auto package=root/"Package";check(PublishScreenWidgetPrefab(root.generic_u8string(),session,w.id,package.generic_u8string(),error),"publish portable minimap");
    const auto target=root/"Target";fs::create_directories(target);ScreenPortableAdoptionResult adopted;
    check(AdoptPortableScreenPackage(target.generic_u8string(),GenerateStableId(),package.generic_u8string(),adopted,error),"adopt portable minimap");
    ScreenDocument adoptedDoc;DocumentEnvelope envelope;
    check(ReadDocumentEnvelope((target/adopted.pathHint).generic_u8string(),envelope,error)&&ReadScreenDocument((target/adopted.pathHint).generic_u8string(),envelope.projectId,adoptedDoc,error),"read adopted map");
    if(!adoptedDoc.widgets.empty()){
        auto n=*adoptedDoc.widgets.front().navigation;for(const auto* p:NavigationResources(n))if(!p->empty())check(fs::is_regular_file(target/fs::u8path(*p)),"map/player/category artwork dependency retained");
    }
    wi::scene::Scene scene;const auto original=scene.Entity_CreateCamera("Authored camera",256,256);
    WorldNavigationSnapshot state;state.player=player;state.views.emplace(w.id,NavigationView{w,true});MinimapCameraService cameras;
    cameras.Update(scene,1,state,{w.id});check(cameras.Count()==1&&scene.cameras.GetCount()==2,"one native transient camera");
    for(size_t i=0;i<scene.cameras.GetCount();++i)if(scene.cameras.GetEntity(i)!=original){const auto& camera=scene.cameras[i];
        check(camera.IsOrtho()&&std::abs(camera.At.y+1)<.001f&&std::abs(camera.Up.z-1)<.001f,"native overhead north alignment");}
    cameras.Update(scene,1,state,{});check(cameras.Count()==0&&scene.cameras.Contains(original),"hidden map releases only its own camera");
    cameras.Update(scene,2,state,{w.id});cameras.Reset(scene);check(scene.cameras.GetCount()==1,"camera lifecycle reset");
    auto input=MakeDefaultGameplayInputMap();check(input.bindings[size_t(GameplayAction::Map)].keyboard=="M","governed M default");
    check(WriteGameplayInputMap(root.generic_u8string(),input,error),"write map input");
    const auto inputPath=fs::u8path(GameplayInputDocumentPath(root.generic_u8string()));
    std::ifstream stream(inputPath);std::string wire((std::istreambuf_iterator<char>(stream)),{});const auto at=wire.find("[action.map]");
    if(at!=std::string::npos){const auto end=wire.find("[",at+1);wire.erase(at,end==wire.npos?wire.npos:end-at);}
    std::ofstream(inputPath,std::ios::trunc)<<wire;GameplayInputMap legacy;
    check(ReadGameplayInputMap(root.generic_u8string(),legacy,error)&&legacy.bindings[size_t(GameplayAction::Map)].keyboard=="M","old input maps gain only the new default");
    fs::create_directories(root/"Content/Scripts");
    std::ofstream(root/"Content/Scripts/maps.lua")<<R"LUA(
return {on_start=function(self)
 local m=renegade.minimap
 local id=assert(m.create{range=120,live=false,map_image="Content/UI/map.png",world_map_image="Content/UI/world.png",player_icon="Content/UI/player.png",icons={enemy="Content/UI/enemy.png"}})
 local before=m.get(id);assert(before.range==120 and before.world_map_image=="Content/UI/world.png")
 assert(m.configure(id,{rotate=true,round=false,show_enemies=false}))
 assert(m.get(id).rotate and not m.get(id).round and not m.get(id).show_enemies)
 assert(not m.configure(id,{range=0}) and m.get(id).range==120)
 assert(not m.configure(id,{map_image="../../escape.png"}))
 assert(renegade.map.open() and renegade.map.is_open())
 assert(renegade.map.close() and not renegade.map.is_open())
 assert(m.remove(id))
end}
)LUA";
    auto scripts=CreateScriptDocument(doc.envelope.projectId,GenerateStableId(),"Content/Scenes/Proof.wiscene","map-api");
    ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/maps.lua";binding.apiVersion=1;
    auto attachment=CreateScriptAttachment(ScriptScope::Level,{},binding);attachment.presentation=ScriptPresentation::GlobalScript;scripts.attachments={attachment};
    bool mapOpen=false;RuntimeScriptRuntime lua;lua.SetWorldMapApi([&](bool open,std::string&){mapOpen=open;return true;},[&]{return mapOpen;});
    check(lua.StartScene(scripts,scene,root.generic_u8string(),error)&&lua.ActiveInstanceCount()==1&&lua.DisabledInstanceCount()==0&&!mapOpen,"governed minimap and full-map API parity");
    for(const auto& d:lua.Diagnostics())std::cerr<<d.message<<'\n';lua.StopScene();
    std::cout<<"MinimapTests failures="<<failures<<'\n';return failures?1:0;
}
