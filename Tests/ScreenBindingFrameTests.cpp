#include <chrono>
#include "renegade/bridge/ScreenLiveBinding.h"
#include "../Studio/src/ScreenRuntimeLiveProbe.h"
#include "renegade/bridge/ScreenBindingFrame.h"
#include <iostream>
using namespace renegade::bridge;
int main()
{
    int failures=0;
    const auto check=[&](bool valid,const char* message) {
        if (!valid) { ++failures; std::cerr<<message<<'\n'; }
    };
    ScreenDocument document;
    document.envelope=CreateDocumentEnvelope(GenerateStableId(),RuntimeScreenDocumentType,
        "Content/UI/Frame.renegade-screen","Binding frame test");
    document.usage=ScreenUsage::Hud;
    ScreenWidget panel;
    panel.id=GenerateStableId();panel.name="Panel";panel.kind=ScreenWidgetKind::Image;
    panel.rect={0,0,600,300};
    ScreenBinding visible;
    visible.property=ScreenBindingProperty::Visible;
    visible.source.attributeId="alive";
    panel.bindings={visible};
    ScreenWidget text;
    text.id=GenerateStableId();text.name="Health";text.text="Fallback";text.parentId=panel.id;
    text.rect={10,10,300,50};text.style=MakeScreenWidgetStyleTemplate(text.kind,50);
    ScreenBinding label;
    label.source.attributeId="health.current";label.prefix="Health ";label.decimalPlaces=1;
    text.bindings={label};
    ScreenWidget progress;
    progress.id=GenerateStableId();progress.name="Bar";progress.kind=ScreenWidgetKind::Progress;
    progress.rect={100,400,300,30};progress.style=MakeScreenWidgetStyleTemplate(progress.kind,30);
    ScreenBinding fill;
    fill.property=ScreenBindingProperty::Fill;fill.source.attributeId="health.current";
    fill.denominator.attributeId="health.maximum";fill.operation=ScreenBindingOperation::Ratio;
    fill.missingPolicy=ScreenBindingMissingPolicy::Hide;
    progress.bindings={fill,visible};
    document.widgets={panel,text,progress};
    ScreenBindingSnapshot snapshot{77,12,{
        {visible.source,false,9},{fill.source,25.0,9},{fill.denominator,100.0,9}}};
    ScreenBindingContext context;
    context.session=77;
    context.generations[ {ScreenBindingScope::PossessedPlayer,{}} ]=9;
    ScreenBindingFrame frame;
    std::string error;
    check(BuildScreenBindingFrame(document,snapshot,context,frame,error),"Coherent frame failed");
    if (frame.widgets.size()==3)
    {
        check(frame.ready && frame.issues.empty(),"Valid bindings produced readiness errors");
        check(frame.widgets[1].text=="Health 25.0","Frame text formatter failed");
        check(!frame.widgets[0].visible && !frame.widgets[1].visible,
            "A child's own state revealed it through its hidden parent");
        check(frame.widgets[2].fill==.25f,"Frame ratio did not resolve Progress fill");
    }
    else check(false,"Frame lost widget identities");
    snapshot.values[0].value=true;
    snapshot.values.pop_back();
    check(BuildScreenBindingFrame(document,snapshot,context,frame,error),"Missing-source fallback frame failed");
    if (frame.widgets.size()==3)
        check(!frame.ready && frame.issues.size()==1 && !frame.widgets[2].visible &&
            frame.widgets[2].fill==.5f,
            "A later true visibility binding defeated the missing-source hide policy");
    check(document.widgets[1].text=="Fallback" && document.widgets[2].fill==.5f &&
        document.widgets[2].visible,"Live frame mutated the authored fallback document");
    snapshot.values.push_back({fill.denominator,100.0,9});
    snapshot.session=76;
    check(BuildScreenBindingFrame(document,snapshot,context,frame,error) && !frame.ready &&
        !frame.issues.empty() && frame.issues[0].diagnostic==ScreenBindingDiagnostic::StaleSession,
        "Old-session snapshot was not diagnosed");
    snapshot.session=77;
    snapshot.values[1].generation=8;
    check(BuildScreenBindingFrame(document,snapshot,context,frame,error) && !frame.ready,
        "Replaced entity generation was accepted");
    snapshot.values.clear();
    for (auto& widget:document.widgets)
        for (auto& binding:widget.bindings) binding.required=false;
    check(BuildScreenBindingFrame(document,snapshot,context,frame,error) && frame.ready &&
        !frame.issues.empty(),"Optional missing sources incorrectly blocked readiness");
    const auto previousDocument=frame.documentId;
    document.widgets[2].bindings.push_back(fill);
    check(!BuildScreenBindingFrame(document,snapshot,context,frame,error) &&
        frame.documentId==previousDocument,"Malformed document partially replaced a resolved frame");
    // Exercise the supported 256-widget document limit, not a mock render
    // loop. All labels read the same actual immutable snapshot generation.
    ScreenDocument budget=document;budget.widgets.clear();
    auto root=panel;root.id=GenerateStableId();root.rect={0,0,1280,720};
    root.container={ScreenChildLayout::Grid,0,0,16};budget.widgets.push_back(root);
    for(int n=0;n<255;++n)
    {
        auto child=text;child.id=GenerateStableId();child.parentId=root.id;
        child.rect={0,0,100,40};budget.widgets.push_back(std::move(child));
    }
    snapshot={77,20,{{visible.source,true,9},{label.source,25.0,9}}};
    check(ValidateScreenDocument(budget,budget.envelope.projectId,error),
        "Supported widget-limit document was rejected");
    const auto started=std::chrono::steady_clock::now();
    for(int repeat=0;repeat<20;++repeat)
        check(BuildScreenBindingFrame(budget,snapshot,context,frame,error)&&frame.ready&&
            frame.widgets.size()==256&&frame.widgets.back().text=="Health 25.0",
            "Supported widget-limit bindings failed");
    const auto ended=std::chrono::steady_clock::now();
    std::cout<<"BINDING_FRAME_CPU_US widgets=256 mean="
        <<std::chrono::duration<double,std::micro>(ended-started).count()/20<<"\n";
    ScreenLiveBinding live{snapshot,context},decoded;
    std::string wire;
    check(EncodeScreenLiveBinding(live,wire,error)&&DecodeScreenLiveBinding(wire,decoded,error)&&
        decoded.snapshot.values.size()==snapshot.values.size()&&decoded.snapshot.session==77,
        "Live snapshot round trip failed");
    const auto retained=decoded.snapshot.revision;
    check(!DecodeScreenLiveBinding(wire+"00",decoded,error)&&decoded.snapshot.revision==retained,
        "Malformed live snapshot mutated destination");
    live.snapshot.values.push_back(live.snapshot.values.front());
    check(!EncodeScreenLiveBinding(live,wire,error),"Duplicate live source accepted");
    live={snapshot,context};live.snapshot.values.front().value=std::numeric_limits<double>::infinity();
    check(!EncodeScreenLiveBinding(live,wire,error),"Nonfinite live source accepted");
    live={snapshot,context};live.snapshot.values.front().value=std::string("UTF8 \xc3\xa9 {quoted}");
    check(EncodeScreenLiveBinding(live,wire,error)&&DecodeScreenLiveBinding(wire,decoded,error),
        "UTF8 live text rejected");
    check(renegade::studio::LiveJsonObject("{\"x\":{\"text\":\"}\",\"v\":1}}","x")==
        "\"text\":\"}\",\"v\":1","Quoted closing brace broke bounded JSON group");
    using namespace renegade::studio;
    std::string packet="{\"schema\":\"renegade.diagnostics.v2\",\"elapsed_ms\":1000,"
        "\"transport_alive\":true,\"application_heartbeat_age_ms\":0,"
        "\"state\":{\"process\":{\"type\":\"runtime\",\"pid\":44,\"started_unix_ms\":10},"
        "\"ui\":{\"project_id\":\"test-project\"},\"ui_live_binding\":{"
        "\"available\":true,\"published_elapsed_ms\":900,\"chunk_count\":";
    const auto chunks=(wire.size()+959)/960;
    packet+=std::to_string(chunks);
    for(std::size_t n=0;n<chunks;++n)packet+=",\"chunk_"+std::to_string(n)+"\":\""+wire.substr(n*960,960)+"\"";
    packet+="}}}";
    auto liveStatus=ProbeScreenRuntimeLive(packet,"test-project");
    check(ReadScreenRuntimeLiveBinding(packet,liveStatus,decoded,error),"Coherent matching live packet rejected");
    liveStatus.projectMatched=false;
    check(!ReadScreenRuntimeLiveBinding(packet,liveStatus,decoded,error),"Foreign project values accepted");
    liveStatus.projectMatched=true;liveStatus.gameplayFresh=false;
    check(!ReadScreenRuntimeLiveBinding(packet,liveStatus,decoded,error),"Frozen gameplay values accepted");
    liveStatus.gameplayFresh=true;
    auto stalePacket=packet;const auto published=stalePacket.find("\"published_elapsed_ms\":900");
    stalePacket.replace(published,26,"\"published_elapsed_ms\":9999");
    check(!ReadScreenRuntimeLiveBinding(stalePacket,liveStatus,decoded,error),"Future snapshot accepted");
    if (failures) return 1;
    std::cout<<"PASS: immutable resolved UI frames, hierarchy, readiness, fallbacks and lifecycle diagnostics\n";
}
