#include "renegade/bridge/ScreenLibraryService.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include <filesystem>
#include "json.hpp"
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
int main()
{
    int failures=0;std::string error;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<": "<<error<<'\n';}};
    const auto root=std::filesystem::temp_directory_path()/("renegade-ui-library-"+GenerateStableId());
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{root};
    std::filesystem::create_directories(root/"Content/UI");
    const auto project=GenerateStableId();
    ScreenDocument hud;hud.usage=ScreenUsage::Hud;
    hud.envelope=CreateDocumentEnvelope(project,RuntimeScreenDocumentType,"Content/UI/Player.renegade-screen","Library proof");
    const auto originalId=hud.envelope.documentId;
    check(WriteScreenDocument((root/hud.envelope.pathHint).generic_u8string(),hud,error),"HUD fixture write failed");
    auto disabled=hud;disabled.envelope.documentId=GenerateStableId();disabled.envelope.pathHint="Content/UI/Disabled.renegade-screen";
    disabled.enabledInGame=false;
    check(WriteScreenDocument((root/disabled.envelope.pathHint).generic_u8string(),disabled,error),"Disabled fixture write failed");
    auto catalogue=EnumerateProjectScreenLibrary(root.generic_u8string(),project);
    check(catalogue.succeeded && catalogue.screens.size()==2 && catalogue.diagnostics.empty(),"Project Screens not discovered without Flow nodes");
    bool editableDisabled=false;
    for(const auto& entry:catalogue.screens)if(entry.documentId==disabled.envelope.documentId)
        editableDisabled=entry.available && !entry.enabledInGame && entry.usage==ScreenUsage::Hud;
    check(editableDisabled,"Disabled Screen was removed from authoring access");
    const auto newHud=CreateProjectScreenLibraryDocument(root.generic_u8string(),project,ScreenUsage::Hud);
    const auto secondHud=CreateProjectScreenLibraryDocument(root.generic_u8string(),project,ScreenUsage::Hud);
    const auto newMenu=CreateProjectScreenLibraryDocument(root.generic_u8string(),project,ScreenUsage::Menu);
    const auto newOverlay=CreateProjectScreenLibraryDocument(root.generic_u8string(),project,ScreenUsage::Overlay);
    const auto newModal=CreateProjectScreenLibraryDocument(root.generic_u8string(),project,ScreenUsage::Modal);
    check(newHud.succeeded && secondHud.succeeded && newHud.documentId!=secondHud.documentId &&
        newHud.pathHint!=secondHud.pathHint,"Repeated HUD creation did not preserve unique IDs and files");
    error="Menu: "+newMenu.error+"; Overlay: "+newOverlay.error+"; Modal: "+newModal.error;
    check(newMenu.succeeded && newOverlay.succeeded && newModal.succeeded,
        "Standalone UI category creation failed");
    for(const auto& created : {newHud,secondHud,newMenu,newOverlay,newModal})
    {
        ScreenDocument reopened;
        const bool reopenedOk=created.succeeded &&
            ReadScreenDocument((root/created.pathHint).generic_u8string(),project,reopened,error) &&
            reopened.envelope.documentId==created.documentId;
        check(reopenedOk,"Created UI document failed save/reopen");
        if(!reopenedOk)continue;
        const bool needsControls=reopened.usage==ScreenUsage::Menu || reopened.usage==ScreenUsage::Modal;
        check(needsControls ? (reopened.actions.size()==1 && reopened.widgets.size()==1 &&
            reopened.widgets.front().kind==ScreenWidgetKind::Button &&
            reopened.widgets.front().actionId==reopened.actions.front().id &&
            reopened.focusOrder.size()==1 && reopened.focusOrder.front()==reopened.widgets.front().id)
            : (reopened.actions.empty() && reopened.focusOrder.empty()),
            "Standalone UI creation fabricated routing or omitted required controls");
    }
    ScreenDocument openedCreated;
    std::string resolvedPath,resolvedHint;
    check(ResolveProjectScreenLibraryDocument(root.generic_u8string(),project,
        newHud.documentId,newHud.pathHint,openedCreated,resolvedPath,resolvedHint,error) &&
        resolvedHint==newHud.pathHint && openedCreated.usage==ScreenUsage::Hud,
        "New HUD could not be opened through standalone library resolution");
    const auto renamedCreated=std::string("Content/UI/RenamedNewHUD.renegade-screen");
    std::filesystem::rename(root/newHud.pathHint,root/renamedCreated);
    check(ResolveProjectScreenLibraryDocument(root.generic_u8string(),project,
        newHud.documentId,newHud.pathHint,openedCreated,resolvedPath,resolvedHint,error) &&
        resolvedHint==renamedCreated && openedCreated.envelope.documentId==newHud.documentId,
        "Moved standalone HUD did not repair its stable-ID path hint");
    check(!CreateProjectScreenLibraryDocument({},project,ScreenUsage::Hud).succeeded &&
        !CreateProjectScreenLibraryDocument(root.generic_u8string(),"invalid",ScreenUsage::Hud).succeeded,
        "Standalone UI creation accepted invalid project identity");
    catalogue=EnumerateProjectScreenLibrary(root.generic_u8string(),project);
    check(catalogue.succeeded && catalogue.screens.size()==7,
        "Standalone UI documents were not added to the project library");

    std::filesystem::rename(root/hud.envelope.pathHint,root/"Content/UI/Renamed.renegade-screen");
    catalogue=EnumerateProjectScreenLibrary(root.generic_u8string(),project);
    bool renamed=false;for(const auto& entry:catalogue.screens)if(entry.documentId==originalId)
        renamed=entry.name=="Renamed" && entry.pathHint=="Content/UI/Renamed.renegade-screen" && entry.available;
    check(renamed,"Rename changed Screen identity or retained stale path authority");
    const auto package=root/"PortablePackage";
    check(PublishPortableScreenPackage(root.generic_u8string(),project,originalId,
        "Content/UI/Renamed.renegade-screen",package.generic_u8string(),error),"Portable publish failed");
    check(!PublishPortableScreenPackage(root.generic_u8string(),project,originalId,
        "Content/UI/Renamed.renegade-screen",package.generic_u8string(),error),"Existing package was overwritten");
    const auto destination=root/"OtherProject";std::filesystem::create_directories(destination/"Content/UI");
    const auto otherProject=GenerateStableId();ScreenPortableAdoptionResult adopted;
    check(AdoptPortableScreenPackage(destination.generic_u8string(),otherProject,package.generic_u8string(),adopted,error),
        "Portable project adoption failed");
    ScreenDocument adoptedDoc;
    check(adopted.documentId!=originalId&&ReadScreenDocument((destination/adopted.pathHint).generic_u8string(),
        otherProject,adoptedDoc,error)&&adoptedDoc.envelope.documentId==adopted.documentId,"Portable identity remap/reopen failed");
    const auto rollbackRoot=root/"RollbackProject";std::filesystem::create_directories(rollbackRoot/"Content/UI");
    ScreenPortableAdoptionResult rollbackResult;
    check(!AdoptPortableScreenPackage(rollbackRoot.generic_u8string(),GenerateStableId(),package.generic_u8string(),
        rollbackResult,error,[](ProjectDocumentTransactionStage stage,std::size_t,const std::string&,std::string& e){
            if(stage==ProjectDocumentTransactionStage::AfterReplace){e="Injected adoption failure";return ProjectDocumentTransactionHookAction::Fail;}
            return ProjectDocumentTransactionHookAction::Continue;
        }),"Injected transaction failure did not reject adoption");
    bool rollbackClean=true;
    for(const auto& file:std::filesystem::recursive_directory_iterator(rollbackRoot))
        if(file.is_regular_file())rollbackClean=false;
    check(rollbackClean&&rollbackResult.documentId.empty(),"Failed adoption retained partial project files/identity");
    const auto hidden=root/"PackageOffline";std::filesystem::rename(package,hidden);
    check(ReadScreenDocument((destination/adopted.pathHint).generic_u8string(),otherProject,adoptedDoc,error),
        "Adopted UI retained personal library dependence");
    std::filesystem::rename(hidden,package);
    {std::ofstream tamper(package/"Content/UI/Renamed.renegade-screen",std::ios::app);tamper<<"\\nTAMPER";}
    const auto countBefore=std::distance(std::filesystem::recursive_directory_iterator(destination),
        std::filesystem::recursive_directory_iterator{});
    ScreenPortableAdoptionResult rejected;
    check(!AdoptPortableScreenPackage(destination.generic_u8string(),otherProject,package.generic_u8string(),rejected,error)&&
        countBefore==std::distance(std::filesystem::recursive_directory_iterator(destination),
        std::filesystem::recursive_directory_iterator{}),"Tampered package partially adopted");

    auto sourceHud=hud;sourceHud.envelope=CreateDocumentEnvelope(project,RuntimeScreenDocumentType,
        "Content/UI/PortableSource.renegade-screen","Portable linked source");
    ScreenWidget exemplar;exemplar.id=GenerateStableId();exemplar.kind=ScreenWidgetKind::Text;
    exemplar.name="Imported Health";exemplar.text="UNASSIGNED";exemplar.rect={20,20,300,60};
    exemplar.style=MakeScreenWidgetStyleTemplate(exemplar.kind,60);
    exemplar.style.text.fontResource="Content/UI/PortableFont.ttf";
    exemplar.style.normal.imageResourcePath="Content/UI/PortableArt.png";
    ScreenBinding sourceBinding;sourceBinding.source.attributeId="health.current";
    exemplar.bindings={sourceBinding};sourceHud.widgets={exemplar};
    {std::ofstream font(root/"Content/UI/PortableFont.ttf",std::ios::binary);font<<"font fixture";}
    {std::ofstream art(root/"Content/UI/PortableArt.png",std::ios::binary);art<<"image fixture";}
    check(WriteScreenDocument((root/sourceHud.envelope.pathHint).generic_u8string(),sourceHud,error),"Portable source write");
    auto complexHud=sourceHud;complexHud.envelope=CreateDocumentEnvelope(project,RuntimeScreenDocumentType,
        "Content/UI/PortableLinked.renegade-screen","Portable linked entry");
    complexHud.widgets.front().id=GenerateStableId();
    complexHud.widgets.front().component=ScreenComponentReference{sourceHud.envelope.documentId,
        sourceHud.envelope.pathHint,exemplar.id,0,{}};
    check(WriteScreenDocument((root/complexHud.envelope.pathHint).generic_u8string(),complexHud,error),"Linked portable write");
    const auto complexPackage=root/"ComplexPackage";
    check(PublishPortableScreenPackage(root.generic_u8string(),project,complexHud.envelope.documentId,
        complexHud.envelope.pathHint,complexPackage.generic_u8string(),error),"Linked/resource package publish");
    const auto multiRollback=root/"MultiRollback";std::filesystem::create_directories(multiRollback/"Content/UI");
    check(!AdoptPortableScreenPackage(multiRollback.generic_u8string(),GenerateStableId(),complexPackage.generic_u8string(),
        rejected,error,[](ProjectDocumentTransactionStage stage,std::size_t index,const std::string&,std::string& e){
            if(stage==ProjectDocumentTransactionStage::AfterReplace&&index==1)
            {e="Injected second-file failure";return ProjectDocumentTransactionHookAction::Fail;}
            return ProjectDocumentTransactionHookAction::Continue;
        }),"Second-file failure did not roll back");
    bool multiClean=true;
    for(const auto& file:std::filesystem::recursive_directory_iterator(multiRollback))if(file.is_regular_file())multiClean=false;
    check(multiClean,"Multi-file adoption rollback left assets");
    const auto badPackage=root/"BadManifestPackage";
    std::filesystem::copy(complexPackage,badPackage,std::filesystem::copy_options::recursive);
    nlohmann::json manifest;{std::ifstream input(badPackage/"renegade-ui-package.json");input>>manifest;}
    const auto originalManifest=manifest;
    manifest["version"]=999;{std::ofstream output(badPackage/"renegade-ui-package.json");output<<manifest.dump(2);}
    check(!AdoptPortableScreenPackage(destination.generic_u8string(),otherProject,badPackage.generic_u8string(),rejected,error),
        "Future UI package version accepted");
    manifest=originalManifest;manifest["files"][0]["path"]="../outside.ttf";
    {std::ofstream output(badPackage/"renegade-ui-package.json");output<<manifest.dump(2);}
    check(!AdoptPortableScreenPackage(destination.generic_u8string(),otherProject,badPackage.generic_u8string(),rejected,error),
        "UI package traversal accepted");
    ScreenPortableAdoptionResult complexAdopted;
    check(AdoptPortableScreenPackage(destination.generic_u8string(),otherProject,complexPackage.generic_u8string(),
        complexAdopted,error)&&!complexAdopted.unresolvedBindings.empty(),"Linked/resource adoption");
    if(complexAdopted.documentId.empty())return 1;
    nlohmann::json receipt;{std::ifstream in(destination/complexAdopted.receiptPathHint);in>>receipt;}
    check(receipt["entry_id"]==complexAdopted.documentId&&receipt["gameplay_slots"].size()>=1&&
        receipt["gameplay_slots"][0]["original_attribute"]=="health.current"&&receipt["files"].size()==4,
        "Adoption provenance/gameplay slot receipt was not transactionally preserved");
    std::filesystem::rename(root/"Content",root/"OriginContentOffline");
    std::filesystem::rename(complexPackage,root/"ComplexPackageOffline");
    ScreenDocument adoptedLinked,effectiveLinked;
    check(ReadScreenDocument((destination/complexAdopted.pathHint).generic_u8string(),otherProject,adoptedLinked,error)&&
        ResolveScreenTheme(destination.generic_u8string(),adoptedLinked,effectiveLinked,error)&&
        adoptedLinked.widgets.front().id!=complexHud.widgets.front().id&&
        adoptedLinked.widgets.front().component->documentId!=sourceHud.envelope.documentId&&
        effectiveLinked.widgets.front().bindings.front().source.attributeId.rfind("library.unassigned.",0)==0&&
        std::filesystem::is_regular_file(destination/effectiveLinked.widgets.front().style.text.fontResource)&&
        std::filesystem::is_regular_file(destination/effectiveLinked.widgets.front().style.normal.imageResourcePath),
        "Offline linked/font/art identity and explicit gameplay-slot closure failed");
    std::filesystem::rename(root/"OriginContentOffline",root/"Content");
    std::filesystem::copy_file(root/"Content/UI/Renamed.renegade-screen",root/"Content/UI/Duplicate.renegade-screen");
    catalogue=EnumerateProjectScreenLibrary(root.generic_u8string(),project);
    int conflicts=0;for(const auto& entry:catalogue.screens)if(entry.documentId==originalId && !entry.available)++conflicts;
    check(conflicts==2 && catalogue.diagnostics.size()==2,"Duplicate identity was guessed or silently removed");
    std::ofstream corrupt(root/"Content/UI/Corrupt.renegade-screen");corrupt<<"not a Screen";corrupt.close();
    catalogue=EnumerateProjectScreenLibrary(root.generic_u8string(),project);
    bool brokenVisible=false;for(const auto& entry:catalogue.screens)if(entry.name=="Corrupt")brokenVisible=!entry.available && !entry.diagnostic.empty();
    check(brokenVisible,"Broken Screen disappeared without a repair diagnostic");
    check(!EnumerateProjectScreenLibrary({},project).succeeded &&
        !EnumerateProjectScreenLibrary(root.generic_u8string(),"invalid").succeeded,"Invalid library root/project accepted");
    if(failures)return 1;
    std::cout<<"PASS: standalone HUD discovery, disabled authoring, stable rename, duplicate and corrupt-file diagnostics\n";
}
