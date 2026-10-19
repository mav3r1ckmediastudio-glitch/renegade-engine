#include "RuntimeBootstrap.h"
#include "RuntimeFlow.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ProjectUiScaffoldService.h"
#include "renegade/bridge/StoryFlowScreenLifecycleService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenLibraryService.h"
#include "renegade/bridge/StoryFlowScreenReferenceService.h"
#include "renegade/bridge/GameplayInputService.h"
#include <wiScene.h>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <fstream>
#include <set>
#include <algorithm>

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
    bridge::StoryFlowScreenReferenceService references;
    for(const auto& node:flow.nodes)if(node.kind==bridge::FlowNodeKind::Screen)
    {
        const auto audit=references.AuditScreenOutcomes(metadata.rootPath,metadata.projectId,flow,node.id);
        check(audit.succeeded,node.name+": "+audit.message);
    }
    int optionalCount=0;std::set<std::string> identities;
    for(const auto& item:fs::recursive_directory_iterator(root/"Content/Screens"))
    {
        if(item.path().extension()!=".renegade-screen")continue;
        bridge::ScreenDocument screen;
        if(!bridge::ReadScreenDocument(item.path().generic_u8string(),metadata.projectId,screen,error))
        {check(false,error);continue;}
        check(identities.insert(screen.envelope.documentId).second,"Unique Screen document identity");
        if(!screen.enabledInGame)++optionalCount;
        if(screen.usage!=bridge::ScreenUsage::Hud&&screen.enabledInGame)
            check(!screen.focusOrder.empty(),"Every enabled menu has an eligible control");
    }
    check(optionalCount==8,"Eight optional modules retained as disabled editable assets");
    check(identities.size()==25,"Full editable Screen catalogue written");
    // The same catalogue recreates every standard Screen, including disabled
    // optional capabilities, without copying project identities or widget IDs.
    check(bridge::ProjectUiScaffoldTemplateNames().size()==25,"Complete reusable starter catalogue");
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
