#include "renegade/bridge/InventoryScreenAdapter.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <utility>

namespace renegade::bridge {
namespace {
constexpr std::size_t MaxSlotsPerContainer=24;
const std::string prefix="inventory.slot.";
std::string SlotAction(std::size_t panel,std::size_t row) {
    return prefix+std::to_string(panel)+"."+std::to_string(row);
}
bool ParseSlot(const std::string& name,std::size_t& panel,std::size_t& row) {
    if(name.compare(0,prefix.size(),prefix)!=0)return false;
    const auto rest=name.substr(prefix.size());
    const auto dot=rest.find('.');
    if(dot==std::string::npos||dot==0||dot+1>=rest.size())return false;
    const auto lhs=rest.substr(0,dot),rhs=rest.substr(dot+1);
    for(char c:lhs)if(c<'0'||c>'9')return false;
    for(char c:rhs)if(c<'0'||c>'9')return false;
    if(lhs.size()!=1||lhs[0]<'0'||lhs[0]>'1'||rhs.size()>2)return false;
    if(rhs.size()>1&&rhs[0]=='0')return false;
    const auto n=static_cast<unsigned>(std::stoul(rhs));
    if(n>=MaxSlotsPerContainer)return false;
    panel=static_cast<std::size_t>(lhs[0]-'0');row=n;
    return true;
}
ScreenWidget ItemWidget(ScreenWidgetKind kind,const std::string& name,
                        ScreenRect bounds,std::string caption) {
    ScreenWidget widget;
    widget.id=GenerateStableId();
    widget.kind=kind;
    widget.name=name;
    widget.rect=bounds;
    widget.text=std::move(caption);
    widget.style=MakeScreenWidgetStyleTemplate(kind,bounds.height);
    return widget;
}
} // namespace

bool ParseInventoryScreenSlotAction(const std::string& id,
                                    std::size_t& panel,std::size_t& row) noexcept {
    return ParseSlot(id,panel,row);
}

std::string InventoryStarterIconForDefinition(const std::string& definitionId) {
    // Explicit mapping, not a path derived from creator-controlled IDs.
    // The live Inventory-Proof Lua fixture uses health-potion, while the
    // generic starter/test definition is named potion.
    const std::string asset=(definitionId=="health-potion"||
        definitionId=="potion")?"potion":
        (definitionId=="key"||definitionId=="coin")?
        definitionId:"unknown";
    return "Content/UI/InventoryIcons/"+asset+".png";
}

ScreenDocument MakeInventoryIconGridScreen(
    const StableId& projectId,const std::string& path) {
    auto doc=MakeInventoryStarterScreen(projectId,path,8);
    // Keep the document envelope supplied by the standard starter factory.
    // Retain stable slot action IDs, standard Button control surface and
    // authorable ScreenService schema. Only the initial presentation changes.
    doc.widgets[0].rect={89,67,1102,583};
    doc.widgets[0].style.normal.background={12,19,29,245};
    doc.widgets[1].rect={136,93,400,51};
    doc.widgets[1].style.text.fontSize=33;
    for(auto& widget:doc.widgets) {
        if(widget.name=="Player Container Label"||
           widget.name=="Storage Container Label") {
            const bool player=widget.name=="Player Container Label";
            widget.rect={player?140.0f:677.0f,167,448,39};
            widget.style.text.fontSize=21;
            continue;
        }
        if(widget.kind!=ScreenWidgetKind::Button ||
           widget.actionId.rfind("inventory.slot.",0)!=0)continue;
        std::size_t panel=0,index=0;
        if(!ParseSlot(widget.actionId,panel,index))continue;
        const float x=138+static_cast<float>(panel)*538+
                      static_cast<float>(index%4)*111;
        const float y=218+static_cast<float>(index/4)*137;
        widget.rect={x,y,103,122};
        widget.name="Inventory grid slot "+std::to_string(panel)+":"+
                    std::to_string(index);
        widget.style.normal.background={31,45,58,246};
        widget.style.hover.background={43,69,86,255};
        widget.style.pressed.background={62,97,116,255};
        widget.style.focused.background={56,90,112,255};
        widget.style.disabled.background={22,33,44,220};
        widget.style.borderColor={80,115,136,173};
        widget.style.borderWidth=1.5f;
        widget.style.cornerRadius=8;
        // Button is the semantic interactive slot. Its visual caption is
        // deliberately blank: a separate Text widget draws a properly
        // positioned quantity badge, preventing clipped baselines.
        widget.text="Empty slot";
        // Normal button caption must remain non-empty for valid Screen
        // action semantics. Hide just its visual glyphs; the adjacent
        // quantity badge and hover strip provide legible presentation.
        for(auto* visual:{&widget.style.normal,&widget.style.hover,
                &widget.style.pressed,&widget.style.focused,
                &widget.style.disabled})
            visual->foreground={255,255,255,0};
        widget.style.text.fontSize=13;
        widget.style.text.verticalAlignment=ScreenVerticalAlignment::Center;
        widget.style.text.horizontalAlignment=ScreenHorizontalAlignment::Center;
    }
    // Normal noninteractive Images sit on top of each action button. They
    // are ordinary editable Screen widgets, not a parallel slot renderer.
    for(std::size_t panel=0;panel<2;++panel)
        for(std::size_t row=0;row<8;++row) {
            const float x=138+static_cast<float>(panel)*538+
                          static_cast<float>(row%4)*111;
            const float y=218+static_cast<float>(row/4)*137;
            auto art=ItemWidget(ScreenWidgetKind::Image,
                "Inventory Slot Icon "+std::to_string(panel)+":"+
                   std::to_string(row),{x+18,y+9,67,75},"");
            art.resourcePath=InventoryStarterIconForDefinition("unknown");
            art.visible=false;art.enabled=false;
            doc.widgets.push_back(std::move(art));
        }
    // Quantity badges are independent, noninteractive, ordinary Text
    // widgets, so artwork and captions never consume click/drag gestures.
    for(std::size_t panel=0;panel<2;++panel)
        for(std::size_t row=0;row<8;++row) {
            const float x=138+static_cast<float>(panel)*538+
                          static_cast<float>(row%4)*111;
            const float y=218+static_cast<float>(row/4)*137;
            auto quantity=ItemWidget(ScreenWidgetKind::Text,
                "Inventory Slot Count "+std::to_string(panel)+":"+
                std::to_string(row),{x+56,y+90,40,25},"x0");
            quantity.style.text.fontSize=16;
            quantity.style.text.horizontalAlignment=
                ScreenHorizontalAlignment::Right;
            quantity.style.text.verticalAlignment=
                ScreenVerticalAlignment::Center;
            quantity.style.normal.foreground={237,246,250,255};
            quantity.visible=false;quantity.enabled=false;
            doc.widgets.push_back(std::move(quantity));
        }
    for(auto& widget:doc.widgets)
        if(widget.name=="Inventory Transfer Status") {
            widget.rect={139,615,988,28};
            widget.style.text.fontSize=15;
            widget.text="CLICK/DRAG: 1   SHIFT+DRAG: STACK   E: TRANSFER   I: CLOSE";
        }
    auto details=ItemWidget(ScreenWidgetKind::Text,
        "Inventory Hover Details",{139,528,990,57},
        "POINT AT AN ITEM FOR DETAILS");
    details.style.text.fontSize=17;
    doc.widgets.push_back(std::move(details));
    return doc;
}

ScreenDocument MakeInventoryStarterScreen(
    const StableId& projectId,const std::string& path,std::size_t rowsPerContainer) {
    ScreenDocument doc;
    doc.envelope=CreateDocumentEnvelope(projectId,RuntimeScreenDocumentType,path,
                                        "Inventory & Storage (editable starter)");
    doc.designWidth=1280;doc.designHeight=720;
    doc.usage=ScreenUsage::Overlay;
    doc.enabledInGame=false; // Author explicitly enables/links it.
    const auto count=std::clamp(rowsPerContainer,std::size_t{1},std::size_t{8});
    auto background=ItemWidget(ScreenWidgetKind::Image,"Inventory Panel",
        {130,76,1020,570},"");
    background.enabled=false;
    background.style.normal.background={10,16,26,244};
    doc.widgets.push_back(background);
    const auto title=ItemWidget(ScreenWidgetKind::Text,"Inventory Heading",
        {172,96,500,54},"INVENTORY");
    doc.widgets.push_back(title);
    for(std::size_t p=0;p<2;++p) {
        const float x=166+static_cast<float>(p)*490;
        doc.widgets.push_back(ItemWidget(ScreenWidgetKind::Text,
            p==0?"Player Container Label":"Storage Container Label",
            {x,169,440,48},p==0?"PLAYER BACKPACK":"STORAGE CHEST"));
        for(std::size_t row=0;row<count;++row) {
            auto button=ItemWidget(ScreenWidgetKind::Button,
                "Inventory slot "+std::to_string(p)+":"+std::to_string(row+1),
                {x,228+static_cast<float>(row)*43,440,38},"Empty slot");
            button.actionId=SlotAction(p,row);
            // This overlay has a dedicated inventory action sink; do not
            // suppress the normal Screen button activation callback.
            button.localOnly=false;
            button.enabled=false; // Enabled only by live scoped inventory.
            button.style.normal.background={31,45,60,240};
            button.style.focused.background={56,92,118,255};
            button.style.hover.background={49,75,95,255};
            button.style.pressed.background={64,111,139,255};
            button.style.text.fontSize=17;
            doc.actions.push_back({button.actionId});
            doc.focusOrder.push_back(button.id);
            doc.widgets.push_back(std::move(button));
        }
    }
    doc.widgets.push_back(ItemWidget(ScreenWidgetKind::Text,
        "Inventory Transfer Status",{172,585,938,40},
        "CLICK/DRAG: MOVE 1   SHIFT+DRAG: FULL STACK   E: SCRIPT   I: CLOSE"));
    return doc;
}

bool ProjectInventoryIntoScreen(
    const ScreenDocument& authored,const InventoryOverlayView& view,
    ScreenDocument& projected,std::string& error) {
    if(view.panels.size()>2){
        error="Inventory screen projection supports at most two containers.";
        return false;
    }
    ScreenDocument candidate=authored;
    for(auto& widget:candidate.widgets){
        std::size_t panel=0,row=0;
        if(widget.kind==ScreenWidgetKind::Text &&
           widget.name.rfind("Inventory Slot Count ",0)==0) {
            const auto suffix=widget.name.substr(21);
            if(suffix.size()!=3 || suffix[0]<'0'||suffix[0]>'1'||
               suffix[1]!=':'||suffix[2]<'0'||suffix[2]>'7')
                continue;
            panel=static_cast<std::size_t>(suffix[0]-'0');
            row=static_cast<std::size_t>(suffix[2]-'0');
            const bool occupied=panel<view.panels.size() &&
                row<view.panels[panel].rows.size() &&
                view.panels[panel].rows[row].quantity>0;
            widget.visible=occupied;
            widget.text=occupied?
                "x"+std::to_string(view.panels[panel].rows[row].quantity):"x0";
            continue;
        }
        if(widget.kind==ScreenWidgetKind::Image &&
           widget.name.rfind("Inventory Slot Icon ",0)==0) {
            const auto suffix=widget.name.substr(20);
            if(suffix.size()!=3 || suffix[0]<'0'||suffix[0]>'1'||
               suffix[1]!=':'||suffix[2]<'0'||suffix[2]>'7')
                continue;
            panel=static_cast<std::size_t>(suffix[0]-'0');
            row=static_cast<std::size_t>(suffix[2]-'0');
            const bool occupied=panel<view.panels.size() &&
                row<view.panels[panel].rows.size() &&
                view.panels[panel].rows[row].quantity>0;
            widget.visible=occupied;
            widget.resourcePath=occupied?
                InventoryStarterIconForDefinition(
                    view.panels[panel].rows[row].definitionId):
                InventoryStarterIconForDefinition("unknown");
            continue;
        }
        if(widget.kind!=ScreenWidgetKind::Button)continue;
        if(!ParseSlot(widget.actionId,panel,row))continue;
        if(panel>=view.panels.size()||row>=view.panels[panel].rows.size()){
            widget.text="Empty slot";
            widget.enabled=false;
            continue;
        }
        const auto& item=view.panels[panel].rows[row];
        const bool graphical=widget.name.rfind("Inventory grid slot ",0)==0;
        if(graphical) {
            // Full accessible item details remain in the hover strip;
            // the clickable tile's actual visible text is intentionally
            // empty because the independent quantity badge draws it.
            widget.text=item.itemName.empty()?"Item":item.itemName;
        } else
            widget.text=item.itemName+"  x"+std::to_string(item.quantity);
        if(widget.text.size()>100)widget.text.resize(100);
        widget.enabled=item.quantity>0&&!item.instanceId.empty();
    }
    if(!ValidateScreenDocument(candidate,candidate.envelope.projectId,error))
        return false;
    projected=std::move(candidate);error.clear();return true;
}

InventoryResult ExecuteInventoryScreenSlotAction(
    const std::string& actionId,const InventoryOverlayView& view,
    InventorySelectionController& selection,InventoryGameplayGateway& gateway) {
    std::size_t panel=0,row=0;
    if(!ParseSlot(actionId,panel,row)||panel>=view.panels.size()||
       row>=view.panels[panel].rows.size())
        return InventoryResult::InvalidArgument;
    if(!selection.Select(view,panel,row))return InventoryResult::NotFound;
    return selection.TransferSelected(view,gateway,1);
}
} // namespace renegade::bridge
