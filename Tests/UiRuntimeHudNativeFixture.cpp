#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenWidgetPresets.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/PlayerService.h"
#include "renegade/bridge/GameplayAttributeDocumentService.h"
#include "renegade/bridge/ScriptDocumentService.h"
#include <wiArchive.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <optional>
#include <unordered_map>
using namespace renegade::bridge;
int main(int argc,char** argv)
{
    if((argc!=2&&argc!=3)||(argc==3&&std::string(argv[2])!="--containers"&&std::string(argv[2])!="--animations"&&std::string(argv[2])!="--controls"&&std::string(argv[2])!="--attributes"&&std::string(argv[2])!="--local-only"&&std::string(argv[2])!="--local-flow"&&std::string(argv[2])!="--hud-events"&&std::string(argv[2])!="--reduced-motion")){std::cerr<<"Supply a new disposable fixture parent directory.\n";return 2;}
    const bool hudEvents=argc==3&&std::string(argv[2])=="--hud-events";
    const auto parent=std::filesystem::absolute(std::filesystem::u8path(argv[1]));
    if(parent.filename()!="ui-runtime-hud-native-qa" || std::filesystem::exists(parent))
    {std::cerr<<"Refusing to replace an existing directory or write outside the named QA fixture.\n";return 2;}
    std::filesystem::create_directories(parent);
    ProjectService projects;projects.Initialize((parent/"Editor.ini").generic_u8string());
    if(!projects.CreateStoryFlowProject(parent.generic_u8string(),"HUD Native QA", false))
    {std::cerr<<projects.LastError()<<'\n';return 1;}
    const auto project=projects.CurrentProject();const auto root=std::filesystem::u8path(project.rootPath);
    std::filesystem::create_directories(root/"Content/UI");std::filesystem::create_directories(root/"Content/Scenes");
    std::string error;
    ScreenDocument hud;hud.usage=ScreenUsage::Hud;
    hud.envelope=CreateDocumentEnvelope(project.projectId,RuntimeScreenDocumentType,"Content/UI/PlayerHud.renegade-screen","Native HUD QA fixture");
    auto text=MakeScreenWidgetPreset(ScreenWidgetPreset::PlayerHealthText,1280,720);
    text.id=GenerateStableId();text.rect={32,180,480,60};
    text.style.text.fontSize=32;
    auto progress=MakeScreenWidgetPreset(ScreenWidgetPreset::PlayerHealthBar,1280,720);
    progress.id=GenerateStableId();progress.rect={32,250,400,28};
    hud.widgets={text,progress};
    if(argc==3&&std::string(argv[2])!="--attributes")
    {
        auto panel=MakeScreenWidgetPreset(ScreenWidgetPreset::VerticalStack,1280,720);
        panel.id=GenerateStableId();panel.rect={32,180,480,180};
        panel.container.padding=16;panel.container.gap=12;
        text.parentId=panel.id;text.rect={0,0,448,60};
        progress.parentId=panel.id;progress.rect={0,70,400,28};
        if(std::string(argv[2])=="--animations"||std::string(argv[2])=="--reduced-motion")
        {
            ScreenAnimationTrack move;move.property=ScreenAnimationProperty::OffsetX;
            move.loop=ScreenAnimationLoop::PingPong;move.easing=ScreenAnimationEasing::Linear;
            move.keys={{0,0},{2,240}};panel.animations={move};
        }
        hud.widgets={panel,text,progress};
    }
    if(argc==3&&std::string(argv[2])=="--controls")
    {
        auto policy=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Hud);
        policy.uiInput=ScreenUiInputPolicy::Modal;policy.cursor=ScreenCursorPolicy::Show;
        policy.blockGameplayInput=true;policy.pauseGameplay=true;policy.takeKeyboardFocus=true;
        hud.composition=policy;
        int row=0;
        for(const auto preset:{ScreenWidgetPreset::Toggle,ScreenWidgetPreset::Slider,
            ScreenWidgetPreset::Selector,ScreenWidgetPreset::TextInput})
        {
            auto control=MakeScreenWidgetPreset(preset,1280,720);control.id=GenerateStableId();
            control.rect={600,160+float(row++)*90,420,64};
            hud.focusOrder.push_back(control.id);hud.widgets.push_back(std::move(control));
        }
    }
    if(argc==3&&(std::string(argv[2])=="--local-only"||std::string(argv[2])=="--local-flow"))
    {
        // A real interactive modal HUD Button first hides a bound health Text.
        // Local Only must not forward its otherwise destructive 'quit' action;
        // Local + Flow is the positive control that must quit Runtime.
        const bool localOnly=std::string(argv[2])=="--local-only";
        auto policy=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Hud);
        policy.uiInput=ScreenUiInputPolicy::Modal;
        policy.cursor=ScreenCursorPolicy::Show;
        policy.blockGameplayInput=true;
        policy.pauseGameplay=true;
        policy.takeKeyboardFocus=true;
        hud.composition=policy;
        hud.actions.push_back({RuntimeScreenQuitAction});
        ScreenWidget trigger;
        trigger.id=GenerateStableId();
        trigger.kind=ScreenWidgetKind::Button;
        trigger.name="Hide health without navigation";
        trigger.text=localOnly?"LOCAL ONLY: HIDE HEALTH":"LOCAL + FLOW: QUIT";
        trigger.rect={600,165,500,84};
        trigger.visible=true;
        trigger.enabled=true;
        trigger.actionId=RuntimeScreenQuitAction;
        trigger.localOnly=localOnly;
        trigger.style=MakeScreenWidgetStyleTemplate(trigger.kind,trigger.rect.height);
        trigger.interactions.push_back(
            {ScreenInteractionEvent::Activate,ScreenLocalActionKind::SetVisible,text.id,false,{}});
        hud.focusOrder.push_back(trigger.id);
        hud.widgets.push_back(std::move(trigger));
    }
    if(argc==3&&std::string(argv[2])=="--reduced-motion")
    {
        auto policy=DefaultScreenCompositionPolicy(ScreenCompositionLayer::Hud);
        policy.uiInput=ScreenUiInputPolicy::Modal;
        policy.cursor=ScreenCursorPolicy::Show;
        policy.blockGameplayInput=true;
        policy.takeKeyboardFocus=true;
        hud.composition=policy;
        hud.actions.push_back({RuntimeUiReducedMotionToggleAction});
        ScreenWidget trigger;
        trigger.id=GenerateStableId();
        trigger.kind=ScreenWidgetKind::Button;
        trigger.name="Player reduced motion";
        trigger.text="TOGGLE REDUCED MOTION";
        trigger.rect={600,165,500,84};
        trigger.visible=true;trigger.enabled=true;
        trigger.actionId=RuntimeUiReducedMotionToggleAction;
        trigger.style=MakeScreenWidgetStyleTemplate(trigger.kind,trigger.rect.height);
        hud.focusOrder.push_back(trigger.id);
        hud.widgets.push_back(std::move(trigger));
    }
    if(argc==3&&std::string(argv[2])=="--attributes")
    {
        // Native passive HUD evidence: one shared profile, independent Project and actual
        // spawned Player instances, per-scope default/rate overrides. No invented Player.
        std::filesystem::create_directories(root/"Content/Gameplay");
        GameplayAttributeDocument attributes;
        attributes.envelope=CreateDocumentEnvelope(project.projectId,GameplayAttributeDocumentType,
            "Content/Gameplay/NativeEnergy.renegade-attributes","Native gameplay attribute QA");
        GameplayAttributeProfile profile{GenerateStableId(),"Resource Profile",
            {{"energy","Energy",10.0,0.0,100.0,3.0}}};
        GameplayAttributeOverride playerOverride;
        playerOverride.attributeId="energy";
        playerOverride.initial=GameplayAttributeValue{20.0};
        playerOverride.regeneratePerSecond=6.0;
        attributes.profiles={profile};
        attributes.attachments={
            {GameplayAttributeScope::Project,{},profile.id,{}},
            {GameplayAttributeScope::Player,{},profile.id,{playerOverride}}};
        if(!WriteGameplayAttributeDocument((root/attributes.envelope.pathHint).generic_u8string(),attributes,error) ||
            !projects.SetGameplayAttributes(attributes.envelope.documentId,attributes.envelope.pathHint))
        {std::cerr<<"Native attribute fixture: "<<error<<" "<<projects.LastError()<<'\n';return 1;}

        auto projectText=MakeScreenWidgetPreset(ScreenWidgetPreset::Counter,1280,720);
        projectText.id=GenerateStableId();projectText.name="Project Energy";
        projectText.text="PROJECT ENERGY unavailable";projectText.rect={32,320,680,54};
        projectText.style.text.fontSize=32;
        ScreenBinding projectBinding;projectBinding.property=ScreenBindingProperty::Text;
        projectBinding.source.scope=ScreenBindingScope::ProjectAttribute;
        projectBinding.source.attributeId="energy";
        projectBinding.prefix="PROJECT ENERGY: ";
        projectText.bindings={projectBinding};
        hud.widgets.push_back(std::move(projectText));

        auto playerText=MakeScreenWidgetPreset(ScreenWidgetPreset::Counter,1280,720);
        playerText.id=GenerateStableId();playerText.name="Player Energy";
        playerText.text="PLAYER ENERGY unavailable";playerText.rect={32,388,680,54};
        playerText.style.text.fontSize=32;
        ScreenBinding playerBinding;playerBinding.property=ScreenBindingProperty::Text;
        playerBinding.source.scope=ScreenBindingScope::PossessedPlayer;
        playerBinding.source.attributeId="energy";
        playerBinding.prefix="PLAYER ENERGY: ";
        playerText.bindings={playerBinding};
        hud.widgets.push_back(std::move(playerText));
    }
    if(!WriteScreenDocument((root/hud.envelope.pathHint).generic_u8string(),hud,error) ||
        !projects.SetDefaultHud(hud.envelope.documentId,hud.envelope.pathHint))
    {std::cerr<<error<<' '<<projects.LastError()<<'\n';return 1;}
    auto overrideHud=hud;
    overrideHud.envelope.documentId=GenerateStableId();
    overrideHud.envelope.pathHint="Content/UI/LevelHud.renegade-screen";
    std::unordered_map<StableId,StableId> remap;
    for(const auto& widget:overrideHud.widgets)remap.emplace(widget.id,GenerateStableId());
    for(auto& focus:overrideHud.focusOrder)focus=remap.at(focus);
    for(auto& widget:overrideHud.widgets)
    {
        widget.id=remap.at(widget.id);
        if(!widget.parentId.empty())widget.parentId=remap.at(widget.parentId);
        for(auto& action:widget.interactions)
        {
            if(const auto target=remap.find(action.targetId);target!=remap.end())
                action.targetId=target->second;
            if(action.condition)
                if(const auto source=remap.find(action.condition->widgetId);source!=remap.end())
                    action.condition->widgetId=source->second;
        }
        if(widget.kind==ScreenWidgetKind::Text&&!widget.bindings.empty())
            widget.bindings.front().prefix="LEVEL HEALTH: ";
    }
    if(!WriteScreenDocument((root/overrideHud.envelope.pathHint).generic_u8string(),overrideHud,error))
    {std::cerr<<error<<'\n';return 1;}
    std::optional<HudReference> eventHudReference;
    if(hudEvents)
    {
        auto contextHud=overrideHud;
        contextHud.envelope.documentId=GenerateStableId();
        contextHud.envelope.pathHint="Content/UI/EventHud.renegade-screen";
        std::unordered_map<StableId,StableId> ids;
        for(const auto& widget:contextHud.widgets)ids.emplace(widget.id,GenerateStableId());
        for(auto& focus:contextHud.focusOrder)focus=ids.at(focus);
        for(auto& widget:contextHud.widgets)
        {
            widget.id=ids.at(widget.id);
            if(!widget.parentId.empty())widget.parentId=ids.at(widget.parentId);
            for(auto& action:widget.interactions)
                if(const auto found=ids.find(action.targetId);found!=ids.end())
                    action.targetId=found->second;
            if(widget.kind==ScreenWidgetKind::Text&&!widget.bindings.empty())
                widget.bindings.front().prefix="EVENT HUD HEALTH: ";
        }
        if(!WriteScreenDocument((root/contextHud.envelope.pathHint).generic_u8string(),
            contextHud,error)){std::cerr<<"Context HUD: "<<error<<'\n';return 1;}
        eventHudReference=HudReference{contextHud.envelope.documentId,contextHud.envelope.pathHint};
    }
    const auto sceneId=GenerateStableId();const auto scenePath=root/"Content/Scenes/HudLevel.wiscene";
    wi::scene::Scene scene;TransformState transform;
    CreatePlayerStartCommand start(scene,transform);
    if(!start.Execute()){std::cerr<<"Could not create native Player Start.\n";return 1;}
    StableId hudEventEmitterId;
    if(hudEvents)
    {
        const auto emitter=scene.Entity_CreateTransform("Native HUD Event Emitter");
        hudEventEmitterId=GenerateStableId();
        if(!AssignPersistentEntityId(scene,emitter,hudEventEmitterId,error))
        {std::cerr<<"Could not bind native HUD event producer: "<<error<<'\n';return 1;}
    }
    auto settings=CapturePlayerControllerSettings(scene,start.CreatedEntity());settings.gravityFactor=0;
    SetPlayerControllerSettingsCommand configure(scene,start.CreatedEntity(),settings);
    if(!configure.Execute() || !EnsurePersistentEntityIdentities(scene,error))
    {std::cerr<<"Could not configure native Player Start: "<<error<<'\n';return 1;}
    {
        wi::Archive archive(scenePath.generic_u8string(),false,false);
        if(!archive.IsOpen())return 1;scene.Serialize(archive);
        if(!archive.SaveFile(scenePath.generic_u8string()))return 1;
    }
    auto envelope=CreateDocumentEnvelope(project.projectId,SceneDocumentType,"Content/Scenes/HudLevel.wiscene","Native HUD QA fixture");
    envelope.documentId=sceneId;
    if(!WriteDocumentEnvelope(scenePath.generic_u8string()+".rmeta",envelope,error)){std::cerr<<error<<'\n';return 1;}
    if(hudEvents)
    {
        std::filesystem::create_directories(root/"Content/Scripts");
        const auto scriptHint=std::string("Content/Scripts/HudContextEvents.lua");
        std::ofstream lua(root/scriptHint,std::ios::binary|std::ios::trunc);
        lua<<"return {\n"
           " on_start=function(self) self.elapsed=0 end,\n"
           " on_update=function(self,dt)\n"
           "  self.elapsed=(self.elapsed or 0)+dt\n"
           "  if self.elapsed>=2.0 and not self.entered then\n"
           "   local ok,err=renegade.events.emit('qa.vehicle.enter','native')\n"
           "   assert(ok,err)\n"
           "   self.entered=true\n"
           "  end\n"
           "  if self.elapsed>=9.0 and not self.exited then\n"
           "   local ok,err=renegade.events.emit('qa.vehicle.exit','native')\n"
           "   assert(ok,err)\n"
           "   self.exited=true\n"
           "  end\n"
           " end\n"
           "}\n";
        lua.flush();
        if(!lua){std::cerr<<"Could not write native HUD event source.\n";return 1;}
        ScriptDocument companion=CreateScriptDocument(project.projectId,sceneId,
            "Content/Scenes/HudLevel.wiscene","Native HUD event script");
        ScriptSourceBinding source;
        source.sourceId=GenerateStableId();
        source.sourcePath=scriptHint;
        source.presentation=ScriptPresentation::Script;
        source.apiVersion=1;
        source.provenance.kind=ScriptProvenanceKind::Project;
        source.provenance.contentHash="native-hud-events-fixture";
        if(!AddScriptAttachment(companion,
                CreateScriptAttachment(ScriptScope::Entity,hudEventEmitterId,source),error) ||
           !WriteScriptDocument(ScriptDocumentPathForScene(scenePath.generic_u8string()),
                companion,error))
        {std::cerr<<"Native HUD event attachment: "<<error<<'\n';return 1;}
    }
    FlowDocument flow;flow.envelope=CreateDocumentEnvelope(project.projectId,StoryFlowDocumentType,project.startupFlow,"Native HUD QA fixture");
    flow.envelope.documentId=project.startupFlowId;
    FlowNode gameStart;gameStart.id=GenerateStableId();gameStart.kind=FlowNodeKind::GameStart;gameStart.name="Game Start";
    FlowNode level;level.id=GenerateStableId();level.kind=FlowNodeKind::Level;level.name="HUD Level";
    level.sceneAssetId=sceneId;level.scenePathHint="Content/Scenes/HudLevel.wiscene";
    if(hudEvents&&eventHudReference)
    {
        level.levelHud={LevelHudMode::Override,
            HudReference{overrideHud.envelope.documentId,overrideHud.envelope.pathHint}};
        level.hudContextRules.push_back(
            {"qa.vehicle.enter","qa.vehicle.exit",*eventHudReference});
    }
    flow.startNodeId=gameStart.id;flow.nodes={gameStart,level};
    flow.routes={{GenerateStableId(),gameStart.id,GameStartOutcome,level.id,"start",0,{}}};
    if(!WriteFlowDocument((root/project.startupFlow).generic_u8string(),flow,error)){std::cerr<<error<<'\n';return 1;}
    std::cout<<"Prepared real Player + inherited project HUD fixture: "<<project.descriptorPath<<'\n';
}
