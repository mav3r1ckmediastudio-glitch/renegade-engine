#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include "renegade/bridge/InventoryScreenAdapter.h"
#include <charconv>
#include <string_view>
#include <utility>
#include <algorithm>
#include <cmath>
#include <array>

namespace renegade::bridge {
namespace {
ScreenWidget Widget(ScreenWidgetKind kind,const std::string& name,
                    ScreenRect rect,const std::string& text) {
    ScreenWidget w;
    w.id=GenerateStableId();w.kind=kind;w.name=name;w.rect=rect;w.text=text;
    w.style=MakeScreenWidgetStyleTemplate(kind,rect.height);
    return w;
}
std::string SlotKey(std::size_t panel,std::uint32_t cell) {
    return "inventory.paged."+std::to_string(panel)+".slot."+std::to_string(cell);
}
std::string ScrollKey(std::size_t panel,int direction) {
    return "inventory.paged."+std::to_string(panel)+
           (direction>0?".scroll.down":".scroll.up");
}
bool ParseKey(const std::string& value,const std::string_view middle,
              std::size_t& panel,std::string_view& suffix) noexcept {
    constexpr std::string_view prefix="inventory.paged.";
    const std::string_view s(value);
    if(s.size()<=prefix.size() || s.substr(0,prefix.size())!=prefix)
        return false;
    const auto rest=s.substr(prefix.size());
    if(rest.size()<2 || rest[0]<'0'||rest[0]>'1' ||
       rest.substr(1,middle.size())!=middle)return false;
    panel=static_cast<std::size_t>(rest[0]-'0');
    suffix=rest.substr(1+middle.size());
    return true;
}
bool DecodeSlotName(const std::string& name,const std::string_view prefix,
                    std::size_t& panel,std::uint32_t& index) noexcept {
    const std::string_view s(name);
    if(s.substr(0,prefix.size())!=prefix)return false;
    const auto rest=s.substr(prefix.size());
    if(rest.size()<3||rest[0]<'0'||rest[0]>'1'||rest[1]!=':')return false;
    const auto num=rest.substr(2);
    if(num.size()>1 && num.front()=='0')return false;
    const char* first=num.data(),*last=first+num.size();
    std::uint32_t value=0;
    const auto parsed=std::from_chars(first,last,value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=last || value>=1024)return false;
    panel=static_cast<std::size_t>(rest[0]-'0');index=value;
    return true;
}
const InventoryGridCell* Cell(const InventoryGridPage& p,std::uint32_t index){
    if(index>=p.cells.size())return nullptr;
    return &p.cells[index];
}
}
bool ParseInventoryPagedGridSlotAction(const std::string& action,
        std::size_t& panel,std::uint32_t& index) noexcept {
    panel=0;index=0;
    std::string_view number;
    if(!ParseKey(action,".slot.",panel,number)||number.empty()||
       (number.size()>1 && number.front()=='0'))return false;
    const auto parsed=std::from_chars(
        number.data(),number.data()+number.size(),index);
    return parsed.ec==std::errc{} &&
           parsed.ptr==number.data()+number.size() && index<1024;
}
bool ParseInventoryPagedGridScrollAction(const std::string& action,
        std::size_t& panel,int& direction) noexcept {
    panel=0;direction=0;
    std::string_view suffix;
    if(!ParseKey(action,".scroll.",panel,suffix))return false;
    if(suffix=="up")direction=-1;
    else if(suffix=="down")direction=1;
    return direction!=0;
}
bool IsInventoryPagedLocalAction(const std::string& action) noexcept {
    std::size_t panel;std::uint32_t index;int direction;
    return ParseInventoryPagedGridSlotAction(action,panel,index) ||
           ParseInventoryPagedGridScrollAction(action,panel,direction);
}
bool ReadInventoryPagedGridLayout(const ScreenDocument& screen,
        InventoryGridLayout& layout,std::string& error) {
    std::array<std::array<const ScreenWidget*,InventoryPagedMaximumVisibleCells>,2> cells{};
    std::array<unsigned,2> counts{};
    for(const auto& w:screen.widgets) {
        std::size_t panel;std::uint32_t index;
        if(w.kind!=ScreenWidgetKind::Button ||
           !ParseInventoryPagedGridSlotAction(w.actionId,panel,index))continue;
        if(index>=InventoryPagedMaximumVisibleCells || cells[panel][index]) {
            error="Inventory has duplicate or unsupported slot actions.";return false;
        }
        cells[panel][index]=&w;++counts[panel];
    }
    if(!counts[0] || counts[0]!=counts[1]) {
        error="Inventory needs matching Backpack and Chest grids.";return false;
    }
    for(unsigned p=0;p<2;++p)
        for(unsigned i=0;i<counts[p];++i)
            if(!cells[p][i]) {error="Inventory slots must be consecutive.";return false;}
    const auto& first=cells[0][0]->rect;
    unsigned cols=0;
    while(cols<counts[0] && std::fabs(cells[0][cols]->rect.y-first.y)<0.5f)++cols;
    if(!cols || counts[0]%cols) {error="Inventory rows are incomplete.";return false;}
    InventoryGridLayout next;
    next.columns=cols;next.visibleRows=counts[0]/cols;
    next.slotWidth=first.width;next.slotHeight=first.height;
    next.squareSlots=std::fabs(first.width-first.height)<0.5f;
    next.horizontalGap=cols>1?cells[0][1]->rect.x-first.x-first.width:0;
    next.verticalGap=counts[0]>cols?cells[0][cols]->rect.y-first.y-first.height:0;
    for(unsigned p=0;p<2;++p) {
        const auto& origin=cells[p][0]->rect;
        for(unsigned i=0;i<counts[p];++i) {
            const auto& r=cells[p][i]->rect;
            if(std::fabs(r.width-first.width)>0.5f || std::fabs(r.height-first.height)>0.5f ||
               std::fabs(r.x-origin.x-(i%cols)*(first.width+next.horizontalGap))>0.5f ||
               std::fabs(r.y-origin.y-(i/cols)*(first.height+next.verticalGap))>0.5f) {
                error="Inventory slots need a regular paired grid. Undo individual geometry edits first.";return false;
            }
        }
    }
    for(const auto& w:screen.widgets) {
        if(w.name=="Paged Icon 0:0") {
            next.iconPadding=w.rect.x-first.x;
            next.iconFit=w.imageFit==ScreenImageFit::Contain?InventoryIconFit::Contain:
                w.imageFit==ScreenImageFit::Cover?InventoryIconFit::Cover:InventoryIconFit::Stretch;
        }
        if(w.name=="Paged Scroll Track 0")next.showScrollbar=w.visible;
    }
    if(!ValidateInventoryGridLayout(next,error))return false;
    layout=next;error.clear();return true;
}
bool FitInventoryPagedGridSlots(InventoryGridLayout& layout,std::string& error) {
    if(!ValidateInventoryGridLayout(layout,error))return false;
    if(layout.columns*layout.visibleRows>InventoryPagedMaximumVisibleCells) {
        error="This paired Screen supports up to 40 visible slots per panel. Reduce rows or columns.";return false;
    }
    auto next=layout;
    const float availableWidth=std::floor((452-(layout.columns-1)*layout.horizontalGap)/layout.columns);
    const float availableHeight=std::floor((337-(layout.visibleRows-1)*layout.verticalGap)/layout.visibleRows);
    next.slotWidth=std::min({layout.slotWidth,82.0f,availableWidth});
    next.slotHeight=std::min({layout.slotHeight,82.0f,availableHeight});
    if(next.squareSlots)next.slotWidth=next.slotHeight=std::min(next.slotWidth,next.slotHeight);
    if(next.slotWidth<42||next.slotHeight<42) {
        error="These rows or columns cannot fit at the minimum 42 px slot size. Reduce rows, columns or spacing.";return false;
    }
    if(!ValidateInventoryGridLayout(next,error))return false;
    if(next.slotHeight-2*next.iconPadding-11<1) {
        error="Reduce icon padding to leave room for icons and quantities.";return false;
    }
    layout=next;error.clear();return true;
}
ScreenDocument MakeInventoryPagedGridScreen(const StableId& projectId,
        const std::string& path,const InventoryGridLayout& config) {
    ScreenDocument doc;
    std::string error;
    if(!ValidateInventoryGridLayout(config,error) ||
       config.columns*config.visibleRows>InventoryPagedMaximumVisibleCells ||
       config.slotWidth>82 || config.slotHeight>82)return doc;
    doc.envelope=CreateDocumentEnvelope(projectId,
        RuntimeScreenDocumentType,path,"Inventory - editable paged grid");
    doc.usage=ScreenUsage::Overlay;doc.enabledInGame=false;
    doc.designWidth=1280;doc.designHeight=720;
    auto panel=Widget(ScreenWidgetKind::Image,"Inventory Paged Background",
        {80,56,1120,613},"");
    panel.enabled=false;panel.style.normal.background={12,19,29,243};
    doc.widgets.push_back(std::move(panel));
    doc.widgets.push_back(Widget(ScreenWidgetKind::Text,"Inventory Paged Heading",
        {120,82,700,50},"INVENTORY  /  PAGED GRID"));
    const float spacingX=config.slotWidth+config.horizontalGap;
    const float spacingY=config.slotHeight+config.verticalGap;
    for(std::size_t p=0;p<2;++p) {
        const float x=126.0f+static_cast<float>(p)*548.0f;
        auto heading=Widget(ScreenWidgetKind::Text,
            p==0?"Player Container Label":"Storage Container Label",
            {x,152,466,35},p==0?"PLAYER BACKPACK":"STORAGE CHEST");
        heading.style.text.fontSize=22;
        doc.widgets.push_back(std::move(heading));
        for(std::uint32_t cell=0;cell<config.columns*config.visibleRows;++cell){
            const float cx=x+(cell%config.columns)*spacingX;
            const float cy=209+(cell/config.columns)*spacingY;
            auto button=Widget(ScreenWidgetKind::Button,
                "Paged Slot "+std::to_string(p)+":"+std::to_string(cell),
                {cx,cy,config.slotWidth,config.slotHeight},"Empty slot");
            button.actionId=SlotKey(p,cell);
            button.enabled=false;button.localOnly=true;
            button.style.normal.background={29,44,57,244};
            button.style.hover.background={46,75,92,255};
            button.style.pressed.background={59,93,113,255};
            button.style.focused.background={55,92,114,255};
            button.style.disabled.background={21,32,43,230};
            button.style.borderColor={92,132,153,190};
            button.style.borderWidth=1;
            button.style.cornerRadius=6;
            for(auto* style:{&button.style.normal,&button.style.hover,
                    &button.style.pressed,&button.style.focused,
                    &button.style.disabled})
                style->foreground={255,255,255,0};
            doc.actions.push_back({button.actionId});
            doc.focusOrder.push_back(button.id);
            doc.widgets.push_back(std::move(button));
            const float inset=config.iconPadding;
            auto icon=Widget(ScreenWidgetKind::Image,
                "Paged Icon "+std::to_string(p)+":"+std::to_string(cell),
                {cx+inset,cy+inset,config.slotWidth-2*inset,
                    config.slotHeight-2*inset-11},"");
            icon.resourcePath=InventoryStarterIconForDefinition("unknown");
            icon.imageFit=config.iconFit==InventoryIconFit::Contain?ScreenImageFit::Contain:
                config.iconFit==InventoryIconFit::Cover?ScreenImageFit::Cover:ScreenImageFit::Stretch;
            icon.visible=false;icon.enabled=false;
            doc.widgets.push_back(std::move(icon));
            auto count=Widget(ScreenWidgetKind::Text,
                "Paged Count "+std::to_string(p)+":"+std::to_string(cell),
                {cx+config.slotWidth-35,cy+config.slotHeight-24,32,21},"x0");
            count.style.text.fontSize=14;
            count.style.text.horizontalAlignment=ScreenHorizontalAlignment::Right;
            count.style.normal.foreground={242,246,251,255};
            count.visible=false;count.enabled=false;
            doc.widgets.push_back(std::move(count));
        }
        for(const int dir:{-1,1}) {
            auto scroll=Widget(ScreenWidgetKind::Button,
                "Paged Scroll "+std::to_string(p)+(dir<0?" Up":" Down"),
                {x+468,209+(dir<0?0.0f:365.0f),32,31},
                dir<0?"UP":"DN");
            scroll.actionId=ScrollKey(p,dir);
            scroll.localOnly=true;
            scroll.style.text.fontSize=14;
            doc.actions.push_back({scroll.actionId});
            doc.focusOrder.push_back(scroll.id);
            doc.widgets.push_back(std::move(scroll));
        }
        // Each bar remains a pair of ordinary editable Screen Images.
        // Runtime only projects the thumb's fraction/height; no asset or
        // inventory logic is embedded in the authored Screen.
        auto track=Widget(ScreenWidgetKind::Image,
            "Paged Scroll Track "+std::to_string(p),
            {x+476,246,16,323},"");
        track.enabled=false;
        track.visible=config.showScrollbar;
        track.style.cornerRadius=7;
        track.style.normal.background={25,38,52,255};
        track.style.disabled.background=track.style.normal.background;
        track.style.borderColor={77,104,122,225};
        track.style.borderWidth=1;
        doc.widgets.push_back(std::move(track));
        auto thumb=Widget(ScreenWidgetKind::Image,
            "Paged Scroll Thumb "+std::to_string(p),
            {x+478,248,12,65},"");
        thumb.enabled=false;
        thumb.visible=config.showScrollbar;
        thumb.style.cornerRadius=5;
        thumb.style.normal.background={155,170,182,255};
        thumb.style.disabled.background=thumb.style.normal.background;
        thumb.style.borderColor={187,200,211,255};
        thumb.style.borderWidth=1;
        doc.widgets.push_back(std::move(thumb));
        auto label=Widget(ScreenWidgetKind::Text,
            "Paged Range "+std::to_string(p),
            {x,592,495,25},std::to_string(config.columns*config.visibleRows)+" VISIBLE SLOTS");
        label.style.text.fontSize=14;
        doc.widgets.push_back(std::move(label));
    }
    auto status=Widget(ScreenWidgetKind::Text,"Inventory Transfer Status",
        {126,635,1000,25},"CLICK/DRAG: 1  SHIFT+DRAG: STACK  UP/DN: PAGE  I: CLOSE");
    status.style.text.fontSize=15;
    doc.widgets.push_back(std::move(status));
    doc.widgets.push_back(Widget(ScreenWidgetKind::Text,
        "Inventory Hover Details",{126,548,990,32},
        "POINT AT AN ITEM FOR DETAILS"));
    return doc;
}
bool ProjectPagedInventoryIntoScreen(const ScreenDocument& authored,
        const InventoryGridPage& player,const InventoryGridPage& chest,
        ScreenDocument& projected,std::string& error) {
    if(player.containerId.empty() || chest.containerId.empty() ||
       player.containerId==chest.containerId ||
       player.revision!=chest.revision) {
        error="Paged Screen requires two different authorised containers at one revision.";
        return false;
    }
    ScreenDocument candidate=authored;
    for(auto& w:candidate.widgets) {
        std::size_t panel=0;std::uint32_t index=0;
        const auto& source=[&](std::size_t p)->const InventoryGridPage& {
            return p==0?player:chest;
        };
        if(w.kind==ScreenWidgetKind::Button &&
           ParseInventoryPagedGridSlotAction(w.actionId,panel,index)) {
            const auto* cell=Cell(source(panel),index);
            w.enabled=cell&&cell->Occupied();
            w.text=(cell&&cell->Occupied())?cell->definitionId:"Empty slot";
            continue;
        }
        if(w.kind==ScreenWidgetKind::Button) {
            int direction=0;
            if(ParseInventoryPagedGridScrollAction(w.actionId,panel,direction)) {
                const auto& page=source(panel);
                w.enabled=direction<0?page.viewport.topRow>0:
                    page.viewport.topRow<page.viewport.maximumTopRow;
                continue;
            }
        }
        if(w.kind==ScreenWidgetKind::Text &&
           DecodeSlotName(w.name,"Paged Count ",panel,index)) {
            const auto* cell=Cell(source(panel),index);
            w.visible=cell&&cell->Occupied();
            w.text=w.visible?"x"+std::to_string(cell->quantity):"x0";
            continue;
        }
        if(w.kind==ScreenWidgetKind::Image &&
           DecodeSlotName(w.name,"Paged Icon ",panel,index)) {
            const auto* cell=Cell(source(panel),index);
            w.visible=cell&&cell->Occupied();
            if(w.resourcePath.empty() || w.resourcePath==InventoryStarterIconForDefinition("unknown"))
                w.resourcePath=w.visible?InventoryStarterIconForDefinition(cell->definitionId):
                    InventoryStarterIconForDefinition("unknown");
            continue;
        }
        if(w.kind==ScreenWidgetKind::Image &&
           (w.name=="Paged Scroll Track 0" || w.name=="Paged Scroll Thumb 0" ||
            w.name=="Paged Scroll Track 1" || w.name=="Paged Scroll Thumb 1")) {
            panel=w.name.back()=='0'?0:1;
            w.visible=source(panel).viewport.scrollbarNeeded;
            continue;
        }
        if(w.kind==ScreenWidgetKind::Text && w.name=="Paged Range 0")panel=0;
        else if(w.kind==ScreenWidgetKind::Text && w.name=="Paged Range 1")panel=1;
        else continue;
        const auto& page=source(panel);
        const auto first=page.viewport.firstContainerSlot;
        const auto last=first+page.viewport.numberOfContainerSlotsVisible;
        w.text="SLOTS "+std::to_string(first+1)+"-"+
            std::to_string(last)+" / "+std::to_string(page.viewport.containerCapacity);
    }
    if(!ValidateScreenDocument(candidate,candidate.envelope.projectId,error))
        return false;
    projected=std::move(candidate);error.clear();return true;
}
} // namespace renegade::bridge
