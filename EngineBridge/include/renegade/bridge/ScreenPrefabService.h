#pragma once
#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/ScreenLibraryService.h"
#include "renegade/bridge/DependencyService.h"
#include <filesystem>
#include <set>
#include <map>
namespace renegade::bridge
{
    // Save one selected hierarchy as a self-contained portable Screen package.
    // Baking through CopyWidgets retains effective appearance without source links.
    inline bool PublishScreenWidgetPrefab(const std::string& root,const ScreenAuthoringSession& session,
        const StableId& selected,const std::string& destination,std::string& error)
    {
        namespace fs=std::filesystem;
        ScreenWidgetClipboard clipboard;
        if(!session.CopyWidgets({selected},clipboard,error))return false;
        ScreenDocument doc;doc.usage=ScreenUsage::Overlay;
        doc.envelope=CreateDocumentEnvelope(session.ProjectId(),RuntimeScreenDocumentType,
            "Content/UI/Prefab.renegade-screen","ui-prefab-v1");
        doc.widgets=std::move(clipboard.widgets);doc.focusOrder=std::move(clipboard.focusOrder);
        doc.designWidth=session.Document().designWidth;doc.designHeight=session.Document().designHeight;
        std::set<std::string> paths;
        const auto add=[&](const std::string& p){if(!p.empty()&&p.rfind("builtin:",0)!=0)paths.insert(p);};
        for(auto& w:doc.widgets)
        {
            if(w.parentId.empty()){w.rect.x=0;w.rect.y=0;}
            if(w.video||w.cameraSource||!w.hintListPath.empty())
            {error="Save image/text/control prefabs without video, cameras or hint-list files.";return false;}
            if(w.kind==ScreenWidgetKind::Button)doc.actions.push_back({w.actionId});
            add(w.resourcePath);add(w.style.text.fontResource);
            add(w.progressFrameResourcePath);add(w.progressFillResourcePath);
            for(const auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})add(v->imageResourcePath);
            if(w.carousel)for(const auto& slide:w.carousel->entries)add(slide.image);
            if(w.navigation)for(const auto* path:NavigationResources(*w.navigation))add(*path);
            if(w.hurt)for(const auto& f:HurtResources())add((*w.hurt).*(f.member));
            if(w.liquid){add(w.liquid->mask);add(w.liquid->increaseSound);add(w.liquid->decreaseSound);}
        }
        // Publish compact resource names so saving an imported prefab cannot
        // accumulate Imported/<id> directories or overflow Windows staging paths.
        std::map<std::string,std::string> resources;
        for(const auto& path:paths)
            resources.emplace(path,"Content/UI/Assets/"+std::to_string(resources.size())+fs::u8path(path).extension().generic_u8string());
        const auto remap=[&](std::string& path){const auto found=resources.find(path);if(found!=resources.end())path=found->second;};
        for(auto& w:doc.widgets)
        {
            remap(w.resourcePath);remap(w.style.text.fontResource);
            remap(w.progressFrameResourcePath);remap(w.progressFillResourcePath);
            for(auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})remap(v->imageResourcePath);
            if(w.carousel)for(auto& slide:w.carousel->entries)remap(slide.image);
            if(w.navigation)for(auto* path:NavigationResources(*w.navigation))remap(*path);
            if(w.hurt)for(const auto& f:HurtResources())remap((*w.hurt).*(f.member));
            if(w.liquid){remap(w.liquid->mask);remap(w.liquid->increaseSound);remap(w.liquid->decreaseSound);}
        }
        if(!ValidateScreenDocument(doc,session.ProjectId(),error))return false;
        const auto stage=fs::temp_directory_path()/fs::u8path("RenegadeUiPrefab-"+GenerateStableId());
        struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{stage};
        try
        {
            fs::create_directories(stage/"Content/UI");
            for(const auto& path:paths)
            {
                const auto source=ResolveDependencyPath(root,path);
                if(!source.accepted||!source.exists){error="Prefab resource is unavailable: "+path;return false;}
                const auto output=stage/fs::u8path(resources.at(path));fs::create_directories(output.parent_path());
                fs::copy_file(fs::u8path(source.absolutePath),output);
            }
            if(!WriteScreenDocument((stage/fs::u8path(doc.envelope.pathHint)).generic_u8string(),doc,error))return false;
            return PublishPortableScreenPackage(stage.generic_u8string(),session.ProjectId(),doc.envelope.documentId,
                doc.envelope.pathHint,destination,error);
        }
        catch(const std::exception& e){error=std::string("Cannot save UI prefab: ")+e.what();return false;}
    }
    inline bool InsertScreenWidgetPrefab(const std::string& root,ScreenAuthoringSession& target,
        const std::string& package,float x,float y,std::vector<StableId>& created,std::string& error)
    {
        ScreenPortableAdoptionResult adopted;
        if(!AdoptPortableScreenPackage(root,target.ProjectId(),package,adopted,error))return false;
        ScreenAuthoringSession source;
        if(!source.Open((std::filesystem::u8path(root)/std::filesystem::u8path(adopted.pathHint)).generic_u8string(),target.ProjectId(),error))return false;
        std::vector<StableId> roots;
        for(const auto& w:source.Document().widgets)if(w.parentId.empty())roots.push_back(w.id);
        if(roots.size()!=1){error="Choose a saved UI prefab with one root hierarchy.";return false;}
        ScreenWidgetClipboard clipboard;
        if(!source.CopyWidgets(roots,clipboard,error))return false;
        return target.PasteWidgets(clipboard,x,y,created,error);
    }
}
