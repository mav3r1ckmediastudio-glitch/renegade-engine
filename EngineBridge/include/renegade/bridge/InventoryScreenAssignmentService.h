#pragma once
// G5E: explicit project-owned Inventory Screen assignment used by BOTH the
// production Windows dependency collector and Runtime (including packaged).
// A preview .rinvui marker cannot make a developer-only test Screen ship.
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/InventoryScreenAdapter.h"
#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <filesystem>
#include <fstream>
#include <string>
namespace renegade::bridge {
inline constexpr const char* InventoryScreenAssignmentPath=
    "Content/Gameplay/Inventory.renegade-ui";
struct InventoryScreenAssignment {
    StableId screenId;
    std::string screenPath;
    bool paged=false;
};
inline bool ReadInventoryScreenAssignment(
        const std::string& projectRoot,const StableId& expectedProjectId,
        InventoryScreenAssignment& result,std::string& error) {
    namespace fs=std::filesystem;
    result={};
    std::error_code ec;
    const auto root=fs::weakly_canonical(fs::u8path(projectRoot),ec);
    if(ec || !fs::is_directory(root) || !IsValidStableId(expectedProjectId)) {
        error="Inventory Screen assignment requires a valid project identity.";
        return false;
    }
    const auto assignment=root/fs::u8path(InventoryScreenAssignmentPath);
    if(!fs::is_regular_file(assignment,ec) || ec ||
       fs::file_size(assignment,ec)>4096 || ec || fs::is_symlink(assignment,ec)) {
        error="Inventory Screen assignment is missing, oversized or unsafe.";
        return false;
    }
    std::ifstream input(assignment,std::ios::binary);
    if(!input) {error="Cannot read inventory Screen assignment.";return false;}
    std::string header,id,path,mode,extra;
    auto line=[&](std::string& s){
        if(!std::getline(input,s))return false;
        if(!s.empty()&&s.back()=='\r')s.pop_back();
        return true;
    };
    if(!line(header)||!line(id)||!line(path)||!line(mode)||
       line(extra)||header!="RENEGADE_INVENTORY_SCREEN 1" ||
       !IsValidStableId(id) || path.size()>240 ||
       path.rfind("Content/UI/",0)!=0 ||
       path.find('\\')!=std::string::npos ||
       path.find("..")!=std::string::npos ||
       fs::u8path(path).extension()!=".renegade-screen" ||
       (mode!="grid" && mode!="paged")) {
        error="Inventory Screen assignment has an invalid schema or path.";
        return false;
    }
    const auto candidate=fs::weakly_canonical(root/fs::u8path(path),ec);
    if(ec || !fs::is_regular_file(candidate,ec) || ec ||
       fs::is_symlink(candidate,ec)) {
        error="Assigned inventory Screen is missing or unsafe.";
        return false;
    }
    const auto relative=fs::relative(candidate,root,ec);
    if(ec || relative.empty() || relative.is_absolute() ||
       relative.generic_u8string()!=path) {
        error="Assigned inventory Screen escapes its project.";
        return false;
    }
    ScreenDocument doc;
    if(!ReadScreenDocument(candidate.generic_u8string(),expectedProjectId,
                           doc,error))
        return false;
    if(doc.envelope.documentId!=id || doc.usage!=ScreenUsage::Overlay ||
       doc.enabledInGame) {
        error="Assigned inventory Screen identity or Overlay usage is invalid.";
        return false;
    }
    bool hasActions=false;
    for(const auto& widget:doc.widgets) {
        if(widget.kind!=ScreenWidgetKind::Button)continue;
        std::size_t panel=0,row=0;
        std::uint32_t index=0;
        if(ParseInventoryScreenSlotAction(widget.actionId,panel,row) ||
           ParseInventoryPagedGridSlotAction(widget.actionId,panel,index)) {
            hasActions=true;break;
        }
    }
    if(!hasActions) {
        error="Assigned inventory Screen has no inventory slots.";
        return false;
    }
    result={id,path,mode=="paged"};
    error.clear();
    return true;
}

// Studio's creator-facing assignment writer. Uses the shared journaled project
// transaction owner rather than silently overwriting a developer's Screen.
inline bool SetInventoryScreenAssignment(
        const std::string& projectRoot,const StableId& expectedProjectId,
        const StableId& screenId,const std::string& screenPath,
        std::string& error) {
    namespace fs=std::filesystem;
    if(!IsValidStableId(expectedProjectId)) {
        error="Select a valid project before changing its inventory Screen.";
        return false;
    }
    std::error_code ec;
    const auto root=fs::weakly_canonical(fs::u8path(projectRoot),ec);
    if(ec || !fs::is_directory(root)) {
        error="Project root cannot be resolved.";
        return false;
    }
    if(screenId.empty()!=screenPath.empty()) {
        error="Inventory Screen identity and path must be selected together.";
        return false;
    }
    const auto assignment=root/fs::u8path(InventoryScreenAssignmentPath);
    const bool removing=screenId.empty();
    if(removing && !fs::exists(assignment,ec) && !ec) {
        error.clear();
        return true;
    }
    std::string data;
    if(!removing) {
        if(!IsValidStableId(screenId) || screenPath.size()>240 ||
           screenPath.rfind("Content/UI/",0)!=0 ||
           screenPath.find("..")!=std::string::npos ||
           screenPath.find('\\')!=std::string::npos ||
           fs::u8path(screenPath).extension()!=".renegade-screen") {
            error="Choose an existing project Inventory Screen, not an external file.";
            return false;
        }
        const auto candidate=fs::weakly_canonical(root/fs::u8path(screenPath),ec);
        if(ec || !fs::is_regular_file(candidate,ec) || ec ||
           fs::is_symlink(candidate,ec)) {
            error="Chosen Screen file is missing or unsafe.";
            return false;
        }
        const auto relative=fs::relative(candidate,root,ec);
        if(ec || relative.generic_u8string()!=screenPath) {
            error="Chosen Screen escapes the project root.";
            return false;
        }
        ScreenDocument doc;
        if(!ReadScreenDocument(candidate.generic_u8string(),
            expectedProjectId,doc,error))return false;
        if(doc.envelope.documentId!=screenId ||
           doc.usage!=ScreenUsage::Overlay || doc.enabledInGame) {
            error="Choose an editable, disabled Overlay Screen.";
            return false;
        }
        bool standard=false,paged=false;
        for(const auto& widget:doc.widgets) {
            if(widget.kind!=ScreenWidgetKind::Button)continue;
            std::size_t panel=0,row=0;
            std::uint32_t slot=0;
            standard|=ParseInventoryScreenSlotAction(widget.actionId,panel,row);
            paged|=ParseInventoryPagedGridSlotAction(widget.actionId,panel,slot);
        }
        if(!standard && !paged) {
            error="Chosen Overlay has no inventory slot actions.";
            return false;
        }
        data="RENEGADE_INVENTORY_SCREEN 1\n"+screenId+"\n"+
            screenPath+"\n"+(paged?"paged":"grid")+"\n";
    }
    ProjectDocumentWrite write;
    write.destinationPath=assignment.generic_u8string();
    write.remove=removing;
    write.content.assign(data.begin(),data.end());
    if(!removing)write.validator=[data](const std::string& staged,std::string& validationError) {
        std::ifstream input(fs::u8path(staged),std::ios::binary);
        if(!input) {validationError="Could not read staged inventory assignment.";return false;}
        const std::string bytes((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
        if(bytes!=data) {validationError="Staged inventory assignment is incomplete.";return false;}
        validationError.clear();return true;
    };
    ProjectDocumentTransactionOptions options;
    options.allowedRoot=root.generic_u8string();
    options.journalDirectory=(root/"Saved/Transactions").generic_u8string();
    const auto result=ProjectDocumentTransaction{}.Execute({std::move(write)},options);
    if(!result.success) {
        error="Could not save inventory Screen setting: "+result.message;
        return false;
    }
    error.clear();
    return true;
}
}
