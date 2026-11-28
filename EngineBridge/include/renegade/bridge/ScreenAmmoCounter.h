#pragma once
#include "renegade/bridge/ScreenAuthoringSession.h"
namespace renegade::bridge
{
    // Ordinary editable layers; no renderer special case or separate ammo owner.
    inline ScreenDocument MakeAmmoCounterPrefab(const StableId& projectId)
    {
        ScreenDocument doc;doc.usage=ScreenUsage::Overlay;
        doc.envelope=CreateDocumentEnvelope(projectId,RuntimeScreenDocumentType,
            "Content/UI/Prefab.renegade-screen","ammo-counter-v1");
        ScreenWidget root;root.id=GenerateStableId();root.name="Ammo counter";
        root.kind=ScreenWidgetKind::Image;root.rect={0,0,340,128};root.enabled=false;
        root.style=MakeScreenWidgetStyleTemplate(root.kind,root.rect.height);
        root.style.borderColor={0,0,0,0};root.style.borderWidth=0;root.style.cornerRadius=0;
        for(auto* v:{&root.style.normal,&root.style.hover,&root.style.pressed,&root.style.focused,&root.style.disabled})
            v->background={0,0,0,0};
        root.container.padding=0;
        ScreenBinding visibility;visibility.source.attributeId="ammo.available";
        visibility.property=ScreenBindingProperty::Visible;visibility.required=false;
        visibility.missingPolicy=ScreenBindingMissingPolicy::Hide;root.bindings={visibility};
        doc.widgets.push_back(root);
        const auto text=[&](const char* name,const char* fallback,const char* attribute,
            const char* prefix,ScreenRect rect,float size,ScreenColor colour)
        {
            ScreenWidget w;w.id=GenerateStableId();w.parentId=root.id;w.name=name;
            w.kind=ScreenWidgetKind::Text;w.text=fallback;w.rect=rect;w.enabled=false;
            w.style=MakeScreenWidgetStyleTemplate(w.kind,rect.height);w.style.text.fontSize=size;
            for(auto* v:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})
                v->foreground=colour;
            ScreenBinding b;b.source.attributeId=attribute;b.prefix=prefix;b.required=false;
            b.missingPolicy=ScreenBindingMissingPolicy::Hide;w.bindings={b};
            doc.widgets.push_back(std::move(w));
        };
        text("Weapon name","EQUIPPED WEAPON","equipment.name","",{16,10,250,26},18,{230,230,230,255});
        text("Loaded rounds","AMMO","ammo.loaded","AMMO: ",{16,39,270,45},34,{255,255,255,255});
        text("Magazine capacity","CAPACITY","ammo.capacity","CAPACITY: ",{16,93,230,24},16,{180,190,200,255});
        text("Weapon slot","SLOT","equipment.slot","SLOT ",{264,94,65,22},14,{232,88,10,255});
        return doc;
    }
    // Explicit stock creation binds the current player. Importing an arbitrary
    // portable package still requires the creator to assign gameplay ownership.
    inline bool InsertAmmoCounterPrefab(ScreenAuthoringSession& target,float x,float y,
        std::vector<StableId>& created,std::string& error)
    {
        auto doc=MakeAmmoCounterPrefab(target.ProjectId());
        ScreenWidgetClipboard clipboard;clipboard.projectId=target.ProjectId();
        clipboard.widgets=std::move(doc.widgets);
        return target.PasteWidgets(clipboard,x,y,created,error);
    }
}
