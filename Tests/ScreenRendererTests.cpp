#include <chrono>
#include "renegade/screen/ScreenRenderer.h"
#include "renegade/screen/ScreenTextLayout.h"
#include "renegade/bridge/ScreenWidgetPresets.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    bool Near(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "RenegadeScreenRendererTests: " << message << '\n';
        return 1;
    }
}

int main()
{
    using namespace renegade;

    screen::ScreenRenderer inputModeProbe;
    if (!inputModeProbe.IsInputEnabled())
        return Fail("Screen renderer Runtime input must default enabled");
    inputModeProbe.SetInputEnabled(false);
    if (inputModeProbe.IsInputEnabled())
        return Fail("Screen renderer authoring input suppression did not latch");
    inputModeProbe.SetInputEnabled(true);
    if (!inputModeProbe.IsInputEnabled())
        return Fail("Screen renderer input could not be restored for Runtime use");

    bridge::ScreenDocument document;
    document.envelope = bridge::CreateDocumentEnvelope(
        bridge::GenerateStableId(),
        bridge::RuntimeScreenDocumentType,
        "Content/UI/Renderer.renegade-screen",
        "Gate 8B shared renderer proof");
    document.designWidth = 1280.0f;
    document.designHeight = 720.0f;

    bridge::ScreenWidget background;
    background.id = bridge::GenerateStableId();
    background.kind = bridge::ScreenWidgetKind::Image;
    background.name = "Background";
    background.rect = {0.0f, 0.0f, 1280.0f, 720.0f};
    background.enabled = false;
    background.resourcePath = "Content/UI/background.png";
    background.style = bridge::MakeScreenWidgetStyleTemplate(
        background.kind, background.rect.height);

    bridge::ScreenWidget button;
    button.id = bridge::GenerateStableId();
    button.kind = bridge::ScreenWidgetKind::Button;
    button.name = "Continue";
    button.rect = {440.0f, 320.0f, 400.0f, 80.0f};
    button.text = "CONTINUE";
    button.actionId = "continue";
    button.style = bridge::MakeScreenWidgetStyleTemplate(
        button.kind, button.rect.height);
    button.style.borderColor = {10, 20, 30, 255};
    button.style.borderWidth = 2.0f;
    button.style.cornerRadius = 12.0f;
    button.style.text.fontSize = 24.0f;
    button.style.text.characterSpacing = 2.0f;
    button.style.text.lineSpacing = 4.0f;
    button.style.text.shadowOffsetX = 3.0f;
    button.style.text.shadowOffsetY = 5.0f;
    button.style.normal.background = {1, 2, 3, 255};
    button.style.hover.background = {4, 5, 6, 255};
    button.style.pressed.background = {7, 8, 9, 255};
    button.style.focused.background = {10, 11, 12, 255};
    button.style.disabled.background = {13, 14, 15, 255};

    const auto measure=[](const std::string& text)
    {
        std::size_t count=0;
        for(std::size_t i=0;i<text.size();i=screen::ScreenTextCodepointEnd(text,i))++count;
        return float(count*10);
    };
    const auto wrapped=screen::WrapScreenText("alpha beta gamma",100,measure);
    if(wrapped.size()!=2||wrapped[0].text!="alpha beta"||wrapped[1].text!="gamma")
        return Fail("word wrapping lost or split a fitting word");
    const auto explicitLines=screen::WrapScreenText("one\r\n\ntwo",100,measure);
    if(explicitLines.size()!=3||explicitLines[1].text!=""||explicitLines[2].text!="two")
        return Fail("explicit blank lines/CRLF not preserved");
    const std::string unicode="a\xC3\xA9\xC3\xA9z";
    const auto longWord=screen::WrapScreenText(unicode,20,measure);
    if(longWord.size()!=2||longWord[0].text+longWord[1].text!=unicode||
        longWord[0].width>20||longWord[1].width>20)
        return Fail("long word wrapping damaged UTF-8 boundaries");
    const auto center=screen::ResolveScreenTextLineOrigin({30,40,100,120},80,80,0,
        bridge::ScreenHorizontalAlignment::Center,bridge::ScreenVerticalAlignment::Center);
    const auto overflow=screen::ResolveScreenTextLineOrigin({30,40,100,120},180,180,0,
        bridge::ScreenHorizontalAlignment::Right,bridge::ScreenVerticalAlignment::Bottom);
    if(center.x!=40||center.y!=60||overflow.x!=30||overflow.y!=40)
        return Fail("wrapped alignment hid the beginning of overflowing text");
    // Presentation transforms are reversible and do not modify project/theme state.
    const auto accessible=bridge::ResolveAccessibleScreenStyle(button.style,button.kind,{2,3,1,4});
    if(!Near(accessible.text.fontSize,48)||!accessible.text.wrap||
        accessible.normal.foreground.red!=255||accessible.normal.foreground.green!=255||
        accessible.normal.foreground.blue!=0||accessible.normal.background.red!=0||
        !Near(button.style.text.fontSize,24)||button.style.normal.background.red!=1)
        return Fail("accessible typography/contrast changed authored content or failed to resolve");
    const auto authoredAgain=bridge::ResolveAccessibleScreenStyle(button.style,button.kind,{});
    if(!Near(authoredAgain.text.fontSize,24)||authoredAgain.normal.background.red!=1)
        return Fail("returning to authored accessibility mode failed");
    if(bridge::ValidateScreenAccessibilitySettings({0,0,0,2})||
        bridge::ValidateScreenAccessibilitySettings({1,4,0,2})||
        bridge::ValidateScreenAccessibilitySettings({1,0,3,2})||
        bridge::ValidateScreenAccessibilitySettings({1,0,0,0}))
        return Fail("invalid accessibility policy accepted");
    const auto focus=bridge::ScreenAccessibleFocusColor({1,0,2,2});
    if(focus.red!=0||focus.green!=255||focus.blue!=255)
        return Fail("custom focus colour not resolved");
    document.actions = {{"continue"}};
    document.widgets = {background, button};
    document.focusOrder = {button.id};

    std::unordered_map<bridge::StableId, screen::ScreenInteractionState> states;
    states.emplace(button.id, screen::ScreenInteractionState::Focused);
    std::vector<screen::ScreenRenderItem> items;
    std::string error;
    if (!screen::BuildScreenRenderItems(
            document, 1920.0f, 1080.0f, states, items, error))
        return Fail("valid render frame was rejected");
    if (items.size() != 2 || items[0].widgetId != background.id ||
        items[1].widgetId != button.id)
        return Fail("authored back-to-front order was not preserved");

    {
        auto moved=items;
        if(!screen::ApplyScreenTransitionFrame(document,{0,0,1920,1080},{.5f,20,0,.8f},moved,error)||
            !Near(moved[1].logicalRect.x,750)||!Near(moved[1].logicalRect.y,492)||
            !Near(moved[1].logicalRect.width,480)||!Near(moved[1].logicalRect.height,96)||
            !Near(moved[1].fontSize,28.8f)||!Near(moved[1].opacity,.5f)||
            !Near(items[1].logicalRect.x,660))
            return Fail("Screen transition drawing/hit geometry or authored isolation failed");
        const auto before=moved[1].logicalRect;
        if(screen::ApplyScreenTransitionFrame(document,{0,0,1920,1080},{2,0,0,1},moved,error)||
            !Near(before.x,moved[1].logicalRect.x))
            return Fail("invalid transition frame partially changed hit geometry");
    }
    const auto& rendered = items[1];
    if (!Near(rendered.logicalRect.x, 660.0f) ||
        !Near(rendered.logicalRect.y, 480.0f) ||
        !Near(rendered.logicalRect.width, 600.0f) ||
        !Near(rendered.logicalRect.height, 120.0f) ||
        !Near(rendered.scaleX, 1.5f) || !Near(rendered.scaleY, 1.5f) ||
        !Near(rendered.borderWidth, 3.0f) ||
        !Near(rendered.cornerRadius, 18.0f) ||
        !Near(rendered.fontSize, 36.0f) ||
        !Near(rendered.characterSpacing, 3.0f) ||
        !Near(rendered.lineSpacing, 6.0f) ||
        !Near(rendered.shadowOffsetX, 4.5f) ||
        !Near(rendered.shadowOffsetY, 7.5f))
        return Fail("geometry, border or typography did not share one scale");
    if (rendered.interaction != screen::ScreenInteractionState::Focused ||
        rendered.visual.background.red != 10)
        return Fail("focused visual state was not selected");

    // Shared frame visibility must reflect every ancestor, not only the
    // locally visible child. Runtime's ApplyLayout consumes this value for
    // both drawing and input on dynamic Group hide/show.
    {
        auto grouped=document;
        auto parent=bridge::MakeScreenWidgetPreset(
            bridge::ScreenWidgetPreset::VerticalStack,1280,720);
        parent.id=bridge::GenerateStableId();
        parent.name="Visibility Group";parent.rect={440,320,500,140};
        parent.visible=false;
        grouped.widgets[1].parentId=parent.id;
        grouped.widgets[1].rect={20,20,400,80};
        grouped.widgets.insert(grouped.widgets.begin()+1,parent);
        std::vector<screen::ScreenRenderItem> groupedItems;
        if(!screen::BuildScreenRenderItems(grouped,1280,720,states,groupedItems,error)||
            groupedItems.size()!=3||groupedItems.back().visible)
            return Fail("hidden Group descendant rendered as visible");
        grouped.widgets[1].visible=true;
        if(!screen::BuildScreenRenderItems(grouped,1280,720,states,groupedItems,error)||
            !groupedItems.back().visible)
            return Fail("showing Group failed to restore child presentation");
    }

    const bridge::ScreenRect editorViewport{300.0f, 200.0f, 960.0f, 540.0f};
    if (!screen::BuildScreenRenderItemsInViewport(
            document, editorViewport, states, items, error))
        return Fail("valid Screen Editor preview viewport was rejected");
    const auto& previewButton = items[1];
    if (!Near(previewButton.logicalRect.x, 630.0f) ||
        !Near(previewButton.logicalRect.y, 440.0f) ||
        !Near(previewButton.logicalRect.width, 300.0f) ||
        !Near(previewButton.logicalRect.height, 60.0f) ||
        !Near(previewButton.scaleX, 0.75f) ||
        !Near(previewButton.scaleY, 0.75f))
        return Fail("Screen Editor viewport diverged from shared render geometry");

    const bridge::ScreenRect invalidViewport{0.0f, 0.0f, 0.0f, 540.0f};
    if (screen::BuildScreenRenderItemsInViewport(
            document, invalidViewport, states, items, error))
        return Fail("invalid Screen Editor preview viewport was accepted");

    states[button.id] = screen::ScreenInteractionState::Pressed;
    if (!screen::BuildScreenRenderItems(
            document, 1280.0f, 720.0f, states, items, error) ||
        items[1].visual.background.red != 7)
        return Fail("pressed visual state was not selected");

    document.widgets[1].enabled = false;
    states[button.id] = screen::ScreenInteractionState::Hover;
    if (!screen::BuildScreenRenderItems(
            document, 1280.0f, 720.0f, states, items, error) ||
        items[1].interaction != screen::ScreenInteractionState::Disabled ||
        items[1].visual.background.red != 13)
        return Fail("disabled state did not override pointer state exactly");
    if (items[0].interaction == screen::ScreenInteractionState::Disabled)
        return Fail("non-interactive image was incorrectly faded as disabled");

    bridge::ScreenDocument packagedFixture;
    const std::filesystem::path fixturePath =
        std::filesystem::path(RENEGADE_SOURCE_DIR) /
        "Runtime/fixtures/LP03/Valid Screen/Content/UI/Main.renegade-screen";
    if (!bridge::ReadScreenDocument(
            fixturePath.generic_u8string(),
            "61111111-1111-4111-8111-111111111111",
            packagedFixture,
            error))
        return Fail("packaged visual fixture is not a valid schema-v2 Screen");
    if (packagedFixture.schemaVersion != 2 ||
        packagedFixture.widgets.size() != 4 ||
        packagedFixture.widgets[1].style.borderWidth <= 0.0f ||
        packagedFixture.widgets[2].style.borderWidth <= 0.0f ||
        packagedFixture.widgets[2].style.focused.background.red ==
            packagedFixture.widgets[2].style.hover.background.red)
        return Fail("packaged fixture does not visibly prove Gate 8B fidelity");


    document.schemaVersion=3;document.usage=bridge::ScreenUsage::Hud;
    document.widgets[1].parentId=background.id;
    document.widgets[0].visible=false;
    if(!screen::BuildScreenRenderItems(document,1280,720,states,items,error)||
       items[1].visible)
        return Fail("hidden container did not hide its descendant in shared render frame");
    document.widgets[0].visible=true;
    if(!screen::BuildScreenRenderItems(document,1280,720,states,items,error)||
       !items[1].visible)
        return Fail("restored container failed to restore visible descendant");


    document.widgets[0].container.mode=bridge::ScreenChildLayout::Horizontal;
    document.widgets[0].container.padding=20;
    document.widgets[0].container.gap=10;
    document.widgets[1].enabled=true;
    if(!screen::BuildScreenRenderItems(document,1280,720,states,items,error)||
       !Near(items[1].logicalRect.x,20)||!Near(items[1].logicalRect.y,20)||
       !Near(items[1].logicalRect.width,1240)||!Near(items[1].logicalRect.height,680))
        return Fail("container cell diverged from shared render/hit geometry");


    // Animation evaluation, inherited transforms and actual pointer rectangles share one path.
    using namespace bridge;
    ScreenAnimationTrack track;track.easing=ScreenAnimationEasing::Linear;track.delay=.25;
    track.keys={{0,0},{1,1}};
    if(!Near(EvaluateScreenAnimation(track,0),0)||!Near(EvaluateScreenAnimation(track,.75),.5f)||
       !Near(EvaluateScreenAnimation(track,5),1)||!Near(EvaluateScreenAnimation(track,0,true),1))
        return Fail("animation delay/interpolation/hold/reduced motion");
    track.loop=ScreenAnimationLoop::Repeat;
    if(!Near(EvaluateScreenAnimation(track,1.5),.25f))return Fail("repeat clock");
    track.loop=ScreenAnimationLoop::PingPong;
    if(!Near(EvaluateScreenAnimation(track,1.5),.75f))return Fail("ping pong clock");
    track.loop=ScreenAnimationLoop::Once;track.delay=0;track.easing=ScreenAnimationEasing::EaseIn;
    if(!Near(EvaluateScreenAnimation(track,.5),.25f))return Fail("easing curve");
    std::vector<ScreenAnimationKey> parsedKeys;
    if(!ParseAnimationKeys("0:0;0.5:1;1:0",parsedKeys)||parsedKeys.size()!=3||
        ParseAnimationKeys("0:0;1:1;",parsedKeys)||ParseAnimationKeys("0:0junk;1:1",parsedKeys))
        return Fail("strict keyframe input parser");
    ScreenDocument animDoc=document;animDoc.widgets={background,button};
    animDoc.widgets[0].resourcePath="";
    animDoc.widgets[1].parentId=background.id;
    ScreenAnimationTrack parentOffset;parentOffset.property=ScreenAnimationProperty::OffsetX;
    parentOffset.keys={{0,0},{1,100}};
    ScreenAnimationTrack parentOpacity;parentOpacity.keys={{0,0},{1,1}};
    animDoc.widgets[0].animations={parentOffset,parentOpacity};
    ScreenAnimationTrack childScale;childScale.property=ScreenAnimationProperty::Scale;
    childScale.keys={{0,1},{1,2}};
    animDoc.widgets[1].animations={childScale};
    if(!screen::BuildAnimatedScreenRenderItems(animDoc,{10,20,1280,720},.5,false,states,items,error)||
       !Near(items[1].logicalRect.x,400)||!Near(items[1].logicalRect.y,320)||
       !Near(items[1].logicalRect.width,600)||!Near(items[1].logicalRect.height,120)||
       !Near(items[1].opacity,.5f)||!Near(items[1].fontSize,36))
        return Fail("animated parent/child geometry/opacity/font projection");
    if(!screen::BuildAnimatedScreenRenderItems(animDoc,{10,20,1280,720},0,true,states,items,error)||
        !Near(items[1].logicalRect.width,800)||!Near(items[1].opacity,1))
        return Fail("shared reduced motion did not resolve final state");
    if(screen::BuildAnimatedScreenRenderItems(animDoc,{0,0,1280,720},-1,false,states,items,error))
        return Fail("invalid animation clock accepted");


    auto slider=bridge::MakeScreenWidgetPreset(bridge::ScreenWidgetPreset::Slider,1280,720);
    slider.id=bridge::GenerateStableId();slider.enabled=false;slider.control.value=.75;
    auto sliderDoc=document;sliderDoc.widgets={slider};
    if(!screen::BuildScreenRenderItems(sliderDoc,1280,720,{},items,error)||
       items.size()!=1||!Near(items.front().controlFraction,.75f)||
       items.front().interaction!=screen::ScreenInteractionState::Disabled||
       std::get<double>(items.front().controlValue)!=.75)
        return Fail("shared control value/disabled-state projection");
    sliderDoc.widgets.front().control.value=std::string("bad");
    if(screen::BuildScreenRenderItems(sliderDoc,1280,720,{},items,error))
        return Fail("shared renderer accepted invalid control value type");

    // Bulk geometry must preserve paint order, nested parent semantics and
    // old frames on failure, rather than merely timing a synthetic fast path.
    ScreenDocument bulk=document;bulk.widgets.clear();
    auto group=background;group.id="bulk-root";group.parentId="";
    group.rect={0,0,1280,720};group.container={ScreenChildLayout::Grid,8,4,32};
    bulk.widgets.push_back(group);
    for(int n=0;n<512;++n)
    {
        auto child=button;child.id="bulk-"+std::to_string(n);child.parentId=group.id;
        child.sizeConstraints={};bulk.widgets.push_back(child);
    }
    std::vector<ScreenRect> rects;
    if(!ResolveScreenWidgetRects(bulk,rects,error)||rects.size()!=bulk.widgets.size())
        return Fail("bulk geometry failed");
    const auto started=std::chrono::steady_clock::now();
    for(std::size_t n=0;n<bulk.widgets.size();++n)
    {
        ScreenRect legacy;
        if(!ResolveScreenWidgetRect(bulk,bulk.widgets[n].id,legacy,error)||
            !Near(legacy.x,rects[n].x)||!Near(legacy.y,rects[n].y)||
            !Near(legacy.width,rects[n].width)||!Near(legacy.height,rects[n].height))
            return Fail("bulk/individual layout diverged");
    }
    const auto individual=std::chrono::steady_clock::now();
    for(int repeat=0;repeat<20;++repeat)
        if(!ResolveScreenWidgetRects(bulk,rects,error))return Fail("repeated bulk layout");
    const auto ended=std::chrono::steady_clock::now();
    std::cout<<"LAYOUT_CPU_US widgets="<<bulk.widgets.size()
        <<" individual="<<std::chrono::duration<double,std::micro>(individual-started).count()
        <<" bulk="<<std::chrono::duration<double,std::micro>(ended-individual).count()/20<<"\n";
    bulk.widgets.front().container.compactBelowWidth=1400;
    bulk.widgets.front().container.compactMode=ScreenChildLayout::Grid;
    bulk.widgets.front().container.compactColumns=16;
    std::vector<ScreenRect> narrow,wide;
    if(!ResolveScreenWidgetRects(bulk,narrow,error,1366)||
        !ResolveScreenWidgetRects(bulk,wide,error,1920)||
        !Near(narrow[1].width,75.25f)||!Near(wide[1].width,35.625f))
        return Fail("compact breakpoint geometry");
    if(!screen::BuildScreenRenderItems(bulk,1366,768,{},items,error)||
        !Near(items[1].logicalRect.width,narrow[1].width*std::min(1366.0f/1280.0f,768.0f/720.0f)))
        return Fail("Runtime compact geometry projection");
    if(!screen::BuildScreenRenderItemsInViewport(bulk,{10,20,600,338},{},items,error,1920)||
        !Near(items[1].logicalRect.width,wide[1].width*(600.0f/1280.0f)))
        return Fail("preview logical width coupled to zoom");
    auto badLayout=bulk.widgets.front().container;badLayout.compactBelowWidth=-1;
    if(ValidateScreenContainerLayout(badLayout,error))return Fail("negative breakpoint accepted");
    const auto previous=rects;bulk.widgets[1].parentId=bulk.widgets[1].id;
    if(ResolveScreenWidgetRects(bulk,rects,error)||rects.size()!=previous.size()||
        !Near(rects[1].x,previous[1].x))return Fail("bulk failure replaced previous frame");
    bulk.widgets[1].parentId=group.id;bulk.widgets[1].id=group.id;
    if(ResolveScreenWidgetRects(bulk,rects,error))return Fail("bulk duplicate identity accepted");

    bridge::ScreenDocument accessDocument;
    accessDocument.envelope=document.envelope;
    accessDocument.actions={{"continue"}};
    auto small=button;small.id=bridge::GenerateStableId();small.rect={50,50,20,20};
    small.layoutMode=bridge::ScreenLayoutMode::Anchored;
    small.anchors={.5f,.5f,.5f,.5f,-10,-10,10,10};
    small.style=bridge::MakeScreenWidgetStyleTemplate(small.kind,20);
    accessDocument.widgets={small};accessDocument.focusOrder={small.id};
    bridge::ScreenAccessibilitySettings access;
    if(!screen::BuildAccessibleScreenRenderItems(accessDocument,{0,0,1280,720},access,{},{},{},items,error)||
        items.size()!=1||!Near(items[0].logicalRect.width,20)||!Near(items[0].logicalRect.x,630))
        return Fail("default accessibility geometry changed authored bounds");
    access.uiScale=1.5f;
    if(!screen::BuildAccessibleScreenRenderItems(accessDocument,{0,0,1280,720},access,{},{},{},items,error)||
        !Near(items[0].logicalRect.width,30)||!Near(items[0].logicalRect.x,625)||
        !Near(items[0].scaleX,1.5f))
        return Fail("UI scale did not enlarge and reflow anchored geometry");
    access.minimumTargetSize=56;
    if(!screen::BuildAccessibleScreenRenderItems(accessDocument,{0,0,1280,720},access,{},{},{},items,error)||
        !Near(items[0].logicalRect.width,56)||!Near(items[0].logicalRect.height,56)||
        !Near(items[0].logicalRect.x,612))
        return Fail("accessDocument visible interaction target geometry mismatch");
    if(accessDocument.designWidth!=1280||accessDocument.widgets[0].rect.width!=20)
        return Fail("accessibility modified authored document");
    access.uiScale=3;
    if(screen::BuildAccessibleScreenRenderItems(accessDocument,{0,0,1280,720},access,{},{},{},items,error))
        return Fail("invalid UI scale accepted");

    std::cout << "PASS: Gate 8B shared Screen render contract\n";
    return 0;
}
