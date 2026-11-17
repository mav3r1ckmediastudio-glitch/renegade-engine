#include "renegade/bridge/DialogueDisplayService.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
namespace renegade::bridge {
namespace fs = std::filesystem;
namespace {
const char* Roles[] = {"speaker","text","reply1","reply2","reply3","reply4","continue","previous","next","close"};
bool Path(const std::string& root,const std::string& relative,std::string& absolute,std::string& error) {
    return ResolveScreenResourcePath(root,relative,absolute,error);
}
}
ScreenDocument MakeDialogueDisplayTemplate(const StableId& project,const std::string& path) {
    ScreenDocument d;d.envelope=CreateDocumentEnvelope(project,RuntimeScreenDocumentType,path,"dialogue-display-v1");
    d.designWidth=1280;d.designHeight=720;d.usage=ScreenUsage::Overlay;d.enabledInGame=false;
    auto add=[&](ScreenWidgetKind kind,std::string role,std::string text,ScreenRect rect,StableId parent={}) {
        ScreenWidget w;w.id=GenerateStableId();w.name=role.empty()?"Dialogue background":("Dialogue "+role);
        w.kind=kind;w.dialogueRole=role;w.text=std::move(text);w.rect=rect;w.parentId=std::move(parent);
        w.style=MakeScreenWidgetStyleTemplate(kind,rect.height);w.style.opacity=1;
        w.style.normal.background=kind==ScreenWidgetKind::Button?ScreenColor{26,31,35,255}:ScreenColor{8,12,16,255};
        w.style.normal.foreground={240,243,242,255};w.style.text.fontSize=22;w.style.text.wrap=true;
        w.style.hover=w.style.focused=w.style.normal;w.style.hover.background={65,52,41,255};
        w.style.focused.background={65,52,41,255};w.style.pressed=w.style.normal;
        w.style.pressed.background={222,91,29,255};w.style.disabled=w.style.normal;
        w.style.borderColor={222,91,29,255};w.style.cornerRadius=5;
        if(kind==ScreenWidgetKind::Button){w.style.borderWidth=1;w.actionId="dialogue.ui."+role;
            d.actions.push_back({w.actionId});d.focusOrder.push_back(w.id);}
        else {w.style.text.horizontalAlignment=ScreenHorizontalAlignment::Left;w.style.text.verticalAlignment=ScreenVerticalAlignment::Top;}
        d.widgets.push_back(w);return w.id;
    };
    auto panel=add(ScreenWidgetKind::Image,{}, {},{24,420,1232,280});
    // The whole box can be moved/resized with the ordinary UI editor.
    add(ScreenWidgetKind::Text,"speaker","Speaker name",{24,18,520,36},panel);
    d.widgets.back().style.normal.foreground={222,91,29,255};
    add(ScreenWidgetKind::Text,"text","Dialogue text appears here.",{24,62,520,144},panel);
    for(int n=0;n<4;++n)add(ScreenWidgetKind::Button,"reply"+std::to_string(n+1),"Player reply "+std::to_string(n+1),{580,float(18+n*42),620,36},panel);
    // Continue shares the first reply's position; the adapter shows only one.
    add(ScreenWidgetKind::Button,"continue","Continue",{580,18,620,36},panel);
    d.widgets.back().visible=false;
    add(ScreenWidgetKind::Button,"previous","Previous",{580,224,186,32},panel);
    add(ScreenWidgetKind::Button,"next","Next",{780,224,186,32},panel);
    add(ScreenWidgetKind::Button,"close","Close",{1010,224,190,32},panel);
    return d;
}
bool ValidateDialogueDisplay(const ScreenDocument& d,const StableId& project,std::string& error) {
    if(!ValidateScreenDocument(d,project,error))return false;
    if(d.usage!=ScreenUsage::Overlay||d.enabledInGame){error="Dialogue displays must be disabled Overlay Screens; dialogue opens them automatically.";return false;}
    std::set<std::string> roles;
    for(const auto& w:d.widgets) {
        if(w.kind==ScreenWidgetKind::Button&&w.dialogueRole.empty()) {error="Assign a dialogue role to button '"+w.name+"'.";return false;}
        if(w.dialogueRole.empty())continue;
        if(!roles.insert(w.dialogueRole).second){error="Duplicate dialogue role: "+w.dialogueRole+". Assign each role once.";return false;}
        if(w.component || !w.bindings.empty() || !w.interactions.empty() || w.localOnly)
        {error="Dialogue elements must use their dialogue role without competing data bindings, linked components or local actions.";return false;}
        auto parent=w.parentId;size_t depth=0;
        while(!parent.empty()&&depth++<d.widgets.size()){
            auto p=std::find_if(d.widgets.begin(),d.widgets.end(),[&](const auto& x){return x.id==parent;});
            if(p==d.widgets.end()||!p->visible||!p->enabled){error="Dialogue elements need visible enabled parent containers.";return false;}
            parent=p->parentId;
        }
    }
    for(auto role:Roles)if(!roles.count(role)){error=std::string("Missing dialogue role: ")+role+". Restore it using the element's ROLE control.";return false;}
    error.clear();return true;
}
bool ReadDialogueDisplayAssignment(const std::string& root,const StableId& project,DialogueDisplayAssignment& out,std::string& error) {
    out={};error.clear();std::error_code ec;
    if(!IsValidStableId(project)){error="A valid project is required.";return false;}
    auto file=fs::u8path(root)/DialogueDisplaySettingsPath;
    if(!fs::exists(file,ec)&&!ec)return true;
    std::string absolute;if(!Path(root,DialogueDisplaySettingsPath,absolute,error))return false;
    if(fs::file_size(fs::u8path(absolute),ec)>4096||ec){error="Dialogue display setting is oversized or unavailable.";return false;}
    std::ifstream in(fs::u8path(absolute));std::string magic,owner,extra;int version=0;DialogueDisplayAssignment selected;
    if(!(in>>magic>>version>>std::quoted(owner)>>std::quoted(selected.screenId)>>std::quoted(selected.path))||
       (in>>extra)||magic!="renegade-dialogue-display"||version!=1||owner!=project||selected.screenId.empty()!=selected.path.empty())
    {error="Dialogue display setting has an invalid schema or project identity.";return false;}
    if(!selected.screenId.empty()){
        ScreenDocument d;std::string path;
        if(!IsValidStableId(selected.screenId)||!ResolveRuntimeScreenDocumentPath(root,project,selected.screenId,selected.path,path,error)||
           !ReadScreenDocument(path,project,d,error))return false;
        // Resolve both spellings before making the hint relative. Windows temp
        // roots may use a short-name alias, and callers may supply dot segments.
        const auto relative=fs::relative(fs::u8path(path),fs::u8path(root),ec);
        if(ec||relative.empty()){error="Could not make the dialogue display path project-relative.";return false;}
        selected.path=relative.generic_u8string();
    }
    out=std::move(selected);return true;
}
bool SetDialogueDisplayAssignment(const std::string& root,const StableId& project,const DialogueDisplayAssignment& a,std::string& error) {
    if(!IsValidStableId(project)||a.screenId.empty()!=a.path.empty()){error="Choose a valid project display.";return false;}
    if(!a.screenId.empty()){
        std::string absolute;ScreenDocument d;
        if(!Path(root,a.path,absolute,error)||!ReadScreenDocument(absolute,project,d,error)||
           d.envelope.documentId!=a.screenId||!ValidateDialogueDisplay(d,project,error))return false;
    }
    std::ostringstream out;out<<"renegade-dialogue-display 1\n"<<std::quoted(project)<<'\n'<<std::quoted(a.screenId)<<'\n'<<std::quoted(a.path)<<'\n';
    const auto bytes=out.str();ProjectDocumentWrite w;w.destinationPath=(fs::u8path(root)/DialogueDisplaySettingsPath).generic_u8string();
    w.content.assign(bytes.begin(),bytes.end());ProjectDocumentTransactionOptions options;options.allowedRoot=root;
    options.journalDirectory=(fs::u8path(root)/"Saved/Transactions").generic_u8string();
    const auto result=ProjectDocumentTransaction{}.Execute({std::move(w)},options);
    if(!result.success){error=result.message;return false;}error.clear();return true;
}
bool LoadDialogueDisplay(const std::string& root,const StableId& project,ScreenDocument& out,std::string& error) {
    DialogueDisplayAssignment a;if(!ReadDialogueDisplayAssignment(root,project,a,error))return false;
    out={};if(a.screenId.empty())return true;
    std::string path;if(!ResolveRuntimeScreenDocumentPath(root,project,a.screenId,a.path,path,error)||!ReadScreenDocument(path,project,out,error))return false;
    ScreenDocument resolved;if(!ResolveScreenTheme(root,out,resolved,error))return false;
    if(!ValidateDialogueDisplay(resolved,project,error))return false;
    RuntimeScreenDependencyDocument dependencies;
    if(!MakeRuntimeScreenDependencyReader(project)(path,dependencies,error))return false;
    for(const auto& relative:dependencies.imagePaths){std::string absolute;if(!Path(root,relative,absolute,error))return false;}
    for(const auto& relative:dependencies.fontPaths){std::string absolute;if(!Path(root,relative,absolute,error))return false;}
    out=std::move(resolved);return true;
}
bool BuildDialogueDisplay(const ScreenDocument& source,const DialogueView& view,size_t page,ScreenDocument& out,
                         std::vector<StableId>& controls,std::vector<std::string>& actions,std::string& error) {
    if(!ValidateDialogueDisplay(source,source.envelope.projectId,error))return false;
    auto d=source;controls.clear();actions.clear();const size_t pages=(view.replies.size()+3)/4;
    page=pages?std::min(page,pages-1):0;
    for(auto& w:d.widgets){const auto& role=w.dialogueRole;if(role.empty())continue;
        bool visible=true;std::string action;
        if(role=="speaker")w.text=view.speaker.empty()?" ":view.speaker;
        else if(role=="text")w.text=view.text.empty()?" ":view.text;
        else if(role.rfind("reply",0)==0){const size_t index=page*4+size_t(role.back()-'1');visible=index<view.replies.size();
            w.text=visible?view.replies[index].second:" ";if(visible)action=view.replies[index].first;}
        else if(role=="continue"){visible=view.replies.empty();action="@continue";}
        else if(role=="previous"){visible=pages>1&&page>0;action="@previous";}
        else if(role=="next"){visible=pages>1&&page+1<pages;action="@next";}
        else if(role=="close")action="@close";
        w.visible=visible;w.enabled=visible;
        if(visible&&!action.empty()){controls.push_back(w.id);actions.push_back(std::move(action));}
    }
    // Follow the user's authored keyboard/controller focus order.
    std::vector<StableId> ordered;std::vector<std::string> orderedActions;
    for(const auto& id:d.focusOrder){auto it=std::find(controls.begin(),controls.end(),id);if(it!=controls.end()){
        ordered.push_back(id);orderedActions.push_back(actions[size_t(it-controls.begin())]);}}
    controls=std::move(ordered);actions=std::move(orderedActions);out=std::move(d);return true;
}
bool SnapshotDialogueDisplay(const std::string& root,const std::string& destination,const StableId& project,std::string& error) {
    DialogueDisplayAssignment assignment;if(!ReadDialogueDisplayAssignment(root,project,assignment,error))return false;
    ScreenDocument validated;if(!LoadDialogueDisplay(root,project,validated,error))return false;
    std::error_code ec;std::set<std::string> files,screenPaths;
    if(fs::exists(fs::u8path(root)/DialogueDisplaySettingsPath,ec))files.insert(DialogueDisplaySettingsPath);
    if(!assignment.path.empty())screenPaths.insert(assignment.path);
    auto reader=MakeRuntimeScreenDependencyReader(project);
    std::set<std::string> visited;
    while(!screenPaths.empty()){
        auto path=*screenPaths.begin();screenPaths.erase(screenPaths.begin());if(!visited.insert(path).second)continue;
        if(visited.size()>128){error="Dialogue UI dependency graph is too large.";return false;}
        std::string absolute;if(!Path(root,path,absolute,error))return false;
        RuntimeScreenDependencyDocument dependencies;if(!reader(absolute,dependencies,error))return false;
        files.insert(path);files.insert(dependencies.imagePaths.begin(),dependencies.imagePaths.end());
        files.insert(dependencies.fontPaths.begin(),dependencies.fontPaths.end());
        screenPaths.insert(dependencies.screenPaths.begin(),dependencies.screenPaths.end());
    }
    for(const auto& relative:files){std::string absolute;if(!Path(root,relative,absolute,error))return false;
        auto to=fs::u8path(destination)/fs::u8path(relative);fs::create_directories(to.parent_path(),ec);
        if(!ec)fs::copy_file(fs::u8path(absolute),to,fs::copy_options::overwrite_existing,ec);
        if(ec){error="Could not stage dialogue display: "+ec.message();return false;}}
    return true;
}
}
