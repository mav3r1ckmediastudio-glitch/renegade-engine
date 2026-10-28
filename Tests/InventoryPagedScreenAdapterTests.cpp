#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include "renegade/bridge/InventoryScreenAdapter.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
namespace fs=std::filesystem;
using namespace renegade::bridge;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" at "<<__LINE__<<"\n";std::abort();}}while(false)
int main(){
    InventoryGridLayout layout;
    layout.visibleRows=4;
    const auto project=GenerateStableId();
    const std::string asset="Content/UI/InventoryPaged.renegade-screen";
    const auto author=MakeInventoryPagedGridScreen(project,asset,layout);
    std::string error;
    CHECK(author.widgets.size()==136); // 2+ 2*(heading+60+2+track+thumb+range)+2
    CHECK(author.focusOrder.size()==44 && author.actions.size()==44);
    CHECK(ValidateScreenDocument(author,project,error));
    const auto root=fs::temp_directory_path()/("inv-paged-"+project);
    fs::create_directories(root/"Content/UI");
    struct Cleanup {fs::path dir;~Cleanup(){std::error_code ec;fs::remove_all(dir,ec);}} clear{root};
    const auto path=root/fs::u8path(asset);
    CHECK(WriteScreenDocument(path.generic_u8string(),author,error));
    ScreenDocument reopened;
    CHECK(ReadScreenDocument(path.generic_u8string(),project,reopened,error));
    CHECK(reopened.widgets.size()==author.widgets.size());
    CHECK(reopened.focusOrder==author.focusOrder);
    std::size_t p=9;std::uint32_t cell=99;
    CHECK(ParseInventoryPagedGridSlotAction("inventory.paged.1.slot.19",p,cell));
    CHECK(p==1&&cell==19);
    int dir=0;
    CHECK(ParseInventoryPagedGridScrollAction("inventory.paged.0.scroll.up",p,dir));
    CHECK(p==0&&dir==-1);
    for(const char* invalid:{"inventory.paged.2.slot.0","inventory.paged.1.slot.20x",
        "inventory.paged.0.slot.-1","inventory.paged.0.slot.01",
        "inventory.paged.0.slot.1024","inventory.slot.0.0"})
        CHECK(!ParseInventoryPagedGridSlotAction(invalid,p,cell));
    InventoryService s;
    CHECK(s.CreateContainer("player",100,0)==InventoryResult::Success);
    CHECK(s.CreateContainer("chest",100,0)==InventoryResult::Success);
    for(int i=0;i<100;++i){
        const auto name="scroll-"+std::to_string(i);
        CHECK(s.RegisterItem({name,name,10,0,{}})==InventoryResult::Success);
        CHECK(s.Add("player",name,1)==InventoryResult::Success);
    }
    InventoryGameplayGateway gateway(s);
    gateway.Grant("player",{true,true,true});
    gateway.Grant("chest",{true,true,true});
    InventoryGridPage player,chest;
    CHECK(ReadInventoryGridPage(gateway,"player",layout,0,player,error)==InventoryResult::Success);
    CHECK(ReadInventoryGridPage(gateway,"chest",layout,0,chest,error)==InventoryResult::Success);
    ScreenDocument shown;
    CHECK(ProjectPagedInventoryIntoScreen(reopened,player,chest,shown,error));
    CHECK(shown.widgets.size()==136);
    auto custom=reopened;
    for(auto& w:custom.widgets)if(w.name=="Paged Icon 0:0")w.resourcePath="Content/UI/custom.png";
    ScreenDocument customProjected;
    CHECK(ProjectPagedInventoryIntoScreen(custom,player,chest,customProjected,error));
    for(const auto& w:customProjected.widgets)
        if(w.name=="Paged Icon 0:0")CHECK(w.resourcePath=="Content/UI/custom.png");
    std::size_t occupied=0,images=0,buttons=0,scroll=0,bars=0;
    for(const auto& w:shown.widgets) {
        if(w.name.rfind("Paged Icon ",0)==0){++images;if(w.visible)++occupied;}
        if(w.name.rfind("Paged Slot ",0)==0){++buttons;}
        if(w.name.rfind("Paged Scroll Track ",0)==0 ||
           w.name.rfind("Paged Scroll Thumb ",0)==0) {
            ++bars;
            CHECK(w.visible); // 100 slots, 4 of 20 rows visible
            CHECK(w.kind==ScreenWidgetKind::Image && !w.enabled);
        }
        if(w.name.rfind("Paged Scroll ",0)==0 &&
           (w.name.find(" Up")!=std::string::npos ||
            w.name.find(" Down")!=std::string::npos))++scroll;
    }
    CHECK(images==40&&occupied==20&&buttons==40&&scroll==4&&bars==4);
    CHECK(!reopened.widgets[4].enabled); // Original button unbound
    CHECK(ReadInventoryGridPage(gateway,"player",layout,16,player,error)==InventoryResult::Success);
    CHECK(player.viewport.topRow==16 && player.cells[0].definitionId=="scroll-80");
    CHECK(ProjectPagedInventoryIntoScreen(reopened,player,chest,shown,error));
    bool found=false;
    for(const auto& w:shown.widgets)
        if(w.actionId=="inventory.paged.0.slot.0") {
            CHECK(w.enabled && w.text=="scroll-80");found=true;
        }
    CHECK(found);
    CHECK(ValidateScreenDocument(shown,project,error));
    // Reopen must still have original labels, ID and styling.
    ScreenDocument fresh;
    CHECK(ReadScreenDocument(path.generic_u8string(),project,fresh,error));
    CHECK(fresh.widgets.size()==reopened.widgets.size());
    CHECK(fresh.focusOrder==reopened.focusOrder);
    CHECK(!fresh.widgets[4].enabled);
    CHECK(fresh.widgets[4].style.normal.background.red==
          reopened.widgets[4].style.normal.background.red);
    // Alternative authored grid shapes must project their own visible
    // action sets without modifying inventory capacity or instance identity.
    for(const std::uint32_t side:{3u,5u}) {
        InventoryGridLayout variant=layout;
        variant.visibleRows=side;
        variant.columns=side==3?4:5;
        auto document=MakeInventoryPagedGridScreen(project,asset,variant);
        CHECK(ValidateScreenDocument(document,project,error));
        const auto visible=variant.visibleRows*variant.columns;
        CHECK(document.focusOrder.size()==2*visible+4);
        CHECK(ReadInventoryGridPage(gateway,"player",variant,100,player,error)==InventoryResult::Success);
        CHECK(ReadInventoryGridPage(gateway,"chest",variant,0,chest,error)==InventoryResult::Success);
        CHECK(player.viewport.topRow==player.viewport.maximumTopRow);
        CHECK(player.viewport.firstContainerSlot==player.viewport.topRow*variant.columns);
        CHECK(ProjectPagedInventoryIntoScreen(document,player,chest,shown,error));
        std::size_t slotButtons=0;
        for(const auto& widget:shown.widgets)
            if(widget.name.rfind("Paged Slot ",0)==0)++slotButtons;
        CHECK(slotButtons==2*visible);
    }
    std::cout<<"G4B3C3 editable 4x3 and 5x5 grid projection PASS\n";
}
