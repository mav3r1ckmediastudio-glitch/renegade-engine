#include "renegade/bridge/ScreenAuthoringSession.h"
#include "renegade/screen/ScreenRenderer.h"
#include "../Runtime/src/RuntimeScreen.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

using namespace renegade::bridge;
namespace fs = std::filesystem;
int main()
{
    int failures = 0;
    const auto check = [&](bool valid, const char* message)
    { if (!valid) { ++failures; std::cerr << message << '\n'; } };
    const auto approximatelyEqual = [](float a,float b) { return std::abs(a-b)<.001f; };
    const auto rect = [&](ScreenRect actual, ScreenRect expected, const char* message)
    {
        check(approximatelyEqual(actual.x,expected.x) && approximatelyEqual(actual.y,expected.y) &&
            approximatelyEqual(actual.width,expected.width) && approximatelyEqual(actual.height,expected.height),message);
    };
    rect(ResolveScreenFillRect({10,20,200,100},.25f,ScreenFillDirection::LeftToRight,10),
        {20,30,45,80},"Left-to-right fill geometry failed");
    rect(ResolveScreenFillRect({10,20,200,100},.25f,ScreenFillDirection::RightToLeft,10),
        {155,30,45,80},"Right-to-left fill geometry failed");
    rect(ResolveScreenFillRect({10,20,200,100},.25f,ScreenFillDirection::BottomToTop,10),
        {20,90,180,20},"Bottom-to-top fill geometry failed");
    rect(ResolveScreenFillRect({10,20,200,100},.25f,ScreenFillDirection::TopToBottom,10),
        {20,30,180,20},"Top-to-bottom fill geometry failed");

    const auto project = GenerateStableId();
    ScreenDocument document;
    document.envelope = CreateDocumentEnvelope(project,RuntimeScreenDocumentType,
        "Content/UI/Progress.renegade-screen","Progress integration test");
    document.usage = ScreenUsage::Hud;
    ScreenWidget widget;
    widget.id = GenerateStableId();
    widget.name = "Resource";
    widget.kind = ScreenWidgetKind::Progress;
    widget.rect = {100,100,400,40};
    widget.style = MakeScreenWidgetStyleTemplate(widget.kind,40);
    widget.style.borderWidth = 2;
    widget.fill = .25f;
    widget.progressFrameResourcePath = "Content/UI/Frame.png";
    widget.progressFillResourcePath = "Content/UI/Fill.png";
    document.widgets = {widget};
    std::string error;
    std::vector<renegade::screen::ScreenRenderItem> items;
    check(renegade::screen::BuildScreenRenderItems(document,1280,720,{},items,error),
        "Progress frame projection failed");
    if (items.size()==1)
        rect(items[0].fillRect,{102,102,99,36},"Frame evidence disagrees with drawn fill geometry");
    check(renegade::screen::BuildScreenRenderItemsInViewport(document,{200,100,640,360},{},items,error),
        "Preview progress projection failed");
    if (items.size()==1)
        rect(items[0].fillRect,{251,151,49.5f,18},"Preview fill did not follow viewport translation/scale");
    renegade::runtime::RuntimeScreenController runtime;
    check(runtime.Initialize(document,error) && !runtime.HasFocus(),
        "Passive Progress widget invented input/focus");

    const auto root = fs::temp_directory_path() / ("renegade-progress-" + GenerateStableId());
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code e; fs::remove_all(path,e); } } cleanup{root};
    fs::create_directories(root/"Content/UI");
    const auto path = root/"Content/UI/Progress.renegade-screen";
    check(WriteScreenDocument(path.generic_u8string(),document,error),"Progress save failed");
    ScreenAuthoringSession session;
    check(session.Open(path.generic_u8string(),project,error),"Progress authoring open failed");
    const auto revision = session.Revision();
    ScreenWidgetAuthoringEdit edit{widget.name,widget.text,{120,130,500,50},true,false};
    edit.fill = .75f;
    edit.fillDirection = ScreenFillDirection::BottomToTop;
    check(session.UpdateWidget(widget.id,edit,error) && session.UndoCount()==1 &&
        session.Revision()>revision,"Geometry and fill were not one atomic history command");
    check(session.Save(error),"Edited Progress save failed");
    ScreenDocument reopened;
    check(ReadScreenDocument(path.generic_u8string(),project,reopened,error),"Edited Progress reopen failed");
    if (reopened.widgets.size()==1)
        check(reopened.widgets[0].fill==.75f &&
            reopened.widgets[0].fillDirection==ScreenFillDirection::BottomToTop &&
            reopened.widgets[0].rect.width==500 &&
            reopened.widgets[0].progressFrameResourcePath=="Content/UI/Frame.png" &&
            reopened.widgets[0].progressFillResourcePath=="Content/UI/Fill.png",
            "Reopen lost Progress data/direction/geometry/artwork paths");
    check(session.Undo(error) && session.Document().widgets[0].fill==.25f &&
        session.Document().widgets[0].rect.x==100,"Progress Undo lost atomic geometry/data history");
    check(session.Redo(error) && session.Document().widgets[0].fill==.75f,"Progress Redo failed");
    const auto beforeArtwork = session.UndoCount();
    check(session.UpdateProgressArtwork(widget.id, "Content/UI/NewFrame.png",
        "Content/UI/NewFill.png", error) && session.UndoCount()==beforeArtwork+1,
        "Progress artwork change did not create a history entry");
    check(session.Undo(error) &&
        session.Document().widgets[0].progressFrameResourcePath=="Content/UI/Frame.png",
        "Progress artwork Undo failed");
    check(session.Redo(error) &&
        session.Document().widgets[0].progressFillResourcePath=="Content/UI/NewFill.png",
        "Progress artwork Redo failed");
    const auto invalidArtworkRevision = session.Revision();
    check(!session.UpdateProgressArtwork(widget.id, "../outside.png", "", error) &&
        session.Revision()==invalidArtworkRevision,
        "Progress artwork accepted a path outside Content");
    const auto beforeInvalid = session.Revision();
    check(!session.UpdateWidgetFill(widget.id,-.1f,ScreenFillDirection::LeftToRight,error) &&
        !session.UpdateWidgetFill(widget.id,std::numeric_limits<float>::quiet_NaN(),
            ScreenFillDirection::LeftToRight,error) && session.Revision()==beforeInvalid,
        "Invalid fill partially mutated authoring state");

    ScreenBinding fillBinding;
    fillBinding.property=ScreenBindingProperty::Fill;
    fillBinding.source.attributeId="health.current";
    fillBinding.denominator.attributeId="health.maximum";
    fillBinding.operation=ScreenBindingOperation::Ratio;
    check(session.UpdateWidgetBindings(widget.id,{fillBinding},error) && session.Save(error),
        "Progress rejected explicit ratio binding");
    check(ReadScreenDocument(path.generic_u8string(),project,reopened,error) &&
        reopened.widgets[0].bindings.size()==1,"Progress binding did not round-trip");
    document.widgets[0].fillDirection=static_cast<ScreenFillDirection>(99);
    check(!ValidateScreenDocument(document,project,error),"Unknown fill direction was accepted");
    if (failures) return 1;
    std::cout << "PASS: Progress geometry, viewport parity, passive Runtime, persistence, history and ratio binding\n";
}
