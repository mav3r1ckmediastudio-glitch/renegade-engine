#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/bridge/InventoryPagedScreenAdapter.h"
#include <filesystem>
#include "renegade/bridge/StoryFlowScreenReferenceService.h"
#include <algorithm>
#include "renegade/bridge/ProjectUiScaffoldService.h"
#include <iostream>
#include <cstdlib>
using namespace renegade::bridge;
namespace fs=std::filesystem;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<" line "<<__LINE__<<": "<<error<<"\n";std::abort();}}while(false)
int main() {
    const auto project=GenerateStableId();
    const auto dir=fs::temp_directory_path()/("inventory-author-"+project);
    fs::create_directories(dir);
    const auto file=dir/"Grid.renegade-screen";
    std::string error;
    ScreenDocument starter;
    CHECK(MakeProjectUiScaffoldTemplate(project,"Inventory",starter,error));
    InventoryGridLayout starterLayout;
    CHECK(ReadInventoryPagedGridLayout(starter,starterLayout,error));
    CHECK(starterLayout.columns==5&&starterLayout.visibleRows==4&&starter.usage==ScreenUsage::Overlay);
    InventoryGridLayout original;original.visibleRows=4;
    auto screen=MakeInventoryPagedGridScreen(project,"Content/UI/Grid.renegade-screen",original);
    // Legacy screen without tracks or local_only flags remains editable and routable locally.
    screen.widgets.erase(std::remove_if(screen.widgets.begin(),screen.widgets.end(),[](const auto& w){
        return w.name.rfind("Paged Scroll Track ",0)==0||w.name.rfind("Paged Scroll Thumb ",0)==0;
    }),screen.widgets.end());
    for(auto& w:screen.widgets)w.localOnly=false;
    screen.widgets[3].style.borderWidth=3;
    screen.widgets[0].rect.x=67; // Unrelated authored geometry must survive resize.
    CHECK(WriteScreenDocument(file.generic_u8string(),screen,error));
    ScreenAuthoringSession editor;
    CHECK(editor.Open(file.generic_u8string(),project,error));
    const auto stable=editor.Document().widgets[3].id;
    const auto initialSize=editor.Document().widgets.size();
    InventoryGridLayout smaller=original;
    smaller.columns=4;smaller.visibleRows=3;
    CHECK(editor.ResizePagedInventoryGrid(smaller,error));
    CHECK(editor.Document().focusOrder.size()==28);
    CHECK(editor.Document().widgets.size()<initialSize);
    CHECK(editor.Document().widgets[3].id==stable);
    CHECK(editor.Document().widgets[3].style.borderWidth==3);
    CHECK(editor.Document().widgets[0].rect.x==67);
    InventoryGridLayout inferred;
    CHECK(ReadInventoryPagedGridLayout(editor.Document(),inferred,error));
    CHECK(inferred.columns==4&&inferred.visibleRows==3);
    for(const auto& w:editor.Document().widgets)
        if(IsInventoryPagedLocalAction(w.actionId))CHECK(w.localOnly);
    CHECK(editor.ApplyPagedInventoryAppearance(stable,error));
    for(const auto& w:editor.Document().widgets)
        if(w.name.rfind("Paged Slot ",0)==0)CHECK(w.style.borderWidth==3);
    CHECK(editor.Undo(error)); // Style is one history transaction.

    CHECK(editor.Undo(error));
    CHECK(editor.Document().widgets.size()==initialSize);
    CHECK(editor.Redo(error));
    CHECK(editor.Document().focusOrder.size()==28);
    InventoryGridLayout bigger=smaller;
    bigger.columns=5;bigger.visibleRows=5;
    bigger.slotWidth=62;bigger.slotHeight=62;
    CHECK(editor.ResizePagedInventoryGrid(bigger,error));
    CHECK(editor.Document().focusOrder.size()==54);
    CHECK(editor.Save(error));
    ScreenAuthoringSession reopened;
    CHECK(reopened.Open(file.generic_u8string(),project,error));
    CHECK(reopened.Document().focusOrder.size()==54);
    const auto contained=FitScreenImage({0,0,100,100},200,100,ScreenImageFit::Contain);
    CHECK(contained.width==100&&contained.height==50&&contained.y==25);
    const auto covered=FitScreenImage({0,0,100,100},200,100,ScreenImageFit::Cover);
    CHECK(covered.width==200&&covered.x==-50&&covered.height==100);
    for(const auto& w:reopened.Document().widgets)
        if(w.name.rfind("Paged Icon ",0)==0)CHECK(w.imageFit==ScreenImageFit::Contain);
    CHECK(ReadInventoryPagedGridLayout(reopened.Document(),inferred,error));
    CHECK(inferred.columns==5&&inferred.visibleRows==5&&inferred.slotWidth==62);
    auto malformed=reopened.Document();
    malformed.widgets[3].rect.x+=1;
    CHECK(!ReadInventoryPagedGridLayout(malformed,inferred,error));
    CHECK(IsInventoryPagedLocalAction("inventory.paged.0.slot.0"));
    CHECK(IsInventoryPagedLocalAction("inventory.paged.1.scroll.down"));
    for(const char* key:{"inventory.paged.quit","inventory.paged.2.slot.0",
        "inventory.paged.0.slot.01","inventory.paged.0.scroll.sideways","new_game"})
        CHECK(!IsInventoryPagedLocalAction(key));
    bigger.slotHeight=72;bigger.slotWidth=72;
    const auto unchanged=reopened.Revision();
    CHECK(!reopened.ResizePagedInventoryGrid(bigger,error));
    CHECK(reopened.Revision()==unchanged);
    auto six=original;six.columns=6;six.visibleRows=6;
    const auto rejectedRevision=reopened.Revision(),rejectedUndo=reopened.UndoCount();
    CHECK(!reopened.ResizePagedInventoryGrid(six,error));
    CHECK(error.find("does not fit")!=std::string::npos);
    CHECK(reopened.Revision()==rejectedRevision&&reopened.UndoCount()==rejectedUndo);
    CHECK(FitInventoryPagedGridSlots(six,error));
    CHECK(six.columns==6&&six.visibleRows==6&&six.slotWidth==51&&six.slotHeight==51);
    CHECK(reopened.ResizePagedInventoryGrid(six,error));
    CHECK(reopened.Document().focusOrder.size()==76&&reopened.Document().widgets.size()==232);
    CHECK(reopened.Save(error));
    ScreenAuthoringSession sixReopened;
    CHECK(sixReopened.Open(file.generic_u8string(),project,error));
    CHECK(ReadInventoryPagedGridLayout(sixReopened.Document(),inferred,error));
    CHECK(inferred.columns==6&&inferred.visibleRows==6);
    CHECK(sixReopened.Document().actions.size()==76);
    auto oversized=six;oversized.visibleRows=7;
    const auto beforeInvalid=sixReopened.Revision();
    CHECK(!sixReopened.ResizePagedInventoryGrid(oversized,error));
    CHECK(error.find("40 visible slots")!=std::string::npos);
    CHECK(sixReopened.Revision()==beforeInvalid);
    CHECK(reopened.Undo(error));
    CHECK(reopened.Document().focusOrder.size()==54);
    // Artwork replacement is independent of Screen behaviour and one bulk Undo.
    const auto beforeArt=sixReopened.Document();
    const auto artUndo=sixReopened.UndoCount();
    const auto slotId=std::find_if(beforeArt.widgets.begin(),beforeArt.widgets.end(),[](const auto& w){
        return w.actionId=="inventory.paged.0.slot.0";
    })->id;
    CHECK(sixReopened.ChangeWidgetGraphic(slotId,"Content/UI/Images/test-slot.png",true,error));
    CHECK(sixReopened.UndoCount()==artUndo+1);
    std::size_t slotCount=0;
    for(std::size_t i=0;i<beforeArt.widgets.size();++i) {
        const auto& before=beforeArt.widgets[i];const auto& after=sixReopened.Document().widgets[i];
        CHECK(before.id==after.id&&before.rect.x==after.rect.x&&before.rect.width==after.rect.width);
        CHECK(before.actionId==after.actionId&&before.localOnly==after.localOnly);
        CHECK(before.text==after.text&&before.visible==after.visible&&before.enabled==after.enabled);
        std::size_t p;std::uint32_t cell;
        if(before.kind==ScreenWidgetKind::Button&&ParseInventoryPagedGridSlotAction(before.actionId,p,cell)) {
            ++slotCount;
            for(const auto* state:{&after.style.normal,&after.style.hover,&after.style.pressed,
                                  &after.style.focused,&after.style.disabled})
                CHECK(state->imageResourcePath=="Content/UI/Images/test-slot.png");
            CHECK(before.style.borderWidth==after.style.borderWidth);
        } else CHECK(before.style.normal.imageResourcePath==after.style.normal.imageResourcePath);
    }
    CHECK(slotCount==72);
    CHECK(sixReopened.Undo(error));
    CHECK(sixReopened.FindWidget(slotId)->style.normal.imageResourcePath==
          std::find_if(beforeArt.widgets.begin(),beforeArt.widgets.end(),[&](const auto& w){return w.id==slotId;})->style.normal.imageResourcePath);
    CHECK(sixReopened.Redo(error));
    const auto backgroundId=beforeArt.widgets.front().id;
    const auto beforeBad=sixReopened.Revision();
    CHECK(!sixReopened.ChangeWidgetGraphic(backgroundId,"Content/UI/Images/test.png",true,error));
    CHECK(!sixReopened.ChangeWidgetGraphic(slotId,"../outside.png",false,error));
    CHECK(sixReopened.Revision()==beforeBad);
    CHECK(sixReopened.ChangeWidgetGraphic(backgroundId,"Content/UI/Images/background.jpg",false,error));
    CHECK(sixReopened.FindWidget(backgroundId)->resourcePath=="Content/UI/Images/background.jpg");
    const auto textId=beforeArt.widgets[1].id;
    CHECK(sixReopened.ChangeWidgetGraphic(textId,"Content/UI/Images/title.png",false,error));
    CHECK(sixReopened.FindWidget(textId)->text==beforeArt.widgets[1].text);
    CHECK(sixReopened.Save(error));
    ScreenAuthoringSession artReopened;
    CHECK(artReopened.Open(file.generic_u8string(),project,error));
    CHECK(artReopened.FindWidget(slotId)->style.disabled.imageResourcePath=="Content/UI/Images/test-slot.png");
    CHECK(artReopened.FindWidget(backgroundId)->resourcePath=="Content/UI/Images/background.jpg");
    auto five=six;five.columns=5;five.visibleRows=5;
    CHECK(artReopened.ResizePagedInventoryGrid(five,error));
    CHECK(artReopened.ResizePagedInventoryGrid(six,error));
    for(const auto& w:artReopened.Document().widgets) {
        std::size_t p;std::uint32_t cell;
        if(w.kind==ScreenWidgetKind::Button&&ParseInventoryPagedGridSlotAction(w.actionId,p,cell))
            CHECK(w.style.normal.imageResourcePath=="Content/UI/Images/test-slot.png");
    }
    std::error_code ec;fs::remove_all(dir,ec);
    std::cout<<"Inventory atomic resize / Undo / Redo / save PASS\n";
    return 0;
}
