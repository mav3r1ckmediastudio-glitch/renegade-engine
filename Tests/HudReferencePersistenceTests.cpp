#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/FlowService.h"
#include "renegade/bridge/StoryFlowAuthoringSession.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/DependencyService.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
int main()
{
    int failures=0;std::string error;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<": "<<error<<'\n';}};
    const auto root=std::filesystem::temp_directory_path()/("renegade-hud-reference-"+GenerateStableId());
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{root};
    std::filesystem::create_directories(root);
    ProjectService projects;projects.Initialize((root/"Editor.ini").generic_u8string());
    check(projects.CreateStoryFlowProject(root.generic_u8string(),"HUD Reference QA", false),"Project creation failed");
    if(!projects.HasProject())return 1;
    const auto project=projects.CurrentProject();
    std::filesystem::create_directories(std::filesystem::u8path(project.rootPath)/"Content/UI");
    ScreenDocument hud;hud.usage=ScreenUsage::Hud;
    hud.envelope=CreateDocumentEnvelope(project.projectId,RuntimeScreenDocumentType,"Content/UI/Hud.renegade-screen","HUD reference proof");
    const auto hudPath=(std::filesystem::u8path(project.rootPath)/hud.envelope.pathHint).generic_u8string();
    check(WriteScreenDocument(hudPath,hud,error),"HUD fixture write failed");
    check(projects.SetDefaultHud(hud.envelope.documentId,hud.envelope.pathHint),"Global HUD transaction failed");
    ProjectMetadata reopened;
    check(projects.InspectProject(project.descriptorPath,reopened,error) && reopened.defaultHudId==hud.envelope.documentId &&
        reopened.defaultHud==hud.envelope.pathHint,"Global HUD did not round-trip");
    const auto movedHint=std::string("Content/UI/RenamedHud.renegade-screen");
    const auto movedPath=(std::filesystem::u8path(project.rootPath)/movedHint).generic_u8string();
    std::filesystem::rename(std::filesystem::u8path(hudPath),std::filesystem::u8path(movedPath));
    ProjectDependencyDocument renamedDependencies;
    check(MakeProjectDependencyReader()(project.descriptorPath,renamedDependencies,error) &&
        renamedDependencies.defaultHud==movedHint,
        "Packaging dependency did not repair moved HUD path by stable ID");
    check(projects.SetDefaultHud(hud.envelope.documentId,hud.envelope.pathHint),
        "Moved HUD stable-ID resolution failed");
    check(projects.InspectProject(project.descriptorPath,reopened,error) && reopened.defaultHud==movedHint,
        "Moved HUD did not repair persisted path hint");
    std::filesystem::rename(std::filesystem::u8path(movedPath),std::filesystem::u8path(hudPath));
    check(projects.SetDefaultHud(hud.envelope.documentId,hud.envelope.pathHint),
        "Restored HUD path transaction failed");
    check(!projects.SetDefaultHud(hud.envelope.documentId,"Content/../Outside.renegade-screen") &&
        projects.CurrentProject().defaultHud==hud.envelope.pathHint,"Invalid HUD partially changed project");
    hud.enabledInGame=false;check(WriteScreenDocument(hudPath,hud,error),"Disabled HUD setup failed");
    check(!projects.SetDefaultHud(hud.envelope.documentId,hud.envelope.pathHint),"Disabled HUD accepted as new default");
    hud.enabledInGame=true;check(WriteScreenDocument(hudPath,hud,error),"HUD restore failed");
    ProjectDependencyDocument dependencies;
    check(MakeProjectDependencyReader()(project.descriptorPath,dependencies,error) && dependencies.defaultHud==hud.envelope.pathHint,
        "Global HUD absent from typed dependency view");
    FlowDocument flow;
    flow.envelope=CreateDocumentEnvelope(project.projectId,StoryFlowDocumentType,"Content/StoryFlow/HudTest.renegade-flow","HUD override proof");
    FlowNode start;start.id=GenerateStableId();start.kind=FlowNodeKind::GameStart;start.name="Game Start";
    FlowNode level;level.id=GenerateStableId();level.kind=FlowNodeKind::Level;level.name="Level";
    level.sceneAssetId=GenerateStableId();level.scenePathHint="Content/Scenes/Test.wiscene";
    level.levelHud={LevelHudMode::Override,HudReference{hud.envelope.documentId,hud.envelope.pathHint}};
    flow.startNodeId=start.id;flow.nodes={start,level};
    flow.routes={{GenerateStableId(),start.id,GameStartOutcome,level.id,"start",0,{ }}};
    const auto flowPath=(std::filesystem::u8path(project.rootPath)/flow.envelope.pathHint).generic_u8string();
    check(WriteFlowDocument(flowPath,flow,error),"Level HUD write failed");
    FlowDocument loaded;
    check(ReadFlowDocument(flowPath,project.projectId,loaded,error) && loaded.nodes[1].levelHud.hud &&
        loaded.nodes[1].levelHud.hud->documentId==hud.envelope.documentId,"Level HUD did not round-trip");
    StoryFlowDependencyDocument flowDependencies;
    check(MakeStoryFlowDependencyReader(project.projectId)(flowPath,flowDependencies,error) &&
        flowDependencies.screenPathHints.size()==1 && flowDependencies.screenPathHints[0]==hud.envelope.pathHint,
        "Level HUD absent from dependency closure");
    // An optional contextual vehicle HUD must survive Flow save/reopen and
    // enter the packaged dependency closure even if it is never the default.
    auto vehicleHud=hud;
    vehicleHud.envelope=CreateDocumentEnvelope(project.projectId,RuntimeScreenDocumentType,
        "Content/UI/VehicleHud.renegade-screen","Vehicle HUD");
    const auto vehicleHudPath=(std::filesystem::u8path(project.rootPath)/
        vehicleHud.envelope.pathHint).generic_u8string();
    check(WriteScreenDocument(vehicleHudPath,vehicleHud,error),
        "Context HUD fixture could not be written");
    const HudContextEventRule vehicleRule{"vehicle.enter","vehicle.exit",
        {vehicleHud.envelope.documentId,vehicleHud.envelope.pathHint}};
    flow.nodes[1].hudContextRules={vehicleRule};
    check(WriteFlowDocument(flowPath,flow,error),
        "Context HUD authored Flow did not save");
    FlowDocument contextReopened;
    check(ReadFlowDocument(flowPath,project.projectId,contextReopened,error) &&
        contextReopened.nodes[1].hudContextRules.size()==1 &&
        contextReopened.nodes[1].hudContextRules[0].enterEvent=="vehicle.enter" &&
        contextReopened.nodes[1].hudContextRules[0].hud.documentId==vehicleHud.envelope.documentId,
        "Context HUD rule lost stable identity or event pair on save/reopen");
    StoryFlowDependencyDocument contextDependencies;
    check(MakeStoryFlowDependencyReader(project.projectId)(
        flowPath,contextDependencies,error) &&
        contextDependencies.screenPathHints.size()==2 &&
        std::find(contextDependencies.screenPathHints.begin(),
            contextDependencies.screenPathHints.end(),vehicleHud.envelope.pathHint)!=
            contextDependencies.screenPathHints.end(),
        "Context HUD was not included in portable Flow package closure");
    auto invalidFlow=flow;
    invalidFlow.nodes[1].hudContextRules.push_back(vehicleRule);
    check(!ValidateFlowDocument(invalidFlow,project.projectId,error),
        "Duplicate event rule failed to reject ambiguous Level routing");
    invalidFlow=flow;
    invalidFlow.nodes[0].hudContextRules={vehicleRule};
    check(!ValidateFlowDocument(invalidFlow,project.projectId,error),
        "Non-Level Flow node accepted contextual HUD event ownership");
    flow.nodes[1].hudContextRules.clear();
    check(WriteFlowDocument(flowPath,flow,error),
        "Could not restore baseline Flow after contextual HUD proof");

    std::filesystem::rename(std::filesystem::u8path(hudPath),std::filesystem::u8path(movedPath));
    StoryFlowDependencyDocument movedFlowDependencies;
    check(MakeStoryFlowDependencyReader(project.projectId)(flowPath,movedFlowDependencies,error) &&
        movedFlowDependencies.screenPathHints.size()==1 &&
        movedFlowDependencies.screenPathHints.front()==movedHint,
        "Level HUD package dependency failed to repair renamed HUD by stable ID");
    const auto nestedFlowPath=(std::filesystem::u8path(project.rootPath)/
        "Content/StoryFlow/Subfolder/HudTest.renegade-flow").generic_u8string();
    std::filesystem::create_directories(std::filesystem::u8path(nestedFlowPath).parent_path());
    auto nestedFlow=flow;
    nestedFlow.envelope.pathHint="Content/StoryFlow/Subfolder/HudTest.renegade-flow";
    check(WriteFlowDocument(nestedFlowPath,nestedFlow,error),"Nested HUD Flow fixture write failed");
    StoryFlowDependencyDocument nestedDependencies;
    check(MakeStoryFlowDependencyReader(project.projectId)(nestedFlowPath,nestedDependencies,error) &&
        nestedDependencies.screenPathHints.size()==1 &&
        nestedDependencies.screenPathHints.front()==movedHint,
        "Nested Flow HUD dependency did not resolve project root");
    std::filesystem::rename(std::filesystem::u8path(movedPath),std::filesystem::u8path(hudPath));
    hud.enabledInGame=false;
    check(WriteScreenDocument(hudPath,hud,error),"Disabled override setup failed");
    StoryFlowDependencyDocument disabledDependencies;
    check(!MakeStoryFlowDependencyReader(project.projectId)(flowPath,disabledDependencies,error),
        "Packaging accepted disabled Level HUD override");
    hud.enabledInGame=true;
    check(WriteScreenDocument(hudPath,hud,error),"Enabled override restoration failed");
    StoryFlowAuthoringSession session;
    check(session.Open(flowPath,project.projectId,error),"HUD authoring session open failed");
    check(!session.SetLevelHudContextRules(start.id,{vehicleRule},error),
        "Non-Level node accepted HUD event rule authoring");
    check(session.SetLevelHudContextRules(level.id,{vehicleRule},error) &&
        session.Document().nodes[1].hudContextRules.size()==1,
        "Contextual HUD authoring command could not add a validated event pair");
    check(session.Undo(error) && session.Document().nodes[1].hudContextRules.empty() &&
        session.Redo(error) && session.Document().nodes[1].hudContextRules.size()==1,
        "Contextual HUD event rules lack atomic Undo/Redo");
    check(session.SetLevelHudContextRules(level.id,{},error) &&
        session.Document().nodes[1].hudContextRules.empty(),
        "Contextual HUD rule removal was not committed");
    check(!session.SetLevelHud(start.id,{},error),"Non-Level HUD edit was permitted");
    check(session.SetLevelHud(level.id,{LevelHudMode::None,{}},error) && session.IsDirty() &&
        session.Document().nodes[1].levelHud.mode==LevelHudMode::None,"Level HUD None mutation failed");
    check(session.Undo(error) && session.Document().nodes[1].levelHud.mode==LevelHudMode::Override,
        "Level HUD Undo failed");
    check(session.Redo(error) && session.Document().nodes[1].levelHud.mode==LevelHudMode::None,
        "Level HUD Redo failed");
    check(session.Save(error),"Level HUD Save failed");
    FlowDocument savedFlow;
    check(ReadFlowDocument(flowPath,project.projectId,savedFlow,error) &&
        savedFlow.nodes[1].levelHud.mode==LevelHudMode::None,"Level HUD save/reopen failed");
    StoryFlowDependencyDocument noneDependencies;
    check(MakeStoryFlowDependencyReader(project.projectId)(flowPath,noneDependencies,error) &&
        noneDependencies.screenPathHints.empty(),
        "Explicit None Level HUD still emitted a packaging dependency");
    flow.nodes[0].levelHud=level.levelHud;
    check(!ValidateFlowDocument(flow,project.projectId,error),"Non-Level node accepted a HUD override");
    check(projects.SetDefaultHud({},{}),"Global HUD clear failed");
    check(projects.InspectProject(project.descriptorPath,reopened,error) && reopened.defaultHud.empty() && reopened.defaultHudId.empty(),
        "Global HUD clear did not persist");
    if(failures)return 1;
    std::cout<<"PASS: global/per-Level HUD references, atomic validation, persistence and typed dependency closure\n";
}
