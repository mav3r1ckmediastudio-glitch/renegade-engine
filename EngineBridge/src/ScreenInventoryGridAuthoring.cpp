#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace renegade::bridge {
namespace {
bool IsGridChild(const std::string& name) {
    for(const char* prefix:{"Paged Slot ","Paged Icon ","Paged Count ",
                            "Paged Scroll "})
        if(name.rfind(prefix,0)==0)return true;
    return false;
}
bool IsPagedAction(const std::string& name) {
    return name.rfind("inventory.paged.",0)==0;
}
}
bool ScreenAuthoringSession::ChangeWidgetGraphic(const StableId& source,
    const std::string& resource,bool allInventorySlots,std::string& error) {
    if(!IsLoaded()){error="Open a Screen first.";return false;}
    const auto* original=FindWidget(source);
    if(!original){error="Select a Screen element first.";return false;}
    std::size_t panel;std::uint32_t cell;
    if(allInventorySlots && (original->kind!=ScreenWidgetKind::Button ||
        !ParseInventoryPagedGridSlotAction(original->actionId,panel,cell))) {
        error="Select an inventory slot to change all slot graphics.";return false;
    }
    auto next=Document();
    for(auto& w:next.widgets) {
        const bool target=allInventorySlots?
            w.kind==ScreenWidgetKind::Button&&ParseInventoryPagedGridSlotAction(w.actionId,panel,cell):
            w.id==source;
        if(!target)continue;
        if(w.kind==ScreenWidgetKind::Image)w.resourcePath=resource;
        for(auto* visual:{&w.style.normal,&w.style.hover,&w.style.pressed,
                         &w.style.focused,&w.style.disabled})
            visual->imageResourcePath=resource;
        w.themeOverrides|=8;
        if(w.component)w.component->overrides|=8;
    }
    return CommitMutation(std::move(next),error);
}
bool ScreenAuthoringSession::ApplyPagedInventoryAppearance(const StableId& source,std::string& error) {
    if(!IsLoaded()){error="Open an inventory Screen first.";return false;}
    const auto* original=FindWidget(source);
    if(!original){error="Select a slot, icon, quantity or scrollbar to copy its style.";return false;}
    std::string prefix;
    for(const char* candidate:{"Paged Slot ","Paged Icon ","Paged Count ","Paged Scroll Track ","Paged Scroll Thumb "})
        if(original->name.rfind(candidate,0)==0){prefix=candidate;break;}
    if(prefix.empty()){error="Select a slot, icon, quantity or scrollbar first; backgrounds are edited individually.";return false;}
    const auto style=original->style;const auto resource=original->resourcePath;
    auto next=Document();
    for(auto& w:next.widgets)if(w.kind==original->kind&&w.name.rfind(prefix,0)==0) {
        w.style=style;w.resourcePath=resource;w.imageFit=original->imageFit;w.themeOverrides=63;
        if(w.component)w.component->overrides|=63;
    }
    return CommitMutation(std::move(next),error);
}
bool ScreenAuthoringSession::ResizePagedInventoryGrid(
    const InventoryGridLayout& layout,std::string& error) {
    if(!IsLoaded()) {error="Open a Screen before editing its inventory.";return false;}
    const auto& old=Document();
    if(old.usage!=ScreenUsage::Overlay) {
        error="Inventory layout editing requires an Overlay Screen.";return false;
    }
    const auto has=[&](const std::string& name) {
        return std::any_of(old.widgets.begin(),old.widgets.end(),
            [&](const ScreenWidget& w){return w.name==name;});
    };
    if(!has("Paged Slot 0:0") || !has("Paged Slot 1:0")) {
        error="Select an editable paged inventory Screen.";return false;
    }
    InventoryGridLayout priorLayout;
    if(!ReadInventoryPagedGridLayout(old,priorLayout,error))return false;
    if(!ValidateInventoryGridLayout(layout,error))return false;
    if(layout.slotHeight-2*layout.iconPadding-11<1) {
        error="Reduce icon padding to leave room for the icon and quantity label.";return false;
    }
    if(layout.columns*layout.visibleRows>InventoryPagedMaximumVisibleCells) {
        error="This paired Screen supports up to 40 visible slots per panel. Reduce rows or columns.";return false;
    }
    const float gridWidth=layout.columns*layout.slotWidth+
        (layout.columns-1)*layout.horizontalGap;
    const float gridBottom=209+layout.visibleRows*layout.slotHeight+
        (layout.visibleRows-1)*layout.verticalGap;
    if(layout.slotWidth<42 || layout.slotHeight<42 ||
       layout.slotWidth>82 || layout.slotHeight>82 || gridWidth>452 || gridBottom>546) {
        error=std::to_string(layout.columns)+" columns x "+std::to_string(layout.visibleRows)+
            " does not fit inside the panels. Reduce slot size or spacing (slots must be 42-82 px).";
        return false;
    }
    // A regular authored grid may be translated by the creator.
    float offsetX[2]{},offsetY[2]{};
    for(const auto& w:old.widgets)for(int panel=0;panel<2;++panel)
        if(w.actionId=="inventory.paged."+std::to_string(panel)+".slot.0") {
            offsetX[panel]=w.rect.x-(126+548*panel);offsetY[panel]=w.rect.y-209;
        }
    auto templateDoc=MakeInventoryPagedGridScreen(
        ProjectId(),old.envelope.pathHint,layout);
    if(templateDoc.widgets.empty()) {
        error="Cannot generate the requested inventory grid.";return false;
    }
    ScreenDocument next=old;
    next.widgets.clear();
    next.actions.clear();
    next.focusOrder.clear();
    std::unordered_map<StableId,StableId> remap;
    std::unordered_set<std::string> ownedNames;
    for(auto& w:templateDoc.widgets) {
        const auto colon=w.name.find(':');
        if(colon!=std::string::npos && colon>0 && IsGridChild(w.name)) {
            const int panel=w.name[colon-1]-'0';
            if(panel>=0&&panel<2){w.rect.x+=offsetX[panel];w.rect.y+=offsetY[panel];}
        }
        ownedNames.insert(w.name);
        const auto prior=std::find_if(old.widgets.begin(),old.widgets.end(),
            [&](const ScreenWidget& value){
                return value.name==w.name && value.kind==w.kind;
            });
        if(prior!=old.widgets.end()) {
            const auto generated=w;
            remap[generated.id]=prior->id;
            w=*prior; // Retain native art, theme links, overrides and authored local interactions.
            if(w.name.rfind("Paged Range ",0)==0)w.text=generated.text;
            if(IsGridChild(w.name)) {
                if(w.name.rfind("Paged Scroll ",0)==0) {
                    if(w.kind==ScreenWidgetKind::Image)w.visible=layout.showScrollbar;
                } else {
                    w.rect=generated.rect;
                    if(w.name.rfind("Paged Icon ",0)==0)w.imageFit=generated.imageFit;
                }
                if(w.kind==ScreenWidgetKind::Button)w.localOnly=true;
            }
        } else {
            // Added cells adopt that panel's first matching element as their visual exemplar.
            const auto colon=w.name.find(':');
            if(colon!=std::string::npos) {
                const auto exemplar=w.name.substr(0,colon+1)+"0";
                const auto first=std::find_if(old.widgets.begin(),old.widgets.end(),
                    [&](const auto& value){return value.name==exemplar&&value.kind==w.kind;});
                if(first!=old.widgets.end()) {
                    w.style=first->style;w.resourcePath=first->resourcePath;
                    w.themeOverrides=first->themeOverrides;
                }
            }
        }
        next.widgets.push_back(std::move(w));
    }
    for(const auto& w:old.widgets)
        if(!IsGridChild(w.name) && !ownedNames.count(w.name))
            next.widgets.push_back(w);
    next.actions=templateDoc.actions;
    for(const auto& action:old.actions)
        if(!IsPagedAction(action.id))next.actions.push_back(action);
    for(auto id:templateDoc.focusOrder) {
        const auto mapped=remap.find(id);
        if(mapped!=remap.end())id=mapped->second;
        next.focusOrder.push_back(std::move(id));
    }
    for(const auto& id:old.focusOrder)
        if(std::find(next.focusOrder.begin(),next.focusOrder.end(),id)==
              next.focusOrder.end() &&
           std::any_of(next.widgets.begin(),next.widgets.end(),
               [&](const ScreenWidget& w){return w.id==id;}))
            next.focusOrder.push_back(id);
    return CommitMutation(std::move(next),error);
}
} // namespace renegade::bridge
