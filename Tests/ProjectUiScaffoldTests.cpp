#include "RuntimeScriptRuntime.h"
#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ProjectUiScaffoldService.h"
#include "renegade/bridge/StoryFlowScreenLifecycleService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenBackgroundPanel.h"
#include "renegade/bridge/ScreenLibraryService.h"
#include "renegade/bridge/StoryFlowScreenReferenceService.h"
#include "renegade/bridge/StoryFlowAuthoringModel.h"
#include "renegade/bridge/StoryFlowLayoutService.h"
#include "renegade/bridge/StoryFlowJourneyModel.h"
#include "renegade/bridge/GameplayInputService.h"
#include <wiScene.h>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <fstream>
#include <set>
#include <algorithm>
#include <cctype>

int main(int argc,char** argv)
{
    namespace fs=std::filesystem;using namespace renegade;
    const bool fixture=argc==3&&std::string(argv[1])=="--fixture";
    const auto parent=fixture?fs::u8path(argv[2]):fs::temp_directory_path()/
        fs::u8path("rg-ui-"+bridge::GenerateStableId().substr(0,8));
    fs::create_directories(parent);
    struct Cleanup{fs::path path;bool keep;~Cleanup(){if(!keep){std::error_code e;fs::remove_all(path,e);}}}cleanup{parent,fixture};
    int failures=0;auto check=[&](bool pass,const std::string& message)
    {if(!pass){++failures;std::cerr<<"FAIL: "<<message<<'\n';}};
    bridge::ProjectService project;
    if(!project.CreateStoryFlowProject(parent.generic_u8string(),"UI Starter"))
    {std::cerr<<project.LastError()<<'\n';return 1;}
    const auto metadata=project.CurrentProject();std::string error;
    check(metadata.startupScene.empty()&&!metadata.startupFlowId.empty(),"Flow remains the launch authority");
    check(!metadata.defaultHudId.empty(),"Default HUD assigned");
    const auto root=fs::u8path(metadata.rootPath);
    bridge::FlowDocument flow;
    if(!bridge::ReadFlowDocument((root/fs::u8path(metadata.startupFlow)).generic_u8string(),metadata.projectId,flow,error))
    {std::cerr<<error<<'\n';return 1;}
    // The default Player HUD must be discoverable in the starter graph,
    // while remaining a passive overlay rather than a navigation destination.
    int playerHudNodes=0;
    for(const auto& node:flow.nodes)if(node.name=="Player HUD")
    {
        ++playerHudNodes;
        check(node.kind==bridge::FlowNodeKind::Screen &&
            node.screenDocumentId==metadata.defaultHudId &&
            node.screenPathHint==metadata.defaultHud,
            "Player HUD node references the project default HUD");
        for(const auto& route:flow.routes)
            check(route.sourceNodeId!=node.id && route.destinationNodeId!=node.id,
                "Passive Player HUD must not become a navigation route");
    }
    check(playerHudNodes==1,"Starter StoryFlow visibly includes one Player HUD node");
    bridge::StoryFlowAuthoringModel authored;
    check(authored.Load(flow,metadata.projectId,error),
        "Starter Flow authoring model loads: "+error);
    for(const auto& diagnostic:authored.Diagnostics())
        check(diagnostic.nodeId.empty() ||
            std::none_of(authored.Nodes().begin(),authored.Nodes().end(),
                [&](const auto& node){return node.id==diagnostic.nodeId &&
                    bridge::IsStarterPlayerHud(node);}),
            "Player HUD must not be flagged as an unreachable screen");
    const auto graph=bridge::BuildDeterministicStoryFlowLayout(
        authored,metadata.projectId,flow.envelope.documentId);
    const auto hudNode=std::find_if(authored.Nodes().begin(),authored.Nodes().end(),
        [](const auto& node){return bridge::IsStarterPlayerHud(node);});
    const auto levelNode=std::find_if(authored.Nodes().begin(),authored.Nodes().end(),
        [](const auto& node){return node.kind==bridge::FlowNodeKind::Level;});
    if(hudNode!=authored.Nodes().end() && levelNode!=authored.Nodes().end())
    {
        const auto hudPosition=std::find_if(graph.nodes.begin(),graph.nodes.end(),
            [&](const auto& n){return n.nodeId==hudNode->id;});
        const auto levelPosition=std::find_if(graph.nodes.begin(),graph.nodes.end(),
            [&](const auto& n){return n.nodeId==levelNode->id;});
        check(hudPosition!=graph.nodes.end() &&
            levelPosition!=graph.nodes.end() &&
            hudPosition->x==levelPosition->x &&
            hudPosition->y>levelPosition->y &&
            hudPosition->y-levelPosition->y<=500.0f,
            "Starter Graph positions Player HUD beside the gameplay Level");
    }
    bridge::StoryFlowJourneyModel journey;
    check(journey.Build(authored,error),"Starter Journey view loads: "+error);
    if(hudNode!=authored.Nodes().end())
    {
        const auto card=journey.FindCard(hudNode->id);
        check(card!=nullptr && !journey.Tracks().at(card->trackIndex).detached,
            "Starter Journey presents Player HUD as an overlay rather than detached content");
    }
    bridge::ScreenDocument starterHud;
    check(bridge::ReadScreenDocument(
        (root/fs::u8path(metadata.defaultHud)).generic_u8string(),
        metadata.projectId,starterHud,error), "Read project Player HUD: "+error);
    check(starterHud.usage==bridge::ScreenUsage::Hud,
        "Player HUD renders as a passive gameplay overlay");
    for(const auto& attribute: {"health","stamina","mana"})
    {
        std::string barName(attribute);
        std::transform(barName.begin(),barName.end(),barName.begin(),
            [](unsigned char c){return static_cast<char>(std::toupper(c));});
        barName+=" Bar";
        const auto bar=std::find_if(starterHud.widgets.begin(),starterHud.widgets.end(),
            [&](const auto& widget){return widget.name==barName;});
        check(bar!=starterHud.widgets.end() &&
            bar->kind==bridge::ScreenWidgetKind::Progress &&
            bar->bindings.size()==1 &&
            bar->bindings.front().property==bridge::ScreenBindingProperty::Fill &&
            bar->bindings.front().operation==bridge::ScreenBindingOperation::Ratio &&
            bar->bindings.front().source.attributeId==std::string(attribute)+".current" &&
            bar->bindings.front().denominator.attributeId==std::string(attribute)+".maximum",
            std::string("Starter HUD meter has editable gameplay ratio binding: ")+attribute);
    }
    check(std::any_of(starterHud.widgets.begin(),starterHud.widgets.end(),
        [](const auto& widget){return widget.name=="Crosshair";}),
        "Starter HUD includes a removable crosshair");
    bridge::StoryFlowScreenReferenceService references;
    for(const auto& node:flow.nodes)if(node.kind==bridge::FlowNodeKind::Screen)
    {
        const auto audit=references.AuditScreenOutcomes(metadata.rootPath,metadata.projectId,flow,node.id);
        check(audit.succeeded,node.name+": "+audit.message);
    }
    bridge::ScreenDocument loading;
    const std::string loadingPath="Content/Screens/Loading.renegade-screen";
    const bool loadingReady=bridge::ReadScreenDocument((root/loadingPath).generic_u8string(),metadata.projectId,loading,error);
    check(loadingReady && loading.usage==bridge::ScreenUsage::Overlay && loading.widgets.size()==4 &&
        loading.widgets[0].backgroundPanel && loading.widgets[1].video && loading.widgets[2].video &&
        !loading.widgets[1].video->assetId.empty() && !loading.widgets[2].video->assetId.empty() &&
        loading.widgets[2].video->additive && fs::is_regular_file(root/loading.widgets.back().hintListPath),
        "Loading template reopens with two independent governed clips and hint list: "+error);
    int optionalCount=0;std::set<std::string> identities;
    for(const auto& item:fs::recursive_directory_iterator(root/"Content/Screens"))
    {
        if(item.path().extension()!=".renegade-screen")continue;
        bridge::ScreenDocument screen;
        if(!bridge::ReadScreenDocument(item.path().generic_u8string(),metadata.projectId,screen,error))
        {check(false,error);continue;}
        check(identities.insert(screen.envelope.documentId).second,"Unique Screen document identity");
        if(!screen.enabledInGame)++optionalCount;
        if((screen.usage==bridge::ScreenUsage::Menu || screen.usage==bridge::ScreenUsage::Modal) && screen.enabledInGame)
            check(!screen.focusOrder.empty(),"Every enabled menu has an eligible control");
    }
    check(optionalCount==8,"Eight optional modules retained as disabled editable assets");
    check(identities.size()==26,"Full editable Screen catalogue written");
    // The same catalogue recreates every standard Screen, including disabled
    // optional capabilities, without copying project identities or widget IDs.
    check(bridge::ProjectUiScaffoldTemplateNames().size()==26,"Complete reusable starter catalogue");
    for(const auto& name:bridge::ProjectUiScaffoldTemplateNames())
    {
        bridge::ScreenDocument restored,second;
        const bool built=bridge::MakeProjectUiScaffoldTemplate(metadata.projectId,name,restored,error);
        check(built&&bridge::ValidateScreenDocument(restored,metadata.projectId,error),"Reusable starter template: "+name+": "+error);
        check(bridge::MakeProjectUiScaffoldTemplate(metadata.projectId,name,second,error)&&
            restored.envelope.documentId!=second.envelope.documentId&&
            restored.widgets.front().id!=second.widgets.front().id,"Restored templates receive fresh identities");
    }
    bridge::ScreenDocument unknown;
    check(!bridge::MakeProjectUiScaffoldTemplate(metadata.projectId,"../Unknown",unknown,error),"Unknown template cannot invent a path");
    check(std::find(metadata.alwaysInclude.begin(),metadata.alwaysInclude.end(),
        "data:Content/Data/GameplayInput.renegade-input")!=metadata.alwaysInclude.end(),"Starter gameplay map belongs to standalone closure");
    bridge::GameplayInputMap input;
    check(bridge::ReadGameplayInputMap(metadata.rootPath,input,error),error);
    auto bootstrap=runtime::ResolveRuntimeProject(runtime::ParseRuntimeLaunchArguments({"--project",metadata.descriptorPath}));
    check(bootstrap.succeeded,bootstrap.message);
    bridge::SceneService scenes;runtime::RuntimeFlowController runtimeFlow;
    bootstrap=runtime::LoadRuntimeProjectFlow(scenes,runtimeFlow,std::move(bootstrap));
    check(bootstrap.succeeded&&bootstrap.flowNodeName=="Title",bootstrap.message);
    auto step=[&](const char* outcome,const char* expected)
    {
        const auto next=runtimeFlow.EmitOutcome(outcome);
        const bool applied=next.succeeded&&runtimeFlow.ApplyStep(scenes,bootstrap,next,error);
        check(applied&&bootstrap.flowNodeName==expected,std::string(outcome)+" -> "+expected+": "+error);
        return applied;
    };
    step("continue","MainMenu");step("options","Options");step("accessibility","Accessibility");
    step("more","AccessibilityMore");step("targets","AccessibilityTargets");
    step("ui.back","AccessibilityMore");step("ui.back","Accessibility");step("ui.back","Options");
    step("graphics","Graphics");step("ui.back","Options");step("audio","Audio");step("ui.back","Options");
    step("controls","Controls");step("ui.back","Options");step("ui.back","MainMenu");
    step("load_game","SaveLoad");step("ui.back","MainMenu");
    step("credits","Credits");step("ui.back","MainMenu");
    step("new_game","NewGame");step("ui.back","MainMenu");step("new_game","NewGame");
    if(!step("start","Starter Level"))return 1;
    const auto count=scenes.EntityCount();
    const auto sentinel=scenes.GetScene().Entity_CreateTransform("Retained UI resume sentinel");
    check(scenes.GetScene().transforms.Contains(sentinel),"Live marker created");
    step("ui.pause","Pause");step("options","Paused Options");step("audio","Paused Audio");
    step("ui.back","Paused Options");step("accessibility","Paused Accessibility");
    step("ui.back","Paused Options");step("ui.back","Pause");step("ui.back","Starter Level");
    check(scenes.GetScene().transforms.Contains(sentinel)&&scenes.EntityCount()==count+1,
        "Pause/settings/resume retain the actual Level scene and all live entities");
    step("level.failed","Death");step("retry","Starter Level");
    check(!scenes.GetScene().transforms.Contains(sentinel),"Retry creates a fresh Level");
    step("level.complete","Victory");step("continue","Complete");
    check(bootstrap.flowTerminalAction==bridge::FlowTerminalAction::CompleteGame,"Authored complete route reaches terminal");
    // Resume fails if the target Level session is absent, rather than loading it.
    auto fresh=runtime::ResolveRuntimeProject(runtime::ParseRuntimeLaunchArguments({"--project",metadata.descriptorPath}));
    runtime::RuntimeFlowController orphan;check(orphan.Initialize(fresh,error),error);
    bridge::FlowStepResult resume;resume.currentNodeKind=bridge::FlowNodeKind::Level;
    for(const auto& node:flow.nodes)if(node.kind==bridge::FlowNodeKind::Level)
    {resume.sceneAssetId=node.sceneAssetId;resume.scenePathHint=node.scenePathHint;}
    resume.destinationEntry="renegade.ui.resume";bridge::SceneService noSession;
    check(!orphan.ApplyStep(noSession,fresh,resume,error)&&noSession.CurrentPath().empty(),"Missing retained session fails explicitly");
    if(!fixture)
    {
        std::map<std::string,std::string> targets;
        bridge::ScreenDocument menu;
        std::map<std::string,std::string> previousIds;
        auto readBytes=[](const fs::path& path){
            std::ifstream in(path,std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(in),{});
        };
        const auto hudBefore=readBytes(root/metadata.defaultHud);
        for(const auto& node:flow.nodes)
        {
            if(node.kind!=bridge::FlowNodeKind::Screen || node.screenDocumentId==metadata.defaultHudId)continue;
            bridge::ScreenDocument document;
            if(!bridge::ReadScreenDocument((root/node.screenPathHint).generic_u8string(),metadata.projectId,document,error))continue;
            const auto panel=std::find_if(document.widgets.begin(),document.widgets.end(),
                [&](const auto& w){return bridge::IsScreenBackground(document,w);});
            if(panel==document.widgets.end())continue;
            if(menu.widgets.empty()){menu=document;continue;}
            if(node.screenDocumentId!=menu.envelope.documentId && targets.size()<2)
            {
                targets.emplace(node.screenDocumentId,node.screenPathHint);
                previousIds.emplace(node.screenDocumentId,panel->id);
            }
        }
        check(!menu.widgets.empty() && targets.size()==2,"Stock screen backgrounds recognised");
        if(!menu.widgets.empty() && targets.size()==2)
        {
            auto panel=*std::find_if(menu.widgets.begin(),menu.widgets.end(),
                [&](const auto& w){return bridge::IsScreenBackground(menu,w);});
            panel.imageFit=bridge::ScreenImageFit::Cover;
            const auto cameraId=bridge::GenerateStableId();
            const auto levelId=bridge::GenerateStableId();
            panel.cameraSource=bridge::ScreenCameraSource{levelId,"Content/Scenes/Starter.wiscene",cameraId,"Overlook",true};
            check(bridge::CopyScreenBackgrounds(metadata.rootPath,menu,panel,targets,error),"Copy selected backgrounds: "+error);
            for(const auto& [id,path]:targets)
            {
                bridge::ScreenDocument saved;
                check(bridge::ReadScreenDocument((root/path).generic_u8string(),metadata.projectId,saved,error)
                    && !saved.widgets.empty() && saved.widgets.front().backgroundPanel
                    && saved.widgets.front().cameraSource
                    && saved.widgets.front().cameraSource->cameraId==cameraId
                    && saved.widgets.front().cameraSource->levelId==levelId
                    && saved.widgets.front().cameraSource->slowPan
                    && saved.widgets.front().id==previousIds.at(id)
                    && saved.widgets.front().imageFit==bridge::ScreenImageFit::Cover,
                    "Reopen independent back layer, retaining target identity");
            }
            // Run the public Lua surface through the normal project script VM.
            const auto screenPath=targets.begin()->second;
            const std::string scriptPath="Content/Scripts/CameraModeTest.lua";
            fs::create_directories((root/scriptPath).parent_path());
            std::ofstream(root/scriptPath)
                << "return {on_start=function() local camera=renegade.ui.background_camera; local screen='"
                << screenPath << "'; local mode,level,id=camera(screen); assert(mode=='pan' and id=='"
                << cameraId << "'); assert(camera(screen,'fixed')=='fixed'); assert(camera(screen)=='fixed');"
                << "assert(camera(screen,'image')=='image'); assert(camera(screen,'authored')=='pan');"
                << "local media=renegade.ui.screen_media; local loading='" << loadingPath
                << "'; local panel='" << (loading.widgets.size()>1?loading.widgets[1].id:"") << "';"
                << "assert(media(loading,panel).loop); assert(media(loading,panel,{loop=false,mute=false}).loop==false);"
                << "assert(media(loading,panel,'authored').mute);end}";
            auto scripts=bridge::CreateScriptDocument(metadata.projectId,bridge::GenerateStableId(),
                "Content/Scenes/Starter.wiscene","camera-test");
            bridge::ScriptSourceBinding binding;
            binding.sourceId=bridge::GenerateStableId();binding.sourcePath=scriptPath;
            binding.presentation=bridge::ScriptPresentation::GlobalScript;
            binding.apiVersion=runtime::RuntimeScriptRuntime::ApiVersion;
            binding.provenance.kind=bridge::ScriptProvenanceKind::Project;
            binding.provenance.contentHash="camera-mode-test";
            wi::scene::Scene privateScene;
            runtime::RuntimeScriptRuntime vm;
            check(bridge::AddScriptAttachment(scripts,bridge::CreateScriptAttachment(bridge::ScriptScope::Level,{},binding),error)
                && vm.StartScene(scripts,privateScene,metadata.rootPath,error)
                && vm.DisabledInstanceCount()==0 && vm.Diagnostics().empty(),"Camera modes through project Lua: "+error);
            vm.StopScene();
            auto second=std::next(targets.begin());
            const auto secondBefore=readBytes(root/second->second);
            panel.imageFit=bridge::ScreenImageFit::Contain;
            const auto first=targets.begin();
            check(bridge::CopyScreenBackgrounds(metadata.rootPath,menu,panel,
                {{first->first,first->second}},error),"Replace one copied panel");
            check(readBytes(root/second->second)==secondBefore,"Sibling copy retains its previous artwork settings");
            const auto firstBefore=readBytes(root/first->second);
            auto invalid=targets;
            invalid.emplace(bridge::GenerateStableId(),"Content/Screens/Missing.renegade-screen");
            check(!bridge::CopyScreenBackgrounds(metadata.rootPath,menu,panel,invalid,error)
                && readBytes(root/first->second)==firstBefore && readBytes(root/second->second)==secondBefore,
                "Invalid batch preserves all selected screen files");
            check(readBytes(root/metadata.defaultHud)==hudBefore,"Unselected HUD byte-identical");
        }
        bridge::StoryFlowNewScreenRequest request;request.projectRoot=metadata.rootPath;
        request.projectId=metadata.projectId;request.flowPath=(root/fs::u8path(metadata.startupFlow)).generic_u8string();
        request.flow=flow;request.screenName="Restored Graphics";request.screenPathHint="Content/Screens/RestoredGraphics.renegade-screen";
        request.screenTemplate=static_cast<bridge::StoryFlowScreenTemplate>(static_cast<int>(bridge::StoryFlowScreenTemplate::ProjectScaffold)+5);
        const auto created=bridge::StoryFlowScreenLifecycleService{}.CreateNewScreen(request);
        check(created.succeeded,"Native composer starter enum commits through governed lifecycle: "+created.message);
        bridge::ScreenDocument restored;
        check(created.succeeded&&bridge::ReadScreenDocument(created.screenPath,metadata.projectId,restored,error)&&
            restored.widgets.size()>=4&&restored.envelope.documentId==created.screenDocumentId,
            "Governed recreated template is editable and owns its new identity");
    }
    // A conflict must preserve the current project and all files.
    const auto originalId=project.CurrentProject().projectId;
    std::ofstream(root/"owner-sentinel.txt")<<"keep";
    check(!project.CreateStoryFlowProject(parent.generic_u8string(),"UI Starter"),"Nonempty destination rejected");
    check(project.CurrentProject().projectId==originalId&&fs::is_regular_file(root/"owner-sentinel.txt"),"Conflict preserves current project and files");
    check(project.OpenProject(metadata.descriptorPath)&&project.CurrentProject().defaultHudId==metadata.defaultHudId,
        "Cold descriptor reopen retains scaffold and HUD reference");
    for(const auto& item:fs::directory_iterator(parent))
        check(item.path().filename().generic_u8string().find(".rg-create-")!=0,"No staging directory left behind");
    if(fixture)std::cout<<"FIXTURE: "<<metadata.descriptorPath<<'\n';
    std::cout<<"Project UI scaffold integration: "<<(failures?"FAIL":"PASS")<<" ("<<failures<<")\n";
    return failures?1:0;
}
