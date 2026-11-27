#pragma once
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>

namespace renegade::bridge
{
    // Legacy templates store a full-canvas root image named Background.
    inline bool IsScreenBackground(const ScreenDocument& screen,const ScreenWidget& w)
    {
        if(w.kind!=ScreenWidgetKind::Image || !w.parentId.empty()) return false;
        if(w.backgroundPanel) return true;
        if(w.name!="Background") return false;
        const auto& a=w.anchors;
        return w.layoutMode==ScreenLayoutMode::Anchored ?
            a.minimumX==0 && a.minimumY==0 && a.maximumX==1 && a.maximumY==1 &&
            a.offsetMinimumX==0 && a.offsetMinimumY==0 && a.offsetMaximumX==0 && a.offsetMaximumY==0 :
            w.rect.x==0 && w.rect.y==0 && w.rect.width==screen.designWidth && w.rect.height==screen.designHeight;
    }

    inline bool CopyScreenBackgrounds(const std::string& root,const ScreenDocument& source,
        const ScreenWidget& background,const std::map<StableId,std::string>& destinations,std::string& error)
    {
        namespace fs=std::filesystem;
        if(!IsScreenBackground(source,background) || destinations.empty())
        {error="Select a background panel and at least one destination.";return false;}
        std::error_code ec;
        const auto staging=fs::temp_directory_path(ec)/fs::u8path(GenerateStableId());
        if(ec || !fs::create_directory(staging,ec)){error="Could not stage the screen backgrounds.";return false;}
        struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{staging};
        std::vector<ProjectDocumentWrite> writes;
        std::map<std::string,std::vector<uint8_t>> originals;
        auto read=[](const std::string& path,std::vector<uint8_t>& bytes){
            std::ifstream in(fs::u8path(path),std::ios::binary);
            if(!in)return false;
            bytes.assign(std::istreambuf_iterator<char>(in),{});
            return !in.bad() && !bytes.empty();
        };
        ScreenDocument appearance;
        if (!ResolveScreenTheme(root,source,appearance,error)) return false;
        const auto graphic = std::find_if(appearance.widgets.begin(),appearance.widgets.end(),
            [&](const auto& widget){return widget.id==background.id;});
        if (graphic==appearance.widgets.end()){error="The source background no longer exists.";return false;}
        for(const auto& [id,hint]:destinations)
        {
            std::string path;
            if(id==source.envelope.documentId){error="The current screen cannot be a destination.";return false;}
            if(!ResolveRuntimeScreenDocumentPath(root,source.envelope.projectId,id,hint,path,error))return false;
            ScreenDocument target;
            if(!read(path,originals[path]) || !ReadScreenDocument(path,source.envelope.projectId,target,error) ||
                target.envelope.documentId!=id){error="Could not read the selected destination screen.";return false;}
            auto found=std::find_if(target.widgets.begin(),target.widgets.end(),
                [&](const auto& w){return IsScreenBackground(target,w);});
            ScreenWidget panel;
            if(found!=target.widgets.end()){panel=*found;target.widgets.erase(found);}
            else {panel.id=GenerateStableId();panel.name="Background";panel.kind=ScreenWidgetKind::Image;panel.enabled=false;}
            panel.backgroundPanel=true;
            panel.cameraSource=background.cameraSource;
            panel.layoutMode=ScreenLayoutMode::Anchored;panel.anchors={0,0,1,1,0,0,0,0};
            panel.rect={0,0,target.designWidth,target.designHeight};
            panel.resourcePath=background.resourcePath;panel.imageFit=background.imageFit;
            panel.style=graphic->style;
            panel.animations=graphic->animations;
            panel.themeOverrides=63;
            target.schemaVersion=ScreenDocument::CurrentSchemaVersion;
            target.widgets.insert(target.widgets.begin(),std::move(panel));
            const auto staged=(staging/fs::u8path(id+".renegade-screen")).generic_u8string();
            if(!PreserveScreenUpgradeOriginal(path,target,error) || !WriteScreenDocument(staged,target,error))return false;
            ProjectDocumentWrite write;write.destinationPath=path;
            if(!read(staged,write.content)){error="Could not read the staged screen.";return false;}
            writes.push_back(std::move(write));
        }
        for(const auto& [path,before]:originals)
        {
            std::vector<uint8_t> current;
            if(!read(path,current) || current!=before){error="A destination changed. Reopen the list and try again.";return false;}
        }
        ProjectDocumentTransactionOptions options;options.allowedRoot=root;
        options.journalDirectory=(fs::u8path(root)/"Saved/Transactions").generic_u8string();
        const auto result=ProjectDocumentTransaction().Execute(std::move(writes),options);
        error=result.message;return result.success;
    }
}
