#pragma once
#include "renegade/bridge/ScreenService.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <filesystem>
namespace renegade::bridge
{
    inline ScreenWidgetStyle ResolveThemeStyle(ScreenWidgetStyle local,
        const ScreenWidgetStyle& source,std::uint32_t overrides)
    {
        ScreenVisualState* targets[]={&local.normal,&local.hover,&local.pressed,&local.focused,&local.disabled};
        const ScreenVisualState* sources[]={&source.normal,&source.hover,&source.pressed,&source.focused,&source.disabled};
        for(int i=0;i<5;++i)
        {
            if(!(overrides&1)){targets[i]->background=sources[i]->background;
                targets[i]->foreground=sources[i]->foreground;targets[i]->imageTint=sources[i]->imageTint;}
            if(!(overrides&8))targets[i]->imageResourcePath=sources[i]->imageResourcePath;
        }
        if(!(overrides&2)){local.borderColor=source.borderColor;local.borderWidth=source.borderWidth;local.cornerRadius=source.cornerRadius;}
        if(!(overrides&4))local.text=source.text;
        if(!(overrides&16))local.opacity=source.opacity;
        return local;
    }
    // Resolve once when loading/reloading an instance, never by scanning in a frame update.
    // Source references stay project-owned; sources cannot refer back into an active chain.
    inline bool ResolveScreenTheme(const std::string& root,const ScreenDocument& authored,
        ScreenDocument& resolved,std::string& error)
    {
        std::map<StableId,ScreenDocument> cache;std::set<StableId> active;
        std::function<bool(const ScreenDocument&,ScreenDocument&,unsigned)> resolve;
        resolve=[&](const ScreenDocument& doc,ScreenDocument& out,unsigned depth)
        {
            if(depth>16||!active.insert(doc.envelope.documentId).second)
            {error="UI source cycle or nesting limit exceeded.";return false;}
            auto next=doc;
            if(doc.theme)
            {
                ScreenDocument source;const auto cached=cache.find(doc.theme->documentId);
                if(cached!=cache.end())source=cached->second;
                else
                {
                    std::string path;ScreenDocument raw;
                    if(!ResolveRuntimeScreenDocumentPath(root,doc.envelope.projectId,doc.theme->documentId,
                        doc.theme->pathHint,path,error)||!ReadScreenDocument(path,doc.envelope.projectId,raw,error)||
                        !resolve(raw,source,depth+1))return false;
                    cache.emplace(source.envelope.documentId,source);
                }
                for(auto& widget:next.widgets)
                {
                    if(widget.themeOverrides==63)continue;
                    const auto style=std::find_if(source.widgets.begin(),source.widgets.end(),
                        [&](const auto& value){return value.kind==widget.kind;});
                    if(style==source.widgets.end())
                    {error="Theme source has no style exemplar for "+std::string(ScreenWidgetKindName(widget.kind))+".";return false;}
                    if(!(widget.themeOverrides&8)&&widget.kind==ScreenWidgetKind::Image)widget.resourcePath=style->resourcePath;
                    widget.style=ResolveThemeStyle(widget.style,style->style,widget.themeOverrides);
                    if(!(widget.themeOverrides&32))widget.animations=style->animations;
                }
            }
            for(auto& widget:next.widgets)
            {
                if(!widget.component)continue;
                const auto link=*widget.component;ScreenDocument source;
                const auto cached=cache.find(link.documentId);
                if(cached!=cache.end())source=cached->second;
                else
                {
                    std::string path;ScreenDocument raw;
                    if(!ResolveRuntimeScreenDocumentPath(root,doc.envelope.projectId,link.documentId,
                        link.pathHint,path,error)||!ReadScreenDocument(path,doc.envelope.projectId,raw,error)||
                        !resolve(raw,source,depth+1))return false;
                    cache.emplace(source.envelope.documentId,source);
                }
                const auto exemplar=std::find_if(source.widgets.begin(),source.widgets.end(),
                    [&](const auto& value){return value.id==link.widgetId;});
                if(exemplar==source.widgets.end())
                {error="Linked component source widget was removed: "+link.widgetId;return false;}
                if(exemplar->kind!=widget.kind)
                {error="Linked component source kind changed; detach or relink explicitly.";return false;}
                // Each linked child must still have the same authored source
                // parent. Do this in the shared resolver so Studio preview,
                // Runtime and packaged dependency checks fail consistently,
                // even before the creator explicitly presses Refresh.
                if(!link.treeRootId.empty()&&widget.id!=link.treeRootId)
                {
                    const auto localParent=std::find_if(next.widgets.begin(),next.widgets.end(),
                        [&](const auto& value){return value.id==widget.parentId;});
                    if(exemplar->parentId.empty()||localParent==next.widgets.end()||
                        !localParent->component||
                        localParent->component->treeRootId!=link.treeRootId||
                        localParent->component->documentId!=link.documentId||
                        localParent->component->widgetId!=exemplar->parentId)
                    {error="Linked UI hierarchy changed at source; refresh, detach or reimport explicitly.";return false;}
                }
                widget.style=ResolveThemeStyle(widget.style,exemplar->style,link.overrides);
                if(!(link.overrides&8)&&widget.kind==ScreenWidgetKind::Image)widget.resourcePath=exemplar->resourcePath;
                if(!(link.overrides&32))widget.animations=exemplar->animations;
                if(!(link.overrides&64))widget.text=exemplar->text;
                if(!(link.overrides&128)&&IsScreenValueControl(widget.kind))widget.control=exemplar->control;
                if(!(link.overrides&256))widget.bindings=exemplar->bindings;
                if(!(link.overrides&512)){widget.fill=exemplar->fill;widget.fillDirection=exemplar->fillDirection;}
            }
            active.erase(doc.envelope.documentId);
            if(!ValidateScreenDocument(next,next.envelope.projectId,error))return false;
            out=std::move(next);return true;
        };
        return resolve(authored,resolved,0);
    }
}
