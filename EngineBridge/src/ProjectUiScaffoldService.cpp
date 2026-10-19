#include "renegade/bridge/ProjectUiScaffoldService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/StoryFlowLevelLifecycleService.h"
#include "renegade/bridge/GameplayInputService.h"
#include <filesystem>
#include <map>
#include <algorithm>
#include <vector>
#include <utility>

namespace renegade::bridge
{
namespace
{
    struct Choice { const char* action; const char* label; bool enabled=true; };
    ScreenWidget Label(const char* name, const std::string& text, float top, float bottom, float size)
    {
        ScreenWidget w;w.id=GenerateStableId();w.kind=ScreenWidgetKind::Text;
        w.name=name;w.text=text;w.enabled=false;w.rect={0,0,400,60};
        w.layoutMode=ScreenLayoutMode::Anchored;w.anchors={.06f,top,.94f,bottom,0,0,0,0};
        w.style=MakeScreenWidgetStyleTemplate(w.kind,60);
        w.style.text.fontSize=size;w.style.text.wrap=true;
        w.style.text.horizontalAlignment=ScreenHorizontalAlignment::Center;
        w.style.text.verticalAlignment=ScreenVerticalAlignment::Center;
        return w;
    }
    ScreenDocument Page(const ProjectMetadata& project, const std::string& name,
        const std::string& message, std::initializer_list<Choice> choices,
        bool enabled=true, ScreenUsage usage=ScreenUsage::Menu)
    {
        ScreenDocument d;const auto hint="Content/Screens/"+name+".renegade-screen";
        d.envelope=CreateDocumentEnvelope(project.projectId,RuntimeScreenDocumentType,hint,
            "Renegade editable project UI scaffold v1");
        d.usage=usage;d.enabledInGame=enabled;
        auto bg=Label("Background","",0,1,24);bg.kind=ScreenWidgetKind::Image;
        bg.anchors={0,0,1,1,0,0,0,0};bg.style=MakeScreenWidgetStyleTemplate(bg.kind,720);
        bg.style.normal.background={18,23,32,255};d.widgets.push_back(bg);
        std::string title=name;
        if(name=="Accessibility")title="ACCESS";
        else if(name=="AccessibilityMore")title="APPEARANCE";
        else if(name=="AccessibilityTargets")title="TARGETS";
        else if(name=="MainMenu")title="MAIN MENU";
        else if(name=="NewGame")title="NEW GAME";
        else if(name=="SaveLoad")title="SAVE / LOAD";
        d.widgets.push_back(Label("Title",title,.025f,.15f,24));
        d.widgets.push_back(Label("Description",message,.16f,choices.size()<=2?.65f:.32f,
            choices.size()<=2?18.0f:12.0f));
        if(choices.size())
        {
            ScreenWidget group;group.id=GenerateStableId();group.kind=ScreenWidgetKind::Image;
            group.name="Actions";group.rect={0,0,800,400};group.enabled=false;
            group.layoutMode=ScreenLayoutMode::Anchored;
            group.anchors={.06f,choices.size()<=2?.72f:.36f,.94f,.94f,0,0,0,0};
            group.container.mode=ScreenChildLayout::Grid;group.container.columns=2;
            group.container.padding=4;group.container.gap=12;
            const auto parent=group.id;d.widgets.push_back(group);
            for(const auto& choice:choices)
            {
                bool declared=false;for(const auto& action:d.actions)declared|=action.id==choice.action;
                if(!declared)d.actions.push_back({choice.action});
                ScreenWidget button;button.id=GenerateStableId();button.kind=ScreenWidgetKind::Button;
                button.name=choice.label;button.text=choice.label;button.actionId=choice.action;
                button.enabled=choice.enabled;button.parentId=parent;button.rect={0,0,300,64};
                button.style=MakeScreenWidgetStyleTemplate(button.kind,64);
                button.style.text.fontSize=20;button.style.text.wrap=true;
                if(choice.enabled)d.focusOrder.push_back(button.id);
                d.widgets.push_back(std::move(button));
            }
        }
        return d;
    }
    std::vector<ScreenDocument> BuildScaffoldPages(const ProjectMetadata& project)
    {
        std::vector<ScreenDocument> pages;
        auto add=[&](const std::string& name,const std::string& message,
            std::initializer_list<Choice> choices,bool enabled=true,ScreenUsage usage=ScreenUsage::Menu)
        {pages.push_back(Page(project,name,message,choices,enabled,usage));};
    add("Title",project.name,{{"continue","CONTINUE"}});
    add("MainMenu","Choose where to go.",{{"new_game","NEW GAME"},{"load_game","LOAD/SAVE"},
        {"options","OPTIONS"},{"credits","CREDITS"},{"exit","EXIT"}});
    add("NewGame","Start the editable starter Level. Add your Player and gameplay in the Level Editor.",
        {{"start","START"},{"ui.back","BACK"}});
    add("SaveLoad","Saving and loading require a configured save-game module. No save slots are available.",
        {{"ui.back","BACK"}});
    add("Options","Player accessibility is ready. Other settings need your game modules.",
        {{"graphics","GRAPHICS"},{"audio","AUDIO"},{"controls","CONTROLS"},
        {"accessibility","ACCESS"},{"ui.back","BACK"}});
    add("Graphics","No graphics-settings module is configured. Add and wire your game's settings here.",{{"ui.back","BACK"}});
    add("Audio","No audio-settings module is configured. Add and wire your game's settings here.",{{"ui.back","BACK"}});
    add("Controls","Default gameplay: WASD move, mouse look, Space jump, Escape pause. Edit the project input map to configure bindings.",{{"ui.back","BACK"}});
    add("Accessibility","Settings belong to this player and persist across game restarts.",
        {{RuntimeUiTextScaleIncreaseAction,"TEXT +"},{RuntimeUiTextScaleDecreaseAction,"TEXT -"},
        {RuntimeUiScaleIncreaseAction,"UI +"},{RuntimeUiScaleDecreaseAction,"UI -"},
        {"more","MORE"},{"ui.back","BACK"}});
    add("AccessibilityMore","Adjust focus, contrast, reduced motion and larger targets.",
        {{RuntimeUiReducedMotionToggleAction,"MOTION"},{RuntimeUiContrastCycleAction,"CONTRAST"},
        {RuntimeUiFocusColorCycleAction,"COLOUR"},{RuntimeUiFocusWidthCycleAction,"WIDTH"},
        {"targets","TARGETS"},{"ui.back","BACK"}});
    add("AccessibilityTargets","Choose target size or restore the default text and UI scale.",
        {{RuntimeUiTargetSizeCycleAction,"TARGET SIZE"},{RuntimeUiScaleResetAction,"UI 1X"},
        {RuntimeUiTextScaleResetAction,"TEXT 1X"},{"ui.back","BACK"}});
    add("Credits","Your game. Your team. Edit this Screen to add your credits.",{{"ui.back","BACK"}});
    add("Exit","Exit the game?",{{"confirm","EXIT"},{"ui.back","BACK"}});
    add("Pause","Gameplay is paused. Resume keeps the current Level session.",
        {{"ui.back","RESUME"},{"options","OPTIONS"},{"main_menu","MAIN MENU"}},true,ScreenUsage::Modal);
    add("Death","Retry the Level or return to the main menu.",{{"retry","RETRY"},{"main_menu","MAIN MENU"}});
    add("Victory","Level complete.",{{"continue","FINISH"},{"main_menu","MAIN MENU"}});
    add("HUD","ESCAPE // PAUSE",{},true,ScreenUsage::Hud);
    // HUD should be a passive, transparent overlay, never a full-screen menu.
    auto& hud=*std::find_if(pages.begin(),pages.end(),[](const auto& d){return d.envelope.pathHint=="Content/Screens/HUD.renegade-screen";});hud.widgets.erase(hud.widgets.begin());
    hud.widgets.erase(hud.widgets.begin());
    hud.widgets[0].anchors={.03f,.84f,.97f,.98f,0,0,0,0};
    hud.widgets[0].style.text.fontSize=14;
    hud.widgets[0].style.text.horizontalAlignment=ScreenHorizontalAlignment::Left;
    const char* optional[]={"Inventory","Trading","Crafting","Character","Skills","Journal","Map","Dialogue"};
    for(const auto* name:optional)add(name,
        std::string(name)+" is disabled until its gameplay module and data bindings are configured. Enable and wire this editable Screen in your project.",{},false,ScreenUsage::Overlay);
        return pages;
    }
}

const std::vector<std::string>& ProjectUiScaffoldTemplateNames()
{
    static const std::vector<std::string> names={"Title","MainMenu","NewGame","SaveLoad","Options",
        "Graphics","Audio","Controls","Accessibility","AccessibilityMore","AccessibilityTargets",
        "Credits","Exit","Pause","Death","Victory","HUD","Inventory","Trading","Crafting",
        "Character","Skills","Journal","Map","Dialogue"};
    return names;
}
bool MakeProjectUiScaffoldTemplate(const std::string& projectId,const std::string& templateName,
    ScreenDocument& document,std::string& error)
{
    const auto& names=ProjectUiScaffoldTemplateNames();
    if(!IsValidStableId(projectId)||std::find(names.begin(),names.end(),templateName)==names.end())
    {error="Choose an available UI scaffold template and a valid project.";return false;}
    ProjectMetadata project;project.projectId=projectId;project.name="YOUR GAME";
    for(auto& page:BuildScaffoldPages(project))
        if(std::filesystem::u8path(page.envelope.pathHint).stem().generic_u8string()==templateName)
        {document=std::move(page);error.clear();return true;}
    error="The selected UI scaffold template is unavailable.";return false;
}

bool SeedProjectUiScaffold(ProjectService& projects,std::string& error)
{
    namespace fs=std::filesystem;
    if(!projects.HasProject()){error="UI scaffold requires a staged new project.";return false;}
    const auto project=projects.CurrentProject();const auto root=fs::u8path(project.rootPath);
    const auto flowPath=(root/fs::u8path(project.startupFlow)).generic_u8string();
    FlowDocument flow;
    if(!ReadFlowDocument(flowPath,project.projectId,flow,error))return false;
    if(flow.nodes.size()!=1||!flow.routes.empty())
    {error="UI scaffold can only seed a new, empty Story Flow.";return false;}
    std::map<std::string,StableId> nodes;nodes["Start"]=flow.startNodeId;
    std::map<std::string,ScreenDocument> pages;
    for(auto d:BuildScaffoldPages(project))
    {
        const auto name=fs::u8path(d.envelope.pathHint).stem().generic_u8string();
        if(d.enabledInGame&&d.usage!=ScreenUsage::Hud)
        {
            FlowNode n;n.id=GenerateStableId();n.kind=FlowNodeKind::Screen;n.name=name;
            n.screenDocumentId=d.envelope.documentId;n.screenPathHint=d.envelope.pathHint;
            nodes[name]=n.id;flow.nodes.push_back(std::move(n));
        }
        pages.emplace(name,std::move(d));
    }
    // Reuse the same editable Options assets with distinct Flow context nodes.
    const char* settings[]={"Options","Graphics","Audio","Controls","Accessibility","AccessibilityMore","AccessibilityTargets"};
    for(const auto* name:settings)
    {
        const auto& d=pages.at(name);FlowNode n;n.id=GenerateStableId();n.kind=FlowNodeKind::Screen;
        n.name=std::string("Paused ")+name;n.screenDocumentId=d.envelope.documentId;
        n.screenPathHint=d.envelope.pathHint;nodes[std::string("Paused ")+name]=n.id;flow.nodes.push_back(std::move(n));
    }
    StoryFlowNewLevelRequest levelRequest;levelRequest.projectRoot=project.rootPath;
    levelRequest.projectId=project.projectId;levelRequest.flowPath=flowPath;levelRequest.flow=flow;
    levelRequest.levelName="Starter Level";levelRequest.scenePathHint="Content/Scenes/Starter.wiscene";
    const auto level=StoryFlowLevelLifecycleService{}.CreateNewLevel(levelRequest);
    if(!level.succeeded){error=level.message;return false;}flow=level.committedFlow;nodes["Level"]=level.levelNodeId;
    for(const auto& item:std::initializer_list<std::pair<const char*,FlowNodeKind>>{
        {"Quit",FlowNodeKind::Quit},{"Complete",FlowNodeKind::CompleteGame}})
    {FlowNode n;n.id=GenerateStableId();n.name=item.first;n.kind=item.second;nodes[item.first]=n.id;flow.nodes.push_back(std::move(n));}
    auto route=[&](const std::string& from,const char* action,const std::string& to,const char* entry="")
    {FlowRoute r;r.id=GenerateStableId();r.sourceNodeId=nodes.at(from);r.outcome=action;
        r.destinationNodeId=nodes.at(to);r.destinationEntry=to=="Level"&&std::string(entry).empty()?"start":entry;
        flow.routes.push_back(std::move(r));};
    route("Start",GameStartOutcome,"Title");route("Title","continue","MainMenu");
    route("MainMenu","new_game","NewGame");route("MainMenu","load_game","SaveLoad");
    route("MainMenu","options","Options");route("MainMenu","credits","Credits");route("MainMenu","exit","Exit");
    route("NewGame","start","Level");route("NewGame","ui.back","MainMenu");
    route("SaveLoad","ui.back","MainMenu");route("Credits","ui.back","MainMenu");
    route("Exit","confirm","Quit");route("Exit","ui.back","MainMenu");
    route("Level","ui.pause","Pause");route("Level","level.failed","Death");route("Level","level.complete","Victory");
    route("Pause","ui.back","Level","renegade.ui.resume");route("Pause","main_menu","MainMenu");
    route("Pause","options","Paused Options");
    route("Death","retry","Level");route("Death","main_menu","MainMenu");
    route("Victory","continue","Complete");route("Victory","main_menu","MainMenu");
    for(const auto& prefix: {std::string{},std::string{"Paused "}})
    {
        route(prefix+"Options","graphics",prefix+"Graphics");route(prefix+"Options","audio",prefix+"Audio");
        route(prefix+"Options","controls",prefix+"Controls");route(prefix+"Options","accessibility",prefix+"Accessibility");
        route(prefix+"Options","ui.back",prefix.empty()?"MainMenu":"Pause");
        for(const auto* name:{"Graphics","Audio","Controls","Accessibility"})route(prefix+name,"ui.back",prefix+"Options");
        route(prefix+"Accessibility","more",prefix+"AccessibilityMore");
        route(prefix+"AccessibilityMore","targets",prefix+"AccessibilityTargets");
        route(prefix+"AccessibilityMore","ui.back",prefix+"Accessibility");
        route(prefix+"AccessibilityTargets","ui.back",prefix+"AccessibilityMore");
    }
    for(const auto& item:pages)
    {
        const auto& d=item.second;const auto path=root/fs::u8path(d.envelope.pathHint);
        fs::create_directories(path.parent_path());
        if(fs::exists(path)){error="Scaffold destination already exists: "+d.envelope.pathHint;return false;}
        if(!WriteScreenDocument(path.generic_u8string(),d,error))return false;
    }
    if(!WriteFlowDocument(flowPath,flow,error)||
        !WriteGameplayInputMap(project.rootPath,MakeDefaultGameplayInputMap(),error))return false;
    const auto& hud=pages.at("HUD");
    if(!projects.SetAlwaysInclude({"data:Content/Data/GameplayInput.renegade-input"}))
    {error=projects.LastError();return false;}
    if(!projects.SetDefaultHud(hud.envelope.documentId,hud.envelope.pathHint))
    {error=projects.LastError();return false;}
    error.clear();return true;
}
}
