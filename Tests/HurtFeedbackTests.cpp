#include "renegade/bridge/HurtFeedbackService.h"
#include "renegade/bridge/ScreenPrefabService.h"
#include "RuntimeCombatService.h"
#include "RuntimeScriptRuntime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace renegade::bridge;
using namespace renegade::runtime;
namespace fs=std::filesystem;
#include "HurtReviewFixture.h"
int main(int argc,char** argv)
{
    if(argc==4&&std::string(argv[1])=="--create-review")return MakeHurtReview(argv[2],argv[3]);
    int failures=0;std::string error;const auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL "<<text<<": "<<error<<'\n';}};
    auto w=MakePlayerHurtWidget();auto config=*w.hurt;
    check(config.hitFlash&&config.directional&&!config.cameraShake&&!config.heartbeatEnabled,"gentle defaults and optional motion/audio");
    wi::scene::Scene scene;RuntimeCombatState combat;bool died=false;
    combat.hurt.Tick(0,true,true,100,100,0);
    check(SampleHurtFeedback(config,combat.hurt.Frame()).flash==0,"first gameplay sample is not a hit");
    check(!ApplyCombatDamage(scene,combat,RuntimePlayerKnowledgeId,-5,died)&&combat.hurt.Frame().sequence==0,"rejected damage has no feedback");
    check(ApplyCombatDamage(scene,combat,RuntimePlayerKnowledgeId,25,died,90.f)&&combat.playerHealth==75,"accepted native damage");
    combat.hurt.Tick(0,true,true,combat.playerHealth,100,0);auto sample=SampleHurtFeedback(config,combat.hurt.Frame());
    check(sample.flash>0&&sample.edge>0&&sample.directions.size()==1&&std::abs(sample.directions[0].first-1.5707963f)<.001f,"actual damage and right-side indicator");
    combat.hurt.Tick(0,true,true,75,100,90);sample=SampleHurtFeedback(config,combat.hurt.Frame());
    check(std::abs(sample.directions[0].first)<.001f,"indicator follows current camera heading");
    config.cameraShake=true;sample=SampleHurtFeedback(config,combat.hurt.Frame(),true);
    check(sample.flash==0&&sample.shake==0&&sample.edge>0&&!sample.directions.empty(),"reduced motion retains non-flashing feedback");
    const auto sequence=combat.hurt.Frame().sequence;combat.playerHealth=100;
    combat.hurt.Tick(0,true,true,100,100,0);check(combat.hurt.Frame().sequence==sequence,"healing is not a new hit");
    combat.hurt.Reset();combat.hurt.Tick(0,true,true,100,100,0);
    check(ApplyCombatDamage(scene,combat,RuntimePlayerKnowledgeId,5,died),"zone/unattributed native damage");
    combat.hurt.Tick(0,true,true,95,100,0);sample=SampleHurtFeedback(config,combat.hurt.Frame());
    check(sample.edge>0&&sample.directions.empty(),"zone damage never invents an attacker direction");
    for(int i=0;i<30;++i)combat.hurt.Tick(.25f,true,true,95,100,0);
    check(SampleHurtFeedback(config,combat.hurt.Frame()).edge==0&&combat.hurt.Frame().hits.empty(),"transient hit expiry is bounded");
    combat.hurt.Tick(0,true,true,10,100,0);sample=SampleHurtFeedback(config,combat.hurt.Frame());
    check(sample.low>0,"low health overlay");auto steady=SampleHurtFeedback(config,combat.hurt.Frame(),true);
    combat.hurt.Tick(.25f,true,true,10,100,0);check(std::abs(steady.low-SampleHurtFeedback(config,combat.hurt.Frame(),true).low)<.0001f,"reduced motion disables pulsing");
    combat.hurt.Tick(0,true,true,100,100,0);check(SampleHurtFeedback(config,combat.hurt.Frame()).low==0,"healing removes low health effect");
    combat.hurt.Damage(25,100,0.f);const auto age=combat.hurt.Frame().hits[0].age;combat.hurt.Tick(0,true,true,100,100,0);
    check(combat.hurt.Frame().hits[0].age==age,"paused gameplay freezes decay");
    combat.hurt.Tick(0,true,false,0,100,0);check(combat.hurt.Frame().hits.empty()&&SampleHurtFeedback(config,combat.hurt.Frame()).low==0,"death clears presentation");
    combat.hurt.Reset();combat.hurt.Tick(0,true,true,50,100,0);check(combat.hurt.Frame().sequence==0&&SampleHurtFeedback(config,combat.hurt.Frame()).flash==0,"load/respawn baseline produces no hit");
    for(int i=0;i<100;++i)combat.hurt.Damage(1,100);check(combat.hurt.Frame().hits.size()==8,"bounded simultaneous hits");
    auto bad=config;bad.fade=std::numeric_limits<float>::quiet_NaN();check(!ValidateHurtFeedback(bad,error),"reject NaN");
    bad=config;bad.pulseHz=10;check(!ValidateHurtFeedback(bad,error),"bound pulse rate");bad=config;bad.edgeImage="Content/../external.png";check(!ValidateHurtFeedback(bad,error),"reject traversal");
    const auto root=fs::temp_directory_path()/("RenegadeHurt-"+GenerateStableId());fs::create_directories(root/"Content/UI");
    struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{root};
    const auto source=fs::path(__FILE__).parent_path().parent_path();fs::copy_file(source/"Tests/assets/ui/liquid/alpha-mask.png",root/"Content/UI/hurt.png");
    fs::copy_file(source/"Runtime/Content/Audio/Impacts/Original/bullet_hitting_wood.wav",root/"Content/UI/hurt.wav");
    w.hurt->edgeImage="Content/UI/hurt.png";w.hurt->painSound="Content/UI/hurt.wav";w.hurt->heartbeatSound="Content/UI/hurt.wav";
    ScreenDocument doc;doc.schemaVersion=3;doc.usage=ScreenUsage::Hud;doc.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,"Content/UI/Hud.renegade-screen","hurt-test");doc.widgets={w};
    const auto file=(root/doc.envelope.pathHint).generic_u8string();check(WriteScreenDocument(file,doc,error),"save hurt component");
    ScreenAuthoringSession session;check(session.Open(file,doc.envelope.projectId,error),"open hurt component");
    auto changed=*session.FindWidget(w.id)->hurt;changed.lowThreshold=.35f;changed.cameraShake=true;
    check(session.ConfigureHurtFeedback(w.id,changed,error)&&session.Undo(error)&&!session.FindWidget(w.id)->hurt->cameraShake&&session.Redo(error)&&session.FindWidget(w.id)->hurt->cameraShake,"atomic settings history");
    check(session.Save(error)&&session.Reload(error)&&session.FindWidget(w.id)->hurt->edgeImage=="Content/UI/hurt.png","save/reopen artwork and settings");
    const auto package=root/"Package";check(PublishScreenWidgetPrefab(root.generic_u8string(),session,w.id,package.generic_u8string(),error),"publish portable hurt prefab");
    const auto target=root/"Target";fs::create_directories(target);ScreenPortableAdoptionResult adopted;
    check(AdoptPortableScreenPackage(target.generic_u8string(),GenerateStableId(),package.generic_u8string(),adopted,error),"adopt custom hurt prefab");
    DocumentEnvelope envelope;ScreenDocument copy;
    check(ReadDocumentEnvelope((target/adopted.pathHint).generic_u8string(),envelope,error)&&ReadScreenDocument((target/adopted.pathHint).generic_u8string(),envelope.projectId,copy,error)&&copy.widgets[0].hurt&&fs::is_regular_file(target/copy.widgets[0].hurt->edgeImage),"portable artwork remap");
    if(!copy.widgets.empty()&&copy.widgets[0].hurt)for(const auto& f:HurtResources()){
        const auto& path=(*copy.widgets[0].hurt).*(f.member);if(!path.empty())check(fs::is_regular_file(target/path),"portable image/audio resource retained");
    }
    ScreenDocument legacy;legacy.schemaVersion=2;legacy.actions.push_back({"exit"});
    ScreenWidget oldButton;oldButton.id=GenerateStableId();oldButton.name="Exit";oldButton.kind=ScreenWidgetKind::Button;oldButton.text="Exit";oldButton.actionId="exit";oldButton.rect={10,10,160,48};oldButton.style=MakeScreenWidgetStyleTemplate(oldButton.kind,48);legacy.widgets={oldButton};legacy.focusOrder={oldButton.id};
    legacy.envelope=CreateDocumentEnvelope(doc.envelope.projectId,RuntimeScreenDocumentType,"Content/UI/Legacy.renegade-screen","legacy-hurt");
    const auto legacyPath=(root/legacy.envelope.pathHint).generic_u8string();
    check(WriteScreenDocument(legacyPath,legacy,error),"write legacy screen");
    ScreenAuthoringSession oldSession;std::string inserted;
    check(oldSession.Open(legacyPath,doc.envelope.projectId,error)&&oldSession.CreateWidget(MakePlayerHurtWidget(),false,inserted,error)&&oldSession.Document().schemaVersion==3&&oldSession.Undo(error)&&oldSession.Document().widgets.size()==1,"legacy insertion upgrades atomically");
        HurtFeedbackService service;service.Observe(doc);auto count=service.Snapshot().views.size();std::string id;auto dynamic=MakePlayerHurtWidget();
    check(service.Create(dynamic,id,error)&&service.Remove(id,error)&&service.Snapshot().views.size()==count,"dynamic view lifecycle");
    fs::create_directories(root/"Content/Scripts");std::ofstream(root/"Content/Scripts/hurt.lua")<<R"LUA(
