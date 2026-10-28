#include "renegade/bridge/InventoryScreenAdapter.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
using namespace renegade::bridge;
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<"\n";std::abort();}}while(false)
int main() {
    const auto project=GenerateStableId();
    const auto path="Content/UI/InventoryStarter.renegade-screen";
    const auto authored=MakeInventoryStarterScreen(project,path,3);
    std::string error;
    CHECK(authored.usage==ScreenUsage::Overlay && !authored.enabledInGame);
    CHECK(authored.focusOrder.size()==6 && authored.actions.size()==6);
    CHECK(ValidateScreenDocument(authored,project,error));
    const fs::path root=fs::temp_directory_path()/("inv-g4b2-screen-"+GenerateStableId());
    fs::create_directories(root/"Content"/"UI");
    struct Cleanup {fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{root};
    const auto file=root/fs::u8path(path);
    CHECK(WriteScreenDocument(file.generic_u8string(),authored,error));
    ScreenDocument reopened;
    CHECK(ReadScreenDocument(file.generic_u8string(),project,reopened,error));
    CHECK(reopened.focusOrder==authored.focusOrder);
    CHECK(reopened.widgets.size()==authored.widgets.size());
    CHECK(!reopened.widgets[3].localOnly);
    CHECK(reopened.widgets[3].actionId=="inventory.slot.0.0");
    auto* editable=&reopened.widgets[3]; // an authored ordinary Button
    CHECK(editable->kind==ScreenWidgetKind::Button);
    editable->style.normal.background={78,63,51,220};
    const auto originalArt=editable->style.normal.background;
    InventoryService inventory;
    CHECK(inventory.RegisterItem({"potion","Health Potion",10,0.2,{}})==InventoryResult::Success);
    CHECK(inventory.CreateContainer("player",12,5)==InventoryResult::Success);
    CHECK(inventory.CreateContainer("chest",12,5)==InventoryResult::Success);
    CHECK(inventory.Add("player","potion",3)==InventoryResult::Success);
    CHECK(inventory.Add("chest","potion",2)==InventoryResult::Success);
    const auto view=[&](){return BuildInventoryOverlayView(
        inventory,{{"player","Player Backpack"},{"chest","Storage Chest"}},3);};
    const auto first=view();
    ScreenDocument rendered;
    auto enabledPreview=reopened;
    enabledPreview.enabledInGame=true; // Runtime transient opt-in; original stays disabled.
    CHECK(ProjectInventoryIntoScreen(enabledPreview,first,rendered,error));
    CHECK(rendered.enabledInGame);
    CHECK(rendered.widgets[3].text=="Health Potion  x3");
    CHECK(rendered.widgets[3].enabled);
    CHECK(rendered.widgets[3].style.normal.background.red==originalArt.red);
    CHECK(!reopened.widgets[3].enabled && reopened.widgets[3].text=="Empty slot");
    CHECK(!rendered.widgets[4].enabled);
    InventoryGameplayGateway scope(inventory);
    InventorySelectionController selection;
    const auto origin=inventory.SerializeSnapshot();
    CHECK(ExecuteInventoryScreenSlotAction(rendered.widgets[3].actionId,
        first,selection,scope)==InventoryResult::PermissionDenied);
    CHECK(inventory.SerializeSnapshot()==origin);
    scope.Grant("player",{true,true,true});
    scope.Grant("chest",{true,true,true});
    CHECK(ExecuteInventoryScreenSlotAction(rendered.widgets[3].actionId,
        first,selection,scope)==InventoryResult::Success);
    const auto changed=view();
    CHECK(changed.panels[0].rows[0].quantity==2);
    CHECK(changed.panels[1].rows[0].quantity==3);
    CHECK(ProjectInventoryIntoScreen(enabledPreview,changed,rendered,error));
    CHECK(rendered.widgets[3].text=="Health Potion  x2");
    CHECK(rendered.widgets[3].style.normal.background.red==originalArt.red);
    CHECK(ExecuteInventoryScreenSlotAction("inventory.slot.0.999",
        changed,selection,scope)==InventoryResult::InvalidArgument);
    CHECK(ExecuteInventoryScreenSlotAction("inventory.slot.0.1",
        changed,selection,scope)==InventoryResult::InvalidArgument);
    CHECK(ExecuteInventoryScreenSlotAction("inventory.slot.0.0junk",
        changed,selection,scope)==InventoryResult::InvalidArgument);
    CHECK(ExecuteInventoryScreenSlotAction("inventory.slot.1.0",
        changed,selection,scope)==InventoryResult::Success);
    InventoryOverlayView absent;
    CHECK(ProjectInventoryIntoScreen(reopened,absent,rendered,error));
    CHECK(!rendered.widgets[3].enabled);
    CHECK(!rendered.widgets[7].enabled);
    CHECK(ValidateScreenDocument(rendered,project,error));
    CHECK(reopened.widgets[3].style.normal.background.red==originalArt.red);
    // Bound view must not persist into the authored asset on disk.
    ScreenDocument persisted;
    CHECK(ReadScreenDocument(file.generic_u8string(),project,persisted,error));
    CHECK(!persisted.widgets[3].enabled && persisted.widgets[3].text=="Empty slot");
    // G4-B3B new authored visual layout, separate from the proven text UI.
    const auto grid=MakeInventoryIconGridScreen(
        project,"Content/UI/InventoryGraphicProof.renegade-screen");
    CHECK(grid.focusOrder.size()==16 && grid.actions.size()==16);
    CHECK(grid.widgets.size()==54); // 21 original widgets + 16 icons + 16 counts + 1 hover
    CHECK(!grid.enabledInGame && grid.usage==ScreenUsage::Overlay);
    if(!ValidateScreenDocument(grid,project,error)) {
        std::cerr << "Graphical Screen validation: " << error << "\n";
        CHECK(false);
    }
    const auto gridFile=root/"Content"/"UI"/"InventoryGraphicProof.renegade-screen";
    CHECK(WriteScreenDocument(gridFile.generic_u8string(),grid,error));
    ScreenDocument gridReopened;
    CHECK(ReadScreenDocument(gridFile.generic_u8string(),project,gridReopened,error));
    CHECK(gridReopened.focusOrder==grid.focusOrder);
    ScreenDocument graphical;
    CHECK(ProjectInventoryIntoScreen(gridReopened,changed,graphical,error));
    std::size_t icons=0,visibleIcons=0,quantityBadges=0,visibleBadges=0;
    for(const auto& widget:graphical.widgets) {
        if(widget.name.rfind("Inventory Slot Count ",0)==0) {
            ++quantityBadges;
            CHECK(widget.kind==ScreenWidgetKind::Text && !widget.enabled);
            if(widget.visible) {
                ++visibleBadges;
                CHECK(widget.text=="x2" || widget.text=="x3");
            } else CHECK(widget.text=="x0");
            continue;
        }
        if(widget.name.rfind("Inventory Slot Icon ",0)!=0)continue;
        ++icons;
        if(widget.visible) {
            ++visibleIcons;
            CHECK(widget.resourcePath=="Content/UI/InventoryIcons/potion.png");
            CHECK(!widget.enabled);
        } else CHECK(widget.resourcePath=="Content/UI/InventoryIcons/unknown.png");
    }
    CHECK(icons==16 && visibleIcons==2);
    CHECK(quantityBadges==16 && visibleBadges==2);
    CHECK(graphical.widgets[3].actionId=="inventory.slot.0.0");
    CHECK(graphical.widgets[3].rect.width==103);
    CHECK(graphical.widgets[3].text=="Health Potion");
    CHECK(graphical.widgets[4].text=="Empty slot");
    CHECK(graphical.widgets[3].style.normal.foreground.alpha==0);
    CHECK(InventoryStarterIconForDefinition("health-potion")==
          "Content/UI/InventoryIcons/potion.png");
    CHECK(InventoryStarterIconForDefinition("other")==
          "Content/UI/InventoryIcons/unknown.png");
    CHECK(InventoryStarterIconForDefinition("../invalid")==
          "Content/UI/InventoryIcons/unknown.png");
    auto healthPotionView=changed;
    healthPotionView.panels[0].rows[0].definitionId="health-potion";
    CHECK(ProjectInventoryIntoScreen(gridReopened,healthPotionView,graphical,error));
    for(const auto& widget:graphical.widgets)
        if(widget.name=="Inventory Slot Icon 0:0")
            CHECK(widget.resourcePath=="Content/UI/InventoryIcons/potion.png");
    CHECK(ProjectInventoryIntoScreen(gridReopened,absent,graphical,error));
    for(const auto& widget:graphical.widgets)
        if(widget.name.rfind("Inventory Slot Icon ",0)==0 ||
           widget.name.rfind("Inventory Slot Count ",0)==0)
            CHECK(!widget.visible);
    CHECK(graphical.widgets[3].text=="Empty slot");
    CHECK(gridReopened.widgets[3].text=="Empty slot");
    CHECK(!gridReopened.widgets[3].enabled);
    std::cout<<"G4B3B standard graphical grid and icon projection PASS\n";
}