return {on_start=function(self)
 local hurt=renegade.hurt
 local id=assert(hurt.create{edge_image="Content/UI/hurt.png",flash_opacity=.15,camera_shake=false,width=960,height=540})
 assert(hurt.configure(id,{low_threshold=.3,colour="ff0020aa",x=20,opacity=.8}))
 assert(math.abs(hurt.get(id).low_threshold-.3)<.00001 and hurt.get(id).colour=="ff0020aa")
 assert(hurt.get(id).x==20 and hurt.get(id).width==960)
 assert(not hurt.configure(id,{width=-1}))
 assert(not hurt.configure(id,{fade=0}) and hurt.get(id).fade>0)
 assert(not hurt.configure(id,{edge_image="../escape.png"}))
 assert(hurt.hit(12,90) and hurt.clear())
 assert(type(hurt.state().available)=="boolean" and #hurt.views()==1)
 assert(hurt.remove(id))
end}
)LUA";
    auto scripts=CreateScriptDocument(doc.envelope.projectId,GenerateStableId(),"Content/Scenes/Proof.wiscene","hurt-api");
    ScriptSourceBinding binding;binding.sourceId=GenerateStableId();binding.sourcePath="Content/Scripts/hurt.lua";binding.apiVersion=1;
    auto attachment=CreateScriptAttachment(ScriptScope::Level,{},binding);attachment.presentation=ScriptPresentation::GlobalScript;scripts.attachments={attachment};
    RuntimeScriptRuntime lua;int pulses=0,clears=0;lua.SetHurtFeedbackApi([&](float n,std::optional<float> bearing,std::string&){if(n==12&&bearing&&*bearing==90)++pulses;return true;},[&]{++clears;});
    check(lua.StartScene(scripts,scene,root.generic_u8string(),error)&&lua.DisabledInstanceCount()==0&&pulses==1&&clears==1,"actual governed Lua parity and presentation-only host");
    for(const auto& message:lua.Diagnostics())std::cerr<<message.message<<'\n';lua.StopScene();
    std::cout<<"HurtFeedbackTests failures="<<failures<<'\n';return failures?1:0;
}
